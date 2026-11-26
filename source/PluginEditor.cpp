#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), sensitivityGraph (p)
{
    addSlider ("depth", "Depth");
    addSlider ("sensitivity", "Sensitivity");
    addSlider ("sharpness", "Sharpness");
    addSlider ("decay", "Decay");
    addSlider ("decay_lfhf", "Decay LF/HF");
    addSlider ("stereo_link", "Stereo Link");
    addSlider ("balance", "Balance");
    addSlider ("mix", "Mix");
    addSlider ("trim", "Trim");

    setUpCombo (modeBox, modeLabel, "mode", "Mode", modeAttachment);
    setUpCombo (stereoModeBox, stereoModeLabel, "stereo_mode", "Stereo Mode", stereoModeAttachment);
    setUpCombo (resolutionBox, resolutionLabel, "resolution", "Resolution", resolutionAttachment);
    setUpCombo (oversampleBox, oversampleLabel, "oversample", "Oversample", oversampleAttachment);
    setUpCombo (phaseBox, phaseLabel, "phase_mode", "Phase", phaseAttachment);
    setUpCombo (windowBox, windowLabel, "window", "Window", windowAttachment);

    addAndMakeVisible (deltaButton);
    addAndMakeVisible (bypassButton);

    deltaAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.apvts, "delta", deltaButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.apvts, "bypass", bypassButton);

    // A/B comparison buttons.
    aButton.setName ("a_button");
    bButton.setName ("b_button");
    copyAbButton.setName ("copy_ab_button");

    aButton.setClickingTogglesState (true);
    bButton.setClickingTogglesState (true);

    addAndMakeVisible (aButton);
    addAndMakeVisible (bButton);
    addAndMakeVisible (copyAbButton);

    aButton.onClick = [this] { processorRef.switchToA(); updateAbButtons(); };
    bButton.onClick = [this] { processorRef.switchToB(); updateAbButtons(); };
    copyAbButton.onClick = [this] { processorRef.copyAToB(); updateAbButtons(); };

    updateAbButtons();

    // Undo/redo buttons.
    undoButton.setName ("undo_button");
    redoButton.setName ("redo_button");

    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);

    undoButton.onClick = [this] { processorRef.undo(); updateUndoRedoButtons(); };
    redoButton.onClick = [this] { processorRef.redo(); updateUndoRedoButtons(); };

    updateUndoRedoButtons();

    // Preset selector + save.
    presetBox.setName ("preset_box");
    presetBox.setTooltip ("Preset");
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();

        if (name.isNotEmpty())
            processorRef.loadPreset (name);
    };

    savePresetButton.setName ("save_preset_button");
    savePresetButton.onClick = [this]
    {
        const auto name = "User Preset " + juce::String (userPresetCounter++);
        processorRef.saveCurrentAsUserPreset (name);
        refreshPresetBox();
    };

    addAndMakeVisible (presetBox);
    addAndMakeVisible (savePresetButton);
    refreshPresetBox();

    processorRef.apvts.state.addListener (this);

    addAndMakeVisible (sensitivityGraph);
    sensitivityGraph.setName ("sensitivity_graph");

    // Band editor panel: frequency / Q / type / enable for the selected band,
    // plus the lo/hi cut controls. These write straight into the APVTS, so they
    // stay in sync with the graph and participate in undo/redo and presets.
    const auto addBandButton = [this] (juce::TextButton& button, const char* name, int band)
    {
        button.setName (name);
        button.setClickingTogglesState (true);
        button.onClick = [this, band] { sensitivityGraph.setSelectedBand (band); };
        addAndMakeVisible (button);
    };
    addBandButton (band1Button, "band1_button", 0);
    addBandButton (band2Button, "band2_button", 1);
    addBandButton (band3Button, "band3_button", 2);

    bandOnButton.setName ("band_on_button");
    bandOnButton.onClick = [this]
    {
        if (updatingBandPanel)
            return;

        setParamValue (bandParamId ("on"), bandOnButton.getToggleState() ? 1.0f : 0.0f);
    };
    addAndMakeVisible (bandOnButton);

    const auto configureBandSlider = [this] (juce::Slider& slider, juce::Label& label,
                                             const juce::String& labelText, const juce::String& suffix, const juce::String& rangeSource)
    {
        slider.setName ("band_" + suffix + "_slider");
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);

        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (processorRef.apvts.getParameter (rangeSource)))
        {
            const auto& range = param->getNormalisableRange();
            slider.setNormalisableRange ({ (double) range.start, (double) range.end, (double) range.interval, (double) range.skew });
        }

        slider.onValueChange = [this, &slider, suffix]
        {
            if (! updatingBandPanel)
                setParamValue (bandParamId (suffix), (float) slider.getValue());
        };

        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (11.0f));

        addAndMakeVisible (slider);
        addAndMakeVisible (label);
    };
    configureBandSlider (bandFreqSlider, bandFreqLabel, "Frequency", "freq", "band1_freq");
    configureBandSlider (bandGainSlider, bandGainLabel, "Gain", "gain", "band1_gain");
    configureBandSlider (bandQSlider, bandQLabel, "Q", "q", "band1_q");

    bandTypeCombo.setName ("band_type_combo");
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter ("band1_type")))
        bandTypeCombo.addItemList (choice->choices, 1);
    bandTypeCombo.onChange = [this]
    {
        if (! updatingBandPanel)
            setParamValue (bandParamId ("type"), (float) bandTypeCombo.getSelectedItemIndex());
    };
    bandTypeLabel.setText ("Type", juce::dontSendNotification);
    bandTypeLabel.setJustificationType (juce::Justification::centred);
    bandTypeLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (bandTypeCombo);
    addAndMakeVisible (bandTypeLabel);

    setUpPanelSlider (loCutFreqSlider, loCutFreqLabel, "Lo Cut", "locut_freq");
    setUpPanelCombo (loCutSlopeCombo, loCutSlopeLabel, "Lo Slope", "locut_slope");
    setUpPanelSlider (hiCutFreqSlider, hiCutFreqLabel, "Hi Cut", "hicut_freq");
    setUpPanelCombo (hiCutSlopeCombo, hiCutSlopeLabel, "Hi Slope", "hicut_slope");

    sensitivityGraph.onBandSelectionChanged = [this] { refreshBandPanel(); };
    refreshBandPanel();

    addAndMakeVisible (inspectButton);

    // this chunk of code instantiates and opens the melatonin inspector
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    setResizable (true, true);
    setResizeLimits (680, 640, 1200, 900);
    setSize (760, 680);
}

PluginEditor::~PluginEditor()
{
    processorRef.apvts.state.removeListener (this);
}

void PluginEditor::updateAbButtons()
{
    aButton.setToggleState (processorRef.isShowingA(), juce::dontSendNotification);
    bButton.setToggleState (! processorRef.isShowingA(), juce::dontSendNotification);
}

void PluginEditor::updateUndoRedoButtons()
{
    undoButton.setEnabled (processorRef.canUndo());
    redoButton.setEnabled (processorRef.canRedo());
}

void PluginEditor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    updateUndoRedoButtons();
    refreshBandPanel();
}

void PluginEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);

    const auto names = processorRef.getPresetNames();
    presetBox.addItemList (names, 1);

    if (names.size() > 0)
        presetBox.setSelectedItemIndex (0, juce::dontSendNotification);
}

PluginEditor::SliderControl& PluginEditor::addSlider (const char* parameterId, const juce::String& name)
{
    auto* control = sliderControls.add (new SliderControl());

    control->slider.setName (parameterId);
    control->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    control->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts,
                                                                                                  parameterId,
                                                                                                  control->slider);

    control->label.setText (name, juce::dontSendNotification);
    control->label.setJustificationType (juce::Justification::centred);
    control->label.setFont (juce::FontOptions (12.0f));

    addAndMakeVisible (control->slider);
    addAndMakeVisible (control->label);

    return *control;
}

void PluginEditor::setUpCombo (juce::ComboBox& combo,
                               juce::Label& label,
                               const char* parameterId,
                               const juce::String& name,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& attachment)
{
    // ComboBoxAttachment does not fill the box, so mirror the choice parameter's items here.
    combo.setName (parameterId);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (parameterId)))
    {
        combo.addItemList (choice->choices, 1);
        combo.setSelectedItemIndex (choice->getIndex(), juce::dontSendNotification);
    }

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processorRef.apvts,
                                                                                           parameterId,
                                                                                           combo);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (juce::FontOptions (12.0f));

    addAndMakeVisible (combo);
    addAndMakeVisible (label);
}

juce::String PluginEditor::bandParamId (const juce::String& suffix) const
{
    return "band" + juce::String (sensitivityGraph.getSelectedBand() + 1) + "_" + suffix;
}

float PluginEditor::getParamValue (const juce::String& parameterId) const
{
    if (auto* param = processorRef.apvts.getRawParameterValue (parameterId))
        return param->load();

    return 0.0f;
}

void PluginEditor::setParamValue (const juce::String& parameterId, float value)
{
    if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (processorRef.apvts.getParameter (parameterId)))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void PluginEditor::setUpPanelSlider (juce::Slider& slider, juce::Label& label, const juce::String& name, const juce::String& parameterId)
{
    slider.setName (parameterId);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);

    if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (processorRef.apvts.getParameter (parameterId)))
    {
        const auto& range = param->getNormalisableRange();
        slider.setNormalisableRange ({ (double) range.start, (double) range.end, (double) range.interval, (double) range.skew });
    }

    slider.setValue (getParamValue (parameterId), juce::dontSendNotification);
    slider.onValueChange = [this, &slider, parameterId]
    {
        if (! updatingBandPanel)
            setParamValue (parameterId, (float) slider.getValue());
    };

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.0f));

    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void PluginEditor::setUpPanelCombo (juce::ComboBox& combo, juce::Label& label, const juce::String& name, const juce::String& parameterId)
{
    combo.setName (parameterId);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (parameterId)))
        combo.addItemList (choice->choices, 1);

    combo.onChange = [this, &combo, parameterId]
    {
        if (! updatingBandPanel)
            setParamValue (parameterId, (float) combo.getSelectedItemIndex());
    };

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.0f));

    addAndMakeVisible (combo);
    addAndMakeVisible (label);
}

void PluginEditor::refreshBandPanel()
{
    updatingBandPanel = true;

    const auto band = sensitivityGraph.getSelectedBand();
    const auto prefix = "band" + juce::String (band + 1) + "_";

    band1Button.setToggleState (band == 0, juce::dontSendNotification);
    band2Button.setToggleState (band == 1, juce::dontSendNotification);
    band3Button.setToggleState (band == 2, juce::dontSendNotification);

    bandFreqSlider.setValue (getParamValue (prefix + "freq"), juce::dontSendNotification);
    bandGainSlider.setValue (getParamValue (prefix + "gain"), juce::dontSendNotification);
    bandQSlider.setValue (getParamValue (prefix + "q"), juce::dontSendNotification);
    bandTypeCombo.setSelectedItemIndex (juce::roundToInt (getParamValue (prefix + "type")), juce::dontSendNotification);
    bandOnButton.setToggleState (getParamValue (prefix + "on") > 0.5f, juce::dontSendNotification);

    loCutFreqSlider.setValue (getParamValue ("locut_freq"), juce::dontSendNotification);
    loCutSlopeCombo.setSelectedItemIndex (juce::roundToInt (getParamValue ("locut_slope")), juce::dontSendNotification);
    hiCutFreqSlider.setValue (getParamValue ("hicut_freq"), juce::dontSendNotification);
    hiCutSlopeCombo.setSelectedItemIndex (juce::roundToInt (getParamValue ("hicut_slope")), juce::dontSendNotification);

    updatingBandPanel = false;
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto area = getLocalBounds().reduced (12);
    auto titleArea = area.removeFromTop (30);

    const auto textColour = getLookAndFeel().findColour (juce::Label::textColourId);

    g.setColour (textColour);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText (PRODUCT_NAME_WITHOUT_VERSION,
                titleArea.removeFromLeft (titleArea.getWidth() * 2 / 3),
                juce::Justification::centredLeft,
                true);

    g.setColour (textColour.withAlpha (0.6f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String ("v" VERSION "  \xc2\xb7  adaptive transient shaper"),
                titleArea,
                juce::Justification::centredRight,
                true);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (46);     // title
    sensitivityGraph.setBounds (area.removeFromTop (150));

    // Band editor panel: band selector + per-band controls (top row) and the
    // lo/hi cut controls (bottom row).
    auto bandPanel = area.removeFromTop (80);
    auto bandRow = bandPanel.removeFromTop (40);

    band1Button.setBounds (bandRow.removeFromLeft (28));
    bandRow.removeFromLeft (3);
    band2Button.setBounds (bandRow.removeFromLeft (28));
    bandRow.removeFromLeft (3);
    band3Button.setBounds (bandRow.removeFromLeft (28));
    bandRow.removeFromLeft (8);
    bandOnButton.setBounds (bandRow.removeFromLeft (40));

    const auto layoutLabelOnTop = [] (juce::Rectangle<int> r, juce::Label& l, juce::Component& c)
    {
        l.setBounds (r.removeFromTop (12));
        c.setBounds (r);
    };

    bandRow.removeFromLeft (8);
    layoutLabelOnTop (bandRow.removeFromLeft (150), bandFreqLabel, bandFreqSlider);
    bandRow.removeFromLeft (6);
    layoutLabelOnTop (bandRow.removeFromLeft (104), bandGainLabel, bandGainSlider);
    bandRow.removeFromLeft (6);
    layoutLabelOnTop (bandRow.removeFromLeft (104), bandQLabel, bandQSlider);
    bandRow.removeFromLeft (6);
    layoutLabelOnTop (bandRow, bandTypeLabel, bandTypeCombo);

    auto cutRow = bandPanel;
    const auto columnWidth = cutRow.getWidth() / 4;
    layoutLabelOnTop (cutRow.removeFromLeft (columnWidth), loCutFreqLabel, loCutFreqSlider);
    cutRow.removeFromLeft (4);
    layoutLabelOnTop (cutRow.removeFromLeft (columnWidth), loCutSlopeLabel, loCutSlopeCombo);
    cutRow.removeFromLeft (4);
    layoutLabelOnTop (cutRow.removeFromLeft (columnWidth), hiCutFreqLabel, hiCutFreqSlider);
    cutRow.removeFromLeft (4);
    layoutLabelOnTop (cutRow, hiCutSlopeLabel, hiCutSlopeCombo);

    area.removeFromTop (12);

    auto bottomRow = area.removeFromBottom (24);
    aButton.setBounds (bottomRow.removeFromLeft (30));
    bButton.setBounds (bottomRow.removeFromLeft (30));
    bottomRow.removeFromLeft (4);
    copyAbButton.setBounds (bottomRow.removeFromLeft (56));
    bottomRow.removeFromLeft (12);
    undoButton.setBounds (bottomRow.removeFromLeft (60));
    bottomRow.removeFromLeft (4);
    redoButton.setBounds (bottomRow.removeFromLeft (60));
    presetBox.setBounds (bottomRow.removeFromRight (180));
    savePresetButton.setBounds (bottomRow.removeFromRight (50));
    inspectButton.setBounds (bottomRow.removeFromRight (120));
    area.removeFromBottom (30);

    auto advancedRow = area.removeFromBottom (46);
    area.removeFromBottom (8);
    auto switches = area.removeFromBottom (46);
    area.removeFromBottom (12);

    constexpr int columns = 5;
    const auto cellWidth = area.getWidth() / columns;
    const auto cellHeight = juce::jlimit (70, 120, area.getHeight() / 2);

    auto row = area.removeFromTop (cellHeight);

    for (int i = 0; i < sliderControls.size(); ++i)
    {
        if (i == columns)
            row = area.removeFromTop (cellHeight);

        const auto column = i % columns;
        auto cell = juce::Rectangle<int> (row.getX() + column * cellWidth, row.getY(),
                                          cellWidth, row.getHeight());

        auto& control = *sliderControls[i];
        control.label.setBounds (cell.removeFromTop (18));
        control.slider.setBounds (cell.reduced (4));
    }

    // Main row: mode, stereo mode, delta, bypass.
    const auto mainComboWidth = juce::jmin (160, switches.getWidth() / 4);

    auto modeArea = switches.removeFromLeft (mainComboWidth);
    modeLabel.setBounds (modeArea.removeFromTop (16));
    modeBox.setBounds (modeArea.removeFromTop (26));

    switches.removeFromLeft (12);

    auto stereoArea = switches.removeFromLeft (mainComboWidth);
    stereoModeLabel.setBounds (stereoArea.removeFromTop (16));
    stereoModeBox.setBounds (stereoArea.removeFromTop (26));

    switches.removeFromLeft (12);

    deltaButton.setBounds (switches.removeFromLeft (80).withSizeKeepingCentre (80, 24));
    switches.removeFromLeft (8);
    bypassButton.setBounds (switches.removeFromLeft (80).withSizeKeepingCentre (80, 24));

    // Advanced row: resolution, oversample, phase mode, window.
    const auto advancedComboWidth = juce::jmin (140, advancedRow.getWidth() / 4);

    auto resolutionArea = advancedRow.removeFromLeft (advancedComboWidth);
    resolutionLabel.setBounds (resolutionArea.removeFromTop (16));
    resolutionBox.setBounds (resolutionArea.removeFromTop (26));

    advancedRow.removeFromLeft (8);

    auto oversampleArea = advancedRow.removeFromLeft (advancedComboWidth);
    oversampleLabel.setBounds (oversampleArea.removeFromTop (16));
    oversampleBox.setBounds (oversampleArea.removeFromTop (26));

    advancedRow.removeFromLeft (8);

    auto phaseArea = advancedRow.removeFromLeft (advancedComboWidth);
    phaseLabel.setBounds (phaseArea.removeFromTop (16));
    phaseBox.setBounds (phaseArea.removeFromTop (26));

    advancedRow.removeFromLeft (8);

    auto windowArea = advancedRow.removeFromLeft (advancedComboWidth);
    windowLabel.setBounds (windowArea.removeFromTop (16));
    windowBox.setBounds (windowArea.removeFromTop (26));
}
