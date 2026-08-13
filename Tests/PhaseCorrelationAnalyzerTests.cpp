#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PhaseCorrelationAnalyzer.h"
#include <cmath>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    void runTones (PhaseCorrelationAnalyzer& analyzer, float freqHz, float mainPhase, float sidechainPhase, int numBlocks)
    {
        juce::AudioBuffer<float> mainBuffer (1, kBlockSize);
        juce::AudioBuffer<float> sidechainBuffer (1, kBlockSize);

        int sampleIndex = 0;
        for (int block = 0; block < numBlocks; ++block)
        {
            for (int n = 0; n < kBlockSize; ++n)
            {
                const auto t = (float) sampleIndex / (float) kSampleRate;
                mainBuffer.setSample (0, n, std::sin (2.0f * juce::MathConstants<float>::pi * freqHz * t + mainPhase));
                sidechainBuffer.setSample (0, n, std::sin (2.0f * juce::MathConstants<float>::pi * freqHz * t + sidechainPhase));
                ++sampleIndex;
            }

            // Both signals are pure in-band tones, so the raw and
            // already-band-isolated versions are effectively identical --
            // a real BandIsolator would pass them through with ~0dB
            // ripple at these frequencies (see BandIsolatorTests.cpp),
            // letting this test exercise the analyzer alone.
            analyzer.processBlock (sidechainBuffer, sidechainBuffer, mainBuffer, kBlockSize);
        }
    }
}

TEST_CASE ("PhaseCorrelationAnalyzer converges to correlation ~1 for in-phase tones", "[PhaseCorrelationAnalyzer]")
{
    PhaseCorrelationAnalyzer analyzer;
    analyzer.prepare (kSampleRate);
    analyzer.setTargetBand (40.0f, 100.0f);
    analyzer.setTransientSensitivity (0.5f);

    runTones (analyzer, 60.0f, 0.0f, 0.0f, 300); // ~3.2s, well past the 300ms release time constant

    const auto& result = analyzer.getLatestResult();
    REQUIRE (result.isValid);
    REQUIRE (result.correlation == Catch::Approx (1.0f).margin (0.05));
}

TEST_CASE ("PhaseCorrelationAnalyzer converges to correlation ~-1 for anti-phase tones", "[PhaseCorrelationAnalyzer]")
{
    PhaseCorrelationAnalyzer analyzer;
    analyzer.prepare (kSampleRate);
    analyzer.setTargetBand (40.0f, 100.0f);
    analyzer.setTransientSensitivity (0.5f);

    runTones (analyzer, 60.0f, 0.0f, juce::MathConstants<float>::pi, 300);

    const auto& result = analyzer.getLatestResult();
    REQUIRE (result.isValid);
    REQUIRE (result.correlation == Catch::Approx (-1.0f).margin (0.05));
}

TEST_CASE ("PhaseCorrelationAnalyzer measures a quarter-cycle phase offset", "[PhaseCorrelationAnalyzer]")
{
    PhaseCorrelationAnalyzer analyzer;
    analyzer.prepare (kSampleRate);
    analyzer.setTargetBand (40.0f, 100.0f);
    analyzer.setTransientSensitivity (0.5f);

    const auto offset = juce::MathConstants<float>::halfPi;
    runTones (analyzer, 60.0f, 0.0f, offset, 300);

    const auto& result = analyzer.getLatestResult();
    REQUIRE (result.isValid);
    REQUIRE (std::abs (result.phaseDeltaRadians - offset) < 0.1f);
}

TEST_CASE ("PhaseCorrelationAnalyzer stays invalid on pure silence", "[PhaseCorrelationAnalyzer]")
{
    PhaseCorrelationAnalyzer analyzer;
    analyzer.prepare (kSampleRate);
    analyzer.setTargetBand (40.0f, 100.0f);
    analyzer.setTransientSensitivity (0.5f);

    juce::AudioBuffer<float> silence (1, kBlockSize);
    silence.clear();

    for (int block = 0; block < 50; ++block)
        analyzer.processBlock (silence, silence, silence, kBlockSize);

    REQUIRE (! analyzer.getLatestResult().isValid);
}
