// Not a Catch2 test: a standalone load generator for real CPU profiling
// (Phase 5's "measured, not assumed" performance requirement). Runs the
// same DSP classes PluginProcessor::processBlock() calls, in the same
// order, on synthetic audio (periodic Kick-like transients on the
// sidechain) for a fixed wall-clock duration -- long enough to attach
// `sample <pid> <duration>` to it mid-run. Must be built Release: a Debug
// build's numbers would be meaningless noise (lesson documented in the
// sibling PhaseLockSub/SmartMask Network projects).
#include <juce_dsp/juce_dsp.h>
#include <chrono>
#include <cstdio>
#include <unistd.h>
#include "BandIsolator.h"
#include "PhaseCorrelationAnalyzer.h"
#include "PhaseRotator.h"

int main()
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr int numChannels = 2;
    constexpr double runDurationSeconds = 15.0;

    BandIsolator mainIsolator, sidechainIsolator;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, (juce::uint32) numChannels };
    mainIsolator.prepare (spec);
    sidechainIsolator.prepare (spec);
    mainIsolator.setTargetBand (20.0f, 150.0f);
    sidechainIsolator.setTargetBand (20.0f, 150.0f);

    PhaseCorrelationAnalyzer correlationAnalyzer;
    correlationAnalyzer.prepare (sampleRate);
    correlationAnalyzer.setTargetBand (20.0f, 150.0f);
    correlationAnalyzer.setTransientSensitivity (0.5f);

    PhaseRotator phaseRotator;
    phaseRotator.prepare (sampleRate, numChannels);
    phaseRotator.setTargetBand (20.0f, 150.0f);

    juce::AudioBuffer<float> mainBuffer (numChannels, blockSize);
    juce::AudioBuffer<float> sidechainBuffer (numChannels, blockSize);
    juce::AudioBuffer<float> mainTarget (numChannels, blockSize);
    juce::AudioBuffer<float> mainResidual (numChannels, blockSize);
    juce::AudioBuffer<float> sidechainTarget (numChannels, blockSize);
    juce::AudioBuffer<float> sidechainResidual (numChannels, blockSize);

    juce::Random rng (7);

    std::printf ("AcousticCloak load test starting (PID %d), running for %.0fs of real time...\n",
                 (int) getpid(), runDurationSeconds);
    std::fflush (stdout);

    const auto start = std::chrono::steady_clock::now();
    long long blocksProcessed = 0;

    while (true)
    {
        const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
        if (elapsed >= runDurationSeconds)
            break;

        const bool injectTransient = (blocksProcessed % 40) == 0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float noise = rng.nextFloat() * 0.05f - 0.025f;
                mainBuffer.setSample (ch, i, noise);
                sidechainBuffer.setSample (ch, i, injectTransient ? (noise + 0.6f) : noise);
            }
        }

        juce::dsp::AudioBlock<const float> mainBlockConst (mainBuffer);
        juce::dsp::AudioBlock<float> mainTargetBlock (mainTarget);
        juce::dsp::AudioBlock<float> mainResidualBlock (mainResidual);
        mainIsolator.process (mainBlockConst, mainTargetBlock, mainResidualBlock);

        juce::dsp::AudioBlock<const float> sideBlockConst (sidechainBuffer);
        juce::dsp::AudioBlock<float> sideTargetBlock (sidechainTarget);
        juce::dsp::AudioBlock<float> sideResidualBlock (sidechainResidual);
        sidechainIsolator.process (sideBlockConst, sideTargetBlock, sideResidualBlock);

        correlationAnalyzer.processBlock (sidechainBuffer, sidechainTarget, mainTarget, blockSize);

        const auto& result = correlationAnalyzer.getLatestResult();
        phaseRotator.setRotationRadians (result.isValid ? result.phaseDeltaRadians : 0.0f);
        phaseRotator.process (mainTarget, mainResidual, blockSize);

        ++blocksProcessed;
    }

    const auto totalElapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    const double simulatedAudioSeconds = (double) blocksProcessed * blockSize / sampleRate;
    const double realTimeRatio = simulatedAudioSeconds / totalElapsed;
    const double cpuPercentAtRealtime = 100.0 / realTimeRatio;

    std::printf ("Processed %lld blocks (%.1fs of simulated audio) in %.2fs wall-clock.\n",
                 blocksProcessed, simulatedAudioSeconds, totalElapsed);
    std::printf ("Real-time ratio: %.1fx  ->  estimated single-instance CPU usage at real-time playback: %.3f%%\n",
                 realTimeRatio, cpuPercentAtRealtime);

    return 0;
}
