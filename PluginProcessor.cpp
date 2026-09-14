#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    return { params.begin(), params.end() };
}

//==============================================================================
void SidechainCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Mono spec: the envelope follower runs a single-channel detector (ch 0) fed the
    // stereo-linked detection signal.
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 1;

    envelopeFilter.prepare (spec);
    envelopeFilter.setLevelCalculationType (
        juce::dsp::BallisticsFilter<float>::LevelCalculationType::peak);
    envelopeFilter.reset();

    // Control layer: 20 ms ramp on makeup gain. All allocation happens here, off the
    // audio thread; processBlock only reads/advances the smoother.
    makeupGainSmoothed.reset (sampleRate, 0.02);
    makeupGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (gainParam->load()));

    // Pre-allocate the per-sample gain scratch buffer so processBlock stays alloc-free.
    gainBuffer.setSize (1, samplesPerBlock, false, false, true);
}

void SidechainCompressorAudioProcessor::releaseResources()
{
    envelopeFilter.reset();
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
//  Pipeline (all state pre-allocated in prepareToPlay):
//    1. Control layer: read APVTS params (lock-free atomics), push makeup gain to the
//       per-sample smoother, feed attack/release to the ballistics filter.
//    2. Detection: pick the sidechain bus (if enabled) or the main bus as the source,
//       stereo-link it to a single positive detector value per sample.
//    3. Gain computer (Mejora 1): run the detector through juce::dsp::BallisticsFilter to
//       get the envelope, then a hard-knee threshold/ratio curve → linear gain. No
//       division: the follower's release governs how gain returns to unity in silence.
//    4. Apply (Mejora 3): the per-sample total gain (compression * smoothed makeup) is
//       written to a scratch buffer and multiplied into each main channel with the
//       SIMD AudioBlock::multiplyBy.
//==============================================================================
void SidechainCompressorAudioProcessor::processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    // --- 1. Control layer: lock-free parameter reads (std::atomic<float>) ---
    envelopeFilter.setAttackTime  (attackParam->load());
    envelopeFilter.setReleaseTime (releaseParam->load());
    const float thresholdDb = thresholdParam->load();
    const float ratio       = ratioParam->load();
    const float slope       = 1.0f / ratio - 1.0f;   // <= 0 : gain-reduction slope in dB

    makeupGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));

    // --- 2. Detection source via bus buffers (robust to channel layout) ---
    auto mainBuffer = getBusBuffer (buffer, false, 0);           // main out (ch 0–1), in place
    float* const mainL = mainBuffer.getWritePointer (0);
    float* const mainR = mainBuffer.getWritePointer (1);

    auto*      scBus        = getBus (true, 1);
    const bool useSidechain = (scBus != nullptr) && scBus->isEnabled();

    const float* detL = mainL;
    const float* detR = mainR;
    if (useSidechain)
    {
        auto scBuffer = getBusBuffer (buffer, true, 1);         // read-only detection source
        detL = scBuffer.getReadPointer (0);
        detR = scBuffer.getReadPointer (scBuffer.getNumChannels() > 1 ? 1 : 0);
    }

    // --- 3. Envelope → gain computer, written into the scratch gain buffer ---
    float* const gainData = gainBuffer.getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        // Stereo-linked peak detection (max of |L|,|R|) avoids the phase cancellation
        // that an L+R sum can produce.
        const float detector = juce::jmax (std::abs (detL[i]), std::abs (detR[i]));

        const float env    = envelopeFilter.processSample (0, detector);
        const float envDb  = juce::Decibels::gainToDecibels (env);
        const float overDb = envDb - thresholdDb;
        const float grDb   = overDb > 0.0f ? overDb * slope : 0.0f;   // hard knee

        gainData[i] = juce::Decibels::decibelsToGain (grDb)
                    * makeupGainSmoothed.getNextValue();
    }

    // --- 4. SIMD gain application to the main channels only ---
    juce::dsp::AudioBlock<float> mainBlock (mainBuffer);
    juce::dsp::AudioBlock<float> gainBlock (gainBuffer);
    const auto gainView = gainBlock.getSubBlock (0, static_cast<size_t> (numSamples));

    for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
        mainBlock.getSingleChannelBlock (static_cast<size_t> (ch)).multiplyBy (gainView);

    // Note: sidechain input channels are never written — hosts treat them as read-only.
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
