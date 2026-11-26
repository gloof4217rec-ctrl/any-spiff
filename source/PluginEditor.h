#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "SensitivityCurveComponent.h"

//==============================================================================
/** The plugin window: nine rotary controls plus mode/stereo switches.

    All controls are bound to the APVTS with attachments, so the editor itself
    never touches the DSP or the parameter values directly.
*/
class PluginEditor : public juce::AudioProcessorEditor,
                     public juce::ValueTree::Listener
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct SliderControl
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    SliderControl& addSlider (const char* parameterId, const juce::String& name);

    void setUpCombo (juce::ComboBox& combo,
                     juce::Label& label,
                     const char* parameterId,
                     const juce::String& name,
                     std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& attachment);

    void updateAbButtons();
    void updateUndoRedoButtons();
    void refreshPresetBox();
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    // Band editor panel helpers.
    void refreshBandPanel();
    juce::String bandParamId (const juce::String& suffix) const;
    float getParamValue (const juce::String& parameterId) const;
    void setParamValue (const juce::String& parameterId, float value);
    void setUpPanelSlider (juce::Slider& slider, juce::Label& label, const juce::String& name, const juce::String& parameterId);
    void setUpPanelCombo (juce::ComboBox& combo, juce::Label& label, const juce::String& name, const juce::String& parameterId);

    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };

    juce::TextButton aButton { "A" };
    juce::TextButton bButton { "B" };
    juce::TextButton copyAbButton { juce::String::fromUTF8 ("A\xE2\x86\x92" "B") };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };

    juce::ComboBox presetBox;
    juce::TextButton savePresetButton { "Save" };
    int userPresetCounter = 1;

    juce::OwnedArray<SliderControl> sliderControls;

    SensitivityCurveComponent sensitivityGraph;

    juce::ComboBox modeBox;
    juce::ComboBox stereoModeBox;
    juce::ComboBox resolutionBox;
    juce::ComboBox oversampleBox;
    juce::ComboBox phaseBox;
    juce::ComboBox windowBox;
    juce::Label modeLabel;
    juce::Label stereoModeLabel;
    juce::Label resolutionLabel;
    juce::Label oversampleLabel;
    juce::Label phaseLabel;
    juce::Label windowLabel;

    juce::ToggleButton deltaButton { "Delta" };
    juce::ToggleButton bypassButton { "Bypass" };

    // Band editor panel: lets the user pick, for each of the three detector
    // bands, the frequency range it acts on, its Q, its filter type and whether
    // it is enabled (the Pro-Q style "point" controls).
    juce::TextButton band1Button { "1" };
    juce::TextButton band2Button { "2" };
    juce::TextButton band3Button { "3" };
    juce::ToggleButton bandOnButton { "On" };

    juce::Slider bandFreqSlider;
    juce::Slider bandGainSlider;
    juce::Slider bandQSlider;
    juce::Label bandFreqLabel;
    juce::Label bandGainLabel;
    juce::Label bandQLabel;
    juce::Label bandTypeLabel;
    juce::ComboBox bandTypeCombo;

    juce::Slider loCutFreqSlider;
    juce::Label loCutFreqLabel;
    juce::ComboBox loCutSlopeCombo;
    juce::Label loCutSlopeLabel;

    juce::Slider hiCutFreqSlider;
    juce::Label hiCutFreqLabel;
    juce::ComboBox hiCutSlopeCombo;
    juce::Label hiCutSlopeLabel;

    bool updatingBandPanel = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stereoModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> resolutionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversampleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> phaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> windowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> deltaAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
