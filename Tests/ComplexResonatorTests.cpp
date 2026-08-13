#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "ComplexResonator.h"
#include <cmath>

namespace
{
    constexpr double kSampleRate = 48000.0;

    float sineSample (int n, float freqHz, float phase = 0.0f)
    {
        return std::sin (2.0f * juce::MathConstants<float>::pi * freqHz * (float) n / (float) kSampleRate + phase);
    }
}

TEST_CASE ("ComplexResonator settles to ~half-amplitude magnitude at its centre frequency", "[ComplexResonator]")
{
    // A real sine of amplitude 1 splits into two equal-amplitude (0.5)
    // phasors at +freq and -freq. A resonator with unity gain AT the
    // matched (+freq) component therefore settles to ~0.5 magnitude for a
    // real input, not ~1.0 -- the other 0.5 phasor sits at -freq, which
    // this single-pole design only partially rejects at very low centre
    // frequencies (relative to the sample rate), causing the small
    // oscillation this margin allows for.
    ComplexResonator resonator;
    resonator.prepare (kSampleRate);
    resonator.setTarget (60.0f, 40.0f); // 40-100Hz-ish band centred at 60Hz

    ComplexResonator::Sample last {};
    // Run long enough for the resonator's transient response to settle.
    for (int n = 0; n < 20000; ++n)
        last = resonator.process (sineSample (n, 60.0f));

    const auto magnitude = std::sqrt (last.real * last.real + last.imag * last.imag);
    REQUIRE (magnitude == Catch::Approx (0.5f).margin (0.15));
}

TEST_CASE ("ComplexResonator attenuates content far outside its band", "[ComplexResonator]")
{
    ComplexResonator resonator;
    resonator.prepare (kSampleRate);
    resonator.setTarget (60.0f, 20.0f); // narrow band around 60Hz

    ComplexResonator::Sample last {};
    for (int n = 0; n < 20000; ++n)
        last = resonator.process (sineSample (n, 2000.0f)); // way outside the band

    const auto magnitude = std::sqrt (last.real * last.real + last.imag * last.imag);
    REQUIRE (magnitude < 0.1f);
}

TEST_CASE ("ComplexResonator produces a stable, non-exploding output for silence", "[ComplexResonator]")
{
    ComplexResonator resonator;
    resonator.prepare (kSampleRate);
    resonator.setTarget (80.0f, 60.0f);

    ComplexResonator::Sample last {};
    for (int n = 0; n < 1000; ++n)
        last = resonator.process (0.0f);

    REQUIRE (last.real == Catch::Approx (0.0f));
    REQUIRE (last.imag == Catch::Approx (0.0f));
}
