#include "SculptorLookAndFeel.h"

namespace
{
    // Warm, dark "studio" palette with a champagne-gold accent.
    const juce::Colour windowBg { 0xff121216 };
    const juce::Colour panelBg { 0xff191920 };
    const juce::Colour raisedBg { 0xff202028 };
    const juce::Colour border { 0xff2b2b34 };
    const juce::Colour textPrimary { 0xffe8e9ee };
    const juce::Colour textSecondary { 0xff8b8c96 };
    const juce::Colour accent { 0xffd9a441 };
    const juce::Colour accentBright { 0xfff0c25f };
    const juce::Colour track { 0xff26262e };
    const juce::Colour onText { 0xff141419 };
}

SculptorLookAndFeel::SculptorLookAndFeel()
{
    // Window & labels.
    setColour (juce::ResizableWindow::backgroundColourId, windowBg);
    setColour (juce::Label::textColourId, textPrimary);

    // Buttons.
    setColour (juce::TextButton::buttonColourId, raisedBg);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, textPrimary);
    setColour (juce::TextButton::textColourOnId, onText);

    // Toggles.
    setColour (juce::ToggleButton::textColourId, textPrimary);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::ToggleButton::tickDisabledColourId, border);

    // Combo boxes.
    setColour (juce::ComboBox::backgroundColourId, raisedBg);
    setColour (juce::ComboBox::textColourId, textPrimary);
    setColour (juce::ComboBox::outlineColourId, border);
    setColour (juce::ComboBox::arrowColourId, textSecondary);
    setColour (juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);

    // Popup menus.
    setColour (juce::PopupMenu::backgroundColourId, panelBg);
    setColour (juce::PopupMenu::textColourId, textPrimary);
    setColour (juce::PopupMenu::headerTextColourId, textSecondary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, textPrimary);

    // Sliders.
    setColour (juce::Slider::backgroundColourId, track);
    setColour (juce::Slider::thumbColourId, accent);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, border);
    setColour (juce::Slider::textBoxTextColourId, textSecondary);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, accent);

    // Tooltips.
    setColour (juce::TooltipWindow::backgroundColourId, raisedBg);
    setColour (juce::TooltipWindow::textColourId, textPrimary);
    setColour (juce::TooltipWindow::outlineColourId, border);
}

void SculptorLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider&)
{
    const auto radius = juce::jmin ((float) width, (float) height) * 0.5f - 3.0f;
    const auto centreX = (float) x + (float) width * 0.5f;
    const auto centreY = (float) y + (float) height * 0.5f;
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto thickness = juce::jmax (2.5f, radius * 0.15f);

    // Inactive track.
    juce::Path trackPath;
    trackPath.addArc (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f,
                      rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (track);
    g.strokePath (trackPath, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    // Active value arc.
    juce::Path valuePath;
    valuePath.addArc (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f,
                      rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (valuePath, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    // Knob face with a soft vertical gradient for depth.
    const auto faceRadius = juce::jmax (2.0f, radius - thickness);
    g.setGradientFill (juce::ColourGradient (raisedBg.brighter (0.06f), centreX, centreY - faceRadius,
                                             panelBg, centreX, centreY + faceRadius, false));
    g.fillEllipse (centreX - faceRadius, centreY - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);
    g.setColour (border);
    g.drawEllipse (centreX - faceRadius, centreY - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f, 1.0f);

    // Pointer.
    const auto pointerLength = faceRadius * 0.72f;
    const auto pointerX = centreX + pointerLength * std::sin (angle);
    const auto pointerY = centreY - pointerLength * std::cos (angle);
    g.setColour (accentBright);
    g.drawLine (centreX, centreY, pointerX, pointerY, juce::jmax (1.5f, faceRadius * 0.14f));
    g.fillEllipse (centreX - 2.0f, centreY - 2.0f, 4.0f, 4.0f);
}

void SculptorLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float minSliderPos, float maxSliderPos,
                                            juce::Slider::SliderStyle style, juce::Slider&)
{
    const auto isHorizontal = style == juce::Slider::LinearHorizontal
                              || style == juce::Slider::LinearBar
                              || style == juce::Slider::TwoValueHorizontal
                              || style == juce::Slider::ThreeValueHorizontal;

    const auto trackThickness = 4.0f;
    const auto thumbRadius = 7.0f;

    if (isHorizontal)
    {
        const auto centreY = (float) y + (float) height * 0.5f;
        const auto trackRect = juce::Rectangle<float> (minSliderPos, centreY - trackThickness * 0.5f,
                                                       maxSliderPos - minSliderPos, trackThickness);

        g.setColour (track);
        g.fillRoundedRectangle (trackRect, trackThickness * 0.5f);

        const auto fillFrom = juce::jmin (minSliderPos, sliderPos);
        const auto fillTo = juce::jmax (minSliderPos, sliderPos);
        g.setColour (accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (fillFrom, trackRect.getY(), fillTo - fillFrom, trackThickness),
                                trackThickness * 0.5f);
    }
    else
    {
        const auto centreX = (float) x + (float) width * 0.5f;
        const auto trackRect = juce::Rectangle<float> (centreX - trackThickness * 0.5f, minSliderPos,
                                                       trackThickness, maxSliderPos - minSliderPos);

        g.setColour (track);
        g.fillRoundedRectangle (trackRect, trackThickness * 0.5f);

        const auto fillFrom = juce::jmin (minSliderPos, sliderPos);
        const auto fillTo = juce::jmax (minSliderPos, sliderPos);
        g.setColour (accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (trackRect.getX(), fillFrom, trackThickness, fillTo - fillFrom),
                                trackThickness * 0.5f);
    }

    // Thumb.
    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearVertical)
    {
        const auto thumbX = isHorizontal ? sliderPos : (float) x + (float) width * 0.5f;
        const auto thumbY = isHorizontal ? (float) y + (float) height * 0.5f : sliderPos;

        g.setColour (accentBright);
        g.fillEllipse (thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);
        g.setColour (border);
        g.drawEllipse (thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f, 1.0f);
    }
}

void SculptorLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto isOn = button.getToggleState();
    const auto corner = bounds.getHeight() * 0.5f;

    auto base = isOn ? accent : raisedBg;
    if (shouldDrawButtonAsDown)
        base = base.darker (0.12f);
    else if (shouldDrawButtonAsHighlighted)
        base = base.brighter (0.07f);

    g.setColour (base);
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (border);
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    g.setColour (isOn ? onText : (button.isEnabled() ? textPrimary : textSecondary.withAlpha (0.5f)));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred, false);
}

void SculptorLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto isOn = button.getToggleState();

    auto base = isOn ? accent : backgroundColour;
    if (shouldDrawButtonAsDown)
        base = base.darker (0.12f);
    else if (shouldDrawButtonAsHighlighted)
        base = base.brighter (0.07f);

    const auto corner = juce::jmin (7.0f, bounds.getHeight() * 0.3f);

    g.setColour (base);
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (border);
    g.drawRoundedRectangle (bounds, corner, 1.0f);
}

void SculptorLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    if (! button.isEnabled())
        g.setColour (textSecondary.withAlpha (0.45f));
    else if (button.getToggleState())
        g.setColour (onText);
    else
        g.setColour (textPrimary);

    g.setFont (juce::FontOptions (12.0f));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

void SculptorLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                        int buttonX, int buttonY, int buttonW, int buttonH,
                                        juce::ComboBox&)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);

    g.setColour (raisedBg);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (border);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    // Small chevron.
    const auto centreX = (float) buttonX + (float) buttonW * 0.5f;
    const auto centreY = (float) buttonY + (float) buttonH * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (centreX - 3.5f, centreY - 2.0f,
                       centreX + 3.5f, centreY - 2.0f,
                       centreX, centreY + 3.0f);
    g.setColour (textSecondary);
    g.fillPath (arrow);
}

void SculptorLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (3, 1, box.getWidth() - box.getHeight() - 5, box.getHeight() - 2);
    label.setFont (juce::FontOptions (12.0f));
}

void SculptorLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    g.setColour (raisedBg);
    g.fillRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 4.0f);
    g.setColour (border);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 4.0f, 1.0f);
    g.setColour (textPrimary);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (text, 6, 0, width - 12, height, juce::Justification::centredLeft, true);
}


