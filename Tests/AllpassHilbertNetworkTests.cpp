#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "AllpassHilbertNetwork.h"
#include <cmath>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;
    constexpr double kSettleSeconds = 4.0;  // generous margin for the lowest covered corner (~8Hz)
    constexpr double kMeasureSeconds = 3.0; // enough cycles even at 15-20Hz for a stable sin/cos fit

    struct FitResult
    {
        float amplitude = 0.0f;
        float phaseRadians = 0.0f;
    };

    // Measures amplitude/phase of a settled sinusoidal branch output via a
    // least-squares fit against sin/cos at the known test frequency --
    // avoids needing a separate FFT dependency just for this test.
    FitResult measureBranch (AllpassHilbertNetwork& network, float toneHz, bool useBranchA)
    {
        int sampleIndex = 0;
        double sumSin = 0.0, sumCos = 0.0;
        int measuredSamples = 0;

        const auto w = 2.0f * juce::MathConstants<float>::pi * toneHz;

        const auto settleSamples = (int) (kSettleSeconds * kSampleRate);
        const auto measureSamples = (int) (kMeasureSeconds * kSampleRate);
        const auto totalSamples = settleSamples + measureSamples;

        for (int i = 0; i < totalSamples; ++i)
        {
            const auto t = (float) sampleIndex / (float) kSampleRate;
            const auto x = std::sin (w * t);
            const auto out = network.process (x);
            const auto y = useBranchA ? out.branchA : out.branchB;

            if (i >= settleSamples)
            {
                sumSin += (double) y * std::sin (w * t);
                sumCos += (double) y * std::cos (w * t);
                ++measuredSamples;
            }

            ++sampleIndex;
        }
        juce::ignoreUnused (kBlockSize);

        const auto ySin = (float) (2.0 * sumSin / measuredSamples);
        const auto yCos = (float) (2.0 * sumCos / measuredSamples);

        FitResult result;
        result.amplitude = std::sqrt (ySin * ySin + yCos * yCos);
        result.phaseRadians = std::atan2 (ySin, yCos);
        return result;
    }
}

TEST_CASE ("AllpassHilbertNetwork keeps unity magnitude on both branches (allpass guarantee)", "[AllpassHilbertNetwork]")
{
    for (float toneHz : { 15.0f, 20.0f, 40.0f, 85.0f, 130.0f, 150.0f, 170.0f })
    {
        AllpassHilbertNetwork network;
        network.prepare (kSampleRate);
        network.setTargetBand (20.0f, 150.0f);
        const auto a = measureBranch (network, toneHz, true);

        AllpassHilbertNetwork network2;
        network2.prepare (kSampleRate);
        network2.setTargetBand (20.0f, 150.0f);
        const auto b = measureBranch (network2, toneHz, false);

        INFO ("tone = " << toneHz << " Hz, branchA amplitude = " << a.amplitude << ", branchB amplitude = " << b.amplitude);
        REQUIRE (a.amplitude == Catch::Approx (1.0f).margin (0.02));
        REQUIRE (b.amplitude == Catch::Approx (1.0f).margin (0.02));
    }
}

TEST_CASE ("AllpassHilbertNetwork's two branches stay close to a 90-degree phase difference across the target band", "[AllpassHilbertNetwork]")
{
    for (float toneHz : { 20.0f, 35.0f, 60.0f, 85.0f, 110.0f, 130.0f, 150.0f })
    {
        AllpassHilbertNetwork networkA;
        networkA.prepare (kSampleRate);
        networkA.setTargetBand (20.0f, 150.0f);
        const auto a = measureBranch (networkA, toneHz, true);

        AllpassHilbertNetwork networkB;
        networkB.prepare (kSampleRate);
        networkB.setTargetBand (20.0f, 150.0f);
        const auto b = measureBranch (networkB, toneHz, false);

        auto diff = a.phaseRadians - b.phaseRadians;
        while (diff > juce::MathConstants<float>::pi) diff -= juce::MathConstants<float>::twoPi;
        while (diff < -juce::MathConstants<float>::pi) diff += juce::MathConstants<float>::twoPi;

        INFO ("tone = " << toneHz << " Hz, phase difference = " << juce::radiansToDegrees (diff) << " degrees");
        REQUIRE (std::abs (std::abs (diff) - juce::MathConstants<float>::halfPi) < juce::degreesToRadians (15.0f));
    }
}

TEST_CASE ("DIAGNOSTIC: branchA's own inherent phase relative to input x", "[.diagnostic]")
{
    for (float toneHz : { 20.0f, 63.2f, 150.0f })
    {
        AllpassHilbertNetwork network;
        network.prepare (kSampleRate);
        network.setTargetBand (40.0f, 100.0f);

        int sampleIndex = 0;
        double sumSin = 0.0, sumCos = 0.0;
        int measured = 0;
        const auto w = 2.0f * juce::MathConstants<float>::pi * toneHz;
        const auto settleSamples = (int) (kSettleSeconds * kSampleRate);
        const auto measureSamples = (int) (kMeasureSeconds * kSampleRate);

        for (int i = 0; i < settleSamples + measureSamples; ++i)
        {
            const auto t = (float) sampleIndex / (float) kSampleRate;
            const auto x = std::sin (w * t);
            const auto out = network.process (x);
            if (i >= settleSamples)
            {
                sumSin += (double) out.branchA * std::sin (w * t);
                sumCos += (double) out.branchA * std::cos (w * t);
                ++measured;
            }
            ++sampleIndex;
        }
        const auto ySin = (float) (2.0 * sumSin / measured);
        const auto yCos = (float) (2.0 * sumCos / measured);
        const auto phiA = std::atan2 (ySin, yCos);
        WARN ("tone=" << toneHz << " phiA(degrees)=" << juce::radiansToDegrees (phiA) << " amplitude=" << std::sqrt(ySin*ySin+yCos*yCos));
    }
}
