#pragma once

#include <juce_dsp/juce_dsp.h>

/**
    A normalised complex one-pole resonator: y[n] = p*y[n-1] + (1-r)*x[n],
    with p = r * (cos(w0) + j*sin(w0)). Fed a real input, it produces a
    complex output that is an approximate analytic (I/Q) signal of the
    input's content near the centre frequency w0 -- the real and imaginary
    parts are naturally in quadrature by construction, with a resonance
    bandwidth controlled by the pole radius r.

    This is the core building block of the Acoustic Cloak engine: instead of
    an STFT/overlap-add phase vocoder (flagged in the blueprint itself as a
    CPU/pre-ringing risk at sub-bass resolution -- a 20-150Hz bin needs an
    impractically large FFT window), a single complex resonator per channel
    both ISOLATES the target band (it's a bandpass filter) AND exposes its
    instantaneous phase (atan2(imag, real)) at a cost of ~4 multiplies + 4
    adds per sample. Two instances (Main/Sidechain) give everything
    PhaseCorrelationAnalyzer needs for a continuous phase-difference
    estimate, and the same mechanism (Phase 3) rotates the Main signal's
    target-band content by feeding its own analytic output back through
    e^{j*theta} and taking the real part.

    setTarget() recomputes the pole from centreFreqHz/bandwidthHz but is only
    safe to call once per block (it does a cos/sin + division) -- like
    CrossoverEngine's setCrossoverFrequency(), continuous per-sample
    modulation is not intended, and the caller should smooth the target
    values itself (SmoothedValue, block-rate) if they're user/automation
    -driven, to avoid an audible jump in the resonator's response.

    Two notes for anything downstream that reads magnitude/phase directly:
    a real sine of amplitude 1 splits into two equal 0.5 phasors at +freq
    and -freq, so a real input settles to ~0.5 output magnitude at the
    matched frequency, not ~1.0 -- and at very low centre frequencies
    (relative to sample rate, e.g. this project's 20-150Hz band), this
    single-pole design only partially rejects the unwanted -freq image,
    which shows up as a small oscillation on top of that ~0.5 baseline.
    Verified acceptable for PhaseCorrelationAnalyzer's phase-DIFFERENCE
    measurement (both signals carry the same artifact, so it mostly cancels
    in z = side*conj(main)); Phase 3's rotator, which uses this output
    directly as the signal to reconstruct, re-checks whether it's clean
    enough on its own.
*/
class ComplexResonator
{
public:
    ComplexResonator() = default;

    void prepare (double newSampleRate) noexcept;
    void reset() noexcept;

    /** Recomputes the resonator's pole. bandwidthHz is clamped to a sane
        minimum so the pole radius never reaches (or exceeds) 1, which would
        make the filter unstable. */
    void setTarget (float centreFreqHz, float bandwidthHz) noexcept;

    struct Sample
    {
        float real = 0.0f;
        float imag = 0.0f;
    };

    Sample process (float x) noexcept;

private:
    double sampleRate = 48000.0;

    float poleReal = 0.0f;
    float poleImag = 0.0f;
    float inputGain = 0.0f; // (1 - r)

    float yReal = 0.0f;
    float yImag = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComplexResonator)
};
