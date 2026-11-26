#include <dsp/SensitivityCurve.h>
#include <dsp/AdaptiveTransientShaper.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace
{
    constexpr double testSampleRate = 48000.0;

    float weightAt (const sensitivity::Parameters& parameters, float frequencyHz)
    {
        return sensitivity::weightAtFrequency (parameters, testSampleRate, frequencyHz);
    }

    float sineRmsThroughBandpass (float sineHz, int numSamples)
    {
        const auto bandpass = sensitivity::makeBandpass (1000.0f, 1.0f, testSampleRate);

        float z1 = 0.0f;
        float z2 = 0.0f;
        double sumSquares = 0.0;
        int count = 0;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto x = std::sin (juce::MathConstants<float>::twoPi * sineHz * (float) i / (float) testSampleRate);
            const auto y = bandpass.processSample (x, z1, z2);

            // Skip the filter settling transient.
            if (i >= numSamples / 2)
            {
                sumSquares += (double) y * (double) y;
                ++count;
            }
        }

        return count > 0 ? (float) std::sqrt (sumSquares / (double) count) : 0.0f;
    }
}

TEST_CASE ("Default curve is perfectly flat", "[sensitivity]")
{
    sensitivity::Parameters parameters;
    std::array<float, AdaptiveTransientShaper::maxNumBins> weights {};

    sensitivity::computeWeights (parameters, testSampleRate, (int) weights.size(), weights.data());

    for (const auto weight : weights)
        REQUIRE (weight == Catch::Approx (1.0f).margin (1.0e-6f));
}

TEST_CASE ("Disabled band does not affect the curve", "[sensitivity]")
{
    sensitivity::Parameters parameters;
    parameters.bands[0].frequencyHz = 1000.0f;
    parameters.bands[0].gainDb = -12.0f;
    parameters.bands[0].enabled = false;

    REQUIRE (weightAt (parameters, 1000.0f) == Catch::Approx (1.0f).margin (1.0e-6f));
}

TEST_CASE ("A -12 dB band reduces weight at its centre only", "[sensitivity]")
{
    sensitivity::Parameters parameters;
    parameters.bands[0].frequencyHz = 1000.0f;
    parameters.bands[0].gainDb = -12.0f;
    parameters.bands[0].q = 1.0f;

    const auto centre = weightAt (parameters, 1000.0f);
    const auto far = weightAt (parameters, 20.0f);

    REQUIRE (centre == Catch::Approx (0.251f).margin (0.05f));
    REQUIRE (far > 0.95f);
}

TEST_CASE ("A +6 dB band boosts weight at its centre", "[sensitivity]")
{
    sensitivity::Parameters parameters;
    parameters.bands[0].frequencyHz = 1000.0f;
    parameters.bands[0].gainDb = 6.0f;
    parameters.bands[0].q = 1.0f;

    const auto centre = weightAt (parameters, 1000.0f);
    const auto far = weightAt (parameters, 20.0f);

    REQUIRE (centre == Catch::Approx (2.0f).margin (0.1f));
    REQUIRE (far == Catch::Approx (1.0f).margin (0.05f));
}

TEST_CASE ("A sibilance notch leaves the voice fundamental untouched", "[sensitivity]")
{
    sensitivity::Parameters parameters;

    // A narrow cut around the "S" consonant energy leaves the fundamental
    // (and the rest of the spectrum) at full processing weight.
    parameters.bands[0].frequencyHz = 6000.0f;
    parameters.bands[0].gainDb = -12.0f;
    parameters.bands[0].q = 8.0f;

    const auto sibilance = weightAt (parameters, 6000.0f);
    const auto fundamental = weightAt (parameters, 200.0f);
    const auto midband = weightAt (parameters, 1000.0f);

    REQUIRE (sibilance < 0.5f);
    REQUIRE (fundamental > 0.95f);
    REQUIRE (midband > 0.9f);
}

TEST_CASE ("Lo cut removes low-frequency processing", "[sensitivity]")
{
    sensitivity::Parameters parameters;
    parameters.loCutFrequencyHz = 1000.0f;
    parameters.loCutSlopeDbPerOctave = 12;

    REQUIRE (weightAt (parameters, 100.0f) < 0.1f);
    REQUIRE (weightAt (parameters, 5000.0f) > 0.9f);
}

TEST_CASE ("Hi cut removes high-frequency processing", "[sensitivity]")
{
    sensitivity::Parameters parameters;
    parameters.hiCutFrequencyHz = 1000.0f;
    parameters.hiCutSlopeDbPerOctave = 12;

    REQUIRE (weightAt (parameters, 5000.0f) < 0.1f);
    REQUIRE (weightAt (parameters, 100.0f) > 0.9f);
}

TEST_CASE ("Off cuts leave the curve flat", "[sensitivity]")
{
    sensitivity::Parameters parameters;
    parameters.loCutFrequencyHz = 1000.0f;
    parameters.loCutSlopeDbPerOctave = 0;
    parameters.hiCutFrequencyHz = 1000.0f;
    parameters.hiCutSlopeDbPerOctave = 0;

    REQUIRE (weightAt (parameters, 100.0f) == Catch::Approx (1.0f).margin (1.0e-6f));
    REQUIRE (weightAt (parameters, 5000.0f) == Catch::Approx (1.0f).margin (1.0e-6f));
}

TEST_CASE ("Band-listen bandpass passes its centre and rejects far frequencies", "[sensitivity]")
{
    const auto centreRms = sineRmsThroughBandpass (1000.0f, 48000);
    const auto farRms = sineRmsThroughBandpass (100.0f, 48000);

    REQUIRE (centreRms > 0.5f);
    REQUIRE (centreRms / farRms > 5.0f);
}
