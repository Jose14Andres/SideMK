#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PumpCurve.h"

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 512;

    // Host transport stand-in: the test sets the position before each block.
    struct FakePlayHead : public juce::AudioPlayHead
    {
        PositionInfo info;

        juce::Optional<PositionInfo> getPosition() const override { return info; }
    };

    void setParameter (SidechainCompressorAudioProcessor& processor, const juce::String& id, float value)
    {
        auto* parameter = processor.apvts.getParameter (id);
        jassert (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    // Stereo main + stereo sidechain, main filled with DC so output == applied gain.
    juce::AudioBuffer<float> makeDcBuffer (int numSamples, float level = 1.0f)
    {
        juce::AudioBuffer<float> buffer (4, numSamples);
        buffer.clear();
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), level, numSamples);
        return buffer;
    }
} // namespace

class PumpModeTest : public juce::UnitTest
{
public:
    PumpModeTest() : juce::UnitTest ("PumpMode", "PluginProcessor") {}

    void runTest() override
    {
        namespace pump = sidemk::pump;
        juce::MidiBuffer midi;

        beginTest ("Free-runs at the fallback tempo without a host transport");
        {
            SidechainCompressorAudioProcessor processor;
            setParameter (processor, "mode", (float) SidechainCompressorAudioProcessor::modePump);
            processor.prepareToPlay (kSampleRate, kBlockSize);

            // Rate 1/4 at 120 BPM: one cycle = 0.5 s
            const int cycle = juce::roundToInt (kSampleRate * 60.0 / pump::kFallbackBpm);
            std::vector<float> outL, outR;

            for (int done = 0; done < cycle; done += kBlockSize)
            {
                const int n = std::min (kBlockSize, cycle - done);
                auto buffer = makeDcBuffer (n);
                processor.processBlock (buffer, midi);
                outL.insert (outL.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + n);
                outR.insert (outR.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + n);
            }

            expectWithinAbsoluteError (outL.front(), 1.0f, 1.0e-4f);
            expectLessThan (*std::min_element (outL.begin(), outL.end()), 0.01f);
            expectGreaterThan (outL.back(), 0.99f);
            expect (outL == outR, "Left and right must receive the same gain");

            // The dip sits right after the 1 ms anti-click fall
            const auto minIndex = std::distance (outL.begin(), std::min_element (outL.begin(), outL.end()));
            expectWithinAbsoluteError ((int) minIndex, juce::roundToInt (kSampleRate * 0.001), 2);
        }

        beginTest ("Locks to the host PPQ position");
        {
            SidechainCompressorAudioProcessor processor;
            FakePlayHead playHead;
            processor.setPlayHead (&playHead);
            setParameter (processor, "mode", (float) SidechainCompressorAudioProcessor::modePump);
            processor.prepareToPlay (kSampleRate, kBlockSize);

            playHead.info.setIsPlaying (true);
            playHead.info.setBpm (128.0);

            const double drop = pump::dropFraction (128.0, 1.0);

            // Block starting halfway through a beat
            playHead.info.setPpqPosition (16.5);
            auto buffer = makeDcBuffer (kBlockSize);
            processor.processBlock (buffer, midi);
            expectWithinAbsoluteError ((double) buffer.getSample (0, 0),
                                       pump::pumpGain (0.5, 1.0, drop, pump::kDefaultCurveK), 1.0e-4);

            // Block starting exactly on a beat: top of the curve, then the dip
            playHead.info.setPpqPosition (17.0);
            buffer = makeDcBuffer (kBlockSize);
            processor.processBlock (buffer, midi);
            expectWithinAbsoluteError (buffer.getSample (0, 0), 1.0f, 1.0e-4f);

            const int dipSample = juce::roundToInt (drop * kSampleRate * 60.0 / 128.0);
            expectLessThan (buffer.getSample (0, dipSample), 0.01f);

            // Negative PPQ (pre-roll) must still map into [0, 1)
            playHead.info.setPpqPosition (-0.25);
            buffer = makeDcBuffer (kBlockSize);
            processor.processBlock (buffer, midi);
            expectWithinAbsoluteError ((double) buffer.getSample (0, 0),
                                       pump::pumpGain (0.75, 1.0, drop, pump::kDefaultCurveK), 1.0e-4);

            processor.setPlayHead (nullptr);
        }

        beginTest ("1 Bar follows the host time signature and bar start");
        {
            SidechainCompressorAudioProcessor processor;
            FakePlayHead playHead;
            processor.setPlayHead (&playHead);
            setParameter (processor, "mode", (float) SidechainCompressorAudioProcessor::modePump);
            setParameter (processor, "rate", (float) SidechainCompressorAudioProcessor::rate1Bar);
            processor.prepareToPlay (kSampleRate, kBlockSize);

            playHead.info.setIsPlaying (true);
            playHead.info.setBpm (120.0);
            playHead.info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 3, 4 });
            playHead.info.setPpqPositionOfLastBarStart (9.0);
            playHead.info.setPpqPosition (10.5); // 1.5 beats into a 3-beat bar

            auto buffer = makeDcBuffer (kBlockSize);
            processor.processBlock (buffer, midi);

            const double drop = pump::dropFraction (120.0, 3.0);
            expectWithinAbsoluteError ((double) buffer.getSample (0, 0),
                                       pump::pumpGain (0.5, 1.0, drop, pump::kDefaultCurveK), 1.0e-4);

            processor.setPlayHead (nullptr);
        }

        beginTest ("Depth 0 leaves the signal untouched");
        {
            SidechainCompressorAudioProcessor processor;
            setParameter (processor, "mode",  (float) SidechainCompressorAudioProcessor::modePump);
            setParameter (processor, "depth", 0.0f);
            processor.prepareToPlay (kSampleRate, kBlockSize);

            auto buffer = makeDcBuffer (kBlockSize, 0.5f);
            processor.processBlock (buffer, midi);

            for (int i = 0; i < kBlockSize; ++i)
                expectWithinAbsoluteError (buffer.getSample (0, i), 0.5f, 1.0e-6f);
        }

        beginTest ("Pump ignores the sidechain input");
        {
            SidechainCompressorAudioProcessor a, b;
            for (auto* p : { &a, &b })
            {
                setParameter (*p, "mode", (float) SidechainCompressorAudioProcessor::modePump);
                p->prepareToPlay (kSampleRate, kBlockSize);
            }

            auto withSc    = makeDcBuffer (kBlockSize);
            auto withoutSc = makeDcBuffer (kBlockSize);
            for (int ch = 2; ch < 4; ++ch)
                juce::FloatVectorOperations::fill (withSc.getWritePointer (ch), 0.9f, kBlockSize);

            a.processBlock (withSc, midi);
            b.processBlock (withoutSc, midi);

            for (int i = 0; i < kBlockSize; ++i)
                expectEquals (withSc.getSample (0, i), withoutSc.getSample (0, i));
        }

        beginTest ("Compressor mode applies its gain only once");
        {
            // Threshold 0 dB with a -20 dBFS signal: no compression, only makeup.
            // Applying the gain twice per sample (the old bug) would give +12 dB.
            SidechainCompressorAudioProcessor processor;
            setParameter (processor, "threshold", 0.0f);
            setParameter (processor, "gain", 6.0f);
            processor.prepareToPlay (kSampleRate, kBlockSize);

            auto buffer = makeDcBuffer (kBlockSize, 0.1f);
            processor.processBlock (buffer, midi);

            const float expected = 0.1f * juce::Decibels::decibelsToGain (6.0f);
            expectWithinAbsoluteError (buffer.getSample (0, kBlockSize - 1), expected, 1.0e-4f);
        }
    }
};

static PumpModeTest pumpModeTest;
