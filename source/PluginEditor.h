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
class PluginEditor : public juce::AudioProcessorEditor
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

    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };

    juce::OwnedArray<SliderControl> sliderControls;

    SensitivityCurveComponent sensitivityGraph;

    juce::ComboBox modeBox;
    juce::ComboBox stereoModeBox;
    juce::ComboBox resolutionBox;
    juce::ComboBox oversampleBox;
    juce::ComboBox phaseBox;
    juce::Label modeLabel;
    juce::Label stereoModeLabel;
    juce::Label resolutionLabel;
    juce::Label oversampleLabel;
    juce::Label phaseLabel;

    juce::ToggleButton deltaButton { "Delta" };
    juce::ToggleButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stereoModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> resolutionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversampleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> phaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> deltaAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
