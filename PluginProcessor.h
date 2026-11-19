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

    // Index values of the "mode" choice parameter
    enum Mode { modeCompressor = 0, modePump = 1 };

    // Index values of the "rate" choice parameter
    enum Rate { rate1_16 = 0, rate1_8, rate1_4, rate1_2, rate1Bar, numRates };

    // Length of one pump cycle in quarter notes (PPQ) for each Rate index.
    // rate1Bar assumes 4/4; processBlock uses the host time signature when it has one.
    static double rateToPpq (int rateIndex) noexcept;

private:
    //==============================================================================
    void processCompressor (float* mainL, float* mainR,
                            const float* scL, const float* scR, int numSamples);
    void processPump (float* mainL, float* mainR, int numSamples);

    // DSP — prepared once in prepareToPlay; used mono (1 channel) for sidechain detection
    juce::dsp::Compressor<float> compressor;

    // Lock-free raw pointers into the APVTS parameter store
    std::atomic<float>* thresholdParam { nullptr }; // dB  [-60, 0]
    std::atomic<float>* ratioParam     { nullptr }; //     [1, 20]
    std::atomic<float>* attackParam    { nullptr }; // ms  [0.1, 500]
    std::atomic<float>* releaseParam   { nullptr }; // ms  [1, 2000]
    std::atomic<float>* gainParam      { nullptr }; // dB  [-24, +24]
    std::atomic<float>* modeParam      { nullptr }; // choice index, see Mode
    std::atomic<float>* rateParam      { nullptr }; // choice index, see Rate
    std::atomic<float>* depthParam     { nullptr }; // %   [0, 100]

    // 20 ms ramps so automation of makeup gain and pump depth does not click
    juce::SmoothedValue<float> makeupSmoothed { 1.0f };
    juce::SmoothedValue<float> depthSmoothed  { 1.0f };

    // Cached parameter values to avoid expensive redundant recalculations in compressor
    float lastThreshold = -1000.0f;
    float lastRatio     = -1000.0f;
    float lastAttack    = -1000.0f;
    float lastRelease   = -1000.0f;

    int    lastMode  = modeCompressor;
    double pumpPhase = 0.0; // [0, 1); carried between blocks when the host transport is stopped

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainCompressorAudioProcessor)
};
