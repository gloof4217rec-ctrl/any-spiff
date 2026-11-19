#include <dsp/AdaptiveTransientShaper.h>

TEST_CASE ("Boot performance")
{
    BENCHMARK_ADVANCED ("Processor constructor")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<Catch::Benchmark::storage_for<PluginProcessor>> storage (size_t (meter.runs()));
        meter.measure ([&] (int i) { storage[(size_t) i].construct(); });
    };

    BENCHMARK_ADVANCED ("Processor destructor")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<Catch::Benchmark::destructable_object<PluginProcessor>> storage (size_t (meter.runs()));
        for (auto& s : storage)
            s.construct();
        meter.measure ([&] (int i) { storage[(size_t) i].destruct(); });
    };

    BENCHMARK_ADVANCED ("Editor open and close")
    (Catch::Benchmark::Chronometer meter)
    {
        PluginProcessor plugin;

        // due to complex construction logic of the editor, let's measure open/close together
        meter.measure ([&] (int /* i */) {
            auto editor = plugin.createEditorAndMakeActive();
            plugin.editorBeingDeleted (editor);
            delete editor;
            return plugin.getActiveEditor();
        });
    };
}

TEST_CASE ("DSP performance")
{
    AdaptiveTransientShaper shaper;
    shaper.prepare (48000.0);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::Random random (42);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample (channel, sample, random.nextFloat() * 2.0f - 1.0f);

    TransientShaperParameters parameters;
    parameters.depth = 1.0f;
    parameters.sensitivity = 0.5f;

    // Ten blocks of 512 samples at 48 kHz == ~106 ms of stereo audio.
    BENCHMARK ("Adaptive shaper, 10 x 512 stereo samples")
    {
        for (int block = 0; block < 10; ++block)
            shaper.process (buffer, parameters);

        return buffer.getRMSLevel (0, 0, buffer.getNumSamples());
    };
}
