#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

/** A snapshot of the user-facing controls, already converted to DSP-friendly units.

    The processor reads the APVTS atoms once per block and fills this in, so the
    audio thread never has to poke around in the parameter tree.
*/
struct TransientShaperParameters
{
    float depth = 0.0f;       // 0..1, 0 = no processing at all
    float sensitivity = 0.5f; // 0..1, higher catches quieter transients
    float sharpness = 0.5f;   // 0..1, higher = narrower processed regions
    float decay = 0.0f;       // 0..10, how long the cut/boost lingers
    float decayLfHf = 0.5f;   // 0..1, 0.5 = same decay across the spectrum
    float stereoLink = 1.0f;  // 0..1, 1 = both channels share one detector
    float balance = 0.0f;     // -1..1, negative = left only, positive = right only
    float mix = 1.0f;         // 0..1
    float trimDb = 0.0f;      // make-up gain for the wet signal only
    bool delta = false;       // output wet - dry
    bool bypass = false;      // soft bypass (the DSP keeps running)
    bool boost = false;       // boost transients instead of cutting them
    bool midSide = false;     // detect/process mid and side instead of left and right
};

/** Adaptive, frequency-dependent transient shaper.

    The signal is analysed with a Hann-windowed STFT (1024 samples, 75% overlap).
    For every bin a fast and a slow envelope are tracked, and their ratio decides
    how "transient" that bin currently is. The ratio is level independent, so the
    result is driven by the Depth parameter rather than by the input level.
    The per-bin transient amounts are smoothed over time (Decay, Decay LF/HF) and
    over frequency (Sharpness), then turned into a gain curve that is applied in
    the STFT domain. Everything is allocated in prepare(), so process() is
    realtime safe.
*/
class AdaptiveTransientShaper
{
public:
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int numBins = fftSize / 2 + 1;
    static constexpr int maxChannels = 2;

    AdaptiveTransientShaper();

    /** Allocates nothing (all buffers are fixed size) but sets up windows,
        coefficients and the smoothed parameters. Call from prepareToPlay().
    */
    void prepare (double newSampleRate);

    /** Drops all history: envelopes, delay lines and overlap-add accumulators. */
    void reset();

    /** Processes the buffer in place. The output is delayed by getLatencySamples(). */
    void process (juce::AudioBuffer<float>& buffer, const TransientShaperParameters& parameters);

    /** The STFT latency (one full FFT frame) that must be reported to the host. */
    int getLatencySamples() const noexcept { return fftSize; }

private:
    struct ChannelState
    {
        std::array<float, fftSize> inputRing {};     // last fftSize input samples
        std::array<float, fftSize> dryRing {};       // dry signal, delayed by fftSize
        std::array<float, 2 * fftSize> wetRing {};   // processed hops, primed with latency
        std::array<float, fftSize> frame {};         // chronological analysis frame
        std::array<float, 2 * fftSize> fftWork {};   // 2 * fftSize for real-only FFTs
        std::array<float, numBins> magnitude {};
        std::array<float, numBins> fastEnvelope {};
        std::array<float, numBins> slowEnvelope {};
        std::array<float, numBins> amount {};
        std::array<float, numBins> scratch {};
        std::array<float, numBins> gain {};
        std::array<float, fftSize> overlap {};       // overlap-add accumulator

        int inputWrite = 0;
        int dryWrite = 0;
        int wetRead = 0;
        int wetWrite = hopSize;                      // trails wetRead by one hop
        float drySample = 0.0f;
    };

    void analyseChannel (ChannelState& channel);
    void synthesiseChannel (ChannelState& channel, float depthScale, bool boost);
    void processFrame (const TransientShaperParameters& parameters, int numActiveChannels);
    void updateBlockCoefficients (const TransientShaperParameters& parameters);
    void applySharpness (ChannelState& channel, float sharpness);
    float pushSample (ChannelState& channel, float input);
    void calibrateFftScale();

    juce::dsp::FFT fft { fftOrder };

    std::array<ChannelState, maxChannels> channels;
    std::array<float, fftSize> window {};

    double sampleRate = 48000.0;
    int hopCounter = 0;
    float synthesisScale = 2.0f / 3.0f;

    float thresholdDb = 6.0f;
    float lastDecay = -1.0f;     // release-curve cache (rebuilt when Decay moves)
    float lastDecayLfHf = -1.0f;
    float fastAttackCoef = 0.0f;
    float fastReleaseCoef = 0.0f;
    float slowAttackCoef = 0.0f;
    float slowReleaseCoef = 0.0f;
    float amountAttackCoef = 0.0f;
    std::array<float, numBins> releaseCoef {};

    juce::SmoothedValue<float> depthSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> trimSmoothed;
    juce::SmoothedValue<float> activeSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdaptiveTransientShaper)
};
