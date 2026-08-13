#pragma once

#include "ComplexResonator.h"
#include <array>

/**
    A small bank of ComplexResonators covering a [lowHz, highHz] band via
    geometrically-spaced sub-bands, instead of one resonator centred on the
    whole range. Addresses a review finding on the original (single
    -resonator) Phase 2 design: with only one resonance point, tuning to the
    band's midpoint leaves the band's own edges several dB down and
    phase-distorted relative to the centre -- exactly the wrong behaviour
    for a band the user expects to be treated uniformly (e.g. the default
    20-150Hz, where the low end is nearly an octave-and-a-half below the
    geometric centre). Splitting into kNumBands narrower sub-bands, each
    handled by its own resonator, keeps every sub-band close to ITS OWN
    centre, so the combined response stays close to flat (in the "how much
    of this content survives" sense) across the whole range.

    process() sums the sub-bands' complex outputs into one combined
    analytic signal -- both for PhaseCorrelationAnalyzer's phase-difference
    measurement and (Phase 3) for reconstructing the rotated band, the same
    global rotation angle is intended to apply uniformly across all
    sub-bands, since a Kick/Bass pair's misalignment is a single physical
    timing relationship, not one that varies by sub-band.
*/
class ResonatorBank
{
public:
    ResonatorBank() = default;

    static constexpr int kNumBands = 3;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Recomputes all kNumBands sub-resonators from a geometric split of
        [lowHz, highHz]. Like ComplexResonator::setTarget(), only safe to
        call once per block -- the caller (PhaseCorrelationAnalyzer) owns
        the block-rate smoothing of lowHz/highHz. */
    void setTargetBand (float lowHz, float highHz) noexcept;

    struct Sample
    {
        float real = 0.0f;
        float imag = 0.0f;
    };

    Sample process (float x) noexcept;

private:
    std::array<ComplexResonator, kNumBands> resonators;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonatorBank)
};
