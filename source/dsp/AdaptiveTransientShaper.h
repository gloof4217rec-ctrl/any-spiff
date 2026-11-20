#pragma once

#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/SensitivityCurve.h"

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
    int resolutionHops = 1;   // detector updates every N hops (1/2/4 = high/medium/low)
    int oversamplingFactor = 1; // 1/2/4/8 = off/2x/4x/8x

    sensitivity::Parameters curve;   // frequency-dependent detector weighting
    bool bandListen = false;         // monitor only the delta inside the listened band
    float listenFrequencyHz = 1000.0f;
    float listenQ = 1.0f;
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

    /** The STFT latency (one full FFT frame) that must be reported to the host.
        Includes the oversampler delay when oversampling is active.
    */
    int getLatencySamples() const noexcept
    {
        const auto factor = juce::jmax (1, currentOversamplingFactor.load());
        const auto osLatency = oversampler != nullptr
                             ? (int) std::llround ((double) oversampler->getLatencyInSamples())
                             : 0;

        return osLatency + fftSize / factor;
    }

    /** Copies the current per-bin processing amount (0..1) for the reduction
        graph. Safe to call from the message thread.
    */
    void copyReduction (std::array<float, numBins>& destination) const noexcept;

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

    void transformChannel (ChannelState& channel);
    void detectChannel (ChannelState& channel);
    void synthesiseChannel (ChannelState& channel, float depthScale, bool boost, bool updateReduction);
    void processFrame (const TransientShaperParameters& parameters, int numActiveChannels, bool updateDetector);
    void processBlock (float* const* channelData, int numChannels, int numSamples, const TransientShaperParameters& parameters);
    void applyOversamplingFactor (int factor);
    void configureDsp (double effectiveSampleRate);
    void updateBlockCoefficients (const TransientShaperParameters& parameters);
    void applySharpness (ChannelState& channel, float sharpness);
    float pushSample (ChannelState& channel, float input);
    void calibrateFftScale();

    juce::dsp::FFT fft { fftOrder };

    std::array<ChannelState, maxChannels> channels;
    std::array<float, fftSize> window {};

    double sampleRate = 48000.0;
    double baseSampleRate = 48000.0;
    int hopCounter = 0;
    int resolutionCounter = 0;
    float synthesisScale = 2.0f / 3.0f;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::atomic<int> currentOversamplingFactor { 1 };

    float thresholdDb = 6.0f;
    float lastDecay = -1.0f;     // release-curve cache (rebuilt when Decay moves)
    float lastDecayLfHf = -1.0f;
    float fastAttackCoef = 0.0f;
    float fastReleaseCoef = 0.0f;
    float slowAttackCoef = 0.0f;
    float slowReleaseCoef = 0.0f;
    float amountAttackCoef = 0.0f;
    std::array<float, numBins> releaseCoef {};

    // Sensitivity curve: per-bin weight, rebuilt only when the curve moves.
    std::array<float, numBins> weight {};
    sensitivity::Parameters lastCurve;
    bool curveCached = false;

    // Band listen: a second-order bandpass on the delta path.
    float lastListenFrequencyHz = -1.0f;
    float lastListenQ = -1.0f;
    sensitivity::Biquad listenBiquad;
    std::array<float, maxChannels> listenZ1 {};
    std::array<float, maxChannels> listenZ2 {};

    // Per-bin processing amount published for the reduction graph. Written on
    // the audio thread (once per frame) and read by the message thread.
    std::array<std::atomic<float>, numBins> reductionAmount {};

    juce::SmoothedValue<float> depthSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> trimSmoothed;
    juce::SmoothedValue<float> activeSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdaptiveTransientShaper)
};
