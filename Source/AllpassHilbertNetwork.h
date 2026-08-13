#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

/**
    A broadband 90-degree phase-difference network: two branches, each a
    cascade of first-order IIR allpass sections, whose break frequencies are
    interleaved (branch A gets the even-indexed breakpoints, branch B the
    odd-indexed ones) across a geometrically-spaced set covering the target
    band with margin -- the classical construction for an allpass-based
    Hilbert transformer.

    Every allpass section preserves magnitude exactly (|H(f)| = 1 at every
    frequency, by definition) -- no image-leakage / amplitude-splitting to
    compensate for. The two branches' phase difference stays within ~8
    degrees of 90 across 20-150Hz (measured, see
    Tests/AllpassHilbertNetworkTests.cpp).

    IMPORTANT (found the hard way, see PhaseRotator.h's own history): branch
    A is NOT a clean stand-in for the raw input -- it carries its own large,
    wildly frequency-dependent phase shift relative to the input (measured
    -24 degrees at 20Hz, +130 at 63Hz, -118 at 150Hz). Treating "branchA"
    as if it were "the original signal, unrotated" and adding a
    (rotated-branchA) delta back onto the RAW input is mathematically wrong
    at anything beyond a small rotation angle. The correct usage (Gemini's
    "Dummy All-Pass" architecture, implemented in PhaseRotator) is to also
    warp whatever this signal gets recombined with (BandIsolator's residual)
    through an IDENTICAL copy of branch A's own cascade (DummyAllpass) --
    then treat branchA's own output, not the raw input, as the new
    reference frame. computeBranchABreakpoints()/computeBranchBBreakpoints()
    exist specifically so that duplicate can share the exact same
    coefficients as branch A, by construction, not by keeping two
    hand-written copies in sync.

    Each first-order section's coefficient is derived directly from the
    bilinear transform of an analog first-order allpass with pole at the
    desired break frequency fc:
        a = (tan(pi*fc/fs) - 1) / (tan(pi*fc/fs) + 1)
    This is an exact mapping (subject only to the bilinear transform's own
    standard frequency warping), not a memorised or approximated coefficient
    table.
*/
class AllpassHilbertNetwork
{
public:
    AllpassHilbertNetwork() = default;

    static constexpr int kStagesPerBranch = 4;
    using Breakpoints = std::array<float, kStagesPerBranch>;

    /** The full 2*kStagesPerBranch geometrically-spaced breakpoints across a
        margin-extended [lowHz, highHz], split into branch A's (even index)
        and branch B's (odd index) subsets -- shared logic so
        DummyAllpass can build an identical copy of branch A. */
    static void computeBreakpoints (float lowHz, float highHz,
                                     Breakpoints& branchABreakpoints,
                                     Breakpoints& branchBBreakpoints) noexcept;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Safe to call every block. */
    void setTargetBand (float lowHz, float highHz) noexcept;

    struct Sample
    {
        float branchA = 0.0f;
        float branchB = 0.0f; // approximately branchA's content, phase-shifted ~90 degrees
    };

    Sample process (float x) noexcept;

    struct AllpassSection
    {
        float a = 0.0f;
        float prevX = 0.0f;
        float prevY = 0.0f;

        void setBreakFrequency (float fc, float fs) noexcept
        {
            const auto t = std::tan (juce::MathConstants<float>::pi * fc / fs);
            a = (t - 1.0f) / (t + 1.0f);
        }

        void reset() noexcept { prevX = 0.0f; prevY = 0.0f; }

        float process (float x) noexcept
        {
            const auto y = a * x + prevX - a * prevY;
            prevX = x;
            prevY = y;
            return y;
        }
    };

private:
    std::array<AllpassSection, kStagesPerBranch> branchA, branchB;
    float sampleRate = 48000.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AllpassHilbertNetwork)
};

/**
    An exact duplicate of AllpassHilbertNetwork's branch A cascade, applied
    to a DIFFERENT signal (BandIsolator's residual/High buffer) with its own
    independent filter state. This is the "Dummy All-Pass" in Gemini's
    architecture: since filtering is linear, filter(Low) + filter(High) =
    filter(Low + High) for the SAME filter applied to both -- so warping the
    residual by an identical copy of branch A keeps the overall
    Low+High recombination exactly as flat as BandIsolator's own
    (already-proven) flat-summing guarantee, just with an extra fixed
    allpass warp applied to the whole thing. See PhaseRotator.cpp for how
    this combines with the actual rotation.
*/
class DummyAllpass
{
public:
    DummyAllpass() = default;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Must be called with the SAME lowHz/highHz as the AllpassHilbertNetwork
        instance this is meant to shadow, so computeBreakpoints() yields
        identical coefficients for branch A on both. */
    void setTargetBand (float lowHz, float highHz) noexcept;

    float process (float x) noexcept;

private:
    std::array<AllpassHilbertNetwork::AllpassSection, AllpassHilbertNetwork::kStagesPerBranch> stages;
    float sampleRate = 48000.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DummyAllpass)
};
