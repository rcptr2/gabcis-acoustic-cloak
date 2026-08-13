#pragma once

#include <juce_dsp/juce_dsp.h>

/**
    Isolates a [lowHz, highHz] target band from a signal via two chained
    4th-order (24dB/oct) Linkwitz-Riley crossover splits (reusing the
    sibling PhaseLockSub project's proven single-filter dual-output
    technique, generalised from a 2-way split to 3-way):

        input --[split @ lowHz]--> belowLow (kept aside)
                                    aboveLow --[split @ highHz]--> targetBand
                                                                    aboveHigh

        targetBand = the isolated [lowHz, highHz] content
        residual   = belowLow + aboveHigh (everything else, untouched)

    Because both splits are flat-magnitude-summing crossovers (same
    guarantee as the sibling project's 2-way CrossoverEngine), targetBand +
    residual reconstructs the original signal's magnitude spectrum exactly
    -- this is what lets Phase 3 rotate ONLY targetBand and recombine with
    residual afterwards without colouring or comb-filtering anything outside
    the user's chosen Target Frequency Range. This addresses a review
    finding: running the whole broadband Main signal through a resonant
    filter and mixing the residual back in (this project's original Phase 2
    draft) is not a magnitude-flat operation and risks exactly that
    coloration -- a real crossover, isolating the band BEFORE any resonant
    processing touches it, avoids the problem entirely.
*/
class BandIsolator
{
public:
    BandIsolator() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Safe to call every block; internally smoothed (50ms ramp, same
        convention as the sibling project's CrossoverEngine) to avoid a
        zipper artifact when the user drags the Target Freq sliders. */
    void setTargetBand (float lowHz, float highHz) noexcept;

    /** targetBand and residual are overwritten (not accumulated). All three
        blocks must share the same channel/sample counts. */
    void process (const juce::dsp::AudioBlock<const float>& input,
                  juce::dsp::AudioBlock<float>& targetBand,
                  juce::dsp::AudioBlock<float>& residual) noexcept;

private:
    juce::dsp::LinkwitzRileyFilter<float> lowSplitFilter;  // splits at lowHz
    juce::dsp::LinkwitzRileyFilter<float> highSplitFilter; // splits at highHz

    juce::SmoothedValue<float> smoothedLowHz, smoothedHighHz;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandIsolator)
};
