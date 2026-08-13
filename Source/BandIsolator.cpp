#include "BandIsolator.h"

namespace
{
    constexpr float kDefaultLowHz = 20.0f;
    constexpr float kDefaultHighHz = 150.0f;
    constexpr float kMinGapHz = 5.0f; // keeps the two splits from crossing/colliding
    constexpr double kSmoothingTimeSeconds = 0.05;
}

void BandIsolator::prepare (const juce::dsp::ProcessSpec& spec)
{
    lowSplitFilter.prepare (spec);
    highSplitFilter.prepare (spec);

    smoothedLowHz.reset (spec.sampleRate, kSmoothingTimeSeconds);
    smoothedHighHz.reset (spec.sampleRate, kSmoothingTimeSeconds);
    smoothedLowHz.setCurrentAndTargetValue (kDefaultLowHz);
    smoothedHighHz.setCurrentAndTargetValue (kDefaultHighHz);

    lowSplitFilter.setCutoffFrequency (kDefaultLowHz);
    highSplitFilter.setCutoffFrequency (kDefaultHighHz);
}

void BandIsolator::reset()
{
    lowSplitFilter.reset();
    highSplitFilter.reset();
}

void BandIsolator::setTargetBand (float lowHz, float highHz) noexcept
{
    smoothedLowHz.setTargetValue (lowHz);
    smoothedHighHz.setTargetValue (juce::jmax (lowHz + kMinGapHz, highHz));
}

void BandIsolator::process (const juce::dsp::AudioBlock<const float>& input,
                             juce::dsp::AudioBlock<float>& targetBand,
                             juce::dsp::AudioBlock<float>& residual) noexcept
{
    jassert (input.getNumChannels() == targetBand.getNumChannels());
    jassert (input.getNumChannels() == residual.getNumChannels());
    jassert (input.getNumSamples()  == targetBand.getNumSamples());
    jassert (input.getNumSamples()  == residual.getNumSamples());

    const auto numChannels = input.getNumChannels();
    const auto numSamples  = input.getNumSamples();

    if (smoothedLowHz.isSmoothing() || smoothedHighHz.isSmoothing())
    {
        lowSplitFilter.setCutoffFrequency (smoothedLowHz.skip ((int) numSamples));
        highSplitFilter.setCutoffFrequency (juce::jmax (smoothedLowHz.getCurrentValue() + kMinGapHz,
                                                          smoothedHighHz.skip ((int) numSamples)));
    }

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        const auto* inSamples  = input.getChannelPointer (channel);
        auto* targetSamples    = targetBand.getChannelPointer (channel);
        auto* residualSamples  = residual.getChannelPointer (channel);

        for (size_t i = 0; i < numSamples; ++i)
        {
            float belowLow, aboveLow;
            lowSplitFilter.processSample ((int) channel, inSamples[i], belowLow, aboveLow);

            float target, aboveHigh;
            highSplitFilter.processSample ((int) channel, aboveLow, target, aboveHigh);

            targetSamples[i]   = target;
            residualSamples[i] = belowLow + aboveHigh;
        }
    }
}
