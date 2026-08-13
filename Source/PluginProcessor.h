#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BandIsolator.h"
#include "PhaseCorrelationAnalyzer.h"
#include "PhaseRotator.h"

namespace AcousticCloakParam
{
    constexpr auto cloakDepth           = "cloakDepth";
    constexpr auto lookaheadMs          = "lookaheadMs";
    constexpr auto targetFreqLow        = "targetFreqLow";
    constexpr auto targetFreqHigh       = "targetFreqHigh";
    constexpr auto transientSensitivity = "transientSensitivity";
    constexpr auto bypass               = "bypass";
}

/**
    Phase 1: I/O bus configuration (Main In/Out stereo + optional Sidechain),
    APVTS parameter set, and a dry-passthrough placeholder editor.
    Phase 2: PhaseCorrelationAnalyzer watches the Sidechain (Kick) and Main
    (Bass) signals within the Target Frequency Range and continuously tracks
    their phase difference and correlation -- analysis only, still no audio
    modification.
    Phase 2 revision (v0.2.1, per design review): a BandIsolator (3-way
    Linkwitz-Riley crossover, flat-summing guaranteed) now isolates the
    Target Frequency Range on both Main and Sidechain BEFORE analysis, so
    the resonator-based phase estimate never touches content outside the
    user's chosen band -- and mainResidualBuffer is already sitting ready
    for Phase 3's recombination.
    Phase 3: PhaseRotator applies that measured phase difference (scaled by
    Cloak Depth) as an actual rotation of mainTargetBandBuffer -- via a
    SINGLE resonator per channel, not ResonatorBank's analysis-only sum (a
    second design review found that summing resonators for reconstruction,
    and a since-abandoned nested-crossover alternative, both risked
    colouring the band; see PhaseRotator.h for the full history) -- then
    recombines it with mainResidualBuffer.
*/
class AcousticCloakAudioProcessor final : public juce::AudioProcessor,
                                           private juce::AsyncUpdater
{
public:
    AcousticCloakAudioProcessor();
    ~AcousticCloakAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** True when the host has an enabled Sidechain bus actually carrying
        channels. Message-thread-safe read; written from the audio thread
        every block. */
    bool isSidechainConnected() const noexcept { return sidechainConnected.load (std::memory_order_relaxed); }

    /** Message-thread-safe published copy of the latest phase-correlation
        analysis, for the GUI's radar/correlation meter. */
    float getDisplayCorrelation() const noexcept { return displayCorrelation.load (std::memory_order_relaxed); }
    float getDisplayPhaseDeltaRadians() const noexcept { return displayPhaseDelta.load (std::memory_order_relaxed); }
    bool isDisplayCorrelationValid() const noexcept { return displayCorrelationValid.load (std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState apvts;

    static constexpr const char* kVersionString = "1.0.0";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void handleAsyncUpdate() override;

    std::atomic<bool> sidechainConnected { false };

    juce::AudioParameterFloat* cloakDepthParam           = nullptr;
    juce::AudioParameterFloat* lookaheadMsParam          = nullptr;
    juce::AudioParameterFloat* targetFreqLowParam        = nullptr;
    juce::AudioParameterFloat* targetFreqHighParam       = nullptr;
    juce::AudioParameterFloat* transientSensitivityParam = nullptr;
    juce::AudioParameterBool*  bypassParam               = nullptr;

    BandIsolator mainBandIsolator, sidechainBandIsolator;
    juce::AudioBuffer<float> mainTargetBandBuffer, mainResidualBuffer;
    juce::AudioBuffer<float> sidechainTargetBandBuffer, sidechainResidualScratchBuffer;

    PhaseCorrelationAnalyzer correlationAnalyzer;
    PhaseRotator phaseRotator;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> lookaheadDelay;
    int lookaheadMaxSamples = 0;
    int lastReportedLatency = 0;
    std::atomic<int> pendingLatencySamples { 0 };

    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float> bypassMix; // 0 = fully processed, 1 = fully dry

    std::atomic<float> displayCorrelation { 0.0f };
    std::atomic<float> displayPhaseDelta { 0.0f };
    std::atomic<bool> displayCorrelationValid { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcousticCloakAudioProcessor)
};
