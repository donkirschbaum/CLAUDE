#include "PluginEditor.h"

using namespace pf;

//==============================================================================
// LookAndFeel
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::bg);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, Colours::panel.brighter (0.08f));
    setColour (juce::ComboBox::outlineColourId, Colours::outline);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::arrowColourId, Colours::dim);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::outline);
    setColour (juce::TextButton::buttonColourId, Colours::panel.brighter (0.08f));
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::ToggleButton::textColourId, Colours::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float lineW = 3.5f;
    const float arcR = radius - lineW;
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path bgArc;
    bgArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::outline);
    g.strokePath (bgArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valArc;
    valArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (accent);
    g.strokePath (valArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = arcR - 6.0f;
    g.setColour (Colours::panel.brighter (0.15f));
    g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f);

    juce::Path tick;
    tick.addRoundedRectangle (-1.5f, -knobR + 3.0f, 3.0f, knobR * 0.5f, 1.5f);
    g.setColour (Colours::text);
    g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    const bool on = b.getToggleState();
    g.setColour (on ? accent.withAlpha (0.9f) : Colours::panel.brighter (highlighted ? 0.18f : 0.08f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? accent : Colours::outline);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (on ? Colours::bg : Colours::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

//==============================================================================
// Knob
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text, juce::Colour c)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, Colours::text);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f));
    label.setColour (juce::Label::textColourId, Colours::dim);
    addAndMakeVisible (label);

    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (16));
    slider.setBounds (r);
}

//==============================================================================
// StepLane
StepLane::StepLane (PulseForgeProcessor& p, Lane l, juce::Colour c) : proc (p), lane (l), colour (c)
{
    for (int i = 0; i < kNumSteps; ++i)
        params[(size_t) i] = p.apvts.getParameter (stepId (l, i));
    refresh();
}

void StepLane::refresh()
{
    bool changed = false;
    for (int i = 0; i < kNumSteps; ++i)
    {
        const float v = params[(size_t) i]->getValue();
        if (std::abs (v - shown[(size_t) i]) > 1.0e-6f) { shown[(size_t) i] = v; changed = true; }
    }
    const int ps = proc.currentStep.load();
    const int ns = (int) proc.apvts.getRawParameterValue ("length")->load();
    if (ps != playStep || ns != numSteps) { playStep = ps; numSteps = ns; changed = true; }
    if (changed) repaint();
}

void StepLane::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (r, 6.0f);

    const float w = r.getWidth() / (float) kNumSteps;
    const bool bipolar = (lane == Lane::Pan);

    if (bipolar)
    {
        g.setColour (Colours::outline);
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    }

    for (int i = 0; i < kNumSteps; ++i)
    {
        auto cell = juce::Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (2.5f, 6.0f);
        const bool active = i < numSteps;
        const bool playing = (i == playStep);

        // beat shading
        g.setColour (((i / 4) % 2 == 0) ? Colours::panel : Colours::panel.brighter (0.04f));
        g.fillRoundedRectangle (cell, 3.0f);

        const float v = shown[(size_t) i];
        juce::Rectangle<float> bar;
        if (bipolar)
        {
            const float mid = cell.getCentreY();
            const float y = cell.getBottom() - v * cell.getHeight();
            bar = juce::Rectangle<float> (cell.getX(), juce::jmin (mid, y), cell.getWidth(), std::abs (y - mid) + 1.0f);
        }
        else
        {
            const float hgt = v * cell.getHeight();
            bar = cell.withTop (cell.getBottom() - hgt);
        }

        auto c = colour.withAlpha (active ? (playing ? 1.0f : 0.75f) : 0.18f);
        g.setColour (c);
        g.fillRoundedRectangle (bar, 3.0f);

        if (playing && active)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawRoundedRectangle (cell.expanded (1.0f), 3.0f, 1.5f);
        }
    }
}

int StepLane::stepAt (float x) const
{
    return juce::jlimit (0, kNumSteps - 1, (int) (x / (getWidth() / (float) kNumSteps)));
}

float StepLane::valueAt (float y) const
{
    const float top = 6.0f, bottom = (float) getHeight() - 6.0f;
    return juce::jlimit (0.0f, 1.0f, (bottom - y) / (bottom - top));
}

void StepLane::setStep (int step, float value)
{
    auto* p = params[(size_t) step];
    if (! gestureOpen[(size_t) step])
    {
        p->beginChangeGesture();
        gestureOpen[(size_t) step] = true;
    }
    p->setValueNotifyingHost (value);
}

void StepLane::mouseDown (const juce::MouseEvent& e)
{
    const int s = stepAt ((float) e.x);
    if (lane == Lane::Gate && ! e.mods.isShiftDown())
    {
        // toggle on/off, then drag paints the same value
        paintValue = params[(size_t) s]->getValue() > 0.5f ? 0.0f : 1.0f;
        setStep (s, paintValue);
    }
    else if (e.mods.isAltDown() || e.getNumberOfClicks() > 1)
    {
        setStep (s, lane == Lane::Gate ? 1.0f : 0.5f); // reset
    }
    else
    {
        setStep (s, valueAt ((float) e.y));
    }
    refresh();
}

void StepLane::mouseDrag (const juce::MouseEvent& e)
{
    const int s = stepAt ((float) e.x);
    if (lane == Lane::Gate && ! e.mods.isShiftDown())
        setStep (s, paintValue);
    else
        setStep (s, valueAt ((float) e.y));
    refresh();
}

void StepLane::mouseUp (const juce::MouseEvent&)
{
    for (int i = 0; i < kNumSteps; ++i)
        if (gestureOpen[(size_t) i])
        {
            params[(size_t) i]->endChangeGesture();
            gestureOpen[(size_t) i] = false;
        }
}

//==============================================================================
// Editor
PulseForgeEditor::PulseForgeEditor (PulseForgeProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    auto knob = [&] (const juce::String& id, const juce::String& label, juce::Colour c)
    {
        auto k = std::make_unique<Knob> (s, id, label, c);
        addAndMakeVisible (*k);
        return k;
    };
    auto combo = [&] (juce::ComboBox& box, const juce::String& id, std::unique_ptr<CA>& att)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
            box.addItemList (choice->choices, 1);
        addAndMakeVisible (box);
        att = std::make_unique<CA> (s, id, box);
    };
    auto toggle = [&] (juce::ToggleButton& b, const juce::String& id, std::unique_ptr<BA>& att, juce::Colour c)
    {
        b.setColour (juce::ToggleButton::tickColourId, c);
        addAndMakeVisible (b);
        att = std::make_unique<BA> (s, id, b);
    };

    // Header
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.applyPreset (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    combo (rateBox, "rate", rateAtt);
    lengthKnob = knob ("length", "Steps", Colours::text);
    swingKnob  = knob ("swing", "Swing", Colours::text);

    // Lanes
    const juce::Colour laneCols[] { Colours::gate, Colours::filter, Colours::pan };
    const char* onIds[] { "gateOn", "filtOn", "panOn" };
    for (int i = 0; i < 3; ++i)
    {
        lanes[(size_t) i] = std::make_unique<StepLane> (proc, (Lane) i, laneCols[i]);
        addAndMakeVisible (*lanes[(size_t) i]);

        laneOn[(size_t) i].setButtonText ("On");
        toggle (laneOn[(size_t) i], onIds[i], laneOnAtt[(size_t) i], laneCols[i]);

        randomBtn[(size_t) i].setButtonText ("Random");
        randomBtn[(size_t) i].onClick = [this, i] { proc.randomiseLane ((Lane) i); };
        addAndMakeVisible (randomBtn[(size_t) i]);
    }

    gateDepth  = knob ("gateDepth", "Depth", Colours::gate);
    gateLen    = knob ("gateLen", "Length", Colours::gate);
    gateSmooth = knob ("gateSmooth", "Smooth", Colours::gate);

    combo (filtType, "filtType", filtTypeAtt);
    cutoff    = knob ("cutoff", "Cutoff", Colours::filter);
    reso      = knob ("reso", "Reso", Colours::filter);
    filtDepth = knob ("filtDepth", "Seq Depth", Colours::filter);
    filtGlide = knob ("filtGlide", "Glide", Colours::filter);

    panDepth = knob ("panDepth", "Depth", Colours::pan);

    drive      = knob ("drive", "Drive", Colours::fx);
    bits       = knob ("bits", "Bits", Colours::fx);
    downsample = knob ("downsample", "Downsample", Colours::fx);

    toggle (delayOn, "delayOn", delayOnAtt, Colours::fx);
    toggle (delayPing, "delayPing", delayPingAtt, Colours::fx);
    combo (delayTime, "delayTime", delayTimeAtt);
    delayFb  = knob ("delayFb", "Feedback", Colours::fx);
    delayMix = knob ("delayMix", "Mix", Colours::fx);

    toggle (revOn, "revOn", revOnAtt, Colours::fx);
    revSize = knob ("revSize", "Size", Colours::fx);
    revDamp = knob ("revDamp", "Damping", Colours::fx);
    revMix  = knob ("revMix", "Mix", Colours::fx);

    mix = knob ("mix", "Dry/Wet", Colours::text);
    out = knob ("out", "Output", Colours::text);

    setSize (960, 650);
    startTimerHz (30);
}

PulseForgeEditor::~PulseForgeEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PulseForgeEditor::timerCallback()
{
    for (auto& l : lanes) l->refresh();
}

void PulseForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);

    // Header
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("PULSEFORGE", 16, 10, 220, 34, juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("rhythmic multi-fx", 190, 18, 140, 20, juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Preset", 360, 4, 60, 14, juce::Justification::centredLeft);
    g.drawText ("Rate", 600, 4, 60, 14, juce::Justification::centredLeft);

    for (auto& s : sections)
    {
        auto r = s.bounds.toFloat();
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Colours::outline);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
        g.setColour (s.colour);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (s.title, s.bounds.getX() + 12, s.bounds.getY() + 6, 120, 18, juce::Justification::centredLeft);
    }
}

void PulseForgeEditor::resized()
{
    sections.clear();

    // Header
    presetBox.setBounds (360, 20, 220, 26);
    rateBox.setBounds (600, 20, 90, 26);
    lengthKnob->setBounds (720, 2, 100, 60);
    swingKnob->setBounds (830, 2, 110, 60);
    lengthKnob->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 16);
    swingKnob->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);

    // Lanes
    const juce::String titles[] { "GATE", "FILTER", "PAN" };
    const juce::Colour cols[] { Colours::gate, Colours::filter, Colours::pan };
    int y = 70;
    const int laneH = 128;
    for (int i = 0; i < 3; ++i)
    {
        juce::Rectangle<int> row (12, y, 936, laneH);
        sections.push_back ({ titles[i], cols[i], row });

        auto inner = row.reduced (10);
        auto left = inner.removeFromLeft (96);
        left.removeFromTop (22);
        laneOn[(size_t) i].setBounds (left.removeFromTop (24).withWidth (88));
        left.removeFromTop (6);
        randomBtn[(size_t) i].setBounds (left.removeFromTop (24).withWidth (88));
        if (i == 1)
        {
            left.removeFromTop (6);
            filtType.setBounds (left.removeFromTop (24).withWidth (88));
        }

        inner.removeFromLeft (8);
        auto knobs = inner.removeFromRight (330);
        lanes[(size_t) i]->setBounds (inner.withTrimmedRight (10));

        const int kw = 80;
        auto place = [&] (pf::Knob& k) { k.setBounds (knobs.removeFromLeft (kw).withSizeKeepingCentre (kw, 100)); knobs.removeFromLeft (3); };
        if (i == 0) { place (*gateDepth); place (*gateLen); place (*gateSmooth); }
        if (i == 1) { place (*cutoff); place (*reso); place (*filtDepth); place (*filtGlide); }
        if (i == 2) { place (*panDepth); }

        y += laneH + 8;
    }

    // Bottom FX row
    juce::Rectangle<int> bottom (12, y, 936, getHeight() - y - 12);
    auto take = [&] (int w, const juce::String& title, juce::Colour c)
    {
        auto r = bottom.removeFromLeft (w);
        bottom.removeFromLeft (8);
        sections.push_back ({ title, c, r });
        auto inner = r.reduced (10);
        inner.removeFromTop (18);
        return inner;
    };
    auto placeK = [] (juce::Rectangle<int>& area, pf::Knob& k, int w = 76)
    {
        k.setBounds (area.removeFromLeft (w).withSizeKeepingCentre (w, 100));
    };

    {
        auto a = take (240, "CHARACTER", Colours::fx);
        placeK (a, *drive); placeK (a, *bits); placeK (a, *downsample);
    }
    {
        auto a = take (270, "DELAY", Colours::fx);
        auto col = a.removeFromLeft (100);
        col.removeFromTop (6);
        delayOn.setBounds (col.removeFromTop (24));
        col.removeFromTop (6);
        delayPing.setBounds (col.removeFromTop (24));
        col.removeFromTop (6);
        delayTime.setBounds (col.removeFromTop (24));
        a.removeFromLeft (6);
        placeK (a, *delayFb); placeK (a, *delayMix);
    }
    {
        auto a = take (258, "REVERB", Colours::fx);
        auto col = a.removeFromLeft (56);
        col.removeFromTop (6);
        revOn.setBounds (col.removeFromTop (24));
        placeK (a, *revSize, 64); placeK (a, *revDamp, 64); placeK (a, *revMix, 64);
    }
    {
        auto a = take (bottom.getWidth(), "MASTER", Colours::text);
        placeK (a, *mix, 60); placeK (a, *out, 60);
    }
}
