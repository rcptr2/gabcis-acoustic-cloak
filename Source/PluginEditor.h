#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "AcousticCloakLookAndFeel.h"
#include "PhaseRadarComponent.h"
#include "AboutPanel.h"

/**
    Phase 4: the plugin's real GUI, replacing the GenericAudioProcessorEditor
    placeholder used since Phase 1. Holographic-radar dark-green theme
    (AcousticCloakLookAndFeel), the live phase-correlation radar
    (PhaseRadarComponent), and a knob row for all six APVTS parameters.
*/
class AcousticCloakAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AcousticCloakAudioProcessorEditor (AcousticCloakAudioProcessor&);
    ~AcousticCloakAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct LabeledKnob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (LabeledKnob& knob, const juce::String& paramID, const juce::String& labelText, bool big);
    static void placeKnob (LabeledKnob& knob, juce::Rectangle<int> area, int labelHeight);

    AcousticCloakAudioProcessor& audioProcessor;
    AcousticCloakLookAndFeel lookAndFeel;

    juce::Label titleLabel;

    PhaseRadarComponent radar;

    LabeledKnob cloakDepthKnob;
    LabeledKnob lookaheadKnob;
    LabeledKnob targetFreqLowKnob;
    LabeledKnob targetFreqHighKnob;
    LabeledKnob transientSensitivityKnob;

    juce::ToggleButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    juce::TextButton aboutButton { "About" };
    AboutPanel aboutPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcousticCloakAudioProcessorEditor)
};
