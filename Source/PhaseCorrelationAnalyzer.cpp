#include "PhaseCorrelationAnalyzer.h"

namespace
{
    constexpr float kMinMagnitude = 1.0e-8f;
    constexpr float kAbsoluteFloor = 1.0e-6f;
}

void PhaseCorrelationAnalyzer::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;

    mainBank.prepare (sampleRate);
    sidechainBank.prepare (sampleRate);

    smoothedLowHz.reset (sampleRate, 0.05);
    smoothedHighHz.reset (sampleRate, 0.05);

    const auto fs = (float) sampleRate;
    fastCoeff = std::exp (-1.0f / (0.002f * fs));
    slowCoeff = std::exp (-1.0f / (0.150f * fs));
    attackCoeff  = std::exp (-1.0f / (0.005f * fs));
    releaseCoeff = std::exp (-1.0f / (0.300f * fs));

    reset();
}

void PhaseCorrelationAnalyzer::reset() noexcept
{
    mainBank.reset();
    sidechainBank.reset();

    fastEnvelope = 0.0f;
    slowFloor = 0.0f;

    smoothedCos = 1.0f;
    smoothedSin = 0.0f;

    everActive = false;

    latestResult = Result{};
}

void PhaseCorrelationAnalyzer::setTargetBand (float lowHz, float highHz) noexcept
{
    smoothedLowHz.setTargetValue (lowHz);
    smoothedHighHz.setTargetValue (highHz);
}

void PhaseCorrelationAnalyzer::setTransientSensitivity (float sensitivity) noexcept
{
    const auto s = juce::jlimit (0.0f, 1.0f, sensitivity);
    sensitivityRatio = juce::jmap (s, 0.0f, 1.0f, 4.0f, 1.1f);
}

void PhaseCorrelationAnalyzer::processBlock (const juce::AudioBuffer<float>& sidechainRawBuffer,
                                              const juce::AudioBuffer<float>& sidechainBandBuffer,
                                              const juce::AudioBuffer<float>& mainBandBuffer,
                                              int numSamples) noexcept
{
    smoothedLowHz.skip (numSamples);
    smoothedHighHz.skip (numSamples);
    mainBank.setTargetBand (smoothedLowHz.getCurrentValue(), smoothedHighHz.getCurrentValue());
    sidechainBank.setTargetBand (smoothedLowHz.getCurrentValue(), smoothedHighHz.getCurrentValue());

    const auto rawChannels  = sidechainRawBuffer.getNumChannels();
    const auto sideChannels = sidechainBandBuffer.getNumChannels();
    const auto mainChannels = mainBandBuffer.getNumChannels();

    for (int n = 0; n < numSamples; ++n)
    {
        float rawMono = 0.0f;
        for (int ch = 0; ch < rawChannels; ++ch)
            rawMono += sidechainRawBuffer.getSample (ch, n);
        if (rawChannels > 0)
            rawMono /= (float) rawChannels;

        // Broadband (non-resonant) activity envelope -- see class doc for
        // why this must not come from the band-limited analysis signal.
        const auto rawAbs = std::abs (rawMono);
        fastEnvelope = rawAbs > fastEnvelope
                            ? fastCoeff * fastEnvelope + (1.0f - fastCoeff) * rawAbs
                            : slowCoeff * fastEnvelope + (1.0f - slowCoeff) * rawAbs;
        slowFloor = slowCoeff * slowFloor + (1.0f - slowCoeff) * rawAbs;

        const auto threshold = juce::jmax (kAbsoluteFloor, slowFloor * sensitivityRatio);
        const bool active = fastEnvelope > threshold;
        everActive = everActive || active;

        float sideMono = 0.0f;
        for (int ch = 0; ch < sideChannels; ++ch)
            sideMono += sidechainBandBuffer.getSample (ch, n);
        if (sideChannels > 0)
            sideMono /= (float) sideChannels;

        float mainMono = 0.0f;
        for (int ch = 0; ch < mainChannels; ++ch)
            mainMono += mainBandBuffer.getSample (ch, n);
        if (mainChannels > 0)
            mainMono /= (float) mainChannels;

        const auto side = sidechainBank.process (sideMono);
        const auto main = mainBank.process (mainMono);

        const auto zReal = side.real * main.real + side.imag * main.imag;
        const auto zImag = side.imag * main.real - side.real * main.imag;
        const auto zMagnitude = std::sqrt (zReal * zReal + zImag * zImag);

        if (zMagnitude > kMinMagnitude)
        {
            const auto instCos = zReal / zMagnitude;
            const auto instSin = zImag / zMagnitude;

            const auto blend = active ? attackCoeff : releaseCoeff;
            smoothedCos = blend * smoothedCos + (1.0f - blend) * instCos;
            smoothedSin = blend * smoothedSin + (1.0f - blend) * instSin;

            const auto norm = std::sqrt (smoothedCos * smoothedCos + smoothedSin * smoothedSin);
            if (norm > kMinMagnitude)
            {
                smoothedCos /= norm;
                smoothedSin /= norm;
            }
        }
    }

    latestResult.isValid = everActive;
    latestResult.phaseDeltaRadians = std::atan2 (smoothedSin, smoothedCos);
    latestResult.correlation = smoothedCos;
}
