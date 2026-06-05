#include "PluginEditor.h"

namespace
{
    constexpr int kHeaderH = 60;
    constexpr int kWinW    = 540;
    constexpr int kWinH    = 300;

    // Palette
    const juce::Colour kBgColour      { 0xff121212 };
    const juce::Colour kHeaderColour  { 0xff0a0a0a };
    const juce::Colour kAccentPink    { 0xffff1493 }; // Re-used for deep pink accents
    const juce::Colour kDivider       { 0xff252b3a };
    const juce::Colour kLabelText     { 0xff6688aa };
    const juce::Colour kValueText     { 0xffaaaacc };
    const juce::Colour kLedOn         { 0xff8a2be2 }; // Neon purple
    const juce::Colour kLedOff        { 0xff444455 };
}

//==============================================================================
void ChromePinkLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, const float rotaryStartAngle,
                                              const float rotaryEndAngle, juce::Slider& /*slider*/)
{
    auto radius = (float) juce::jmin (width / 2, height / 2) - 4.0f;
    auto centreX = (float) x + (float) width  * 0.5f;
    auto centreY = (float) y + (float) height * 0.5f;
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;

    // Outer Chrome Ring (Bevel)
    juce::ColourGradient bevelGradient (juce::Colours::black.withAlpha(0.6f), centreX, centreY - radius,
                                        juce::Colours::white.withAlpha(0.4f), centreX, centreY + radius, false);
    g.setGradientFill (bevelGradient);
    g.fillEllipse (rx, ry, rw, rw);

    // Inner Metallic Base
    float innerRadius = radius - 3.0f;
    float irx = centreX - innerRadius;
    float iry = centreY - innerRadius;
    float irw = innerRadius * 2.0f;
    juce::ColourGradient baseGradient (juce::Colour(0xff2a2a2a), centreX, centreY - innerRadius,
                                       juce::Colour(0xff121212), centreX, centreY + innerRadius, false);
    g.setGradientFill (baseGradient);
    g.fillEllipse (irx, iry, irw, irw);

    // Specular Highlight
    float highlightRadius = innerRadius * 0.8f;
    juce::ColourGradient highlightGradient (juce::Colours::white.withAlpha(0.15f), centreX, centreY - innerRadius,
                                            juce::Colours::transparentWhite, centreX, centreY, false);
    g.setGradientFill (highlightGradient);
    g.fillEllipse (centreX - highlightRadius, centreY - innerRadius, highlightRadius * 2.0f, innerRadius);

    // Track
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    juce::Path trackArc;
    trackArc.addCentredArc (centreX, centreY, innerRadius + 1.0f, innerRadius + 1.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour(0xff1f1f1f));
    g.strokePath (trackArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active Track Gradient (Pink to Purple)
    juce::Path activeTrackArc;
    activeTrackArc.addCentredArc (centreX, centreY, innerRadius + 1.0f, innerRadius + 1.0f, 0.0f, rotaryStartAngle, angle, true);
    juce::ColourGradient activeGradient (juce::Colour(0xffff1493), centreX - radius, centreY,
                                         juce::Colour(0xff8a2be2), centreX + radius, centreY, false);
    g.setGradientFill (activeGradient);
    g.strokePath (activeTrackArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Pointer
    juce::Path p;
    auto pointerLength = innerRadius * 0.7f;
    auto pointerThickness = 3.0f;
    p.addRectangle (-pointerThickness * 0.5f, -innerRadius + 4.0f, pointerThickness, pointerLength);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));

    g.setGradientFill (activeGradient);
    g.fillPath (p);
}

//==============================================================================
SidechainCompressorAudioProcessorEditor::SidechainCompressorAudioProcessorEditor (
    SidechainCompressorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel(&chromePinkLookAndFeel);

    setupKnob (threshold, "THRESHOLD");
    setupKnob (ratio,     "RATIO");
    setupKnob (attack,    "ATTACK");
    setupKnob (release,   "RELEASE");
    setupKnob (gain,      "SIDECHAIN LEVEL");

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
    setLookAndFeel(nullptr);
}

//==============================================================================
void SidechainCompressorAudioProcessorEditor::setupKnob (KnobGroup& kg,
                                                          const juce::String& labelText)
{
    auto& s = kg.slider;
    s.setSliderStyle    (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle   (juce::Slider::TextBoxBelow, false, 72, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId,    kAccentPink);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, kDivider);
    s.setColour (juce::Slider::thumbColourId,               juce::Colours::white);
    s.setColour (juce::Slider::textBoxTextColourId,         kValueText);
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxHighlightColourId,    kAccentPink.withAlpha (0.4f));
    addAndMakeVisible (s);

    auto& l = kg.label;
    l.setText               (labelText, juce::dontSendNotification);
    l.setFont               (juce::FontOptions (9.5f).withStyle ("Bold"));
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

    // Pink accent line at bottom of header
    g.setColour (kAccentPink);
    g.fillRect  (0, kHeaderH - 2, getWidth(), 2);

    // Title
    g.setColour (juce::Colours::white);
    g.setFont   (juce::FontOptions (22.0f).withStyle ("Bold"));
    g.drawText  ("SideMk", 20, 0, 100, kHeaderH, juce::Justification::centredLeft);

    g.setColour (kAccentPink);
    g.setFont   (juce::FontOptions (14.0f).withStyle ("Italic"));
    g.drawText  ("by Lily", 105, 0, 100, kHeaderH, juce::Justification::centredLeft);

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
    g.setFont   (juce::FontOptions (9.5f).withStyle ("Bold"));
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

    // Provide a neat padding around the grid
    area = area.reduced (10, 10);

    KnobGroup* knobs[] = { &threshold, &ratio, &attack, &release, &gain };
    const int numKnobs = 5;

    // Using Rectangle::removeFromLeft is safe, effective, and perfectly forms a professional grid
    const int itemWidth = area.getWidth() / numKnobs;

    for (auto* kg : knobs)
    {
        auto col = area.removeFromLeft (itemWidth);
        auto contentArea = col.reduced (10, 10);
        kg->label.setBounds (contentArea.removeFromBottom (20));
        kg->slider.setBounds (contentArea);
    }
}
