#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class AcousticCloakAudioProcessor;

/**
    The "Holographic Radar": a polar, phosphor-green display of the live
    Kick/Bass phase relationship, reading PluginProcessor's already
    -published thread-safe atomics (getDisplayCorrelation(),
    getDisplayPhaseDeltaRadians(), isDisplayCorrelationValid(),
    isSidechainConnected()) -- no new DSP-side capture buffer was added for
    this, unlike the sibling PhaseLockSub project's raw-waveform
    oscilloscope, since the correlation/phase-delta pair is already exactly
    what this needs to plot.

    Angle = measured phase delta (0 at 12 o'clock = perfectly aligned,
    sweeping toward the rim as misalignment approaches +-180 degrees);
    the dot's glow colour blends from phosphor green (high correlation) to
    amber (poor/negative correlation). A short fading trail (a small
    ring buffer of recent points) gives it a living, radar-sweep quality --
    as Cloak Depth pulls the phase delta toward zero, the trail collapses
    toward the centre instead of drifting around the rim.

    Repaints from a message-thread Timer (30Hz), never the audio thread.
*/
class PhaseRadarComponent final : public juce::Component,
                                   private juce::Timer
{
public:
    explicit PhaseRadarComponent (AcousticCloakAudioProcessor& processorToUse);
    ~PhaseRadarComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawTrail (juce::Graphics& g, juce::Point<float> centre, float maxRadius) const;
    void drawReadout (juce::Graphics& g, juce::Rectangle<float> area) const;

    AcousticCloakAudioProcessor& processor;

    static constexpr int kTrailLength = 40;
    struct TrailPoint { float angle = 0.0f; float radius = 0.0f; float correlation = 0.0f; };
    std::array<TrailPoint, kTrailLength> trail;
    int trailWritePos = 0;
    int trailFilled = 0;

    juce::Font readoutFont, labelFont;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseRadarComponent)
};
