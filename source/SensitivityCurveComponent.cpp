#include "SensitivityCurveComponent.h"

#include <cmath>

namespace
{
    const juce::Colour bandColours[] = {
        juce::Colour (0xffd94f4f),   // red
        juce::Colour (0xffe0c341),   // yellow
        juce::Colour (0xff9b59b6)    // purple
    };
}

SensitivityCurveComponent::SensitivityCurveComponent (PluginProcessor& p)
    : processor (p)
{
    setOpaque (true);
}

SensitivityCurveComponent::~SensitivityCurveComponent() = default;

void SensitivityCurveComponent::timerCallback()
{
    if (! isShowing())
    {
        stopTimer();
        return;
    }

    processor.getReductionSnapshot (reduction);
    repaint();
}

juce::Rectangle<int> SensitivityCurveComponent::getPlotBounds() const
{
    return getLocalBounds().reduced (10, 8);
}

float SensitivityCurveComponent::frequencyToX (float hz) const
{
    const auto plot = getPlotBounds();
    const auto t = std::log2 (juce::jlimit (frequencyMinHz, frequencyMaxHz, hz) / frequencyMinHz)
                   / std::log2 (frequencyMaxHz / frequencyMinHz);

    return plot.getX() + t * (float) plot.getWidth();
}

float SensitivityCurveComponent::xToFrequency (float x) const
{
    const auto plot = getPlotBounds();
    const auto clamped = juce::jlimit ((float) plot.getX(), (float) plot.getRight(), x);
    const auto t = (clamped - (float) plot.getX()) / (float) plot.getWidth();

    return frequencyMinHz * std::pow (frequencyMaxHz / frequencyMinHz, t);
}

float SensitivityCurveComponent::gainToY (float db) const
{
    const auto plot = getPlotBounds();
    const auto t = (gainMaxDb - juce::jlimit (gainMinDb, gainMaxDb, db)) / (gainMaxDb - gainMinDb);

    return (float) plot.getY() + t * (float) plot.getHeight();
}

float SensitivityCurveComponent::yToGain (float y) const
{
    const auto plot = getPlotBounds();
    const auto clamped = juce::jlimit ((float) plot.getY(), (float) plot.getBottom(), y);
    const auto t = (clamped - (float) plot.getY()) / (float) plot.getHeight();

    return gainMaxDb - t * (gainMaxDb - gainMinDb);
}

float SensitivityCurveComponent::qToY (float q) const
{
    const auto plot = getPlotBounds();
    const auto clamped = juce::jlimit (qMin, qMax, q);
    const auto t = std::log10 (clamped / qMin) / std::log10 (qMax / qMin);

    return (float) plot.getBottom() - t * (float) plot.getHeight();
}

float SensitivityCurveComponent::yToQ (float y) const
{
    const auto plot = getPlotBounds();
    const auto clamped = juce::jlimit ((float) plot.getY(), (float) plot.getBottom(), y);
    const auto t = ((float) plot.getBottom() - clamped) / (float) plot.getHeight();

    return qMin * std::pow (qMax / qMin, t);
}

void SensitivityCurveComponent::setSelectedBand (int band)
{
    band = juce::jlimit (0, 2, band);

    if (band == selectedBand)
        return;

    selectedBand = band;
    repaint();

    if (onBandSelectionChanged)
        onBandSelectionChanged();
}

float SensitivityCurveComponent::readParameter (const char* id) const
{
    if (const auto* param = processor.apvts.getRawParameterValue (id))
        return param->load();

    return 0.0f;
}

void SensitivityCurveComponent::writeParameter (const char* id, float value)
{
    if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (id)))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

sensitivity::Parameters SensitivityCurveComponent::currentCurve() const
{
    sensitivity::Parameters curve;

    for (int i = 0; i < 3; ++i)
    {
        const auto& ids = bandIds[(size_t) i];
        curve.bands[(size_t) i].frequencyHz = readParameter (ids.freq);
        curve.bands[(size_t) i].gainDb = readParameter (ids.gain);
        curve.bands[(size_t) i].q = readParameter (ids.q);
        curve.bands[(size_t) i].type = (int) readParameter (ids.type);
        curve.bands[(size_t) i].enabled = readParameter (ids.on) > 0.5f;
    }

    curve.loCutFrequencyHz = readParameter ("locut_freq");
    curve.loCutSlopeDbPerOctave = (int) readParameter ("locut_slope") * 12;
    curve.hiCutFrequencyHz = readParameter ("hicut_freq");
    curve.hiCutSlopeDbPerOctave = (int) readParameter ("hicut_slope") * 12;

    return curve;
}

void SensitivityCurveComponent::paint (juce::Graphics& g)
{
    if (isShowing() && ! isTimerRunning())
        startTimerHz (30);

    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    const auto plot = getPlotBounds();
    const auto textColour = getLookAndFeel().findColour (juce::Label::textColourId);
    const auto gridColour = textColour.withAlpha (0.15f);
    const auto lineColour = textColour.withAlpha (0.6f);

    g.setFont (juce::FontOptions (10.0f));

    for (int db = -24; db <= 12; db += 6)
    {
        const auto y = juce::roundToInt (gainToY ((float) db));
        g.setColour (db == 0 ? lineColour : gridColour);
        g.drawLine ((float) plot.getX(), (float) y, (float) plot.getRight(), (float) y);
        g.setColour (textColour.withAlpha (0.7f));
        g.drawText (juce::String (db) + " dB", plot.getX() + 2, y - 8, 40, 12, juce::Justification::left, false);
    }

    const float ticksHz[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };

    for (const auto hz : ticksHz)
    {
        const auto x = juce::roundToInt (frequencyToX (hz));
        g.setColour (gridColour);
        g.drawLine ((float) x, (float) plot.getY(), (float) x, (float) plot.getBottom());
        g.setColour (textColour.withAlpha (0.7f));
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + "k" : juce::String ((int) hz),
                    x - 20, plot.getBottom() - 14, 40, 12, juce::Justification::centred, false);
    }

    const auto sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    const auto curve = currentCurve();

    drawReduction (g, curve, sampleRate);
    drawCurve (g, curve, sampleRate);
    drawNodes (g, curve);
}

void SensitivityCurveComponent::drawReduction (juce::Graphics& g, const sensitivity::Parameters&, double sampleRate)
{
    const auto plot = getPlotBounds();
    const auto numBins = processor.getNumBins();

    if (numBins < 2)
        return;

    const auto boost = readParameter ("mode") > 0.5f;
    const auto colour = boost ? juce::Colour (0xffd94f4f)   // red
                              : juce::Colour (0xff4f8bd9);  // blue
    const auto nyquist = (float) (sampleRate * 0.5);
    const auto zeroY = gainToY (0.0f);

    juce::Path curvePath;
    juce::Path fillPath;

    fillPath.startNewSubPath ((float) plot.getX(), zeroY);

    for (int bin = 0; bin < numBins; ++bin)
    {
        const auto amount = juce::jlimit (0.0f, 1.0f, reduction[(size_t) bin]);
        const auto frequency = (float) bin / (float) (numBins - 1) * nyquist;
        const auto gainDb = boost ? 20.0f * std::log10 (1.0f + amount)
                                  : 20.0f * std::log10 (juce::jmax (0.001f, 1.0f - amount));

        const auto x = frequencyToX (frequency);
        const auto y = gainToY ((float) gainDb);

        if (bin == 0)
            curvePath.startNewSubPath (x, y);
        else
            curvePath.lineTo (x, y);

        fillPath.lineTo (x, y);
    }

    fillPath.lineTo ((float) plot.getRight(), zeroY);
    fillPath.closeSubPath();

    g.setColour (colour.withAlpha (0.22f));
    g.fillPath (fillPath);
    g.setColour (colour.withAlpha (0.9f));
    g.strokePath (curvePath, juce::PathStrokeType (1.25f));
}

void SensitivityCurveComponent::drawCurve (juce::Graphics& g, const sensitivity::Parameters& curve, double sampleRate)
{
    const auto plot = getPlotBounds();
    const auto textColour = getLookAndFeel().findColour (juce::Label::textColourId);
    const auto curveColour = textColour.interpolatedWith (bandColours[1], 0.4f);

    juce::Path path;
    constexpr int numPoints = 240;

    for (int i = 0; i <= numPoints; ++i)
    {
        const auto t = (float) i / (float) numPoints;
        const auto freq = frequencyMinHz * std::pow (frequencyMaxHz / frequencyMinHz, t);
        const auto weight = sensitivity::weightAtFrequency (curve, sampleRate, freq);
        const auto db = juce::jlimit (gainMinDb, gainMaxDb, 20.0f * std::log10 (juce::jmax (weight, 1.0e-6f)));

        const auto x = (float) plot.getX() + t * (float) plot.getWidth();
        const auto y = gainToY (db);

        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    g.setColour (curveColour.withAlpha (0.25f));
    g.fillPath (path);
    g.setColour (curveColour);
    g.strokePath (path, juce::PathStrokeType (1.5f));
}

void SensitivityCurveComponent::drawNodes (juce::Graphics& g, const sensitivity::Parameters& curve)
{
    const auto plot = getPlotBounds();
    const auto textColour = getLookAndFeel().findColour (juce::Label::textColourId);

    g.setColour (textColour.withAlpha (0.5f));
    const float dashes[] = { 3.0f, 3.0f };

    const auto loCutX = frequencyToX (curve.loCutFrequencyHz);
    g.drawDashedLine (juce::Line<float> (loCutX, (float) plot.getY(), loCutX, (float) plot.getBottom()), dashes, 2);

    const auto hiCutX = frequencyToX (curve.hiCutFrequencyHz);
    g.drawDashedLine (juce::Line<float> (hiCutX, (float) plot.getY(), hiCutX, (float) plot.getBottom()), dashes, 2);

    g.setColour (textColour.withAlpha (0.7f));
    {
        juce::Path loCutHandle;
        loCutHandle.addTriangle (loCutX, (float) plot.getBottom() - 1.0f,
                                 loCutX - 6.0f, (float) plot.getBottom() + 8.0f,
                                 loCutX + 6.0f, (float) plot.getBottom() + 8.0f);
        g.fillPath (loCutHandle);

        juce::Path hiCutHandle;
        hiCutHandle.addTriangle (hiCutX, (float) plot.getY() + 1.0f,
                                 hiCutX - 6.0f, (float) plot.getY() - 8.0f,
                                 hiCutX + 6.0f, (float) plot.getY() - 8.0f);
        g.fillPath (hiCutHandle);
    }

    for (int i = 0; i < 3; ++i)
    {
        const auto& band = curve.bands[(size_t) i];
        const auto colour = bandColours[i].withAlpha (band.enabled ? 1.0f : 0.3f);
        const auto centre = juce::Point<float> (frequencyToX (band.frequencyHz), gainToY (band.gainDb));

        g.setColour (colour);
        g.fillEllipse (centre.x - nodeRadius, centre.y - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);
        g.setColour (colour.contrasting (0.6f));
        g.drawEllipse (centre.x - nodeRadius, centre.y - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f, 1.5f);

        if (i == selectedBand)
        {
            g.setColour (colour);
            g.drawEllipse (centre.x - nodeRadius - 4.0f, centre.y - nodeRadius - 4.0f,
                           (nodeRadius + 4.0f) * 2.0f, (nodeRadius + 4.0f) * 2.0f, 1.5f);
        }
    }

    // While Cmd/Ctrl-dragging a band, show the current Q as a horizontal guide.
    if (draggingQ && dragBand >= 0)
    {
        const auto y = qToY (dragQValue);
        const float qDashes[] = { 3.0f, 3.0f };

        g.setColour (bandColours[dragBand].withAlpha (0.9f));
        g.drawDashedLine (juce::Line<float> ((float) plot.getX(), y, (float) plot.getRight(), y), qDashes, 2);
        g.drawText (juce::String ("Q ") + juce::String (dragQValue, 2),
                    (float) plot.getRight() - 64.0f, y - 15.0f, 64.0f, 12.0f,
                    juce::Justification::right, false);
    }
}

SensitivityCurveComponent::DragTarget SensitivityCurveComponent::findDragTarget (const juce::Point<int>& position, int& bandIndex) const
{
    const auto curve = currentCurve();

    for (int i = 0; i < 3; ++i)
    {
        const auto& band = curve.bands[(size_t) i];
        const auto centre = juce::Point<float> (frequencyToX (band.frequencyHz), gainToY (band.gainDb));

        if (centre.getDistanceFrom (position.toFloat()) <= nodeRadius + 4.0f)
        {
            bandIndex = i;
            return DragTarget::band;
        }
    }

    const auto plot = getPlotBounds();

    if (std::abs (position.x - juce::roundToInt (frequencyToX (curve.loCutFrequencyHz))) <= 6
        && position.y >= plot.getY() - 8 && position.y <= plot.getBottom() + 8)
        return DragTarget::loCut;

    if (std::abs (position.x - juce::roundToInt (frequencyToX (curve.hiCutFrequencyHz))) <= 6
        && position.y >= plot.getY() - 8 && position.y <= plot.getBottom() + 8)
        return DragTarget::hiCut;

    bandIndex = -1;
    return DragTarget::none;
}

void SensitivityCurveComponent::mouseDown (const juce::MouseEvent& event)
{
    int bandIndex = -1;
    dragTarget = findDragTarget (event.getPosition(), bandIndex);
    dragBand = bandIndex;

    if (dragTarget != DragTarget::band)
        return;

    setSelectedBand (bandIndex);

    // Double-click toggles the band on/off (frequency-range bypass).
    if (event.getNumberOfClicks() >= 2)
    {
        const auto& ids = bandIds[(size_t) bandIndex];
        writeParameter (ids.on, readParameter (ids.on) > 0.5f ? 0.0f : 1.0f);
        dragTarget = DragTarget::none;
        dragBand = -1;
        return;
    }

    if (event.mods.isAltDown())
    {
        // Alt-drag: band listen, the node stays put.
        listening = true;
        const auto band = currentCurve().bands[(size_t) bandIndex];
        processor.beginBandListen (band.frequencyHz, band.q);
    }
    else
    {
        // Cmd/Ctrl-drag edits Q instead of gain (Pro-Q style).
        draggingQ = event.mods.isCommandDown() || event.mods.isCtrlDown();

        if (draggingQ)
            dragQValue = currentCurve().bands[(size_t) bandIndex].q;
    }
}

void SensitivityCurveComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (dragTarget == DragTarget::none)
        return;

    if (dragTarget == DragTarget::band)
    {
        if (listening)
        {
            // Alt-drag: sweep the band-listen frequency with the cursor without
            // moving the node (it returns to its committed position on release).
            const auto q = currentCurve().bands[(size_t) dragBand].q;
            processor.updateBandListen (xToFrequency ((float) event.position.x), q);
            return;
        }

        const auto freq = xToFrequency ((float) event.position.x);
        writeParameter (bandIds[(size_t) dragBand].freq, freq);

        if (draggingQ)
        {
            dragQValue = yToQ ((float) event.position.y);
            writeParameter (bandIds[(size_t) dragBand].q, dragQValue);
        }
        else
        {
            writeParameter (bandIds[(size_t) dragBand].gain, yToGain ((float) event.position.y));
        }
    }
    else if (dragTarget == DragTarget::loCut)
    {
        writeParameter ("locut_freq", xToFrequency ((float) event.position.x));
    }
    else if (dragTarget == DragTarget::hiCut)
    {
        writeParameter ("hicut_freq", xToFrequency ((float) event.position.x));
    }

    repaint();
}

void SensitivityCurveComponent::mouseUp (const juce::MouseEvent&)
{
    if (listening)
        processor.endBandListen();

    listening = false;
    draggingQ = false;
    dragTarget = DragTarget::none;
    dragBand = -1;
}

void SensitivityCurveComponent::mouseMove (const juce::MouseEvent& event)
{
    int bandIndex = -1;
    const auto target = findDragTarget (event.getPosition(), bandIndex);

    if (target != DragTarget::none)
    {
        if (event.mods.isAltDown())
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        else if (target == DragTarget::band && (event.mods.isCommandDown() || event.mods.isCtrlDown()))
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        else
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
    else
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }
}

void SensitivityCurveComponent::mouseExit (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}


