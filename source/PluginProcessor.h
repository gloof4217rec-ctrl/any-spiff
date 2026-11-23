#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/AdaptiveTransientShaper.h"

class PluginProcessor : public juce::AudioProcessor,
                        public juce::AudioProcessorValueTreeState::Listener,
                        private juce::Timer
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Keep the double-precision overload visible (it is not overridden).
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    /** Message-thread hooks used by the sensitivity graph while Alt-dragging a
        band node (band listen). The audio thread only reads the atomics.
    */
    void beginBandListen (float frequencyHz, float q);
    void endBandListen();

    /** A/B comparison: two parameter snapshots the editor can switch between. */
    void switchToA();
    void switchToB();
    void copyAToB();
    bool isShowingA() const noexcept { return showingA; }

    /** Undo/redo: a snapshot history of parameter edits. */
    void undo();
    void redo();
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }

    /** Presets: factory presets plus user presets persisted to disk. */
    juce::StringArray getPresetNames() const;
    void loadPreset (const juce::String& name);
    void saveCurrentAsUserPreset (const juce::String& name);
    void deleteUserPreset (const juce::String& name);

    /** Copies the current per-bin processing amount (0..1) for the reduction graph. */
    void getReductionSnapshot (std::array<float, AdaptiveTransientShaper::maxNumBins>& destination) const;

    /** The number of active detector bins (depends on the Window parameter). */
    int getNumBins() const noexcept { return transientShaper.getNumBins(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void cacheParameterPointers();

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;
    void noteParameterChange();

    juce::ValueTree buildPresetState (const std::vector<std::pair<juce::String, float>>& overrides);
    juce::ValueTree findPresetByName (const juce::String& name) const;
    void loadUserPresetsFromDisk();
    void writeUserPresetsToDisk() const;

    AdaptiveTransientShaper transientShaper;

    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* depthParam = nullptr;
    std::atomic<float>* sensitivityParam = nullptr;
    std::atomic<float>* sharpnessParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* decayLfHfParam = nullptr;
    std::atomic<float>* stereoModeParam = nullptr;
    std::atomic<float>* stereoLinkParam = nullptr;
    std::atomic<float>* balanceParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* trimParam = nullptr;
    std::atomic<float>* deltaParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* resolutionParam = nullptr;
    std::atomic<float>* oversampleParam = nullptr;
    std::atomic<float>* phaseModeParam = nullptr;
    std::atomic<float>* windowParam = nullptr;

    std::atomic<float>* bandListenParam = nullptr;
    std::atomic<float>* band1FreqParam = nullptr;
    std::atomic<float>* band1GainParam = nullptr;
    std::atomic<float>* band1QParam = nullptr;
    std::atomic<float>* band1TypeParam = nullptr;
    std::atomic<float>* band1OnParam = nullptr;
    std::atomic<float>* band2FreqParam = nullptr;
    std::atomic<float>* band2GainParam = nullptr;
    std::atomic<float>* band2QParam = nullptr;
    std::atomic<float>* band2TypeParam = nullptr;
    std::atomic<float>* band2OnParam = nullptr;
    std::atomic<float>* band3FreqParam = nullptr;
    std::atomic<float>* band3GainParam = nullptr;
    std::atomic<float>* band3QParam = nullptr;
    std::atomic<float>* band3TypeParam = nullptr;
    std::atomic<float>* band3OnParam = nullptr;
    std::atomic<float>* loCutFreqParam = nullptr;
    std::atomic<float>* loCutSlopeParam = nullptr;
    std::atomic<float>* hiCutFreqParam = nullptr;
    std::atomic<float>* hiCutSlopeParam = nullptr;

    std::atomic<float> bandListenFrequency { 1000.0f };
    std::atomic<float> bandListenQ { 1.0f };

    juce::ValueTree stateA;
    juce::ValueTree stateB;
    bool showingA = true;

    std::vector<juce::ValueTree> undoStack;
    std::vector<juce::ValueTree> redoStack;
    juce::ValueTree lastCommitted;
    bool gestureActive = false;
    bool ignoreChanges = false;

    std::vector<std::pair<juce::String, juce::ValueTree>> factoryPresets;
    juce::ValueTree userPresets { "USER_PRESETS" };
    juce::File userPresetsFile;

    int lastReportedLatency = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
