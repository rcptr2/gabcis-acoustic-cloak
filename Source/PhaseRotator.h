#pragma once

#include "AllpassHilbertNetwork.h"
#include <array>

/**
    Phase 3's actual audio-path correction -- the "Dummy All-Pass"
    architecture (from a third round of design review, credited to
    Gemini), the resolution of a real problem found in this file's own
    prior two designs.

    Design history:
    v0.2.1 found summing multiple resonant filters for reconstruction risks
    a "lumpy" magnitude response. A nested 3-way crossover split was tried
    and discarded (~20% interference from two internal crossover points too
    close together). A single ComplexResonator fixed both multi-filter
    problems but then measured ~65% rotation error, traced to the
    resonator's own documented negative-frequency image leakage.
    AllpassHilbertNetwork (magnitude-exact, no leakage) fixed THAT, but
    exposed a deeper problem: adding `(rotated - branchA)` back onto the
    RAW input assumes branchA is a clean stand-in for the input's own
    phase -- it isn't. Measured branchA's own phase relative to the input:
    -24 degrees at 20Hz, +130 at 63Hz, -118 at 150Hz. Mixing that
    phase-warped correction back into UNwarped raw input caused >100%
    error at large rotation angles.

    The fix: stop trying to reference the raw input at all. BandIsolator's
    residual gets warped by an IDENTICAL copy of the same branch-A cascade
    (DummyAllpass) before recombination. Because filtering is linear,
    `filter(Low) + filter(High) == filter(Low + High)` for the SAME filter
    on both -- so `Ilow + Ihigh` stays exactly as flat as BandIsolator's own
    proven flat-summing guarantee (just with a fixed, frequency-independent
    -in-effect allpass warp applied to the whole reconstructed signal,
    which is sonically transparent -- static phase shifts in the sub-bass
    aren't audible as coloration). At Cloak Depth 0% (`theta == 0`),
    `Rlow == Ilow` exactly, so `output == Ilow + Ihigh`: still transparent
    IN THE FLAT-MAGNITUDE sense, not bit-identical to the dry signal --
    true bit-exact passthrough remains the separate Bypass parameter's job
    (PluginProcessor's dryBuffer/bypassMix, untouched by any of this).
*/
class PhaseRotator
{
public:
    PhaseRotator() = default;

    void prepare (double sampleRate, int numChannels) noexcept;
    void reset() noexcept;

    /** Safe to call every block. */
    void setTargetBand (float lowHz, float highHz) noexcept;

    /** thetaRadians: the desired phase rotation (already scaled by Cloak
        Depth by the caller). Internally smoothed via its own cos/sin pair
        (10ms ramp, renormalised each sample) so a sudden change in the
        measured phase delta doesn't click. */
    void setRotationRadians (float thetaRadians) noexcept;

    /** In-place: targetBandBuffer (BandIsolator's isolated band content)
        becomes Rlow (the rotated signal); residualBuffer (everything else)
        becomes Ihigh (the same allpass warp, unrotated). The caller sums
        the two afterwards -- see this class's own doc comment for why that
        sum stays flat regardless of the rotation angle. Both buffers must
        match the channel count passed to prepare(). Real-time safe. */
    void process (juce::AudioBuffer<float>& targetBandBuffer,
                  juce::AudioBuffer<float>& residualBuffer,
                  int numSamples) noexcept;

private:
    static constexpr int kMaxChannels = 2; // Main bus is enforced mono/stereo by isBusesLayoutSupported()

    std::array<AllpassHilbertNetwork, kMaxChannels> networks; // Low band -> Ilow/Qlow
    std::array<DummyAllpass, kMaxChannels> dummies;            // High/residual -> Ihigh, identical warp to branch A
    int numActiveChannels = 0;

    juce::SmoothedValue<float> smoothedCosTheta, smoothedSinTheta;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseRotator)
};
