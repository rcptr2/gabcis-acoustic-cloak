#include "ComplexResonator.h"

void ComplexResonator::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    reset();
}

void ComplexResonator::reset() noexcept
{
    yReal = 0.0f;
    yImag = 0.0f;
}

void ComplexResonator::setTarget (float centreFreqHz, float bandwidthHz) noexcept
{
    // -3dB bandwidth (Hz) of a one-pole resonator with radius r is
    // approximately (1-r)*fs/pi for r close to 1. Solving for r:
    const auto fs = (float) sampleRate;
    const auto minBandwidth = fs * 0.0005f; // avoid r -> 1 (near-infinite ringing)
    const auto bw = juce::jmax (minBandwidth, bandwidthHz);

    auto r = 1.0f - juce::MathConstants<float>::pi * bw / fs;
    r = juce::jlimit (0.0f, 0.999f, r);

    const auto w0 = juce::MathConstants<float>::twoPi * centreFreqHz / fs;

    poleReal = r * std::cos (w0);
    poleImag = r * std::sin (w0);
    inputGain = 1.0f - r;
}

ComplexResonator::Sample ComplexResonator::process (float x) noexcept
{
    // Complex multiply-accumulate: y = p*y + gain*x (x is real, so it only
    // contributes to the real accumulator's input term).
    const auto newReal = poleReal * yReal - poleImag * yImag + inputGain * x;
    const auto newImag = poleReal * yImag + poleImag * yReal;

    yReal = newReal;
    yImag = newImag;

    return { yReal, yImag };
}
