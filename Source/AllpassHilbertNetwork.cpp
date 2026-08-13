#include "AllpassHilbertNetwork.h"

namespace
{
    constexpr float kLowMarginRatio = 0.2f;
    constexpr float kHighMarginRatio = 5.0f;
}

void AllpassHilbertNetwork::computeBreakpoints (float lowHz, float highHz,
                                                 Breakpoints& branchABreakpoints,
                                                 Breakpoints& branchBBreakpoints) noexcept
{
    const auto safeHigh = juce::jmax (lowHz + 1.0f, highHz);
    const auto spanLow  = juce::jmax (1.0f, lowHz * kLowMarginRatio);
    const auto spanHigh = safeHigh * kHighMarginRatio;

    constexpr int kTotalBreakpoints = kStagesPerBranch * 2;
    const auto ratio = std::pow (spanHigh / spanLow, 1.0f / (float) (kTotalBreakpoints - 1));

    for (int i = 0; i < kTotalBreakpoints; ++i)
    {
        const auto fc = spanLow * std::pow (ratio, (float) i);

        if (i % 2 == 0)
            branchABreakpoints[(size_t) (i / 2)] = fc;
        else
            branchBBreakpoints[(size_t) (i / 2)] = fc;
    }
}

void AllpassHilbertNetwork::prepare (double newSampleRate) noexcept
{
    sampleRate = (float) newSampleRate;
    reset();
}

void AllpassHilbertNetwork::reset() noexcept
{
    for (auto& s : branchA) s.reset();
    for (auto& s : branchB) s.reset();
}

void AllpassHilbertNetwork::setTargetBand (float lowHz, float highHz) noexcept
{
    Breakpoints aBreakpoints, bBreakpoints;
    computeBreakpoints (lowHz, highHz, aBreakpoints, bBreakpoints);

    for (int i = 0; i < kStagesPerBranch; ++i)
    {
        branchA[(size_t) i].setBreakFrequency (aBreakpoints[(size_t) i], sampleRate);
        branchB[(size_t) i].setBreakFrequency (bBreakpoints[(size_t) i], sampleRate);
    }
}

AllpassHilbertNetwork::Sample AllpassHilbertNetwork::process (float x) noexcept
{
    auto a = x;
    for (auto& s : branchA)
        a = s.process (a);

    auto b = x;
    for (auto& s : branchB)
        b = s.process (b);

    return { a, b };
}

//==============================================================================
void DummyAllpass::prepare (double newSampleRate) noexcept
{
    sampleRate = (float) newSampleRate;
    reset();
}

void DummyAllpass::reset() noexcept
{
    for (auto& s : stages)
        s.reset();
}

void DummyAllpass::setTargetBand (float lowHz, float highHz) noexcept
{
    AllpassHilbertNetwork::Breakpoints aBreakpoints, bBreakpoints;
    AllpassHilbertNetwork::computeBreakpoints (lowHz, highHz, aBreakpoints, bBreakpoints);

    for (int i = 0; i < AllpassHilbertNetwork::kStagesPerBranch; ++i)
        stages[(size_t) i].setBreakFrequency (aBreakpoints[(size_t) i], sampleRate);
}

float DummyAllpass::process (float x) noexcept
{
    auto y = x;
    for (auto& s : stages)
        y = s.process (y);
    return y;
}
