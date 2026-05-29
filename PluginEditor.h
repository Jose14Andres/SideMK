#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SidechainCompressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                 private juce::Timer
{
public:
    explicit SidechainCompressorAudioProcessorEditor (SidechainCompressorAudioProcessor& p);
    ~SidechainCompressorAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    //==============================================================================
    void timerCallback() override;

    SidechainCompressorAudioProcessor& audioProcessor;

    // Groups each knob with its label — non-copyable by design (contains juce::Component)
    struct KnobGroup
    {
        juce::Slider slider;
        juce::Label  label;
    };

    KnobGroup threshold, ratio, attack, release, gain;

    // unique_ptr allows deferred creation (after setupKnob configures each slider)
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> thresholdAtt;
    std::unique_ptr<SliderAttachment> ratioAtt;
    std::unique_ptr<SliderAttachment> attackAtt;
    std::unique_ptr<SliderAttachment> releaseAtt;
    std::unique_ptr<SliderAttachment> gainAtt;

    // Updated at 10 Hz by the timer; triggers a repaint of the indicator only
    bool sidechainActive { false };

    void setupKnob (KnobGroup& kg, const juce::String& labelText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainCompressorAudioProcessorEditor)
};
