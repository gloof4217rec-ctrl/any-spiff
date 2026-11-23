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

    processorRef.apvts.state.addListener (this);

    addAndMakeVisible (sensitivityGraph);
    sensitivityGraph.setName ("sensitivity_graph");
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
    setResizeLimits (640, 520, 1200, 900);
    setSize (760, 580);
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
