#include "dsp/SensitivityCurve.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double lowestFrequency = 20.0;

    /** Magnitude of a normalised biquad (a0 == 1) at normalised frequency w, in dB. */
    double magnitudeDb (double b0, double b1, double b2, double a1, double a2, double w)
    {
        const auto c1 = std::cos (w);
        const auto s1 = std::sin (w);
        const auto c2 = std::cos (2.0 * w);
        const auto s2 = std::sin (2.0 * w);

        const auto nRe = b0 + b1 * c1 + b2 * c2;
        const auto nIm = -b1 * s1 - b2 * s2;
        const auto dRe = 1.0 + a1 * c1 + a2 * c2;
        const auto dIm = -a1 * s1 - a2 * s2;

        const auto num = std::sqrt (nRe * nRe + nIm * nIm);
        const auto den = std::sqrt (dRe * dRe + dIm * dIm);

        return 20.0 * std::log10 (den > 1.0e-12 ? num / den : 1.0e-6);
    }

    double peakingDb (double frequency, double f0, double gainDb, double q, double sampleRate)
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto w0 = 2.0 * juce::MathConstants<double>::pi * f0 / sampleRate;
        const auto alpha = std::sin (w0) / (2.0 * q);
        const auto a0 = 1.0 + alpha / A;

        const auto b0 = (1.0 + alpha * A) / a0;
        const auto b1 = -2.0 * std::cos (w0) / a0;
        const auto b2 = (1.0 - alpha * A) / a0;
        const auto a1 = -2.0 * std::cos (w0) / a0;
        const auto a2 = (1.0 - alpha / A) / a0;

        return magnitudeDb (b0, b1, b2, a1, a2, 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate);
    }

    double lowShelfDb (double frequency, double f0, double gainDb, double q, double sampleRate)
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto w0 = 2.0 * juce::MathConstants<double>::pi * f0 / sampleRate;
        const auto alpha = std::sin (w0) / (2.0 * q);
        const auto twoSqrtAAlpha = 2.0 * std::sqrt (A) * alpha;
        const auto cosw0 = std::cos (w0);

        const auto b0 = A * ((A + 1.0) - (A - 1.0) * cosw0 + twoSqrtAAlpha);
        const auto b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0);
        const auto b2 = A * ((A + 1.0) - (A - 1.0) * cosw0 - twoSqrtAAlpha);
        const auto a0 = (A + 1.0) + (A - 1.0) * cosw0 + twoSqrtAAlpha;
        const auto a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosw0);
        const auto a2 = (A + 1.0) + (A - 1.0) * cosw0 - twoSqrtAAlpha;

        return magnitudeDb (b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0,
                            2.0 * juce::MathConstants<double>::pi * frequency / sampleRate);
    }

    double highShelfDb (double frequency, double f0, double gainDb, double q, double sampleRate)
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto w0 = 2.0 * juce::MathConstants<double>::pi * f0 / sampleRate;
        const auto alpha = std::sin (w0) / (2.0 * q);
        const auto twoSqrtAAlpha = 2.0 * std::sqrt (A) * alpha;
        const auto cosw0 = std::cos (w0);

        const auto b0 = A * ((A + 1.0) + (A - 1.0) * cosw0 + twoSqrtAAlpha);
        const auto b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0);
        const auto b2 = A * ((A + 1.0) + (A - 1.0) * cosw0 - twoSqrtAAlpha);
        const auto a0 = (A + 1.0) - (A - 1.0) * cosw0 + twoSqrtAAlpha;
        const auto a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosw0);
        const auto a2 = (A + 1.0) - (A - 1.0) * cosw0 - twoSqrtAAlpha;

        return magnitudeDb (b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0,
                            2.0 * juce::MathConstants<double>::pi * frequency / sampleRate);
    }

    double tiltDb (double frequency, double f0, double gainDb)
    {
        // First-order tilt: +gainDb/2 at DC, -gainDb/2 at Nyquist, flat at f0.
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto f = std::max (frequency, 1.0e-3);
        const auto f0c = std::max (f0, 1.0);
        const auto ratio = f / f0c;
        const auto magSq = (ratio * ratio + A * A) / (ratio * ratio * A * A + 1.0);

        return 10.0 * std::log10 (magSq);
    }

    double butterworthDb (double frequency, double cutoffHz, int slopeDbPerOctave, bool highPass)
    {
        if (slopeDbPerOctave <= 0)
            return 0.0;

        const auto order = std::max (1, slopeDbPerOctave / 6);
        const auto fc = std::max ((double) cutoffHz, 1.0);
        const auto ratio = highPass ? fc / frequency : frequency / fc;
        const auto magSq = 1.0 / (1.0 + std::pow (ratio, 2 * order));

        return 10.0 * std::log10 (std::max (magSq, 1.0e-12));
    }

    double totalGainDb (const sensitivity::Parameters& parameters, double sampleRate, double frequency)
    {
        const auto f = std::clamp (frequency, lowestFrequency, sampleRate * 0.5);

        double gainDb = 0.0;

        for (const auto& band : parameters.bands)
        {
            if (! band.enabled)
                continue;

            const auto f0 = std::clamp ((double) band.frequencyHz, lowestFrequency, sampleRate * 0.5);
            const auto q = std::clamp ((double) band.q, 0.1, 10.0);

            switch (band.type)
            {
                case sensitivity::Band::lowShelf:  gainDb += lowShelfDb (f, f0, band.gainDb, q, sampleRate); break;
                case sensitivity::Band::highShelf: gainDb += highShelfDb (f, f0, band.gainDb, q, sampleRate); break;
                case sensitivity::Band::tiltShelf: gainDb += tiltDb (f, f0, band.gainDb); break;
                case sensitivity::Band::parametric:
                default:                           gainDb += peakingDb (f, f0, band.gainDb, q, sampleRate); break;
            }
        }

        gainDb += butterworthDb (f, parameters.loCutFrequencyHz, parameters.loCutSlopeDbPerOctave, true);
        gainDb += butterworthDb (f, parameters.hiCutFrequencyHz, parameters.hiCutSlopeDbPerOctave, false);

        return gainDb;
    }

    float dbToWeight (double gainDb)
    {
        // 0 dB maps to a weight of 1.0 (the default, full processing). Positive
        // gains raise the weight above 1 to boost the per-frequency processing,
        // negative gains lower it towards 0. The top is capped at +36 dB — three
        // stacked +12 dB bands — so the multiplier stays finite and sane.
        return (float) juce::jlimit (0.0, 64.0, std::pow (10.0, gainDb / 20.0));
    }
}

namespace sensitivity
{
    void computeWeights (const Parameters& parameters, double sampleRate,
                         int numBins, float* weights)
    {
        jassert (weights != nullptr && numBins > 1);

        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto frequency = std::max (lowestFrequency,
                                             sampleRate * 0.5 * (double) bin / (double) (numBins - 1));
            weights[bin] = dbToWeight (totalGainDb (parameters, sampleRate, frequency));
        }
    }

    float weightAtFrequency (const Parameters& parameters, double sampleRate, float frequencyHz)
    {
        return dbToWeight (totalGainDb (parameters, sampleRate, frequencyHz));
    }

    Biquad makeBandpass (float frequencyHz, float q, double sampleRate)
    {
        const auto f0 = std::clamp ((double) frequencyHz, lowestFrequency, sampleRate * 0.5);
        const auto qClamped = std::clamp ((double) q, 0.1, 10.0);
        const auto w0 = 2.0 * juce::MathConstants<double>::pi * f0 / sampleRate;
        const auto alpha = std::sin (w0) / (2.0 * qClamped);
        const auto a0 = 1.0 + alpha;

        Biquad biquad;
        biquad.b0 = (float) (alpha / a0);
        biquad.b1 = 0.0f;
        biquad.b2 = (float) (-alpha / a0);
        biquad.a1 = (float) (-2.0 * std::cos (w0) / a0);
        biquad.a2 = (float) ((1.0 - alpha) / a0);

        return biquad;
    }
}

