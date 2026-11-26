#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "dsp/SensitivityCurve.h"

/** The sensitivity graph: draws the detector weighting curve and lets the user
    drag three band nodes (frequency x gain) plus the lo/hi cut handles.

    Clicking a node selects its band; double-clicking toggles the band on/off
    (a frequency-range bypass). Cmd/Ctrl-dragging a node edits its Q instead of
    its gain, and Alt-dragging temporarily enables band listen, so the user hears
    only the delta signal in that band while hunting for a frequency. A second,
    live curve shows the actual per-frequency reduction.
*/
class SensitivityCurveComponent : public juce::Component,
                                  public juce::Timer
{
public:
    explicit SensitivityCurveComponent (PluginProcessor& processor);
    ~SensitivityCurveComponent() override;

    /** The band currently selected (0..2). The editor's band panel edits this band. */
    int getSelectedBand() const noexcept { return selectedBand; }
    void setSelectedBand (int band);

    /** Called (on the message thread) whenever the selected band changes. */
    std::function<void()> onBandSelectionChanged;

    void paint (juce::Graphics&) override;
    void timerCallback() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    enum class DragTarget
    {
        none,
        band,
        loCut,
        hiCut
    };

    struct BandIds
    {
        const char* freq;
        const char* gain;
        const char* q;
        const char* type;
        const char* on;
    };

    static constexpr std::array<BandIds, 3> bandIds
    {
        { { "band1_freq", "band1_gain", "band1_q", "band1_type", "band1_on" },
          { "band2_freq", "band2_gain", "band2_q", "band2_type", "band2_on" },
          { "band3_freq", "band3_gain", "band3_q", "band3_type", "band3_on" } }
    };

    juce::Rectangle<int> getPlotBounds() const;

    float frequencyToX (float hz) const;
    float xToFrequency (float x) const;
    float gainToY (float db) const;
    float yToGain (float y) const;
    float qToY (float q) const;
    float yToQ (float y) const;

    float readParameter (const char* id) const;
    void writeParameter (const char* id, float value);

    sensitivity::Parameters currentCurve() const;
    void drawCurve (juce::Graphics&, const sensitivity::Parameters&, double sampleRate);
    void drawReduction (juce::Graphics&, const sensitivity::Parameters&, double sampleRate);
    void drawNodes (juce::Graphics&, const sensitivity::Parameters&);

    DragTarget findDragTarget (const juce::Point<int>& position, int& bandIndex) const;

    PluginProcessor& processor;

    std::array<float, AdaptiveTransientShaper::maxNumBins> reduction {};

    DragTarget dragTarget = DragTarget::none;
    int dragBand = -1;
    bool listening = false;

    int selectedBand = 0;
    bool draggingQ = false;
    float dragQValue = 1.0f;

    static constexpr float frequencyMinHz = 20.0f;
    static constexpr float frequencyMaxHz = 20000.0f;
    static constexpr float gainMinDb = -24.0f;
    static constexpr float gainMaxDb = 12.0f;
    static constexpr float qMin = 0.1f;
    static constexpr float qMax = 10.0f;
    static constexpr float nodeRadius = 6.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SensitivityCurveComponent)
};
