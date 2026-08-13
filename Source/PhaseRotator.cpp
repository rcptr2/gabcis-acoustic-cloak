#include "PhaseRotator.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace
{
    constexpr double kSmoothingTimeSeconds = 0.01; // 10ms ramp, avoids a click when the measured phase delta changes
    constexpr float kMinMagnitude = 1.0e-8f;
}

void PhaseRotator::prepare (double sampleRate, int numChannels) noexcept
{
    numActiveChannels = juce::jlimit (0, kMaxChannels, numChannels);
    for (auto& n : networks)
        n.prepare (sampleRate);
    for (auto& d : dummies)
        d.prepare (sampleRate);

    smoothedCosTheta.reset (sampleRate, kSmoothingTimeSeconds);
    smoothedSinTheta.reset (sampleRate, kSmoothingTimeSeconds);
    smoothedCosTheta.setCurrentAndTargetValue (1.0f);
    smoothedSinTheta.setCurrentAndTargetValue (0.0f);

    reset();
}

void PhaseRotator::reset() noexcept
{
    for (auto& n : networks)
        n.reset();
    for (auto& d : dummies)
        d.reset();
}

void PhaseRotator::setTargetBand (float lowHz, float highHz) noexcept
{
    for (auto& n : networks)
        n.setTargetBand (lowHz, highHz);
    for (auto& d : dummies)
        d.setTargetBand (lowHz, highHz); // MUST match networks' own lowHz/highHz -- see DummyAllpass's own doc note
}

void PhaseRotator::setRotationRadians (float thetaRadians) noexcept
{
    smoothedCosTheta.setTargetValue (std::cos (thetaRadians));
    smoothedSinTheta.setTargetValue (std::sin (thetaRadians));
}

void PhaseRotator::process (juce::AudioBuffer<float>& targetBandBuffer,
                             juce::AudioBuffer<float>& residualBuffer,
                             int numSamples) noexcept
{
    const auto numChannels = juce::jmin (numActiveChannels,
                                          juce::jmin (targetBandBuffer.getNumChannels(), residualBuffer.getNumChannels()));

    for (int n = 0; n < numSamples; ++n)
    {
        auto cosTheta = smoothedCosTheta.getNextValue();
        auto sinTheta = smoothedSinTheta.getNextValue();

        // Linearly-interpolated cos/sin don't stay on the unit circle
        // mid-ramp (a chord, not an arc) -- renormalise so the applied
        // rotation is always a genuine rotation, not a slight
        // amplitude-modulating one.
        const auto norm = std::sqrt (cosTheta * cosTheta + sinTheta * sinTheta);
        if (norm > kMinMagnitude)
        {
            cosTheta /= norm;
            sinTheta /= norm;
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* lowData  = targetBandBuffer.getWritePointer (channel);
            auto* highData = residualBuffer.getWritePointer (channel);

            const auto analytic = networks[(size_t) channel].process (lowData[n]);
            const auto ihigh = dummies[(size_t) channel].process (highData[n]);

            // Rlow = Ilow*cos(theta) - Qlow*sin(theta): a genuine rotation
            // of the Low band's OWN analytic representation -- unlike the
            // discarded delta-onto-raw-input approach, this never needs
            // Ilow to resemble the raw input, so branchA's own (large,
            // frequency-dependent) phase relative to the input no longer
            // corrupts the result. At theta=0, Rlow == Ilow exactly.
            const auto rLow = analytic.branchA * cosTheta - analytic.branchB * sinTheta;

            lowData[n] = rLow;
            highData[n] = ihigh;
        }
    }
}
