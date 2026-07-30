#include <JuceHeader.h>
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <iostream>
#include <chrono>

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce_gui;

    juce::Image image(juce::Image::ARGB, 100, 100, true);
    juce::Graphics g(image);
    juce::Slider slider;

    ChromePinkLookAndFeel lookAndFeel;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i)
    {
        lookAndFeel.drawRotarySlider(g, 0, 0, 100, 100, (i % 100) / 100.0f, 0.0f, juce::MathConstants<float>::twoPi, slider);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Baseline render time: " << elapsed.count() << " ms\n";

    return 0;
}
