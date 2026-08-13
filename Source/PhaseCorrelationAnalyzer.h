#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "ResonatorBank.h"

/**
    Continuously tracks the instantaneous phase difference between the
    Sidechain (Kick) and Main (Bass) signals within a target frequency
    band, using one ResonatorBank per signal as an analytic-signal
    extractor. Both channels of a stereo buffer are summed to mono before
    analysis, matching the sibling PhaseLockSub project's
    CrossCorrelationAnalyzer convention.

    Revision note (v0.2.1, following a design review): the caller
    (PluginProcessor) is now responsible for isolating the target band via
    BandIsolator BEFORE calling processBlock() here -- this class only ever
    sees already band-limited Main/Sidechain content for the phase
    comparison, not the raw broadband signal. Two reasons: (1) it confines
    ResonatorBank's own (much smaller now, but non-zero) skirts to content
    that's already inside the intended range, so nothing outside the user's
    Target Frequency Range gets touched; (2) it decouples this analysis
    entirely from Phase 3's reconstruction path, which needs that same
    isolated band anyway.

    Transient Sensitivity is measured from the RAW broadband Sidechain
    signal (passed separately, unfiltered) rather than from the band
    -limited/resonant analysis signal -- a resonant filter rings, which
    would make a short Kick transient's detected envelope decay far slower
    than the actual transient, smearing the "is a Kick happening right now"
    read. A plain broadband envelope follower has no such ringing.
*/
class PhaseCorrelationAnalyzer
{
public:
    PhaseCorrelationAnalyzer() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Safe to call every block; internally smoothed (matches
        BandIsolator's own 50ms ramp) so the resonator bank's sub-bands
        track the same target the caller's BandIsolator is converging to. */
    void setTargetBand (float lowHz, float highHz) noexcept;

    /** sensitivity in [0, 1]: 0 = only strong transients register, 1 = even
        quiet Sidechain content is treated as active. */
    void setTransientSensitivity (float sensitivity) noexcept;

    struct Result
    {
        bool isValid = false;

        /** Sidechain phase minus Main phase, wrapped to [-pi, pi]. */
        float phaseDeltaRadians = 0.0f;

        /** cos(phaseDeltaRadians) of the SMOOTHED estimate: +1 = perfectly
            in phase, -1 = perfectly out of phase (polarity issue), 0 =
            quadrature. */
        float correlation = 0.0f;
    };

    /** sidechainRawBuffer: unfiltered Sidechain, used only for the Transient
        Sensitivity activity envelope. sidechainBandBuffer/mainBandBuffer:
        already isolated to the target band by the caller's BandIsolator(s),
        used for the actual phase comparison. Real-time safe. */
    void processBlock (const juce::AudioBuffer<float>& sidechainRawBuffer,
                        const juce::AudioBuffer<float>& sidechainBandBuffer,
                        const juce::AudioBuffer<float>& mainBandBuffer,
                        int numSamples) noexcept;

    const Result& getLatestResult() const noexcept { return latestResult; }

private:
    double sampleRate = 48000.0;

    ResonatorBank mainBank, sidechainBank;
    juce::SmoothedValue<float> smoothedLowHz, smoothedHighHz;

    // Broadband activity envelope follower (fast attack / slow release)
    // plus a slow noise floor, on the RAW Sidechain -- see class doc for why
    // this must not be derived from the resonant analysis signal.
    float fastEnvelope = 0.0f;
    float slowFloor = 0.0f;
    float fastCoeff = 0.0f;
    float slowCoeff = 0.0f;
    float sensitivityRatio = 2.5f;

    // Smoothed phase-difference state, tracked as a unit vector (cos/sin)
    // so the exponential blend never has to unwrap across the +-pi seam.
    float smoothedCos = 1.0f;
    float smoothedSin = 0.0f;
    float attackCoeff = 0.0f;  // ~5ms, used while "active"
    float releaseCoeff = 0.0f; // ~300ms, used while quiet

    bool everActive = false;

    Result latestResult;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseCorrelationAnalyzer)
};
