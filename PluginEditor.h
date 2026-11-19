#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ChromePinkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChromePinkLookAndFeel() = default;
    ~ChromePinkLookAndFeel() override = default;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, const float rotaryStartAngle,
                           const float rotaryEndAngle, juce::Slider& slider) override;
private:
    std::map<std::pair<int, int>, juce::Image> cachedKnobImages;
};

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

    KnobGroup threshold, ratio, attack, release, depth, gain;

    // Mode (Compressor / Pump) and pump rate selectors, shown in the header
    juce::ComboBox modeBox, rateBox;

    // unique_ptr allows deferred creation (after setupKnob configures each slider)
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> thresholdAtt;
    std::unique_ptr<SliderAttachment> ratioAtt;
    std::unique_ptr<SliderAttachment> attackAtt;
    std::unique_ptr<SliderAttachment> releaseAtt;
    std::unique_ptr<SliderAttachment> depthAtt;
    std::unique_ptr<SliderAttachment> gainAtt;

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<ComboBoxAttachment> modeAtt;
    std::unique_ptr<ComboBoxAttachment> rateAtt;

    // Read on the message thread by the timer to dim controls the current mode ignores
    std::atomic<float>* modeParam { nullptr };
    bool pumpModeShown { false };

    // Updated at 10 Hz by the timer; triggers a repaint of the indicator only
    bool sidechainActive { false };

    ChromePinkLookAndFeel chromePinkLookAndFeel;

    void setupKnob (KnobGroup& kg, const juce::String& labelText);
    void setupComboBox (juce::ComboBox& box, const juce::String& parameterID);
    void updateModeDependentControls (bool pumpMode);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainCompressorAudioProcessorEditor)
};
