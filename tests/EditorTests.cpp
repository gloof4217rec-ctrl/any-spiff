#include "helpers/test_helpers.h"

#include <PluginEditor.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

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

    // The band panel sliders are linear and drive the selected band's frequency,
    // gain and Q, plus the lo/hi cut frequencies.
    for (const auto& id : juce::StringArray { "band_freq_slider", "band_gain_slider", "band_q_slider",
                                              "locut_freq", "hicut_freq" })
    {
        auto* slider = dynamic_cast<juce::Slider*> (findChildByName (editor, id));
        REQUIRE (slider != nullptr);
        REQUIRE (slider->getSliderStyle() == juce::Slider::LinearHorizontal);
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

    // Band type and the two cut slopes are also four-entry combos.
    for (const auto& id : juce::StringArray { "band_type_combo", "locut_slope", "hicut_slope" })
    {
        auto* combo = dynamic_cast<juce::ComboBox*> (findChildByName (editor, id));
        REQUIRE (combo != nullptr);
        REQUIRE (combo->getNumItems() == 4);
    }

    // Band selector buttons.
    for (const auto& id : juce::StringArray { "band1_button", "band2_button", "band3_button" })
    {
        auto* button = dynamic_cast<juce::TextButton*> (findChildByName (editor, id));
        REQUIRE (button != nullptr);
    }

    // Three toggles: delta, bypass and the band enable.
    int numToggles = 0;

    for (auto* child : editor.getChildren())
        if (dynamic_cast<juce::ToggleButton*> (child) != nullptr)
            ++numToggles;

    REQUIRE (numToggles == 3);

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
    REQUIRE (editor.getHeight() == 680);
    REQUIRE (editor.isResizable());
}

TEST_CASE ("Band panel edits the selected band's parameters")
{
    EditorFixture fixture;
    auto& editor = fixture.get();
    auto& processor = fixture.processor;

    auto* freq = dynamic_cast<juce::Slider*> (findChildByName (editor, "band_freq_slider"));
    auto* q = dynamic_cast<juce::Slider*> (findChildByName (editor, "band_q_slider"));
    auto* on = dynamic_cast<juce::ToggleButton*> (findChildByName (editor, "band_on_button"));
    auto* band2 = dynamic_cast<juce::TextButton*> (findChildByName (editor, "band2_button"));
    auto* band3 = dynamic_cast<juce::TextButton*> (findChildByName (editor, "band3_button"));

    REQUIRE (freq != nullptr);
    REQUIRE (q != nullptr);
    REQUIRE (on != nullptr);
    REQUIRE (band2 != nullptr);
    REQUIRE (band3 != nullptr);

    // Default: band 1 is selected and its defaults are reflected.
    REQUIRE (juce::approximatelyEqual (freq->getValue(), 100.0));
    REQUIRE (q->getValue() == Catch::Approx (0.7).margin (0.01));
    REQUIRE (on->getToggleState());

    // Selecting band 2 refreshes the panel to band 2's values.
    band2->onClick();
    REQUIRE (juce::approximatelyEqual (freq->getValue(), 1000.0));

    // Moving a panel slider edits the selected band's parameter.
    freq->setValue (1500.0, juce::sendNotificationSync);
    REQUIRE (juce::approximatelyEqual (processor.apvts.getRawParameterValue ("band2_freq")->load(), 1500.0f));

    q->setValue (2.5, juce::sendNotificationSync);
    REQUIRE (juce::approximatelyEqual (processor.apvts.getRawParameterValue ("band2_q")->load(), 2.5f));

    // Switching to band 3 leaves band 2 untouched and exposes band 3's values.
    band3->onClick();
    REQUIRE (juce::approximatelyEqual (freq->getValue(), 6000.0));
    REQUIRE (juce::approximatelyEqual (processor.apvts.getRawParameterValue ("band2_freq")->load(), 1500.0f));

    // Toggling "On" disables the selected (band 3) band only.
    on->setToggleState (false, juce::dontSendNotification);
    on->onClick();
    REQUIRE (processor.apvts.getRawParameterValue ("band3_on")->load() < 0.5f);
    REQUIRE (processor.apvts.getRawParameterValue ("band2_on")->load() > 0.5f);
}

TEST_CASE ("Editor paints with the custom look and feel")
{
    EditorFixture fixture;
    auto& editor = fixture.get();

    // Force a full software paint of the editor and every child, exercising the
    // custom rotary/linear slider, button, toggle and combo drawing.
    juce::Image image (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true);
    {
        juce::Graphics g (image);
        editor.paintEntireComponent (g, false);
    }

    // The custom look and feel must actually draw something (not a blank image).
    REQUIRE (image.getPixelAt (image.getWidth() / 2, image.getHeight() / 2).getAlpha() > 0);
}
