#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "BandIsolator.h"
#include <cmath>
#include <vector>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    // Measures the steady-state RMS gain of (targetBand + residual) vs. the
    // raw input for a single sine tone -- the property BandIsolator must
    // guarantee regardless of where lowHz/highHz are set, per its own
    // flat-summing design goal.
    float measureRecombinedRmsGain (float toneHz, float lowHz, float highHz)
    {
        BandIsolator isolator;
        juce::dsp::ProcessSpec spec { kSampleRate, (juce::uint32) kBlockSize, 1 };
        isolator.prepare (spec);
        isolator.setTargetBand (lowHz, highHz);

        juce::AudioBuffer<float> input (1, kBlockSize);
        juce::AudioBuffer<float> targetBand (1, kBlockSize);
        juce::AudioBuffer<float> residual (1, kBlockSize);

        double sumSquaredIn = 0.0, sumSquaredOut = 0.0;
        int sampleIndex = 0;

        // Run for 2 seconds: long enough for the crossover filters' own
        // transient response (and the 50ms band-target smoothing ramp) to
        // settle before measurement, and to average over many cycles even
        // of the lowest tone tested.
        for (int block = 0; block < 188; ++block)
        {
            for (int n = 0; n < kBlockSize; ++n)
            {
                const auto t = (float) sampleIndex / (float) kSampleRate;
                input.setSample (0, n, std::sin (2.0f * juce::MathConstants<float>::pi * toneHz * t));
                ++sampleIndex;
            }

            juce::dsp::AudioBlock<const float> inBlock (input);
            juce::dsp::AudioBlock<float> targetBlock (targetBand);
            juce::dsp::AudioBlock<float> residualBlock (residual);
            isolator.process (inBlock, targetBlock, residualBlock);

            if (block >= 100) // only measure once fully settled
            {
                for (int n = 0; n < kBlockSize; ++n)
                {
                    const auto in = input.getSample (0, n);
                    const auto out = targetBand.getSample (0, n) + residual.getSample (0, n);
                    sumSquaredIn += (double) in * in;
                    sumSquaredOut += (double) out * out;
                }
            }
        }

        return (float) std::sqrt (sumSquaredOut / sumSquaredIn);
    }
}

TEST_CASE ("BandIsolator recombination is flat (unity gain) across a wide frequency sweep", "[BandIsolator]")
{
    const float lowHz = 20.0f;
    const float highHz = 150.0f;

    for (float toneHz : { 10.0f, 20.0f, 60.0f, 85.0f, 150.0f, 300.0f, 2000.0f, 8000.0f })
    {
        const auto gain = measureRecombinedRmsGain (toneHz, lowHz, highHz);
        INFO ("tone = " << toneHz << " Hz, measured recombined RMS gain = " << gain);
        REQUIRE (gain == Catch::Approx (1.0f).margin (0.02));
    }
}

TEST_CASE ("BandIsolator confines a target-band tone mostly to targetBand output", "[BandIsolator]")
{
    BandIsolator isolator;
    juce::dsp::ProcessSpec spec { kSampleRate, (juce::uint32) kBlockSize, 1 };
    isolator.prepare (spec);
    isolator.setTargetBand (20.0f, 150.0f);

    juce::AudioBuffer<float> input (1, kBlockSize);
    juce::AudioBuffer<float> targetBand (1, kBlockSize);
    juce::AudioBuffer<float> residual (1, kBlockSize);

    double sumSquaredTarget = 0.0, sumSquaredResidual = 0.0;
    int sampleIndex = 0;

    for (int block = 0; block < 188; ++block)
    {
        for (int n = 0; n < kBlockSize; ++n)
        {
            const auto t = (float) sampleIndex / (float) kSampleRate;
            input.setSample (0, n, std::sin (2.0f * juce::MathConstants<float>::pi * 85.0f * t)); // centre of the band
            ++sampleIndex;
        }

        juce::dsp::AudioBlock<const float> inBlock (input);
        juce::dsp::AudioBlock<float> targetBlock (targetBand);
        juce::dsp::AudioBlock<float> residualBlock (residual);
        isolator.process (inBlock, targetBlock, residualBlock);

        if (block >= 100)
        {
            for (int n = 0; n < kBlockSize; ++n)
            {
                sumSquaredTarget += (double) targetBand.getSample (0, n) * targetBand.getSample (0, n);
                sumSquaredResidual += (double) residual.getSample (0, n) * residual.getSample (0, n);
            }
        }
    }

    REQUIRE (sumSquaredTarget > sumSquaredResidual * 10.0); // target band should dominate heavily
}

TEST_CASE ("BandIsolator stays flat close to its own crossover points, not just far from them", "[BandIsolator]")
{
    // Chaining two crossover splits is only EXACTLY flat-summing per split
    // (low+high of a single filter always sums to that filter's own
    // allpass response); a second, chained filter's own phase shift is not
    // generally the identity, so summing three chained outputs can show
    // measurable interference near a crossover point -- a real effect
    // found by testing a since-abandoned Phase 3 design (a further 3-way
    // split INSIDE the target band, whose two internal crossover points
    // were only ~2x apart and interfered badly, ~20% at one tone). Here,
    // lowHz/highHz are ~7.5x apart (the default 20/150Hz), which this test
    // confirms keeps that same effect small (<3%) even right at the edges
    // -- the margin this project relies on when it says BandIsolator's
    // recombination is "flat".
    for (float toneHz : { 15.0f, 18.0f, 20.0f, 22.0f, 25.0f, 30.0f, 140.0f, 145.0f, 150.0f, 155.0f, 160.0f })
    {
        const auto gain = measureRecombinedRmsGain (toneHz, 20.0f, 150.0f);
        INFO ("tone = " << toneHz << " Hz, measured recombined RMS gain = " << gain);
        REQUIRE (gain == Catch::Approx (1.0f).margin (0.03));
    }
}
