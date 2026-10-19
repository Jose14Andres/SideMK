#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <iostream>
#include <chrono>

int main()
{
    juce::ScopedJuceInitialiser_GUI juce_gui;

    SidechainCompressorAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    juce::AudioBuffer<float> buffer(4, 512);
    juce::MidiBuffer midiMessages;

    // Fill buffer with some dummy data
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            buffer.setSample(ch, i, (i % 256) / 256.0f - 0.5f);
        }
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100000; ++i)
    {
        processor.processBlock(buffer, midiMessages);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Baseline processor time: " << elapsed.count() << " ms\n";

    return 0;
}
