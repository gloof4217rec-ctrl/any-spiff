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
    bool minimumPhase = false;  // false = linear (zero) phase, true = minimum phase
    int windowSize = 1024;      // 512 / 1024 / 2048 (short / medium / long)

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
    // Largest supported STFT size; buffers are sized to these and the runtime
    // window simply uses a subset of them.
    static constexpr int maxFftOrder = 11;
    static constexpr int maxFftSize = 1 << maxFftOrder;
    static constexpr int maxHopSize = maxFftSize / 4;
    static constexpr int maxNumBins = maxFftSize / 2 + 1;
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

    /** The current FFT size and bin count (depend on the Window parameter). */
    int getFftSize() const noexcept { return fftSize; }
    int getNumBins() const noexcept { return numBins; }

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
        graph. Safe to call from the message thread. Only the first getNumBins()
        entries are filled; the rest are zeroed.
    */
    void copyReduction (std::array<float, maxNumBins>& destination) const noexcept;

private:
    struct ChannelState
    {
        std::array<float, maxFftSize> inputRing {};     // last fftSize input samples
        std::array<float, maxFftSize> dryRing {};       // dry signal, delayed by fftSize
        std::array<float, 2 * maxFftSize> wetRing {};   // processed hops, primed with latency
        std::array<float, maxFftSize> frame {};         // chronological analysis frame
        std::array<float, 2 * maxFftSize> fftWork {};   // 2 * fftSize for real-only FFTs
        std::array<float, maxNumBins> magnitude {};
        std::array<float, maxNumBins> fastEnvelope {};
        std::array<float, maxNumBins> slowEnvelope {};
        std::array<float, maxNumBins> amount {};
        std::array<float, maxNumBins> scratch {};
        std::array<float, maxNumBins> gain {};
        std::array<float, maxFftSize> overlap {};       // overlap-add accumulator

        int inputWrite = 0;
        int dryWrite = 0;
        int wetRead = 0;
        int wetWrite = 0;                               // set to hopSize in reset()
        float drySample = 0.0f;
    };

    void transformChannel (ChannelState& channel);
    void detectChannel (ChannelState& channel);
    void synthesiseChannel (ChannelState& channel, float depthScale, bool boost, bool updateReduction, bool minimumPhase);
    void applyMinimumPhase (ChannelState& channel);
    void processFrame (const TransientShaperParameters& parameters, int numActiveChannels, bool updateDetector);
    void processBlock (float* const* channelData, int numChannels, int numSamples, const TransientShaperParameters& parameters);
    void applyOversamplingFactor (int factor);
    void applyWindowSize (int windowSize);
    void configureDsp (double effectiveSampleRate);
    void updateBlockCoefficients (const TransientShaperParameters& parameters);
    void applySharpness (ChannelState& channel, float sharpness);
    float pushSample (ChannelState& channel, float input);
    void calibrateFftScale();

    std::unique_ptr<juce::dsp::FFT> fft;

    std::array<ChannelState, maxChannels> channels;
    std::array<float, maxFftSize> window {};

    double sampleRate = 48000.0;
    double baseSampleRate = 48000.0;
    int fftOrder = 10;
    int fftSize = 1024;
    int hopSize = 256;
    int numBins = 513;
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
    std::array<float, maxNumBins> releaseCoef {};

    // Sensitivity curve: per-bin weight, rebuilt only when the curve moves.
    std::array<float, maxNumBins> weight {};
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
    std::array<std::atomic<float>, maxNumBins> reductionAmount {};

    // Scratch for the cepstral minimum-phase reconstruction.
    std::array<float, 2 * maxFftSize> minPhaseWork {};

    juce::SmoothedValue<float> depthSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> trimSmoothed;
    juce::SmoothedValue<float> activeSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdaptiveTransientShaper)
};
