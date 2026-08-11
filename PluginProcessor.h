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
    // DSP — prepared once in prepareToPlay; used mono (1 channel) for sidechain detection
    juce::dsp::Compressor<float> compressor;

    // Lock-free raw pointers into the APVTS parameter store
    std::atomic<float>* thresholdParam { nullptr }; // dB  [-60, 0]
    std::atomic<float>* ratioParam     { nullptr }; //     [1, 20]
    std::atomic<float>* attackParam    { nullptr }; // ms  [0.1, 500]
    std::atomic<float>* releaseParam   { nullptr }; // ms  [1, 2000]
    std::atomic<float>* gainParam      { nullptr }; // dB  [-24, +24]

    // Cached parameter values to avoid expensive redundant recalculations in compressor
    float lastThreshold = -1000.0f;
    float lastRatio     = -1000.0f;
    float lastAttack    = -1000.0f;
    float lastRelease   = -1000.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainCompressorAudioProcessor)
};
