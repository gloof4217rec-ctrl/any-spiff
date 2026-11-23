#include "helpers/test_helpers.h"

#include <PluginEditor.h>

#include <catch2/catch_test_macros.hpp>

namespace
{
    juce::Component* findChildByName (juce::Component& parent, const juce::String& name)
    {
        for (auto* child : parent.getChildren())
            if (child->getName() == name)
                return child;

        return nullptr;
    }

    // Owns a processor plus an active editor, and tells the processor when the
    // editor goes away (JUCE asserts if the editor is deleted behind its back).
    struct EditorFixture
    {
        EditorFixture() : editor (processor.createEditorAndMakeActive()) {}

        ~EditorFixture()
        {
            processor.editorBeingDeleted (editor);
            delete editor;

            // Headless Linux: nothing pumps the message socket queue while the Catch
            // session runs synchronously, so a few editors' worth of repaint/async
            // messages would overflow it. Drain it after each editor test.
            if (auto* mm = juce::MessageManager::getInstance())
            {
                mm->setCurrentThreadAsMessageThread();
                mm->runDispatchLoopUntil (5);
            }
        }

        juce::AudioProcessorEditor& get() const { return *editor; }

        PluginProcessor processor;
        juce::AudioProcessorEditor* editor = nullptr;
    };
}

TEST_CASE ("Editor shows every parameter")
{
    EditorFixture fixture;
    auto& editor = fixture.get();

    // Nine rotary controls, each bound to its parameter via a SliderAttachment.
    const juce::StringArray sliderIds { "depth", "sensitivity", "sharpness", "decay", "decay_lfhf",
                                        "stereo_link", "balance", "mix", "trim" };

    for (const auto& id : sliderIds)
    {
        auto* slider = dynamic_cast<juce::Slider*> (findChildByName (editor, id));
        REQUIRE (slider != nullptr);
        REQUIRE (slider->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag);
        REQUIRE (slider->isEnabled());
    }

    // The two choice parameters need their items filled in by the editor.
    for (const auto& id : juce::StringArray { "mode", "stereo_mode" })
    {
        auto* combo = dynamic_cast<juce::ComboBox*> (findChildByName (editor, id));
        REQUIRE (combo != nullptr);
        REQUIRE (combo->getNumItems() == 2);
    }

    auto* resolutionCombo = dynamic_cast<juce::ComboBox*> (findChildByName (editor, "resolution"));
    REQUIRE (resolutionCombo != nullptr);
    REQUIRE (resolutionCombo->getNumItems() == 3);

    auto* oversampleCombo = dynamic_cast<juce::ComboBox*> (findChildByName (editor, "oversample"));
    REQUIRE (oversampleCombo != nullptr);
    REQUIRE (oversampleCombo->getNumItems() == 4);

    auto* phaseCombo = dynamic_cast<juce::ComboBox*> (findChildByName (editor, "phase_mode"));
    REQUIRE (phaseCombo != nullptr);
    REQUIRE (phaseCombo->getNumItems() == 2);

    auto* windowCombo = dynamic_cast<juce::ComboBox*> (findChildByName (editor, "window"));
    REQUIRE (windowCombo != nullptr);
    REQUIRE (windowCombo->getNumItems() == 3);

    // Two toggles, found by their text rather than by ID.
    int numToggles = 0;

    for (auto* child : editor.getChildren())
        if (dynamic_cast<juce::ToggleButton*> (child) != nullptr)
            ++numToggles;

    REQUIRE (numToggles == 2);

    // The sensitivity graph is present and laid out.
    auto* graph = dynamic_cast<SensitivityCurveComponent*> (findChildByName (editor, "sensitivity_graph"));
    REQUIRE (graph != nullptr);
    REQUIRE (graph->isVisible());
    REQUIRE (graph->getWidth() > 0);
    REQUIRE (graph->getHeight() > 0);

    // A/B comparison, undo/redo and preset buttons.
    for (const auto& id : juce::StringArray { "a_button", "b_button", "copy_ab_button", "undo_button", "redo_button", "save_preset_button" })
    {
        auto* button = dynamic_cast<juce::TextButton*> (findChildByName (editor, id));
        REQUIRE (button != nullptr);
    }

    auto* presetBox = dynamic_cast<juce::ComboBox*> (findChildByName (editor, "preset_box"));
    REQUIRE (presetBox != nullptr);
    REQUIRE (presetBox->getNumItems() > 0);
}

TEST_CASE ("Editor controls follow the parameters")
{
    EditorFixture fixture;
    auto& editor = fixture.get();

    auto* depth = dynamic_cast<juce::Slider*> (findChildByName (editor, "depth"));
    auto* trim = dynamic_cast<juce::Slider*> (findChildByName (editor, "trim"));
    auto* mode = dynamic_cast<juce::ComboBox*> (findChildByName (editor, "mode"));

    REQUIRE (depth != nullptr);
    REQUIRE (trim != nullptr);
    REQUIRE (mode != nullptr);

    // Defaults straight from the parameter layout.
    REQUIRE (std::abs (depth->getValue()) < 0.01);
    REQUIRE (std::abs (trim->getValue()) < 0.01);
    REQUIRE (mode->getSelectedItemIndex() == 0);

    // Moving a parameter updates the control through the attachment.
    if (auto* depthParam = fixture.processor.apvts.getParameter ("depth"))
        depthParam->setValueNotifyingHost (depthParam->convertTo0to1 (75.0f));

    if (auto* modeParam = fixture.processor.apvts.getParameter ("mode"))
        modeParam->setValueNotifyingHost (modeParam->convertTo0to1 (1.0f));

    REQUIRE (juce::approximatelyEqual (depth->getValue(), 75.0));
    REQUIRE (mode->getSelectedItemIndex() == 1);

    // ...and moving a control updates the parameter.
    depth->setValue (25.0, juce::sendNotificationSync);

    REQUIRE (juce::approximatelyEqual (fixture.processor.apvts.getRawParameterValue ("depth")->load(), 25.0f));
}

TEST_CASE ("Editor respects its size limits")
{
    EditorFixture fixture;
    auto& editor = fixture.get();

    REQUIRE (editor.getWidth() == 760);
    REQUIRE (editor.getHeight() == 580);
    REQUIRE (editor.isResizable());
}
