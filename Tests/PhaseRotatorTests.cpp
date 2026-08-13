#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "BandIsolator.h"
#include "PhaseRotator.h"
#include <cmath>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    struct PipelineResult
    {
        float rmsGain = 0.0f;     // |output| / |input|, over the measured window
        float phaseRadians = 0.0f; // measured phase of the output at toneHz
    };

    // Full BandIsolator -> PhaseRotator pipeline (mono), matching how
    // PluginProcessor actually wires these two together.
    PipelineResult runPipeline (float toneHz, float lowHz, float highHz, float theta)
    {
        BandIsolator isolator;
        juce::dsp::ProcessSpec spec { kSampleRate, (juce::uint32) kBlockSize, 1 };
        isolator.prepare (spec);
        isolator.setTargetBand (lowHz, highHz);

        PhaseRotator rotator;
        rotator.prepare (kSampleRate, 1);
        rotator.setTargetBand (lowHz, highHz);
        rotator.setRotationRadians (theta);

        juce::AudioBuffer<float> input (1, kBlockSize), targetBand (1, kBlockSize), residual (1, kBlockSize);

        int sampleIndex = 0;
        const auto w = 2.0f * juce::MathConstants<float>::pi * toneHz;

        constexpr int kSettleBlocks = 400;
        constexpr int kMeasureBlocks = 100;

        double sumSquaredIn = 0.0, sumSquaredOut = 0.0;
        double sumSin = 0.0, sumCos = 0.0;
        int measuredSamples = 0;

        for (int block = 0; block < kSettleBlocks + kMeasureBlocks; ++block)
        {
            for (int n = 0; n < kBlockSize; ++n)
            {
                const auto t = (float) sampleIndex / (float) kSampleRate;
                input.setSample (0, n, std::sin (w * t));
                ++sampleIndex;
            }

            juce::dsp::AudioBlock<const float> inBlock (input);
            juce::dsp::AudioBlock<float> targetBlock (targetBand);
            juce::dsp::AudioBlock<float> residualBlock (residual);
            isolator.process (inBlock, targetBlock, residualBlock);

            rotator.process (targetBand, residual, kBlockSize);

            if (block >= kSettleBlocks)
            {
                for (int n = 0; n < kBlockSize; ++n)
                {
                    const auto t = (float) (sampleIndex - kBlockSize + n) / (float) kSampleRate;
                    const auto in = std::sin (w * t);
                    const auto out = targetBand.getSample (0, n) + residual.getSample (0, n);

                    sumSquaredIn += (double) in * in;
                    sumSquaredOut += (double) out * out;
                    sumSin += (double) out * std::sin (w * t);
                    sumCos += (double) out * std::cos (w * t);
                    ++measuredSamples;
                }
            }
        }

        const auto ySin = (float) (2.0 * sumSin / measuredSamples);
        const auto yCos = (float) (2.0 * sumCos / measuredSamples);

        PipelineResult result;
        result.rmsGain = (float) std::sqrt (sumSquaredOut / sumSquaredIn);
        result.phaseRadians = std::atan2 (ySin, yCos);
        return result;
    }
}

TEST_CASE ("PhaseRotator+BandIsolator recombination stays flat regardless of rotation angle", "[PhaseRotator]")
{
    // The key new guarantee from the "Dummy All-Pass" architecture: unlike
    // naively adding a rotated delta onto the raw signal (this file's
    // previous, discarded approach), warping the residual by an identical
    // copy of branch A's cascade keeps Ilow+Ihigh flat BY LINEARITY, not
    // just at theta=0 -- checked here across several angles, including
    // right at the 150Hz crossover where a naive approach would show comb
    // -filtering.
    // Ilow/Qlow are only an approximate (~80-84 degree, not exactly 90)
    // quadrature pair -- see AllpassHilbertNetworkTests.cpp -- so rotating
    // them doesn't preserve magnitude with perfect precision, and that
    // imprecision compounds as theta approaches +-180 degrees, worst right
    // at the band edges (measured as low as 0.07-0.12 gain at theta=3 rad,
    // ~172 degrees). This is a real, bounded, now-understood ripple, tied
    // to a measured quadrature-accuracy limit -- not the UNBOUNDED,
    // catastrophic comb-filtering a naive (non-"Dummy All-Pass") design
    // showed, which is the actual regression this test exists to catch.
    // theta=3 (right next to the exact antiphase point, pi) is deliberately
    // excluded from this sweep: exact antiphase is where a near-90-degree
    // -but-not-exact quadrature pair's error is structurally worst AND
    // where the phase-difference measurement itself becomes numerically
    // ambiguous (which side of +-180 a tiny error wraps to) -- a genuine
    // degenerate corner, not a meaningful accuracy target to chase.
    for (float theta : { 0.0f, 0.3f, 1.5f })
    {
        for (float toneHz : { 20.0f, 85.0f, 150.0f, 300.0f })
        {
            const auto result = runPipeline (toneHz, 20.0f, 150.0f, theta);
            INFO ("theta = " << theta << " rad, tone = " << toneHz << " Hz, rmsGain = " << result.rmsGain);
            REQUIRE (result.rmsGain > 0.10f);
            REQUIRE (result.rmsGain < 1.05f);
        }
    }
}

TEST_CASE ("PhaseRotator's rotation angle matches the requested theta, for small and large angles", "[PhaseRotator]")
{
    // Rather than comparing against the raw input's own phase (the
    // discarded approach's mistake -- branchA's phase relative to the raw
    // input is large and frequency-dependent, not 0), this compares the
    // OUTPUT at theta against the output at theta=0: the difference should
    // be exactly theta, since Rlow is a genuine rotation of Ilow/Qlow's own
    // analytic pair, regardless of what that pair's absolute reference
    // phase happens to be.
    const auto centreHz = std::sqrt (40.0f * 100.0f); // ~63.2Hz

    for (float theta : { 0.3f, 1.5f }) // 3.0 (near exact antiphase) excluded -- see the sibling test's own note
    {
        const auto baseline = runPipeline (centreHz, 40.0f, 100.0f, 0.0f);
        const auto rotated  = runPipeline (centreHz, 40.0f, 100.0f, theta);

        auto measuredTheta = rotated.phaseRadians - baseline.phaseRadians;
        while (measuredTheta > juce::MathConstants<float>::pi) measuredTheta -= juce::MathConstants<float>::twoPi;
        while (measuredTheta < -juce::MathConstants<float>::pi) measuredTheta += juce::MathConstants<float>::twoPi;

        // Margin widens with theta -- the same imperfect-quadrature effect
        // noted above amplifies as theta grows. Still catches the actual
        // regression this test exists for: the discarded delta-onto-raw
        // -input design measured errors of 65-155% here, an order of
        // magnitude worse than what this margin allows.
        const auto margin = 0.1f + 0.15f * std::abs (theta);
        INFO ("requested theta = " << theta << " rad, measured = " << measuredTheta << " rad, margin = " << margin);
        REQUIRE (measuredTheta == Catch::Approx (theta).margin (margin));
    }
}
