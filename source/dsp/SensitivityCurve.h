#pragma once

#include <array>

#include <juce_audio_basics/juce_audio_basics.h>

/** Frequency-dependent weighting of the transient detector.

    The sensitivity curve is spiff's "graph": it decides how strongly each
    frequency contributes to the transient processing. It is NOT an EQ on the
    audio path — it only scales the per-bin transient amount, so the signal is
    left untouched wherever the curve sits at unity (0 dB).

    The curve is modelled as three user bands (parametric / low shelf / high
    shelf / tilt shelf) plus a lo cut (high-pass) and a hi cut (low-pass). All
    contributions are summed in dB and converted to a linear weight: 0 dB maps
    to 1.0, positive gains boost the per-frequency processing above the default
    and negative gains attenuate it towards zero.
*/
namespace sensitivity
{
    struct Band
    {
        enum Type
        {
            parametric = 0,
            lowShelf = 1,
            highShelf = 2,
            tiltShelf = 3
        };

        float frequencyHz = 1000.0f;
        float gainDb = 0.0f;    // -12..+12 dB, 0 = no weighting change
        float q = 0.7f;         // 0.1..10
        int type = parametric;
        bool enabled = true;

        bool operator== (const Band&) const = default;
    };

    struct Parameters
    {
        std::array<Band, 3> bands;

        float loCutFrequencyHz = 20.0f;
        int loCutSlopeDbPerOctave = 0;   // 0 = off, else 12/24/48

        float hiCutFrequencyHz = 20000.0f;
        int hiCutSlopeDbPerOctave = 0;

        bool operator== (const Parameters&) const = default;
    };

    /** A second-order, constant 0 dB peak bandpass (used for band listen). */
    struct Biquad
    {
        float b0 = 0.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;

        float processSample (float x, float& z1, float& z2) const noexcept
        {
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    /** Fills `weights` (size `numBins`) with a per-bin multiplier for the
        centre frequency of each bin. 1.0 is the default; values above 1 boost
        the per-frequency processing and values below 1 attenuate it.
    */
    void computeWeights (const Parameters& parameters, double sampleRate,
                         int numBins, float* weights);

    /** The curve weight at a single frequency. Used by the graph UI. */
    float weightAtFrequency (const Parameters& parameters, double sampleRate, float frequencyHz);

    /** Builds the band-listen bandpass centred on `frequencyHz` with the given Q. */
    Biquad makeBandpass (float frequencyHz, float q, double sampleRate);
}
