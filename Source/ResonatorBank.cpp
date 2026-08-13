#include "ResonatorBank.h"

void ResonatorBank::prepare (double sampleRate) noexcept
{
    for (auto& r : resonators)
        r.prepare (sampleRate);
}

void ResonatorBank::reset() noexcept
{
    for (auto& r : resonators)
        r.reset();
}

void ResonatorBank::setTargetBand (float lowHz, float highHz) noexcept
{
    const auto safeHigh = juce::jmax (lowHz + 1.0f, highHz);
    const auto ratio = std::pow (safeHigh / lowHz, 1.0f / (float) kNumBands);

    for (int i = 0; i < kNumBands; ++i)
    {
        const auto edgeLow  = lowHz * std::pow (ratio, (float) i);
        const auto edgeHigh = lowHz * std::pow (ratio, (float) (i + 1));
        const auto centre    = std::sqrt (edgeLow * edgeHigh); // geometric mean
        const auto bandwidth = edgeHigh - edgeLow;

        resonators[(size_t) i].setTarget (centre, bandwidth);
    }
}

ResonatorBank::Sample ResonatorBank::process (float x) noexcept
{
    Sample combined;

    for (auto& r : resonators)
    {
        const auto s = r.process (x);
        combined.real += s.real;
        combined.imag += s.imag;
    }

    return combined;
}
