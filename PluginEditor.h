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
};

//==============================================================================
// Vertical gain-reduction meter in the "Chrome Pink & Purple" (Lily) aesthetic.
//
// Self-contained display component: it reads the processor's lock-free GR atomic,
// applies ballistic smoothing (instant rise, eased fall) for a readable needle,
// and paints a glowing pink→purple bar. It owns no timer — the editor drives it via
// refresh() so a single 30 Hz clock repaints only this component's bounds.
class GainReductionMeter : public juce::Component
{
public:
    explicit GainReductionMeter (SidechainCompressorAudioProcessor& p) : processor (p) {}

    // Called from the editor timer (message thread). Pulls the latest GR value,
    // eases the displayed value toward it, and repaints iff the bar moved visibly.
    void refresh();

    void paint (juce::Graphics&) override;

private:
    SidechainCompressorAudioProcessor& processor;

    float displayDb { 0.0f };  // smoothed, currently-drawn reduction magnitude (dB)
    float lastDrawnDb { -1.0f }; // guards against needless repaints when idle

    static constexpr float kMaxDb = 24.0f; // meter full-scale (bottom of the bar)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainReductionMeter)
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

    KnobGroup threshold, ratio, attack, release, gain;

    // unique_ptr allows deferred creation (after setupKnob configures each slider)
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> thresholdAtt;
    std::unique_ptr<SliderAttachment> ratioAtt;
    std::unique_ptr<SliderAttachment> attackAtt;
    std::unique_ptr<SliderAttachment> releaseAtt;
    std::unique_ptr<SliderAttachment> gainAtt;

    // Polled by the timer; triggers a repaint of the indicator only when it changes
    bool sidechainActive { false };

    // Gain-reduction meter + its caption, refreshed by the timer at 30 Hz
    GainReductionMeter grMeter { audioProcessor };
    juce::Label        grLabel;

    ChromePinkLookAndFeel chromePinkLookAndFeel;

    void setupKnob (KnobGroup& kg, const juce::String& labelText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainCompressorAudioProcessorEditor)
};
