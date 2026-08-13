#include "AcousticCloakLookAndFeel.h"

const juce::Colour AcousticCloakLookAndFeel::Palette::background    { 0xff050a07 };
const juce::Colour AcousticCloakLookAndFeel::Palette::panel         { 0xff0d1a12 };
const juce::Colour AcousticCloakLookAndFeel::Palette::track         { 0xff1c3626 };
const juce::Colour AcousticCloakLookAndFeel::Palette::phosphorGreen { 0xff3dff9a };
const juce::Colour AcousticCloakLookAndFeel::Palette::amber         { 0xffffb347 };
const juce::Colour AcousticCloakLookAndFeel::Palette::text          { 0xffd8f5e4 };
const juce::Colour AcousticCloakLookAndFeel::Palette::textDim       { 0xff6f9c81 };

AcousticCloakLookAndFeel::AcousticCloakLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Palette::background);

    setColour (juce::Label::textColourId, Palette::text);
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxBackgroundColourId, Palette::panel);
    setColour (juce::Slider::textBoxOutlineColourId, Palette::track);

    setColour (juce::ToggleButton::textColourId, Palette::text);
    setColour (juce::TextButton::textColourOffId, Palette::text);
    setColour (juce::TextButton::textColourOnId, Palette::background);
}

void AcousticCloakLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                  float sliderPosProportional, float rotaryStartAngle,
                                                  float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const float lineWidth = juce::jmax (2.0f, radius * 0.11f);
    const float arcRadius = radius - lineWidth * 0.5f;

    // Background halo behind the whole knob, growing brighter as the value
    // increases -- a soft radial bloom, not a hard-edged shape, so it reads
    // as "the knob is glowing" rather than another ring.
    const float haloRadius = radius * 2.0f;
    const float haloAlpha = 0.05f + 0.55f * sliderPosProportional;
    juce::ColourGradient halo (Palette::phosphorGreen.withAlpha (haloAlpha), centre.x, centre.y,
                                Palette::phosphorGreen.withAlpha (0.0f), centre.x, centre.y - haloRadius,
                                true);
    g.setGradientFill (halo);
    g.fillEllipse (centre.x - haloRadius, centre.y - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (Palette::track);
    g.strokePath (track, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);

    for (int i = 3; i >= 0; --i)
    {
        const float glowWidth = lineWidth + (float) i * 2.2f;
        g.setColour (Palette::phosphorGreen.withAlpha (i == 0 ? 0.95f : 0.10f));
        g.strokePath (valueArc, juce::PathStrokeType (glowWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float knobRadius = radius * 0.62f;
    g.setColour (Palette::panel);
    g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
    g.setColour (Palette::track);
    g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.5f);

    juce::Point<float> tip (centre.x + std::sin (angle) * knobRadius * 0.92f,
                             centre.y - std::cos (angle) * knobRadius * 0.92f);
    g.setColour (Palette::amber);
    g.drawLine ({ centre, tip }, juce::jmax (2.0f, lineWidth * 0.6f));
    g.setColour (Palette::phosphorGreen);
    g.fillEllipse (tip.x - 2.5f, tip.y - 2.5f, 5.0f, 5.0f);
}

void AcousticCloakLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                                  bool shouldDrawButtonAsHighlighted, bool)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();

    g.setColour (on ? Palette::amber : Palette::panel);
    g.fillRoundedRectangle (bounds, 5.0f);

    const auto outlineColour = shouldDrawButtonAsHighlighted ? Palette::phosphorGreen : (on ? Palette::amber : Palette::track);
    for (int i = 2; i >= 0; --i)
    {
        g.setColour (outlineColour.withAlpha (i == 0 ? 0.9f : 0.12f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.2f + (float) i * 1.6f);
    }

    g.setColour (on ? Palette::background : Palette::text);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
}

void AcousticCloakLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                      bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (shouldDrawButtonAsDown ? Palette::track : Palette::panel);
    g.fillRoundedRectangle (bounds, 5.0f);

    const auto outlineColour = shouldDrawButtonAsHighlighted ? Palette::phosphorGreen : Palette::track;
    for (int i = 2; i >= 0; --i)
    {
        g.setColour (outlineColour.withAlpha (i == 0 ? 0.9f : 0.12f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.2f + (float) i * 1.6f);
    }
}
