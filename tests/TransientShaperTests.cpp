#include "helpers/test_helpers.h"

#include <dsp/AdaptiveTransientShaper.h>

#include <catch2/catch_test_macros.hpp>

namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int latencySamples = AdaptiveTransientShaper::fftSize;

    void processInBlocks (AdaptiveTransientShaper& shaper,
                          juce::AudioBuffer<float>& buffer,
                          const TransientShaperParameters& parameters,
                          int blockSize = 256)
    {
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
        {
            const auto numSamples = juce::jmin (blockSize, buffer.getNumSamples() - start);
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(),
                                            buffer.getNumChannels(), start, numSamples);
            shaper.process (block, parameters);
        }
    }

    juce::AudioBuffer<float> makeSine (int numChannels, int numSamples, float frequency, float amplitude)
    {
        juce::AudioBuffer<float> buffer (numChannels, numSamples);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int sample = 0; sample < numSamples; ++sample)
                data[sample] = amplitude * std::sin (juce::MathConstants<float>::twoPi
                                                     * frequency * (float) sample / (float) testSampleRate);
        }

        return buffer;
    }

    /** A quiet sustained tone with short broadband clicks every 250 ms.

        Both channels are identical, which keeps the stereo tests deterministic.
    */
    juce::AudioBuffer<float> makeClickySignal (int numSamples)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        juce::Random random (1234);

        auto* data = buffer.getWritePointer (0);

        for (int sample = 0; sample < numSamples; ++sample)
            data[sample] = 0.1f * std::sin (juce::MathConstants<float>::twoPi * 220.0f
                                            * (float) sample / (float) testSampleRate);

        for (int start = 12000; start < numSamples; start += 12000)
        {
            for (int i = 0; i < 96 && start + i < numSamples; ++i)
            {
                const auto envelope = 1.0f - (float) i / 96.0f;
                data[start + i] += 0.8f * envelope * (random.nextFloat() * 2.0f - 1.0f);
            }
        }

        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

        return buffer;
    }

    float rms (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        const auto end = juce::jmin (startSample + numSamples, buffer.getNumSamples());
        double sum = 0.0;
        int count = 0;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            for (int sample = startSample; sample < end; ++sample)
            {
                const auto value = buffer.getSample (channel, sample);
                sum += (double) value * (double) value;
                ++count;
            }
        }

        return count > 0 ? (float) std::sqrt (sum / (double) count) : 0.0f;
    }

    float maxDifference (const juce::AudioBuffer<float>& input, const juce::AudioBuffer<float>& output,
                         int delaySamples, int startSample)
    {
        float maxDelta = 0.0f;
        const auto end = juce::jmin (input.getNumSamples(), output.getNumSamples() - delaySamples);

        for (int channel = 0; channel < input.getNumChannels(); ++channel)
            for (int sample = startSample; sample < end; ++sample)
                maxDelta = juce::jmax (maxDelta,
                                       std::abs (input.getSample (channel, sample)
                                                 - output.getSample (channel, sample + delaySamples)));

        return maxDelta;
    }

    float maxDifferenceBetweenChannels (const juce::AudioBuffer<float>& buffer, int startSample)
    {
        float maxDelta = 0.0f;

        for (int sample = startSample; sample < buffer.getNumSamples(); ++sample)
            maxDelta = juce::jmax (maxDelta,
                                   std::abs (buffer.getSample (0, sample)
                                             - buffer.getSample (1, sample)));

        return maxDelta;
    }

    TransientShaperParameters makeActiveCutParameters()
    {
        TransientShaperParameters parameters;
        parameters.depth = 1.0f;
        parameters.sensitivity = 0.5f;
        parameters.sharpness = 0.5f;
        parameters.decay = 0.0f;
        return parameters;
    }
}

TEST_CASE ("Transient shaper reports the STFT latency", "[dsp]")
{
    PluginProcessor processor;
    processor.prepareToPlay (testSampleRate, 512);

    CHECK (processor.getLatencySamples() == latencySamples);
}

TEST_CASE ("Zero depth leaves the signal untouched apart from the latency", "[dsp]")
{
    AdaptiveTransientShaper shaper;
    shaper.prepare (testSampleRate);

    const auto input = makeSine (2, 24000, 440.0f, 0.5f);
    auto output = input;

    TransientShaperParameters parameters; // depth 0, mix 100%
    processInBlocks (shaper, output, parameters);

    CHECK (maxDifference (input, output, latencySamples, 0) < 1.0e-4f);
}

TEST_CASE ("Boost mode emphasises clicks without touching the sustain", "[dsp]")
{
    AdaptiveTransientShaper shaper;
    shaper.prepare (testSampleRate);

    const auto input = makeClickySignal (48000);
    auto output = input;

    auto parameters = makeActiveCutParameters();
    parameters.boost = true;

    processInBlocks (shaper, output, parameters);

    const auto clickBefore = rms (input, 12000, 2048);
    const auto clickAfter = rms (output, 12000 + latencySamples, 2048);

    CHECK (clickAfter > 1.2f * clickBefore);

    const auto steadyBefore = rms (input, 14000, 4096);
    const auto steadyAfter = rms (output, 14000 + latencySamples, 4096);

    CHECK (std::abs (steadyAfter - steadyBefore) < 0.01f * steadyBefore);
}

TEST_CASE ("Cut mode removes click energy and keeps the sustained tone", "[dsp]")
{
    AdaptiveTransientShaper shaper;
    shaper.prepare (testSampleRate);

    const auto input = makeClickySignal (48000);
    auto output = input;

    processInBlocks (shaper, output, makeActiveCutParameters());

    // The click sits at sample 12000 of the input, which shows up one FFT frame later.
    constexpr int clickSample = 12000;
    const auto clickBefore = rms (input, clickSample, 2048);
    const auto clickAfter = rms (output, clickSample + latencySamples, 2048);

    CHECK (clickAfter < 0.85f * clickBefore);

    // The steady tone between the clicks should survive almost untouched.
    constexpr int steadySample = clickSample + 2000;
    const auto steadyBefore = rms (input, steadySample, 4096);
    const auto steadyAfter = rms (output, steadySample + latencySamples, 4096);

    CHECK (steadyAfter > 0.95f * steadyBefore);

    // What is left around the click should be roughly the sustained tone itself.
    CHECK (clickAfter < 1.2f * steadyAfter);
}

TEST_CASE ("Delta with zero depth is silent", "[dsp]")
{
    AdaptiveTransientShaper shaper;
    shaper.prepare (testSampleRate);

    const auto input = makeSine (2, 24000, 440.0f, 0.5f);
    auto output = input;

    TransientShaperParameters parameters;
    parameters.delta = true;

    processInBlocks (shaper, output, parameters);

    CHECK (rms (output, latencySamples, 24000 - latencySamples) < 1.0e-4f);
}

TEST_CASE ("Bypass returns the unprocessed signal", "[dsp]")
{
    AdaptiveTransientShaper shaper;
    shaper.prepare (testSampleRate);

    const auto input = makeClickySignal (24000);
    auto output = input;

    auto parameters = makeActiveCutParameters();
    parameters.bypass = true;

    processInBlocks (shaper, output, parameters);

    // Skip the bypass crossfade at the very start.
    CHECK (maxDifference (input, output, latencySamples, 4800) < 1.0e-5f);
}

TEST_CASE ("Mix at zero returns the unprocessed signal", "[dsp]")
{
    AdaptiveTransientShaper shaper;
    shaper.prepare (testSampleRate);

    const auto input = makeClickySignal (24000);
    auto output = input;

    auto parameters = makeActiveCutParameters();
    parameters.mix = 0.0f;

    processInBlocks (shaper, output, parameters);

    CHECK (maxDifference (input, output, latencySamples, 4800) < 1.0e-5f);
}

TEST_CASE ("Identical channels stay identical in stereo and mid/side modes", "[dsp]")
{
    const auto input = makeClickySignal (24000);

    auto stereoOutput = input;
    {
        AdaptiveTransientShaper shaper;
        shaper.prepare (testSampleRate);
        processInBlocks (shaper, stereoOutput, makeActiveCutParameters());
    }

    CHECK (maxDifferenceBetweenChannels (stereoOutput, latencySamples) < 1.0e-6f);

    auto midSideOutput = input;
    {
        AdaptiveTransientShaper shaper;
        shaper.prepare (testSampleRate);

        auto parameters = makeActiveCutParameters();
        parameters.midSide = true;
        processInBlocks (shaper, midSideOutput, parameters);
    }

    CHECK (maxDifferenceBetweenChannels (midSideOutput, latencySamples) < 1.0e-6f);

    // With zero depth the mid/side matrix must still be transparent.
    auto transparentOutput = input;
    {
        AdaptiveTransientShaper shaper;
        shaper.prepare (testSampleRate);

        TransientShaperParameters parameters;
        parameters.midSide = true;
        processInBlocks (shaper, transparentOutput, parameters);
    }

    CHECK (maxDifference (input, transparentOutput, latencySamples, 0) < 1.0e-4f);
}

TEST_CASE ("Extreme settings stay finite and bounded", "[dsp]")
{
    AdaptiveTransientShaper shaper;
    shaper.prepare (testSampleRate);

    juce::AudioBuffer<float> buffer (2, 48000);
    juce::Random random (99);

    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample (channel, sample, random.nextFloat() * 2.0f - 1.0f);

    TransientShaperParameters parameters;
    parameters.depth = 1.0f;
    parameters.sensitivity = 1.0f;
    parameters.sharpness = 0.0f;
    parameters.decay = 10.0f;
    parameters.decayLfHf = 0.0f;
    parameters.stereoLink = 0.0f;
    parameters.mix = 1.0f;
    parameters.trimDb = 24.0f;
    parameters.boost = true;
    parameters.midSide = true;

    processInBlocks (shaper, buffer, parameters);

    auto allFinite = true;
    auto peak = 0.0f;

    for (int channel = 0; channel < 2; ++channel)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = buffer.getSample (channel, sample);
            allFinite = allFinite && std::isfinite (value);
            peak = juce::jmax (peak, std::abs (value));
        }
    }

    CHECK (allFinite);
    CHECK (peak < 100.0f);
}

TEST_CASE ("Sensitivity curve weakens the cut", "[dsp]")
{
    const auto input = makeClickySignal (48000);

    auto flatOutput = input;
    {
        AdaptiveTransientShaper shaper;
        shaper.prepare (testSampleRate);
        processInBlocks (shaper, flatOutput, makeActiveCutParameters());
    }

    auto attenuatedOutput = input;
    {
        AdaptiveTransientShaper shaper;
        shaper.prepare (testSampleRate);

        auto parameters = makeActiveCutParameters();
        for (auto& band : parameters.curve.bands)
            band.gainDb = -12.0f;

        processInBlocks (shaper, attenuatedOutput, parameters);
    }

    constexpr int clickSample = 12000;
    const auto clickBefore = rms (input, clickSample, 2048);
    const auto clickFlat = rms (flatOutput, clickSample + latencySamples, 2048);
    const auto clickAttenuated = rms (attenuatedOutput, clickSample + latencySamples, 2048);

    // Both curves cut the click, but the attenuated curve cuts less.
    CHECK (clickFlat < 0.85f * clickBefore);
    CHECK (clickAttenuated > clickFlat);
}

TEST_CASE ("Reduction snapshot reports active bins", "[dsp]")
{
    AdaptiveTransientShaper shaper;
    shaper.prepare (testSampleRate);

    auto output = makeClickySignal (48000);
    const auto parameters = makeActiveCutParameters();

    std::array<float, AdaptiveTransientShaper::numBins> snapshot {};
    float peakReduction = 0.0f;

    for (int start = 0; start < output.getNumSamples(); start += 256)
    {
        const auto numSamples = juce::jmin (256, output.getNumSamples() - start);
        juce::AudioBuffer<float> block (output.getArrayOfWritePointers(), output.getNumChannels(), start, numSamples);
        shaper.process (block, parameters);
        shaper.copyReduction (snapshot);

        for (auto amount : snapshot)
            peakReduction = juce::jmax (peakReduction, amount);
    }

    CHECK (peakReduction > 0.1f);
}

TEST_CASE ("Reduction snapshot is silent at zero depth", "[dsp]")
{
    AdaptiveTransientShaper shaper;
    shaper.prepare (testSampleRate);

    auto output = makeClickySignal (24000);
    TransientShaperParameters parameters; // depth 0

    std::array<float, AdaptiveTransientShaper::numBins> snapshot {};
    float peakReduction = 0.0f;

    for (int start = 0; start < output.getNumSamples(); start += 256)
    {
        const auto numSamples = juce::jmin (256, output.getNumSamples() - start);
        juce::AudioBuffer<float> block (output.getArrayOfWritePointers(), output.getNumChannels(), start, numSamples);
        shaper.process (block, parameters);
        shaper.copyReduction (snapshot);

        for (auto amount : snapshot)
            peakReduction = juce::jmax (peakReduction, amount);
    }

    CHECK (peakReduction < 1.0e-6f);
}

TEST_CASE ("All resolutions stay finite and still process", "[dsp]")
{
    const auto input = makeClickySignal (24000);

    for (const auto resolutionHops : { 1, 2, 4 })
    {
        AdaptiveTransientShaper shaper;
        shaper.prepare (testSampleRate);

        auto output = input;
        auto parameters = makeActiveCutParameters();
        parameters.resolutionHops = resolutionHops;
        processInBlocks (shaper, output, parameters);

        // The decimated detector path must never produce garbage.
        auto allFinite = true;

        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            for (int sample = 0; sample < output.getNumSamples(); ++sample)
                allFinite = allFinite && std::isfinite (output.getSample (channel, sample));

        CHECK (allFinite);

        // The click is still reduced at every resolution.
        const auto clickBefore = rms (input, 12000, 2048);
        const auto clickAfter = rms (output, 12000 + latencySamples, 2048);
        CHECK (clickAfter < 0.95f * clickBefore);
    }
}

TEST_CASE ("All oversampling factors stay finite and still process", "[dsp]")
{
    const auto input = makeClickySignal (24000);

    for (const auto factor : { 1, 2, 4, 8 })
    {
        AdaptiveTransientShaper shaper;
        shaper.prepare (testSampleRate);

        auto output = input;
        auto parameters = makeActiveCutParameters();
        parameters.oversamplingFactor = factor;
        processInBlocks (shaper, output, parameters);

        const auto latency = shaper.getLatencySamples();

        auto allFinite = true;

        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            for (int sample = 0; sample < output.getNumSamples(); ++sample)
                allFinite = allFinite && std::isfinite (output.getSample (channel, sample));

        CHECK (allFinite);

        const auto clickBefore = rms (input, 12000, 2048);
        const auto clickAfter = rms (output, 12000 + latency, 2048);
        CHECK (clickAfter < 0.95f * clickBefore);
    }
}

TEST_CASE ("Minimum phase mode stays finite and still processes", "[dsp]")
{
    const auto input = makeClickySignal (24000);

    for (const auto minimumPhase : { false, true })
    {
        AdaptiveTransientShaper shaper;
        shaper.prepare (testSampleRate);

        auto output = input;
        auto parameters = makeActiveCutParameters();
        parameters.minimumPhase = minimumPhase;
        processInBlocks (shaper, output, parameters);

        auto allFinite = true;

        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            for (int sample = 0; sample < output.getNumSamples(); ++sample)
                allFinite = allFinite && std::isfinite (output.getSample (channel, sample));

        CHECK (allFinite);

        const auto clickBefore = rms (input, 12000, 2048);
        const auto clickAfter = rms (output, 12000 + latencySamples, 2048);
        CHECK (clickAfter < 0.95f * clickBefore);
    }
}


