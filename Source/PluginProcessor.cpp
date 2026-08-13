#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float kMaxLookaheadMs = 20.0f; // matches the blueprint's own 0-20ms Lookahead range
    constexpr float kDelayLineSafetyMarginMs = 2.0f;
}

//==============================================================================
AcousticCloakAudioProcessor::AcousticCloakAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                           .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false))
    , apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
    , lookaheadDelay (1)
{
    cloakDepthParam           = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (AcousticCloakParam::cloakDepth));
    lookaheadMsParam          = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (AcousticCloakParam::lookaheadMs));
    targetFreqLowParam        = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (AcousticCloakParam::targetFreqLow));
    targetFreqHighParam       = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (AcousticCloakParam::targetFreqHigh));
    transientSensitivityParam = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (AcousticCloakParam::transientSensitivity));
    bypassParam               = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter (AcousticCloakParam::bypass));

    jassert (cloakDepthParam           != nullptr);
    jassert (lookaheadMsParam          != nullptr);
    jassert (targetFreqLowParam        != nullptr);
    jassert (targetFreqHighParam       != nullptr);
    jassert (transientSensitivityParam != nullptr);
    jassert (bypassParam               != nullptr);
}

AcousticCloakAudioProcessor::~AcousticCloakAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AcousticCloakAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { AcousticCloakParam::cloakDepth, 1 },
        "Cloak Depth",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { AcousticCloakParam::lookaheadMs, 1 },
        "Lookahead",
        juce::NormalisableRange<float> (0.0f, kMaxLookaheadMs, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { AcousticCloakParam::targetFreqLow, 1 },
        "Target Freq Low",
        juce::NormalisableRange<float> (20.0f, 200.0f, 0.1f, 0.4f),
        20.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { AcousticCloakParam::targetFreqHigh, 1 },
        "Target Freq High",
        juce::NormalisableRange<float> (30.0f, 300.0f, 0.1f, 0.4f),
        150.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { AcousticCloakParam::transientSensitivity, 1 },
        "Transient Sensitivity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { AcousticCloakParam::bypass, 1 },
        "Bypass",
        false));

    return { params.begin(), params.end() };
}

//==============================================================================
void AcousticCloakAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numMainChannels = static_cast<juce::uint32> (getMainBusNumOutputChannels());

    lookaheadMaxSamples = (int) std::ceil ((kMaxLookaheadMs + kDelayLineSafetyMarginMs) * 0.001 * sampleRate);

    lookaheadDelay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> (juce::jmax (1, lookaheadMaxSamples));

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = numMainChannels;

    lookaheadDelay.prepare (spec);

    dryBuffer.setSize (static_cast<int> (numMainChannels), samplesPerBlock);

    bypassMix.reset (sampleRate, 0.02); // 20ms crossfade, avoids a click on Bypass toggle (see sibling PhaseLockSub project)
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    mainBandIsolator.prepare (spec);
    mainBandIsolator.setTargetBand (targetFreqLowParam->get(), targetFreqHighParam->get());
    mainTargetBandBuffer.setSize (static_cast<int> (numMainChannels), samplesPerBlock);
    mainResidualBuffer.setSize   (static_cast<int> (numMainChannels), samplesPerBlock);

    // Sidechain channel count may differ from Main's (e.g. mono Kick into a
    // stereo Bass) -- size for whatever the host actually configured, with
    // a floor of 1 so BandIsolator always has at least one channel to
    // prepare even before a host has enabled/connected the bus.
    auto* sidechainBus = getBus (true, 1);
    const auto numSidechainChannels = juce::jmax (1, sidechainBus != nullptr ? sidechainBus->getNumberOfChannels() : 1);

    juce::dsp::ProcessSpec sidechainSpec = spec;
    sidechainSpec.numChannels = static_cast<juce::uint32> (numSidechainChannels);

    sidechainBandIsolator.prepare (sidechainSpec);
    sidechainBandIsolator.setTargetBand (targetFreqLowParam->get(), targetFreqHighParam->get());
    sidechainTargetBandBuffer.setSize      (numSidechainChannels, samplesPerBlock);
    sidechainResidualScratchBuffer.setSize (numSidechainChannels, samplesPerBlock);

    correlationAnalyzer.prepare (sampleRate);
    correlationAnalyzer.setTargetBand (targetFreqLowParam->get(), targetFreqHighParam->get());
    correlationAnalyzer.setTransientSensitivity (transientSensitivityParam->get() / 100.0f);

    phaseRotator.prepare (sampleRate, static_cast<int> (numMainChannels));
    phaseRotator.setTargetBand (targetFreqLowParam->get(), targetFreqHighParam->get());
    phaseRotator.setRotationRadians (0.0f);

    // prepareToPlay() runs on the message thread, never the audio callback,
    // so calling setLatencySamples() directly here is safe -- unlike a live
    // mid-playback Lookahead change, which is deferred via AsyncUpdater in
    // processBlock()/handleAsyncUpdate() below (setLatencySamples() calls
    // updateHostDisplay() synchronously, which most hosts don't tolerate
    // from the audio thread -- same reasoning as the sibling PhaseLockSub
    // project's own Lookahead handling).
    lastReportedLatency = (int) std::round (lookaheadMsParam->get() * 0.001 * sampleRate);
    lookaheadDelay.setDelay ((float) lastReportedLatency);
    setLatencySamples (lastReportedLatency);
}

void AcousticCloakAudioProcessor::releaseResources()
{
    lookaheadDelay.reset();
    mainBandIsolator.reset();
    sidechainBandIsolator.reset();
    correlationAnalyzer.reset();
    phaseRotator.reset();
}

void AcousticCloakAudioProcessor::handleAsyncUpdate()
{
    setLatencySamples (pendingLatencySamples.load (std::memory_order_relaxed));
}

bool AcousticCloakAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::stereo() && mainOut != juce::AudioChannelSet::mono())
        return false;

    if (layouts.getMainInputChannelSet() != mainOut)
        return false;

    // Sidechain (input bus index 1) is optional: a host/DAW may leave it
    // disabled entirely (fallback dry mode), or feed it mono or stereo.
    if (layouts.inputBuses.size() > 1)
    {
        const auto sidechain = layouts.getChannelSet (true, 1);

        if (! sidechain.isDisabled()
            && sidechain != juce::AudioChannelSet::mono()
            && sidechain != juce::AudioChannelSet::stereo())
        {
            return false;
        }
    }

    return true;
}

void AcousticCloakAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    auto* sidechainBus = getBus (true, 1);
    const bool sidechainHasSignal = sidechainBus != nullptr
                                     && sidechainBus->isEnabled()
                                     && sidechainBus->getNumberOfChannels() > 0;
    sidechainConnected.store (sidechainHasSignal, std::memory_order_relaxed);

    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    auto mainBuffer        = getBusBuffer (buffer, false, 0);
    const auto numSamples  = mainBuffer.getNumSamples();
    const auto numChannels = mainBuffer.getNumChannels();

    // Lookahead: reserves a fixed pre-delay budget on the Main path so
    // Phase 3's rotator will have temporal margin to react to the Sidechain
    // -- the audio-thread delay target updates immediately (it's just a
    // ramp target), but the HOST-facing latency report is deferred to the
    // message thread, exactly like the sibling PhaseLockSub project's own
    // Lookahead toggle.
    const auto requestedLatency = (int) std::round (lookaheadMsParam->get() * 0.001 * getSampleRate());
    if (requestedLatency != lastReportedLatency)
    {
        lastReportedLatency = requestedLatency;
        pendingLatencySamples.store (requestedLatency, std::memory_order_relaxed);
        triggerAsyncUpdate();
    }
    lookaheadDelay.setDelay ((float) juce::jlimit (0, lookaheadMaxSamples, requestedLatency));

    juce::dsp::AudioBlock<float> mainBlock (mainBuffer);
    juce::dsp::ProcessContextReplacing<float> lookaheadContext (mainBlock);
    lookaheadDelay.process (lookaheadContext);

    // Dry copy taken AFTER the lookahead delay: the reported host latency
    // must stay constant regardless of Bypass, so Bypass only ever mutes
    // Phase 3's (not-yet-implemented) correction, never the Lookahead
    // pre-delay itself.
    for (int channel = 0; channel < numChannels; ++channel)
        dryBuffer.copyFrom (channel, 0, mainBuffer, channel, 0, numSamples);

    mainBandIsolator.setTargetBand (targetFreqLowParam->get(), targetFreqHighParam->get());
    correlationAnalyzer.setTargetBand (targetFreqLowParam->get(), targetFreqHighParam->get());
    correlationAnalyzer.setTransientSensitivity (transientSensitivityParam->get() / 100.0f);

    // Isolate the target band on the Main signal via a flat-summing
    // crossover (BandIsolator) -- mainTargetBandBuffer is what Phase 3 will
    // rotate; mainResidualBuffer is everything outside the user's Target
    // Frequency Range, untouched. Recombining the two unmodified below is
    // this phase's equivalent of the sibling PhaseLockSub project's Phase 2
    // "split and immediately recombine" test summation.
    juce::dsp::AudioBlock<const float> mainBlockConst (mainBuffer);
    juce::dsp::AudioBlock<float> mainTargetBlock (mainTargetBandBuffer);
    juce::dsp::AudioBlock<float> mainResidualBlock (mainResidualBuffer);
    auto mainTargetSub   = mainTargetBlock.getSubBlock (0, (size_t) numSamples);
    auto mainResidualSub = mainResidualBlock.getSubBlock (0, (size_t) numSamples);
    mainBandIsolator.process (mainBlockConst, mainTargetSub, mainResidualSub);

    if (sidechainHasSignal)
    {
        auto sidechainBuffer = getBusBuffer (buffer, true, 1);

        sidechainBandIsolator.setTargetBand (targetFreqLowParam->get(), targetFreqHighParam->get());

        juce::dsp::AudioBlock<const float> sidechainBlockConst (sidechainBuffer);
        juce::dsp::AudioBlock<float> sidechainTargetBlock (sidechainTargetBandBuffer);
        juce::dsp::AudioBlock<float> sidechainResidualBlock (sidechainResidualScratchBuffer);
        auto sidechainTargetSub   = sidechainTargetBlock.getSubBlock (0, (size_t) numSamples);
        auto sidechainResidualSub = sidechainResidualBlock.getSubBlock (0, (size_t) numSamples);
        sidechainBandIsolator.process (sidechainBlockConst, sidechainTargetSub, sidechainResidualSub);

        correlationAnalyzer.processBlock (sidechainBuffer, sidechainTargetBandBuffer, mainTargetBandBuffer, numSamples);

        const auto& result = correlationAnalyzer.getLatestResult();
        displayCorrelationValid.store (result.isValid, std::memory_order_relaxed);
        if (result.isValid)
        {
            displayCorrelation.store (result.correlation, std::memory_order_relaxed);
            displayPhaseDelta.store (result.phaseDeltaRadians, std::memory_order_relaxed);
        }
    }
    else
    {
        displayCorrelationValid.store (false, std::memory_order_relaxed);
    }

    // Phase 3: rotate mainTargetBandBuffer by the measured phase delta,
    // scaled by Cloak Depth. thetaRadians is the angle that would make
    // Main's phase equal Sidechain's (result.phaseDeltaRadians is defined
    // as Sidechain-minus-Main, so adding it to Main's own phase cancels the
    // difference) -- 0 when there's no valid measurement (Sidechain silent
    // or disconnected), which PhaseRotator's own smoothing decays into
    // gracefully rather than snapping.
    float thetaRadians = 0.0f;
    if (sidechainHasSignal && correlationAnalyzer.getLatestResult().isValid)
        thetaRadians = correlationAnalyzer.getLatestResult().phaseDeltaRadians * (cloakDepthParam->get() / 100.0f);

    // PhaseRotator warps BOTH buffers (targetBand by the real rotation,
    // residual by an identical "dummy" copy of the same allpass cascade) --
    // see PhaseRotator.h's own doc comment for why that keeps the
    // recombination below flat regardless of thetaRadians, not just at 0.
    phaseRotator.setTargetBand (targetFreqLowParam->get(), targetFreqHighParam->get());
    phaseRotator.setRotationRadians (thetaRadians);
    phaseRotator.process (mainTargetBandBuffer, mainResidualBuffer, numSamples);

    // Recombine the (now rotated+warped) target band with the
    // identically-warped residual.
    for (int channel = 0; channel < numChannels; ++channel)
    {
        mainBuffer.copyFrom (channel, 0, mainTargetBandBuffer, channel, 0, numSamples);
        mainBuffer.addFrom  (channel, 0, mainResidualBuffer,  channel, 0, numSamples);
    }
    for (int n = 0; n < numSamples; ++n)
    {
        const auto mix = bypassMix.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* channelData = mainBuffer.getWritePointer (channel);
            channelData[n] = channelData[n] * (1.0f - mix) + dryBuffer.getSample (channel, n) * mix;
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* AcousticCloakAudioProcessor::createEditor()
{
    return new AcousticCloakAudioProcessorEditor (*this);
}

bool AcousticCloakAudioProcessor::hasEditor() const
{
    return true;
}

//==============================================================================
const juce::String AcousticCloakAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AcousticCloakAudioProcessor::acceptsMidi() const   { return false; }
bool AcousticCloakAudioProcessor::producesMidi() const  { return false; }
bool AcousticCloakAudioProcessor::isMidiEffect() const  { return false; }
double AcousticCloakAudioProcessor::getTailLengthSeconds() const { return 0.0; }

//==============================================================================
int AcousticCloakAudioProcessor::getNumPrograms()                        { return 1; }
int AcousticCloakAudioProcessor::getCurrentProgram()                     { return 0; }
void AcousticCloakAudioProcessor::setCurrentProgram (int)                {}
const juce::String AcousticCloakAudioProcessor::getProgramName (int)     { return {}; }
void AcousticCloakAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void AcousticCloakAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AcousticCloakAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AcousticCloakAudioProcessor();
}
