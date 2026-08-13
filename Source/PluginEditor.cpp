#include "PluginEditor.h"

namespace
{
    constexpr int editorWidth = 640;
    constexpr int editorHeight = 620;
}

AcousticCloakAudioProcessorEditor::AcousticCloakAudioProcessorEditor (AcousticCloakAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      radar (p)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("GABCI'S ACOUSTIC CLOAK", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, AcousticCloakLookAndFeel::Palette::phosphorGreen);
    addAndMakeVisible (titleLabel);

    addAndMakeVisible (radar);

    setupKnob (cloakDepthKnob, AcousticCloakParam::cloakDepth, "CLOAK DEPTH", true);
    setupKnob (lookaheadKnob, AcousticCloakParam::lookaheadMs, "LOOKAHEAD", false);
    setupKnob (targetFreqLowKnob, AcousticCloakParam::targetFreqLow, "FREQ LOW", false);
    setupKnob (targetFreqHighKnob, AcousticCloakParam::targetFreqHigh, "FREQ HIGH", false);
    setupKnob (transientSensitivityKnob, AcousticCloakParam::transientSensitivity, "TRANSIENT SENS.", false);

    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, AcousticCloakParam::bypass, bypassButton);

    addAndMakeVisible (aboutButton);
    aboutButton.onClick = [this]
    {
        aboutPanel.setVisible (true);
        aboutPanel.toFront (true);
    };

    aboutPanel.setVisible (false);
    addChildComponent (aboutPanel);

    setResizable (false, false);
    setSize (editorWidth, editorHeight);
}

AcousticCloakAudioProcessorEditor::~AcousticCloakAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void AcousticCloakAudioProcessorEditor::setupKnob (LabeledKnob& knob, const juce::String& paramID,
                                                    const juce::String& labelText, bool big)
{
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, big ? 100 : 74, 16);
    addAndMakeVisible (knob.slider);

    knob.label.setText (labelText, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setFont (juce::Font (juce::FontOptions (big ? 14.5f : 11.0f, juce::Font::bold)));
    knob.label.setColour (juce::Label::textColourId,
                           big ? AcousticCloakLookAndFeel::Palette::phosphorGreen : AcousticCloakLookAndFeel::Palette::textDim);
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, paramID, knob.slider);
}

void AcousticCloakAudioProcessorEditor::placeKnob (LabeledKnob& knob, juce::Rectangle<int> area, int labelHeight)
{
    knob.label.setBounds (area.removeFromTop (labelHeight));
    knob.slider.setBounds (area);
}

void AcousticCloakAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (AcousticCloakLookAndFeel::Palette::background);
}

void AcousticCloakAudioProcessorEditor::resized()
{
    aboutPanel.setBounds (getLocalBounds());

    auto bounds = getLocalBounds().reduced (16);

    auto topBar = bounds.removeFromTop (28);
    titleLabel.setBounds (topBar.removeFromLeft (300));
    aboutButton.setBounds (topBar.removeFromRight (80));
    bypassButton.setBounds (topBar.withSizeKeepingCentre (100, 24));

    bounds.removeFromTop (10);

    radar.setBounds (bounds.removeFromTop (320));
    bounds.removeFromTop (14);

    auto knobRow = bounds;

    {
        auto bigArea = knobRow.removeFromLeft (160);
        placeKnob (cloakDepthKnob, bigArea.withSizeKeepingCentre (140, 160), 20);
    }

    LabeledKnob* smallKnobs[] { &lookaheadKnob, &targetFreqLowKnob, &targetFreqHighKnob, &transientSensitivityKnob };
    const int knobWidth = knobRow.getWidth() / (int) std::size (smallKnobs);

    for (auto* knob : smallKnobs)
    {
        auto area = knobRow.removeFromLeft (knobWidth).reduced (6, 10);
        placeKnob (*knob, area, 16);
    }
}
