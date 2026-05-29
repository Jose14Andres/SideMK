#include "PluginEditor.h"

namespace
{
    constexpr int kHeaderH = 60;
    constexpr int kWinW    = 540;
    constexpr int kWinH    = 300;

    // Palette
    const juce::Colour kBgColour      { 0xff1a1f2e };
    const juce::Colour kHeaderColour  { 0xff0d1220 };
    const juce::Colour kAccentBlue    { 0xff4a90d9 };
    const juce::Colour kDivider       { 0xff252b3a };
    const juce::Colour kLabelText     { 0xff6688aa };
    const juce::Colour kValueText     { 0xffaaaacc };
    const juce::Colour kLedOn         { 0xff00e676 };
    const juce::Colour kLedOff        { 0xff444455 };
}

//==============================================================================
SidechainCompressorAudioProcessorEditor::SidechainCompressorAudioProcessorEditor (
    SidechainCompressorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setupKnob (threshold, "THRESHOLD");
    setupKnob (ratio,     "RATIO");
    setupKnob (attack,    "ATTACK");
    setupKnob (release,   "RELEASE");
    setupKnob (gain,      "MAKEUP");

    // Attachments connect each slider to its APVTS parameter (sets range + value automatically)
    thresholdAtt = std::make_unique<SliderAttachment> (p.apvts, "threshold", threshold.slider);
    ratioAtt     = std::make_unique<SliderAttachment> (p.apvts, "ratio",     ratio.slider);
    attackAtt    = std::make_unique<SliderAttachment> (p.apvts, "attack",    attack.slider);
    releaseAtt   = std::make_unique<SliderAttachment> (p.apvts, "release",   release.slider);
    gainAtt      = std::make_unique<SliderAttachment> (p.apvts, "gain",      gain.slider);

    setSize (kWinW, kWinH);
    startTimerHz (10); // poll sidechain status at 10 Hz
}

SidechainCompressorAudioProcessorEditor::~SidechainCompressorAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void SidechainCompressorAudioProcessorEditor::setupKnob (KnobGroup& kg,
                                                          const juce::String& labelText)
{
    auto& s = kg.slider;
    s.setSliderStyle    (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle   (juce::Slider::TextBoxBelow, false, 72, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId,    kAccentBlue);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, kDivider);
    s.setColour (juce::Slider::thumbColourId,               juce::Colours::white);
    s.setColour (juce::Slider::textBoxTextColourId,         kValueText);
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxHighlightColourId,    kAccentBlue.withAlpha (0.4f));
    addAndMakeVisible (s);

    auto& l = kg.label;
    l.setText               (labelText, juce::dontSendNotification);
    l.setFont               (juce::Font (9.5f, juce::Font::bold));
    l.setJustificationType  (juce::Justification::centred);
    l.setColour             (juce::Label::textColourId, kLabelText);
    addAndMakeVisible (l);
}

//==============================================================================
void SidechainCompressorAudioProcessorEditor::timerCallback()
{
    auto* scBus       = audioProcessor.getBus (true, 1);
    const bool active = (scBus != nullptr) && scBus->isEnabled();

    if (active != sidechainActive)
    {
        sidechainActive = active;
        // Repaint only the indicator area (top-right corner), not the entire window
        repaint (getWidth() - 140, 0, 140, kHeaderH);
    }
}

//==============================================================================
void SidechainCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    // --- Background ---
    g.fillAll (kBgColour);

    // --- Header ---
    g.setColour (kHeaderColour);
    g.fillRect  (0, 0, getWidth(), kHeaderH);

    // Blue accent line at bottom of header
    g.setColour (kAccentBlue);
    g.fillRect  (0, kHeaderH - 2, getWidth(), 2);

    // Title
    g.setColour (juce::Colours::white);
    g.setFont   (juce::Font (15.0f, juce::Font::bold));
    g.drawText  ("SIDECHAIN COMPRESSOR", 20, 0, 320, kHeaderH, juce::Justification::centredLeft);

    // --- Sidechain indicator (top-right) ---
    const float ledX  = (float)(getWidth() - 128);
    const float ledY  = 24.0f;
    const float ledD  = 10.0f;

    const auto ledColour = sidechainActive ? kLedOn : kLedOff;

    // Glow halo when active
    if (sidechainActive)
    {
        g.setColour (kLedOn.withAlpha (0.25f));
        g.fillEllipse (ledX - 4.0f, ledY - 4.0f, ledD + 8.0f, ledD + 8.0f);
    }

    // LED dot
    g.setColour (ledColour);
    g.fillEllipse (ledX, ledY, ledD, ledD);
    g.setColour (ledColour.brighter (0.4f));
    g.fillEllipse (ledX + 2.0f, ledY + 1.5f, 3.5f, 3.0f); // specular highlight

    // Status text
    g.setColour (sidechainActive ? kLedOn : kLedOff.brighter (0.2f));
    g.setFont   (juce::Font (9.5f, juce::Font::bold));
    g.drawText  (sidechainActive ? "SIDECHAIN ON" : "SIDECHAIN OFF",
                 (int)ledX + 15, (int)ledY - 2, 110, 14,
                 juce::Justification::centredLeft);

    // --- Dividers between knob columns ---
    const int columnWidth = getWidth() / 5;
    g.setColour (kDivider);
    for (int i = 1; i < 5; ++i)
        g.drawVerticalLine (i * columnWidth, (float)kHeaderH + 8, (float)getHeight() - 8);
}

//==============================================================================
void SidechainCompressorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (kHeaderH);

    const int columnWidth = area.getWidth() / 5;
    KnobGroup* knobs[] = { &threshold, &ratio, &attack, &release, &gain };

    for (auto* kg : knobs)
    {
        auto col = area.removeFromLeft (columnWidth);
        // Label anchored at bottom of column; slider fills the rest
        kg->label.setBounds  (col.removeFromBottom (22).reduced (2, 0));
        kg->slider.setBounds (col.reduced (10, 8));
    }
}
