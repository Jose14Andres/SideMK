#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PumpCurve.h"

namespace
{
    constexpr float  kNoiseFloorThreshold = 1.0e-6f;
    constexpr double kSmoothingSeconds    = 0.02; // 20 ms parameter ramps
} // namespace

double SidechainCompressorAudioProcessor::rateToPpq (int rateIndex) noexcept
{
    switch (rateIndex)
    {
        case rate1_16: return 0.25;
        case rate1_8:  return 0.5;
        case rate1_4:  return 1.0;
        case rate1_2:  return 2.0;
        case rate1Bar: return 4.0;
        default:       return 1.0;
    }
}

//==============================================================================
SidechainCompressorAudioProcessor::SidechainCompressorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          // Bus 0 — main stereo in/out (required)
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          // Bus 1 — sidechain stereo in (optional; false = disabled by default)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Cache raw atomic pointers — reads from the audio thread are lock-free
    thresholdParam = apvts.getRawParameterValue ("threshold");
    ratioParam     = apvts.getRawParameterValue ("ratio");
    attackParam    = apvts.getRawParameterValue ("attack");
    releaseParam   = apvts.getRawParameterValue ("release");
    gainParam      = apvts.getRawParameterValue ("gain");
    modeParam      = apvts.getRawParameterValue ("mode");
    rateParam      = apvts.getRawParameterValue ("rate");
    depthParam     = apvts.getRawParameterValue ("depth");
}

SidechainCompressorAudioProcessor::~SidechainCompressorAudioProcessor() {}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
SidechainCompressorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Threshold: linear dB range
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "threshold", 1 }, "Threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -12.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // Ratio: skewed so lower ratios get more slider real-estate
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ratio", 1 }, "Ratio",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f, 0.5f), 4.0f));

    // Attack: log-skewed ms
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 }, "Attack",
        juce::NormalisableRange<float> (0.1f, 500.0f, 0.1f, 0.4f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    // Release: log-skewed ms
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release", 1 }, "Release",
        juce::NormalisableRange<float> (1.0f, 2000.0f, 1.0f, 0.4f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    // Makeup Gain
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gain", 1 }, "Makeup Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // --- Pump mode (added in parameter version 2) ---------------------------------
    // Compressor keeps the original behaviour and stays the default, so existing
    // sessions load unchanged.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 2 }, "Mode",
        juce::StringArray { "Compressor", "Pump" }, modeCompressor));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "rate", 2 }, "Pump Rate",
        juce::StringArray { "1/16", "1/8", "1/4", "1/2", "1 Bar" }, rate1_4));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "depth", 2 }, "Pump Depth",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    return { params.begin(), params.end() };
}

//==============================================================================
void SidechainCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Mono spec: the compressor runs a single-channel envelope detector
    // that is fed the mixed sidechain signal (ch 0 only).
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 1;

    compressor.prepare (spec);
    compressor.reset();

    makeupSmoothed.reset (sampleRate, kSmoothingSeconds);
    makeupSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (gainParam->load (std::memory_order_relaxed)));

    depthSmoothed.reset (sampleRate, kSmoothingSeconds);
    depthSmoothed.setCurrentAndTargetValue (depthParam->load (std::memory_order_relaxed) * 0.01f);

    pumpPhase = 0.0;
}

void SidechainCompressorAudioProcessor::releaseResources()
{
    compressor.reset();
}

//==============================================================================
bool SidechainCompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Main I/O must be stereo
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;

    // Sidechain (input bus 1) must be stereo or completely disabled
    const auto& sc = layouts.getChannelSet (true, 1);
    if (!sc.isDisabled() && sc != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

//==============================================================================
//  processBlock — NO heap allocation, NO locks
//
//  Buffer layout when sidechain is active:
//    ch 0  →  Main L   (read + write)
//    ch 1  →  Main R   (read + write)
//    ch 2  →  SC L     (read only)
//    ch 3  →  SC R     (read only)
//
//  Two modes, selected by the "mode" parameter:
//
//  Compressor (default, original behaviour)
//    1. Detector = mono mix of SC L+R, or Main L when there is no sidechain
//    2. compressor.processSample() updates the BallisticsFilter and returns
//       (gain * detector); gain = processed / detector
//    3. Apply gain * makeup to Main L and Main R, once per sample
//
//  Pump
//    Gain follows a fixed volume curve that repeats every musical division
//    (see PumpCurve.h). The phase comes from the host transport (PPQ), so the
//    dip lands on the grid; the input level and the sidechain are ignored.
//==============================================================================
void SidechainCompressorAudioProcessor::processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    // --- Lock-free parameter reads (std::atomic<float>) ---
    const auto loadParameter = [] (const std::atomic<float>* parameter)
    {
        return parameter->load (std::memory_order_relaxed);
    };

    makeupSmoothed.setTargetValue (juce::Decibels::decibelsToGain (loadParameter (gainParam)));
    depthSmoothed .setTargetValue (loadParameter (depthParam) * 0.01f);

    const float currentThreshold = loadParameter (thresholdParam);
    if (currentThreshold != lastThreshold)
    {
        compressor.setThreshold (currentThreshold);
        lastThreshold = currentThreshold;
    }

    const float currentRatio = loadParameter (ratioParam);
    if (currentRatio != lastRatio)
    {
        compressor.setRatio (currentRatio);
        lastRatio = currentRatio;
    }

    const float currentAttack = loadParameter (attackParam);
    if (currentAttack != lastAttack)
    {
        compressor.setAttack (currentAttack);
        lastAttack = currentAttack;
    }

    const float currentRelease = loadParameter (releaseParam);
    if (currentRelease != lastRelease)
    {
        compressor.setRelease (currentRelease);
        lastRelease = currentRelease;
    }

    const int mode = juce::roundToInt (loadParameter (modeParam)) == modePump ? modePump
                                                                              : modeCompressor;
    if (mode != lastMode)
    {
        // Do not resume compression from an envelope that froze while pumping.
        if (mode == modeCompressor)
            compressor.reset();

        lastMode = mode;
    }

    if (buffer.getNumChannels() < 2)
        return; // isBusesLayoutSupported guarantees stereo; stay safe if a host ignores it

    const int    numSamples = buffer.getNumSamples();
    float* const mainL      = buffer.getWritePointer (0);
    float* const mainR      = buffer.getWritePointer (1);

    if (mode == modePump)
    {
        processPump (mainL, mainR, numSamples);
    }
    else
    {
        const bool hasSidechain = (buffer.getNumChannels() >= 4);
        processCompressor (mainL, mainR,
                           hasSidechain ? buffer.getReadPointer (2) : nullptr,
                           hasSidechain ? buffer.getReadPointer (3) : nullptr,
                           numSamples);
    }
    // Note: sidechain channels (2–3) are intentionally left untouched in the output buffer.
    // Hosts treat sidechain input channels as read-only and ignore them in output.
}

//==============================================================================
void SidechainCompressorAudioProcessor::processCompressor (float* mainL, float* mainR,
                                                           const float* scL, const float* scR,
                                                           int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        // Sidechain L+R mixed to mono, or Main L as a plain feedforward compressor
        const float detector = (scL != nullptr) ? 0.5f * (scL[i] + scR[i]) : mainL[i];

        // processSample() updates the internal envelope and returns (gain * detector).
        // Dividing recovers the pure gain scalar without any extra state.
        const float processed  = compressor.processSample (0, detector);
        const float gainFactor = (std::abs (detector) > kNoiseFloorThreshold)
                                     ? (processed / detector)
                                     : 1.0f; // signal below noise floor → no compression

        // Apply compression gain + makeup gain to main channels ONLY, once
        const float totalGain = gainFactor * makeupSmoothed.getNextValue();
        mainL[i] *= totalGain;
        mainR[i] *= totalGain;
    }
}

//==============================================================================
void SidechainCompressorAudioProcessor::processPump (float* mainL, float* mainR, int numSamples)
{
    namespace pump = sidemk::pump;

    const double sampleRate = getSampleRate();
    if (sampleRate <= 0.0)
        return; // not prepared yet

    const int rateIndex = juce::jlimit (0, numRates - 1,
                                        juce::roundToInt (rateParam->load (std::memory_order_relaxed)));
    double divisionPpq = rateToPpq (rateIndex);
    double bpm         = pump::kFallbackBpm;

    bool   synced    = false;
    double hostPpq   = 0.0;
    double originPpq = 0.0;

    // getPlayHead() is only valid inside processBlock; the Standalone app has none.
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto hostBpm = position->getBpm())
                if (*hostBpm > 0.0)
                    bpm = *hostBpm;

            if (rateIndex == rate1Bar)
                if (const auto timeSig = position->getTimeSignature())
                    if (timeSig->numerator > 0 && timeSig->denominator > 0)
                        divisionPpq = 4.0 * timeSig->numerator / timeSig->denominator;

            if (position->getIsPlaying())
            {
                if (const auto ppq = position->getPpqPosition())
                {
                    synced  = true;
                    hostPpq = *ppq;

                    if (rateIndex == rate1Bar)
                        if (const auto barStart = position->getPpqPositionOfLastBarStart())
                            originPpq = *barStart;
                }
            }
        }
    }

    // Transport playing: lock to the host grid every block.
    // Stopped / no host: free-run from where the last block ended.
    double       phase    = synced ? pump::phaseFromPpq (hostPpq, divisionPpq, originPpq)
                                   : pumpPhase;
    const double phaseInc = (bpm / 60.0) / sampleRate / divisionPpq;
    const double drop     = pump::dropFraction (bpm, divisionPpq);

    for (int i = 0; i < numSamples; ++i)
    {
        const double curve = pump::pumpGain (phase, depthSmoothed.getNextValue(), drop,
                                             pump::kDefaultCurveK);
        const float totalGain = static_cast<float> (curve) * makeupSmoothed.getNextValue();
        mainL[i] *= totalGain;
        mainR[i] *= totalGain;

        phase += phaseInc;
        if (phase >= 1.0)
            phase = pump::wrapPhase (phase);
    }

    pumpPhase = phase;
}

//==============================================================================
juce::AudioProcessorEditor* SidechainCompressorAudioProcessor::createEditor()
{
    return new SidechainCompressorAudioProcessorEditor (*this);
}

//==============================================================================
void SidechainCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void SidechainCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SidechainCompressorAudioProcessor();
}
