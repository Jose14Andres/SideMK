#include <JuceHeader.h>
#include "PluginProcessor.h"

class BusesLayoutTest : public juce::UnitTest
{
public:
    BusesLayoutTest() : juce::UnitTest ("BusesLayoutSupported", "PluginProcessor") {}

    void runTest() override
    {
        SidechainCompressorAudioProcessor processor;

        beginTest ("Main I/O Stereo");

        juce::AudioProcessor::BusesLayout layout;

        // Stereo in/out
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        expect (processor.isBusesLayoutSupported (layout) == true);

        beginTest ("Main I/O Mono (Unsupported)");
        layout.inputBuses.clear();
        layout.outputBuses.clear();
        layout.inputBuses.add (juce::AudioChannelSet::mono());
        layout.outputBuses.add (juce::AudioChannelSet::mono());
        expect (processor.isBusesLayoutSupported (layout) == false);

        beginTest ("Stereo Main, Stereo Sidechain (Supported)");
        layout.inputBuses.clear();
        layout.outputBuses.clear();
        layout.inputBuses.add (juce::AudioChannelSet::stereo()); // Main Input
        layout.inputBuses.add (juce::AudioChannelSet::stereo()); // Sidechain Input
        layout.outputBuses.add (juce::AudioChannelSet::stereo()); // Main Output
        expect (processor.isBusesLayoutSupported (layout) == true);

        beginTest ("Stereo Main, Mono Sidechain (Unsupported)");
        layout.inputBuses.clear();
        layout.outputBuses.clear();
        layout.inputBuses.add (juce::AudioChannelSet::stereo()); // Main Input
        layout.inputBuses.add (juce::AudioChannelSet::mono()); // Sidechain Input
        layout.outputBuses.add (juce::AudioChannelSet::stereo()); // Main Output
        expect (processor.isBusesLayoutSupported (layout) == false);

        beginTest ("Stereo Main, Disabled Sidechain (Supported)");
        layout.inputBuses.clear();
        layout.outputBuses.clear();
        layout.inputBuses.add (juce::AudioChannelSet::stereo()); // Main Input
        layout.inputBuses.add (juce::AudioChannelSet::disabled()); // Sidechain Input
        layout.outputBuses.add (juce::AudioChannelSet::stereo()); // Main Output
        expect (processor.isBusesLayoutSupported (layout) == true);
    }
};

static BusesLayoutTest busesLayoutTest;

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce_gui;
    juce::UnitTestRunner runner;
    runner.runAllTests();

    // Return non-zero if tests fail
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (runner.getResult(i)->failures > 0)
            return 1;
    }
    return 0;
}
