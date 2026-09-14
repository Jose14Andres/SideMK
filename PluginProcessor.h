#pragma once

// Must be included before <JuceHeader.h> so JucePlugin_* macros are defined
// when JUCE module headers consume them.  In a CMake build, juce_add_plugin()
// also injects these as compiler flags; the #ifndef guards in this file
// prevent any redefinition conflict.
#include "JucePluginDefines.h"

#include <JuceHeader.h>

class SidechainCompressorAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    SidechainCompressorAudioProcessor();
    ~SidechainCompressorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override         { return JucePlugin_Name; }
    bool  acceptsMidi()  const override                 { return false; }
    bool  producesMidi() const override                 { return false; }
    bool  isMidiEffect() const override                 { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }

    //==============================================================================
    int  getNumPrograms()    override                   { return 1; }
    int  getCurrentProgram() override                   { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    //==============================================================================
    // DSP — all state prepared once in prepareToPlay; the audio thread never allocates.
    //
    // Envelope follower (mono, ch 0): replaces the old juce::dsp::Compressor. Feeding it
    // the detection signal and computing gain from the returned envelope avoids the
    // per-sample division used previously, and lets the ballistics' release govern how
    // gain returns to unity when the detection signal falls silent.
    juce::dsp::BallisticsFilter<float> envelopeFilter;

    // Control layer (Mejora 2): makeup gain is ramped per sample to kill automation clicks.
    // Multiplicative smoothing is safe here — makeup gain is never 0 (range [-24, +24] dB).
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> makeupGainSmoothed;

    // Scratch gain vector, sized in prepareToPlay. Holds the per-sample total gain
    // (compression * makeup) so it can be applied to the main bus with a SIMD block multiply.
    juce::AudioBuffer<float> gainBuffer;

    // Lock-free raw pointers into the APVTS parameter store
    std::atomic<float>* thresholdParam { nullptr }; // dB  [-60, 0]
    std::atomic<float>* ratioParam     { nullptr }; //     [1, 20]
    std::atomic<float>* attackParam    { nullptr }; // ms  [0.1, 500]
    std::atomic<float>* releaseParam   { nullptr }; // ms  [1, 2000]
    std::atomic<float>* gainParam      { nullptr }; // dB  [-24, +24]

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainCompressorAudioProcessor)
};
