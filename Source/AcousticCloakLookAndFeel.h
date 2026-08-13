#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Dark-green phosphor "holographic radar / medical monitor" theme, per the
// blueprint's Phase 4 UI concept. A single custom rotary-knob paint routine
// (glowing arc, phosphor-green) plus themed colour IDs for everything else,
// same division of labour as the sibling MorphicPhaser project's own
// LookAndFeel.
class AcousticCloakLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AcousticCloakLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                            float sliderPosProportional, float rotaryStartAngle,
                            float rotaryEndAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    struct Palette
    {
        static const juce::Colour background;
        static const juce::Colour panel;
        static const juce::Colour track;
        static const juce::Colour phosphorGreen;
        static const juce::Colour amber;
        static const juce::Colour text;
        static const juce::Colour textDim;
    };
};
