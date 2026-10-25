#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float kNoiseFloorThreshold = 1.0e-6f;
} // namespace

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

    scFilterActiveParam = apvts.getRawParameterValue ("sc_filter_active");
    scFilterTypeParam   = apvts.getRawParameterValue ("sc_filter_type");
    scFilterCutoffParam = apvts.getRawParameterValue ("sc_filter_cutoff");
    scFilterResParam    = apvts.getRawParameterValue ("sc_filter_res");
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

    // Sidechain Filter Active
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "sc_filter_active", 1 }, "SC Filter Active", false));

    // Sidechain Filter Type (0 = LP, 1 = HP, 2 = BP)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "sc_filter_type", 1 }, "SC Filter Type",
        juce::StringArray { "Low Pass", "High Pass", "Band Pass" }, 0));

    // Sidechain Filter Cutoff
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sc_filter_cutoff", 1 }, "SC Filter Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 1000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    // Sidechain Filter Resonance
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sc_filter_res", 1 }, "SC Filter Resonance",
        juce::NormalisableRange<float> (0.1f, 10.0f, 0.1f), 0.707f));

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

    scFilter.prepare (spec);
    scFilter.reset();
}

void SidechainCompressorAudioProcessor::releaseResources()
{
    compressor.reset();
    scFilter.reset();
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
//  Strategy:
//    1. Mix SC L+R → mono scSample
//    2. Feed scSample into compressor.processSample() → updates the internal
//       BallisticsFilter and returns (gain * scSample)
//    3. Recover gain = processedSC / scSample   (exact; no alloc)
//    4. Apply gain * makeupGain to Main L and Main R only
//==============================================================================
void SidechainCompressorAudioProcessor::processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    // --- Lock-free parameter reads (std::atomic<float>) ---
    const float currentGainParam = gainParam->load(std::memory_order_relaxed);
    if (currentGainParam != cachedGainParam)
    {
        cachedGainParam = currentGainParam;
        cachedMakeupGain = juce::Decibels::decibelsToGain (currentGainParam);
    }
    const float makeupGain = cachedMakeupGain;

    const float currentThreshold = thresholdParam->load(std::memory_order_relaxed);
    if (currentThreshold != lastThreshold)
    {
        compressor.setThreshold (currentThreshold);
        lastThreshold = currentThreshold;
    }

    const float currentRatio = ratioParam->load(std::memory_order_relaxed);
    if (currentRatio != lastRatio)
    {
        compressor.setRatio (currentRatio);
        lastRatio = currentRatio;
    }

    const float currentAttack = attackParam->load(std::memory_order_relaxed);
    if (currentAttack != lastAttack)
    {
        compressor.setAttack (currentAttack);
        lastAttack = currentAttack;
    }

    const float currentRelease = releaseParam->load(std::memory_order_relaxed);
    if (currentRelease != lastRelease)
    {
        compressor.setRelease (currentRelease);
        lastRelease = currentRelease;
    }

    const bool scFilterActive = scFilterActiveParam->load(std::memory_order_relaxed) > 0.5f;

    const float currentScFilterType = scFilterTypeParam->load(std::memory_order_relaxed);
    if (currentScFilterType != lastScFilterType)
    {
        if (currentScFilterType < 0.5f)
            scFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        else if (currentScFilterType < 1.5f)
            scFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        else
            scFilter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);

        lastScFilterType = currentScFilterType;
    }

    const float currentScFilterCutoff = scFilterCutoffParam->load(std::memory_order_relaxed);
    if (currentScFilterCutoff != lastScFilterCutoff)
    {
        scFilter.setCutoffFrequency(currentScFilterCutoff);
        lastScFilterCutoff = currentScFilterCutoff;
    }

    const float currentScFilterRes = scFilterResParam->load(std::memory_order_relaxed);
    if (currentScFilterRes != lastScFilterRes)
    {
        scFilter.setResonance(currentScFilterRes);
        lastScFilterRes = currentScFilterRes;
    }

    const int numSamples = buffer.getNumSamples();

    // Wrap full buffer in an AudioBlock (stack object, zero allocation).
    // getSubsetChannelBlock() returns a lightweight view — just pointer arithmetic.
    juce::dsp::AudioBlock<float> block (buffer);

    const bool hasSidechain = (buffer.getNumChannels() >= 4);

    // Main stereo sub-block (channels 0–1)
    auto        mainBlock = block.getSubsetChannelBlock (0, 2);
    float* const mainL    = mainBlock.getChannelPointer (0);
    float* const mainR    = mainBlock.getChannelPointer (1);

    if (hasSidechain)
    {
        // Sidechain sub-block (channels 2–3) — read-only view
        const auto      scBlock = block.getSubsetChannelBlock (2, 2);
        const float* const scL  = scBlock.getChannelPointer (0);
        const float* const scR  = scBlock.getChannelPointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            // Mix sidechain L+R to mono for detection
            float scSample    = 0.5f * (scL[i] + scR[i]);

            if (scFilterActive)
                scSample = scFilter.processSample(0, scSample);

            // processSample() updates the internal envelope and returns (gain * scSample).
            // Dividing recovers the pure gain scalar without any extra state.
            const float processedSC = compressor.processSample (0, scSample);
            const float gainFactor  = (std::abs (scSample) > kNoiseFloorThreshold)
                                          ? (processedSC / scSample)
                                          : 1.0f; // signal below noise floor → no compression

            // Apply compression gain + makeup gain to main channels ONLY
            const float totalGain = gainFactor * makeupGain;
            mainL[i] *= totalGain;
            mainR[i] *= totalGain;
        }
    }
    else
    {
        // No sidechain bus active → standard feedforward compression on Main L
        // For detection we use a mono mix of the main signal
        for (int i = 0; i < numSamples; ++i)
        {
            float mainSampleMono = 0.5f * (mainL[i] + mainR[i]);

            if (scFilterActive)
                mainSampleMono = scFilter.processSample(0, mainSampleMono);

            const float processed  = compressor.processSample (0, mainSampleMono);
            const float gainFactor = (std::abs (mainSampleMono) > kNoiseFloorThreshold)
                                         ? (processed / mainSampleMono)
                                         : 1.0f;

            const float totalGain = gainFactor * makeupGain;
            mainL[i] *= totalGain;
            mainR[i] *= totalGain;
        }
    }
    // Note: sidechain channels (2–3) are intentionally left untouched in the output buffer.
    // Hosts treat sidechain input channels as read-only and ignore them in output.
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
