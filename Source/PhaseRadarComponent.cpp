#include "PhaseRadarComponent.h"
#include "PluginProcessor.h"
#include "AcousticCloakLookAndFeel.h"

namespace
{
    constexpr int kTimerHz = 30;
}

PhaseRadarComponent::PhaseRadarComponent (AcousticCloakAudioProcessor& processorToUse)
    : processor (processorToUse),
      readoutFont (juce::FontOptions (14.0f, juce::Font::bold)),
      labelFont (juce::FontOptions (11.0f))
{
    startTimerHz (kTimerHz);
}

PhaseRadarComponent::~PhaseRadarComponent()
{
    stopTimer();
}

void PhaseRadarComponent::timerCallback()
{
    TrailPoint point;

    if (processor.isSidechainConnected() && processor.isDisplayCorrelationValid())
    {
        point.angle = processor.getDisplayPhaseDeltaRadians();
        point.correlation = processor.getDisplayCorrelation();
        // Map |phase delta| (0 = aligned .. pi = maximally misaligned) to a
        // 0..1 radius -- aligned content collapses toward the centre.
        point.radius = juce::jlimit (0.0f, 1.0f, std::abs (point.angle) / juce::MathConstants<float>::pi);
    }
    else
    {
        point = {}; // no signal: collapse to the centre, neutral colour
    }

    trail[(size_t) trailWritePos] = point;
    trailWritePos = (trailWritePos + 1) % kTrailLength;
    trailFilled = juce::jmin (trailFilled + 1, kTrailLength);

    repaint();
}

void PhaseRadarComponent::paint (juce::Graphics& g)
{
    g.fillAll (AcousticCloakLookAndFeel::Palette::background);

    auto area = getLocalBounds().toFloat();
    auto readoutArea = area.removeFromBottom (34.0f);
    area.removeFromTop (16.0f); // room for the "ALIGNED" label above the 12 o'clock tick

    drawGrid (g, area);

    const auto centre = area.getCentre();
    const auto maxRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 6.0f;
    drawTrail (g, centre, maxRadius);

    drawReadout (g, readoutArea);
}

void PhaseRadarComponent::resized() {}

void PhaseRadarComponent::drawGrid (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const auto centre = area.getCentre();
    const auto maxRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 6.0f;

    // Soft phosphor bloom behind the whole grid -- a few widening,
    // low-alpha passes around the outer ring and axis lines, the same
    // multi-pass technique the knobs use for their value arc, so every
    // outline in the UI reads as "glowing" rather than hard-edged.
    for (int i = 3; i >= 1; --i)
    {
        const auto glowWidth = 1.0f + (float) i * 2.0f;
        g.setColour (AcousticCloakLookAndFeel::Palette::phosphorGreen.withAlpha (0.05f * (float) i));
        g.drawEllipse (centre.x - maxRadius, centre.y - maxRadius, maxRadius * 2.0f, maxRadius * 2.0f, glowWidth);
    }

    g.setColour (AcousticCloakLookAndFeel::Palette::track.withAlpha (0.9f));
    for (int ring = 1; ring <= 3; ++ring)
    {
        const auto r = maxRadius * (float) ring / 3.0f;
        g.drawEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.0f);
    }

    for (int i = 0; i < 8; ++i)
    {
        const auto angle = (float) i * juce::MathConstants<float>::pi / 4.0f;
        const juce::Point<float> tip (centre.x + std::sin (angle) * maxRadius,
                                       centre.y - std::cos (angle) * maxRadius);

        for (int glow = 2; glow >= 1; --glow)
        {
            g.setColour (AcousticCloakLookAndFeel::Palette::phosphorGreen.withAlpha (0.04f * (float) glow));
            g.drawLine ({ centre, tip }, 1.0f + (float) glow * 1.5f);
        }
        g.setColour (AcousticCloakLookAndFeel::Palette::track.withAlpha (0.9f));
        g.drawLine ({ centre, tip }, 1.0f);
    }

    // Bright tick at 0 degrees (top = perfectly aligned).
    g.setColour (AcousticCloakLookAndFeel::Palette::phosphorGreen.withAlpha (0.5f));
    g.drawLine (centre.x, centre.y - maxRadius, centre.x, centre.y - maxRadius + 10.0f, 2.0f);

    g.setColour (AcousticCloakLookAndFeel::Palette::textDim);
    g.setFont (labelFont);
    g.drawText ("ALIGNED", juce::Rectangle<float> (centre.x - 40.0f, centre.y - maxRadius - 16.0f, 80.0f, 14.0f),
                juce::Justification::centred);
}

void PhaseRadarComponent::drawTrail (juce::Graphics& g, juce::Point<float> centre, float maxRadius) const
{
    for (int i = 0; i < trailFilled; ++i)
    {
        // Oldest first, so the newest point is drawn on top with full alpha.
        const auto index = (size_t) ((trailWritePos - trailFilled + i + kTrailLength * 2) % kTrailLength);
        const auto& p = trail[index];

        const auto age = (float) i / (float) juce::jmax (1, trailFilled - 1); // 0 = oldest, 1 = newest
        const auto alpha = 0.12f + 0.78f * age;

        const juce::Point<float> pos (centre.x + std::sin (p.angle) * p.radius * maxRadius,
                                       centre.y - std::cos (p.angle) * p.radius * maxRadius);

        // Blend phosphor green (good correlation) toward amber (poor/negative).
        const auto blend = juce::jlimit (0.0f, 1.0f, (1.0f - p.correlation) * 0.5f);
        const auto colour = AcousticCloakLookAndFeel::Palette::phosphorGreen
                                 .interpolatedWith (AcousticCloakLookAndFeel::Palette::amber, blend)
                                 .withAlpha (alpha);

        const auto dotRadius = 3.0f + 3.0f * age;
        g.setColour (colour);
        g.fillEllipse (pos.x - dotRadius, pos.y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
    }
}

void PhaseRadarComponent::drawReadout (juce::Graphics& g, juce::Rectangle<float> area) const
{
    g.setFont (readoutFont);

    if (! processor.isSidechainConnected())
    {
        g.setColour (AcousticCloakLookAndFeel::Palette::textDim);
        g.drawText ("NO SIDECHAIN CONNECTED", area, juce::Justification::centred);
        return;
    }

    if (! processor.isDisplayCorrelationValid())
    {
        g.setColour (AcousticCloakLookAndFeel::Palette::textDim);
        g.drawText ("AWAITING KICK TRANSIENT...", area, juce::Justification::centred);
        return;
    }

    const auto correlation = processor.getDisplayCorrelation();
    const auto phaseDeg = juce::radiansToDegrees (processor.getDisplayPhaseDeltaRadians());

    const auto text = juce::String ("CORRELATION: ") + juce::String (correlation, 2)
                       + "   PHASE " + juce::String (juce::CharPointer_UTF8 ("\xce\x94")) + ": "
                       + juce::String (phaseDeg, 1) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));

    g.setColour (correlation >= 0.0f ? AcousticCloakLookAndFeel::Palette::phosphorGreen
                                      : AcousticCloakLookAndFeel::Palette::amber);
    g.drawText (text, area, juce::Justification::centred);
}
