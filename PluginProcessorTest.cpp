#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "JucePluginDefines.h"

class PluginProcessorTest : public juce::UnitTest
{
public:
    PluginProcessorTest() : juce::UnitTest("PluginProcessorTest") {}

    void runTest() override
    {
        beginTest("Initialization");
        {
            SidechainCompressorAudioProcessor processor;
            expect(processor.hasEditor());
            expectEquals(processor.getNumPrograms(), 1);
            expectEquals(processor.getName(), juce::String(JucePlugin_Name));
        }

        beginTest("BusesLayoutSupported");
        {
            SidechainCompressorAudioProcessor processor;

            // Stereo in, Stereo out, Stereo sidechain (supported)
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add(juce::AudioChannelSet::stereo());
            layout.outputBuses.add(juce::AudioChannelSet::stereo());
            layout.inputBuses.add(juce::AudioChannelSet::stereo()); // Sidechain
            expect(processor.isBusesLayoutSupported(layout));

            // Mono in, Mono out (not supported)
            juce::AudioProcessor::BusesLayout badLayout;
            badLayout.inputBuses.add(juce::AudioChannelSet::mono());
            badLayout.outputBuses.add(juce::AudioChannelSet::mono());
            expect(!processor.isBusesLayoutSupported(badLayout));
        }

        beginTest("Audio Processing - Silence");
        {
            SidechainCompressorAudioProcessor processor;
            processor.prepareToPlay(44100.0, 512);

            juce::AudioBuffer<float> buffer(4, 512); // Stereo Main + Stereo SC
            buffer.clear();
            juce::MidiBuffer midiMessages;

            processor.processBlock(buffer, midiMessages);

            // Output should still be silence
            bool isSilent = true;
            for (int ch = 0; ch < 2; ++ch) {
                auto* ptr = buffer.getReadPointer(ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i) {
                    if (ptr[i] != 0.0f) {
                        isSilent = false;
                        break;
                    }
                }
            }
            expect(isSilent);
        }
    }
};

static PluginProcessorTest pluginProcessorTest;

// main() lives in TestMain.cpp so every UnitTest in the target runs from one runner.
