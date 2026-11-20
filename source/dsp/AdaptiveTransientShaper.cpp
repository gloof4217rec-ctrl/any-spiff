#include "dsp/AdaptiveTransientShaper.h"

namespace
{
    constexpr float minEnvelope = 1.0e-9f;
    constexpr float transientRangeDb = 12.0f;
    constexpr float lowestFrequency = 20.0f;
    constexpr float highestFrequency = 20000.0f;
    constexpr float boostGainAmount = 1.0f;   // Depth 100% + full transient = +6 dB
    constexpr float overlapGain = 1.5f;       // sum of Hann^2 at 75% overlap

    float coefficientForTimeConstant (double blockSeconds, double timeConstantSeconds)
    {
        const auto ratio = blockSeconds / juce::jmax (1.0e-6, timeConstantSeconds);
        return (float) (1.0 - std::exp (-ratio));
    }
}

//==============================================================================
AdaptiveTransientShaper::AdaptiveTransientShaper()
{
    calibrateFftScale();
}

void AdaptiveTransientShaper::calibrateFftScale()
{
    // The engines behind juce::dsp::FFT normalise the inverse transform, but measuring
    // the round trip once keeps us honest if that ever changes.
    std::vector<float> work ((size_t) 2 * fftSize, 0.0f);
    work[0] = 1.0f;

    fft.performRealOnlyForwardTransform (work.data());
    fft.performRealOnlyInverseTransform (work.data());

    const auto roundTrip = std::abs (work[0]);
    synthesisScale = roundTrip > 1.0e-6f ? 1.0f / (overlapGain * roundTrip) : 1.0f / overlapGain;
}

void AdaptiveTransientShaper::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);

    const auto hopSeconds = (double) hopSize / sampleRate;

    // Fast envelope: follows the signal almost instantly, forgets within a few frames.
    fastAttackCoef = coefficientForTimeConstant (hopSeconds, 0.001);
    fastReleaseCoef = coefficientForTimeConstant (hopSeconds, 0.030);

    // Slow envelope: the average level of the bin, lags behind on attacks.
    slowAttackCoef = coefficientForTimeConstant (hopSeconds, 0.040);
    slowReleaseCoef = coefficientForTimeConstant (hopSeconds, 0.250);

    // How quickly a detected transient reaches full strength.
    amountAttackCoef = coefficientForTimeConstant (hopSeconds, 0.002);

    // Force the per-bin release curve to be rebuilt for the new sample rate.
    lastDecay = -1.0f;
    lastDecayLfHf = -1.0f;

    depthSmoothed.reset (sampleRate, 0.05);
    mixSmoothed.reset (sampleRate, 0.02);
    trimSmoothed.reset (sampleRate, 0.02);
    activeSmoothed.reset (sampleRate, 0.01);

    reset();
}

void AdaptiveTransientShaper::reset()
{
    for (auto& channel : channels)
    {
        channel.inputRing.fill (0.0f);
        channel.dryRing.fill (0.0f);
        channel.wetRing.fill (0.0f);
        channel.frame.fill (0.0f);
        channel.fftWork.fill (0.0f);
        channel.magnitude.fill (0.0f);
        channel.fastEnvelope.fill (0.0f);
        channel.slowEnvelope.fill (0.0f);
        channel.amount.fill (0.0f);
        channel.scratch.fill (0.0f);
        channel.gain.fill (1.0f);
        channel.overlap.fill (0.0f);

        channel.inputWrite = 0;
        channel.dryWrite = 0;
        channel.wetRead = 0;
        // The re-synthesis reader trails the writer by one hop: the samples that are
        // written at a frame boundary are read out during the following hop, which
        // makes the wet path exactly fftSize samples late (matching the dry delay).
        channel.wetWrite = hopSize;
        channel.drySample = 0.0f;
    }

    hopCounter = 0;

    weight.fill (1.0f);
    curveCached = false;
    lastListenFrequencyHz = -1.0f;
    lastListenQ = -1.0f;
    listenZ1.fill (0.0f);
    listenZ2.fill (0.0f);

    for (auto& amount : reductionAmount)
        amount.store (0.0f);

    depthSmoothed.setCurrentAndTargetValue (0.0f);
    mixSmoothed.setCurrentAndTargetValue (1.0f);
    trimSmoothed.setCurrentAndTargetValue (1.0f);
    activeSmoothed.setCurrentAndTargetValue (1.0f);
}

void AdaptiveTransientShaper::copyReduction (std::array<float, numBins>& destination) const noexcept
{
    for (int bin = 0; bin < numBins; ++bin)
        destination[(size_t) bin] = reductionAmount[(size_t) bin].load();
}

//==============================================================================
void AdaptiveTransientShaper::updateBlockCoefficients (const TransientShaperParameters& parameters)
{
    // Sensitivity lowers the threshold: 0 = only very strong transients, 1 = anything above average.
    thresholdDb = transientRangeDb * (1.0f - juce::jlimit (0.0f, 1.0f, parameters.sensitivity));

    // The per-bin release curve only depends on Decay and Decay LF/HF, so only
    // rebuild it when one of those actually moves (it needs a pow/exp per bin).
    const auto decay = juce::jlimit (0.0f, 10.0f, parameters.decay);
    const auto lfHf = juce::jlimit (0.0f, 1.0f, parameters.decayLfHf);

    if (std::abs (decay - lastDecay) >= 1.0e-4f || std::abs (lfHf - lastDecayLfHf) >= 1.0e-4f)
    {
        lastDecay = decay;
        lastDecayLfHf = lfHf;

        const auto hopSeconds = (double) hopSize / sampleRate;

        // Decay 0..10 maps to a 10 ms .. 640 ms release of the transient amount.
        const auto decayMs = 10.0 * std::pow (2.0, 0.6 * (double) decay);

        // Decay LF/HF tilts the release time between 20 Hz and 20 kHz.
        const auto tilt = (double) lfHf - 0.5;
        const auto logLowFactor = std::log2 (std::pow (4.0, tilt));
        const auto logRange = std::log2 ((double) highestFrequency / (double) lowestFrequency);

        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto frequency = juce::jmax ((double) lowestFrequency,
                                               sampleRate * 0.5 * (double) bin / (double) (numBins - 1));
            const auto position = juce::jlimit (0.0, 1.0, std::log2 (frequency / (double) lowestFrequency) / logRange);
            const auto factor = std::exp2 (logLowFactor * (1.0 - 2.0 * position));
            const auto releaseMs = juce::jlimit (1.0, 2000.0, decayMs * factor);

            releaseCoef[(size_t) bin] = (float) (1.0 - std::exp (-hopSeconds * 1000.0 / releaseMs));
        }
    }

    // The sensitivity curve is only recomputed when a band or cut moves.
    if (! curveCached || ! (parameters.curve == lastCurve))
    {
        lastCurve = parameters.curve;
        curveCached = true;
        sensitivity::computeWeights (parameters.curve, sampleRate, numBins, weight.data());
    }

    // The band-listen bandpass only changes when the listened band moves.
    const auto listenFrequency = juce::jlimit (lowestFrequency, (float) (sampleRate * 0.5), parameters.listenFrequencyHz);
    const auto listenQ = juce::jlimit (0.1f, 10.0f, parameters.listenQ);

    if (std::abs (listenFrequency - lastListenFrequencyHz) > 1.0e-3f
        || std::abs (listenQ - lastListenQ) > 1.0e-3f)
    {
        lastListenFrequencyHz = listenFrequency;
        lastListenQ = listenQ;
        listenBiquad = sensitivity::makeBandpass (listenFrequency, listenQ, sampleRate);

        // Drop the filter history so a coefficient jump cannot ring.
        listenZ1.fill (0.0f);
        listenZ2.fill (0.0f);
    }
}

//==============================================================================
float AdaptiveTransientShaper::pushSample (ChannelState& channel, float input)
{
    const auto wet = channel.wetRing[(size_t) channel.wetRead];
    channel.wetRead = (channel.wetRead + 1) % (2 * fftSize);

    channel.inputRing[(size_t) channel.inputWrite] = input;
    channel.inputWrite = (channel.inputWrite + 1) % fftSize;

    channel.drySample = channel.dryRing[(size_t) channel.dryWrite];
    channel.dryRing[(size_t) channel.dryWrite] = input;
    channel.dryWrite = (channel.dryWrite + 1) % fftSize;

    return wet;
}

void AdaptiveTransientShaper::analyseChannel (ChannelState& channel)
{
    // The oldest sample sits where the next write would go: that is the frame start.
    for (int i = 0; i < fftSize; ++i)
    {
        const auto source = (channel.inputWrite + i) % fftSize;
        channel.frame[(size_t) i] = channel.inputRing[(size_t) source];
        channel.fftWork[(size_t) i] = channel.frame[(size_t) i] * window[(size_t) i];
    }

    std::fill (channel.fftWork.begin() + fftSize, channel.fftWork.end(), 0.0f);

    fft.performRealOnlyForwardTransform (channel.fftWork.data());

    for (int bin = 0; bin < numBins; ++bin)
    {
        const auto real = channel.fftWork[(size_t) (2 * bin)];
        const auto imaginary = channel.fftWork[(size_t) (2 * bin + 1)];
        const auto magnitude = std::sqrt (real * real + imaginary * imaginary);

        channel.magnitude[(size_t) bin] = magnitude;

        auto& fast = channel.fastEnvelope[(size_t) bin];
        auto& slow = channel.slowEnvelope[(size_t) bin];

        fast += (magnitude - fast) * (magnitude > fast ? fastAttackCoef : fastReleaseCoef);
        slow += (magnitude - slow) * (magnitude > slow ? slowAttackCoef : slowReleaseCoef);

        const auto ratio = (fast + minEnvelope) / (slow + minEnvelope);
        const auto ratioDb = juce::Decibels::gainToDecibels (ratio, -100.0f);
        const auto target = juce::jlimit (0.0f, 1.0f, (ratioDb - thresholdDb) / transientRangeDb);

        auto& amount = channel.amount[(size_t) bin];
        amount += (target - amount) * (target > amount ? amountAttackCoef : releaseCoef[(size_t) bin]);
    }
}

void AdaptiveTransientShaper::applySharpness (ChannelState& channel, float sharpness)
{
    // A 1-2-1 blur widens the processed regions: fewer passes = narrower, sharper cuts.
    const auto passes = juce::jlimit (0, 4, (int) std::lround ((1.0f - juce::jlimit (0.0f, 1.0f, sharpness)) * 4.0f));

    for (int pass = 0; pass < passes; ++pass)
    {
        channel.scratch[0] = channel.amount[0];
        channel.scratch[(size_t) (numBins - 1)] = channel.amount[(size_t) (numBins - 1)];

        for (int bin = 1; bin < numBins - 1; ++bin)
            channel.scratch[(size_t) bin] = 0.25f * channel.amount[(size_t) (bin - 1)]
                                          + 0.50f * channel.amount[(size_t) bin]
                                          + 0.25f * channel.amount[(size_t) (bin + 1)];

        std::copy (channel.scratch.begin(), channel.scratch.end(), channel.amount.begin());
    }
}

void AdaptiveTransientShaper::synthesiseChannel (ChannelState& channel, float depthScale, bool boost, bool updateReduction)
{
    const auto depth = depthSmoothed.getCurrentValue() * juce::jlimit (0.0f, 1.0f, depthScale);

    for (int bin = 0; bin < numBins; ++bin)
    {
        const auto amount = channel.amount[(size_t) bin] * depth * weight[(size_t) bin];
        const auto gain = boost ? 1.0f + amount * boostGainAmount
                                : juce::jmax (0.0f, 1.0f - amount);

        channel.gain[(size_t) bin] = gain;

        if (updateReduction)
            reductionAmount[(size_t) bin].store (amount);

        channel.fftWork[(size_t) (2 * bin)] *= gain;
        channel.fftWork[(size_t) (2 * bin + 1)] *= gain;
    }

    // Mirror the gains onto the negative frequencies to keep the spectrum conjugate-symmetric.
    for (int bin = 1; bin < numBins - 1; ++bin)
    {
        const auto mirror = (size_t) (fftSize - bin);
        const auto gain = channel.gain[(size_t) bin];

        channel.fftWork[2 * mirror] *= gain;
        channel.fftWork[2 * mirror + 1] *= gain;
    }

    fft.performRealOnlyInverseTransform (channel.fftWork.data());

    for (int i = 0; i < fftSize; ++i)
        channel.overlap[(size_t) i] += channel.fftWork[(size_t) i] * window[(size_t) i] * synthesisScale;

    for (int i = 0; i < hopSize; ++i)
    {
        channel.wetRing[(size_t) channel.wetWrite] = channel.overlap[(size_t) i];
        channel.wetWrite = (channel.wetWrite + 1) % (2 * fftSize);
    }

    std::memmove (channel.overlap.data(), channel.overlap.data() + hopSize,
                  (size_t) (fftSize - hopSize) * sizeof (float));
    std::fill (channel.overlap.end() - hopSize, channel.overlap.end(), 0.0f);
}

void AdaptiveTransientShaper::processFrame (const TransientShaperParameters& parameters, int numActiveChannels)
{
    for (int c = 0; c < numActiveChannels; ++c)
        analyseChannel (channels[(size_t) c]);

    if (numActiveChannels == 2)
    {
        const auto link = juce::jlimit (0.0f, 1.0f, parameters.stereoLink);

        if (link > 0.0f)
        {
            for (int bin = 0; bin < numBins; ++bin)
            {
                auto& left = channels[0].amount[(size_t) bin];
                auto& right = channels[1].amount[(size_t) bin];
                const auto mean = 0.5f * (left + right);

                left += link * (mean - left);
                right += link * (mean - right);
            }
        }
    }

    depthSmoothed.skip (hopSize);

    for (int c = 0; c < numActiveChannels; ++c)
        applySharpness (channels[(size_t) c], parameters.sharpness);

    auto depthLeft = 1.0f;
    auto depthRight = 1.0f;

    if (numActiveChannels == 2)
    {
        const auto balance = juce::jlimit (-1.0f, 1.0f, parameters.balance);
        depthLeft = 1.0f - juce::jmax (0.0f, balance);
        depthRight = 1.0f - juce::jmax (0.0f, -balance);
    }

    synthesiseChannel (channels[0], depthLeft, parameters.boost, true);

    if (numActiveChannels == 2)
        synthesiseChannel (channels[1], depthRight, parameters.boost, false);
}

//==============================================================================
void AdaptiveTransientShaper::process (juce::AudioBuffer<float>& buffer, const TransientShaperParameters& parameters)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numActiveChannels = juce::jmin (buffer.getNumChannels(), maxChannels);

    if (numSamples <= 0 || numActiveChannels <= 0)
        return;

    updateBlockCoefficients (parameters);

    depthSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, parameters.depth));
    mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, parameters.mix));
    trimSmoothed.setTargetValue (juce::Decibels::decibelsToGain (parameters.trimDb));
    activeSmoothed.setTargetValue (parameters.bypass ? 0.0f : 1.0f);

    const auto isStereo = numActiveChannels > 1;
    const auto isMidSide = isStereo && parameters.midSide;

    auto* left = buffer.getWritePointer (0);
    auto* right = isStereo ? buffer.getWritePointer (1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto inputLeft = left[sample];
        const auto inputRight = right != nullptr ? right[sample] : 0.0f;

        const auto channelLeft = isMidSide ? 0.5f * (inputLeft + inputRight) : inputLeft;
        const auto channelRight = isMidSide ? 0.5f * (inputLeft - inputRight) : inputRight;

        const auto wetLeft = pushSample (channels[0], channelLeft);
        const auto wetRight = right != nullptr ? pushSample (channels[1], channelRight) : 0.0f;

        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;
            processFrame (parameters, numActiveChannels);
        }

        const auto dryLeft = channels[0].drySample;
        const auto dryRight = channels[1].drySample;

        const auto mix = mixSmoothed.getNextValue();
        const auto trim = trimSmoothed.getNextValue();
        const auto active = activeSmoothed.getNextValue();

        const auto wetLeftTrimmed = wetLeft * trim;
        const auto wetRightTrimmed = wetRight * trim;

        float outputLeft = 0.0f;
        float outputRight = 0.0f;

        if (parameters.bandListen)
        {
            // Band listen: hear only the delta inside the listened band. This
            // overrides mix/delta/bypass so the user can hunt for frequencies.
            const auto deltaLeft = wetLeftTrimmed - dryLeft;
            const auto deltaRight = wetRightTrimmed - dryRight;

            outputLeft = listenBiquad.processSample (deltaLeft, listenZ1[0], listenZ2[0]);
            outputRight = listenBiquad.processSample (deltaRight, listenZ1[1], listenZ2[1]);
        }
        else
        {
            const auto targetLeft = parameters.delta ? wetLeftTrimmed - dryLeft : wetLeftTrimmed;
            const auto targetRight = parameters.delta ? wetRightTrimmed - dryRight : wetRightTrimmed;

            outputLeft = dryLeft + mix * (targetLeft - dryLeft);
            outputRight = dryRight + mix * (targetRight - dryRight);

            // Soft bypass: crossfade back to the (latency matched) dry signal.
            outputLeft = dryLeft + active * (outputLeft - dryLeft);
            outputRight = dryRight + active * (outputRight - dryRight);
        }

        if (isMidSide)
        {
            left[sample] = outputLeft + outputRight;
            right[sample] = outputLeft - outputRight;
        }
        else
        {
            left[sample] = outputLeft;

            if (right != nullptr)
                right[sample] = outputRight;
        }
    }
}
