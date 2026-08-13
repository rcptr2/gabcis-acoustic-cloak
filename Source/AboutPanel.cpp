/*
    This plugin was built by Claude Code (Sonnet 5), based on a joint idea by
    Gabor Tomori and Gemini, under Gabor Tomori's direction and with Gemini's
    review, in 2026.
    Tested on Intel Mac OS 15.7, FL Studio 2026
*/
#include "AboutPanel.h"
#include "AcousticCloakLookAndFeel.h"
#include "PluginSignature.h"
#include "PluginProcessor.h"

namespace
{
    const juce::String featureSummary =
        "Sidechain phase-rotation engine: instead of ducking the Bass on\n"
        "every Kick hit, Acoustic Cloak rotates the Bass's phase within an\n"
        "adjustable Target Frequency Range so it sums constructively with\n"
        "the Kick -- full loudness, no volume loss.\n\n"
        "Linkwitz-Riley band isolation, an allpass Hilbert phase-difference\n"
        "network, and a 'Dummy All-Pass' recombination trick keep the\n"
        "correction flat-magnitude at any rotation angle, with no comb\n"
        "-filtering at the band edges.\n\n"
        "Adjustable Lookahead, Transient Sensitivity, and Cloak Depth.\n\n"
        "VST3 + Standalone, Intel Mac and Windows 11 x64, FL Studio compatible.";
}

AboutPanel::AboutPanel()
{
    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { setVisible (false); };
    setInterceptsMouseClicks (true, true);
}

void AboutPanel::resized()
{
    auto bounds = getLocalBounds();
    const auto cardWidth = juce::jmin (480, bounds.getWidth() - 40);
    const auto cardHeight = juce::jmin (400, bounds.getHeight() - 40);
    auto card = juce::Rectangle<int> (0, 0, cardWidth, cardHeight).withCentre (bounds.getCentre());

    closeButton.setBounds (card.getRight() - 90, card.getBottom() - 44, 70, 28);
}

void AboutPanel::mouseUp (const juce::MouseEvent& event)
{
    const auto bounds = getLocalBounds();
    const auto cardWidth = juce::jmin (480, bounds.getWidth() - 40);
    const auto cardHeight = juce::jmin (400, bounds.getHeight() - 40);
    const auto card = juce::Rectangle<int> (0, 0, cardWidth, cardHeight).withCentre (bounds.getCentre());

    if (! card.contains (event.getPosition()))
        setVisible (false);
}

void AboutPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.75f));

    auto bounds = getLocalBounds();
    const auto cardWidth = juce::jmin (480, bounds.getWidth() - 40);
    const auto cardHeight = juce::jmin (400, bounds.getHeight() - 40);
    auto card = juce::Rectangle<float> (0.0f, 0.0f, (float) cardWidth, (float) cardHeight).withCentre (bounds.getCentre().toFloat());

    g.setColour (AcousticCloakLookAndFeel::Palette::panel);
    g.fillRoundedRectangle (card, 10.0f);
    g.setColour (AcousticCloakLookAndFeel::Palette::phosphorGreen.withAlpha (0.6f));
    g.drawRoundedRectangle (card.reduced (0.5f), 10.0f, 1.5f);

    auto textArea = card.reduced (20.0f).toNearestInt();

    g.setColour (AcousticCloakLookAndFeel::Palette::phosphorGreen);
    g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    auto titleArea = textArea.removeFromTop (28);
    g.drawText ("Gabci's Acoustic Cloak", titleArea, juce::Justification::centredLeft);

    g.setColour (AcousticCloakLookAndFeel::Palette::textDim);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    auto versionArea = textArea.removeFromTop (20);
    g.drawText (juce::String ("Version ") + AcousticCloakAudioProcessor::kVersionString, versionArea, juce::Justification::centredLeft);

    textArea.removeFromTop (10);

    g.setColour (AcousticCloakLookAndFeel::Palette::text);
    g.setFont (juce::Font (juce::FontOptions (13.5f)));
    auto featureArea = textArea.removeFromTop (210);
    g.drawFittedText (featureSummary, featureArea, juce::Justification::topLeft, 14);

    textArea.removeFromTop (8);

    g.setColour (AcousticCloakLookAndFeel::Palette::textDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::italic)));
    g.drawFittedText (AcousticCloakSignature::text, textArea, juce::Justification::topLeft, 4);
}
