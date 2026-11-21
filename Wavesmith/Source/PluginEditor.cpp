#include "PluginEditor.h"

namespace ws::ui
{

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::dimText);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, colours::track);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, colours::panelEdge);
    setColour (juce::ComboBox::arrowColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.35f));
    setColour (juce::TextButton::buttonColourId, colours::track);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TabbedButtonBar::tabTextColourId, colours::dimText);
    setColour (juce::TabbedButtonBar::frontTextColourId, colours::accent);
    setColour (juce::TabbedComponent::backgroundColourId, colours::panel);
    setColour (juce::TabbedComponent::outlineColourId, colours::panelEdge);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::TextEditor::backgroundColourId, colours::track);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel);
    setColour (juce::TooltipWindow::textColourId, colours::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, start, end, true);
    g.setColour (colours::track);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, std::min (from, angle), std::max (from, angle), true);
    g.setColour (colour);
    g.strokePath (arc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = radius * 0.62f;
    g.setColour (colours::panelEdge.brighter (0.15f));
    g.fillEllipse (centre.x - inner, centre.y - inner, inner * 2, inner * 2);
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.25f, -inner + 2.0f, 2.5f, inner * 0.55f, 1.0f);
    g.setColour (colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style == juce::Slider::LinearBarVertical)
    {
        const auto r = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (1.5f, 0.0f);
        g.setColour (colours::track);
        g.fillRoundedRectangle (r, 2.0f);
        auto fill = r.withTop (pos);
        g.setColour (s.findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (fill, 2.0f);
        return;
    }
    LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto led = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.22f);
    g.setColour (on ? colours::accent : colours::track.brighter (highlighted ? 0.2f : 0.0f));
    g.fillRoundedRectangle (led, 2.5f);
    g.setColour (on ? colours::text : colours::dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (3), juce::Justification::centredLeft, 1);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text, juce::Colour c)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 14);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    if (auto* p = s.getParameter (id))
        slider.setTooltip (p->getName (64));
    addAndMakeVisible (slider);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, colours::text);

    auto* param = s.getParameter (id);
    const bool isInt = dynamic_cast<juce::AudioParameterInt*> (param) != nullptr;
    const juce::String suffix = param != nullptr ? param->getLabel() : juce::String();
    slider.textFromValueFunction = [isInt, suffix] (double v)
    {
        juce::String t;
        const double a = std::abs (v);
        if (isInt)              t = juce::String ((int) std::lround (v));
        else if (suffix == "Hz" && a >= 1000.0) return juce::String (v / 1000.0, a >= 10000.0 ? 1 : 2) + " kHz";
        else if (suffix == "s" && a < 1.0)      return juce::String (v * 1000.0, a < 0.01 ? 1 : 0) + " ms";
        else if (a >= 100.0)    t = juce::String (v, 0);
        else if (a >= 10.0)     t = juce::String (v, 1);
        else                    t = juce::String (v, 2);
        return suffix.isNotEmpty() ? t + " " + suffix : t;
    };
    slider.updateText();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

Choice::Choice (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    if (label.getText().isNotEmpty())
        label.setBounds (r.removeFromTop (14));
    box.setBounds (r.withSizeKeepingCentre (r.getWidth(), std::min (r.getHeight(), 22)));
}

Toggle::Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text)
{
    button.setButtonText (text);
    addAndMakeVisible (button);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, button);
}

void Toggle::resized()
{
    auto r = getLocalBounds();
    button.setBounds (r.withSizeKeepingCentre (r.getWidth(), std::min (r.getHeight(), 20)));
}

//==============================================================================
WaveDisplay::WaveDisplay (juce::AudioProcessorValueTreeState& s, const juce::String& p)
{
    table = s.getRawParameterValue (p + "Table");
    pos = s.getRawParameterValue (p + "Pos");
    on = s.getRawParameterValue (p + "On");
    lastTable = table->load(); lastPos = pos->load(); lastOn = on->load();
    startTimerHz (30);
}

void WaveDisplay::timerCallback()
{
    const float t = table->load(), p = pos->load(), o = on->load();
    if (t != lastTable || p != lastPos || o != lastOn)
    {
        lastTable = t; lastPos = p; lastOn = o;
        repaint();
    }
}

void WaveDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::background);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::panelEdge);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 4, r.getRight() - 4);

    const auto& bank = WavetableBank::get();
    const int t = juce::jlimit (0, WavetableBank::numTables - 1, (int) lastTable);
    const bool active = lastOn > 0.5f;

    // a few ghost frames behind the current one for depth
    for (int ghost = 3; ghost >= 0; --ghost)
    {
        const float gp = ghost == 0 ? lastPos : (float) (ghost - 1) / 2.0f;
        if (ghost > 0 && std::abs (gp - lastPos) < 0.05f) continue;
        juce::Path path;
        const int points = 200;
        for (int i = 0; i <= points; ++i)
        {
            const float ph = (float) i / (float) points;
            const float v = bank.read (t, gp, 0, std::min (ph, 0.9999f));
            const float px = r.getX() + 4 + ph * (r.getWidth() - 8);
            const float py = r.getCentreY() - v * (r.getHeight() * 0.4f);
            if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
        }
        const auto c = active ? colours::accent : colours::dimText;
        g.setColour (ghost == 0 ? c : c.withAlpha (0.12f));
        g.strokePath (path, juce::PathStrokeType (ghost == 0 ? 1.8f : 1.0f));
    }
    g.setColour (colours::dimText);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("Frame " + juce::String (1 + (int) std::round (lastPos * (WavetableBank::numFrames - 1))),
                getLocalBounds().reduced (6, 3), juce::Justification::topRight);
}

//==============================================================================
StepSeqView::StepSeqView (WavesmithProcessor& p) : proc (p)
{
    for (int i = 0; i < numSeqSteps; ++i)
    {
        auto* s = steps.add (new juce::Slider (juce::Slider::LinearBarVertical, juce::Slider::NoTextBox));
        s->setColour (juce::Slider::trackColourId, colours::accent2);
        s->setTooltip ("Step " + juce::String (i + 1));
        addAndMakeVisible (s);
        attachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (proc.apvts, seqStepId (i), *s));
    }
    startTimerHz (30);
}

void StepSeqView::resized()
{
    auto r = getLocalBounds().withTrimmedBottom (8);
    const float w = (float) r.getWidth() / numSeqSteps;
    for (int i = 0; i < numSeqSteps; ++i)
        steps[i]->setBounds ((int) (r.getX() + i * w) + 1, r.getY(), (int) w - 2, r.getHeight());
}

void StepSeqView::paint (juce::Graphics& g)
{
    const int len = (int) proc.apvts.getRawParameterValue ("seqLen")->load();
    auto r = getLocalBounds();
    const float w = (float) r.getWidth() / numSeqSteps;
    for (int i = 0; i < numSeqSteps; ++i)
    {
        const auto cell = juce::Rectangle<float> (r.getX() + i * w + 2, (float) r.getBottom() - 5, w - 4, 4);
        g.setColour (i == lastStep ? colours::accent : (i < len ? colours::track.brighter (0.3f) : colours::track.darker (0.4f)));
        g.fillRoundedRectangle (cell, 1.5f);
    }
}

void StepSeqView::timerCallback()
{
    const int s = proc.engine.currentSeqStep.load();
    if (s != lastStep) { lastStep = s; repaint(); }
}

//==============================================================================
void Row::resized()
{
    float total = 0;
    for (auto& it : items) total += it.second;
    if (total <= 0) return;
    auto r = getLocalBounds();
    const int gap = 4;
    const float avail = (float) (r.getWidth() - gap * ((int) items.size() - 1));
    float x = (float) r.getX();
    for (auto& it : items)
    {
        const float w = avail * it.second / total;
        it.first->setBounds ((int) x, r.getY(), (int) w, r.getHeight());
        x += w + gap;
    }
}

Row& Section::addRow (float weight)
{
    auto* r = rows.add (new Row());
    weights.push_back (weight);
    addAndMakeVisible (r);
    return *r;
}

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (colours::accent.withAlpha (0.9f));
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (title, getLocalBounds().reduced (10, 0).removeFromTop (22), juce::Justification::centredLeft);
}

void Section::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    auto header = r.removeFromTop (20);
    if (headerWidget != nullptr)
        headerWidget->setBounds (header.removeFromRight (60));
    r.removeFromTop (2);

    float total = 0;
    for (auto w : weights) total += w;
    if (total <= 0) return;
    const int gap = 4;
    const float avail = (float) (r.getHeight() - gap * ((int) rows.size() - 1));
    float y = (float) r.getY();
    for (int i = 0; i < rows.size(); ++i)
    {
        const float h = avail * weights[(size_t) i] / total;
        rows[i]->setBounds (r.getX(), (int) y, r.getWidth(), (int) h);
        y += h + gap;
    }
}

} // namespace ws::ui

//==============================================================================
using namespace ws::ui;

Knob* WavesmithEditor::knob (const juce::String& id, const juce::String& label, juce::Colour c) { return make<Knob> (proc.apvts, id, label, c); }
Choice* WavesmithEditor::choice (const juce::String& id, const juce::String& label) { return make<Choice> (proc.apvts, id, label); }
Toggle* WavesmithEditor::toggle (const juce::String& id, const juce::String& label) { return make<Toggle> (proc.apvts, id, label); }
Section* WavesmithEditor::section (const juce::String& title)
{
    auto* s = new Section (title);
    sections.add (s);
    content.addAndMakeVisible (s);
    return s;
}

void WavesmithEditor::buildOscSection (Section* s, const juce::String& p, bool withFm)
{
    s->setHeaderWidget (toggle (p + "On", "On"));
    s->addRow (0.55f).add (choice (p + "Table", {}), 1.0f).add (toggle (p + "Rand", "Rand Phase"), 0.45f);
    s->addRow (1.3f).add (make<WaveDisplay> (proc.apvts, p), 1.0f);
    s->addRow (1.6f)
        .add (knob (p + "Pos", "Position")).add (knob (p + "Oct", "Octave", colours::accent2))
        .add (knob (p + "Semi", "Semi", colours::accent2)).add (knob (p + "Fine", "Fine", colours::accent2))
        .add (knob (p + "Uni", "Unison", colours::accent2));
    auto& r = s->addRow (1.6f);
    r.add (knob (p + "Detune", "Detune")).add (knob (p + "Spread", "Spread"))
     .add (knob (p + "Level", "Level")).add (knob (p + "Pan", "Pan"));
    if (withFm) r.add (knob ("fmAmt", "FM from B", colours::accent2));
    else        r.add (knob (p + "Phase", "Phase", colours::accent2));
}

WavesmithEditor::WavesmithEditor (WavesmithProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setSize (baseWidth, baseHeight);

    // --- Top bar
    for (auto* c : std::initializer_list<juce::Component*> { &categoryBox, &presetBox, &prevButton, &nextButton, &saveButton })
        content.addAndMakeVisible (c);
    categoryBox.onChange = [this] { selectCategory (categoryBox.getText()); };
    presetBox.onChange = [this]
    {
        const int sel = presetBox.getSelectedItemIndex();
        if (sel >= 0 && sel < (int) presetIndicesInCategory.size())
            proc.setCurrentProgram (presetIndicesInCategory[(size_t) sel]);
    };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { showSavePrompt(); };

    masterSec = section ("OUTPUT");
    masterSec->addRow().add (knob ("masterVol", "Volume")).add (knob ("ampVel", "Velocity", colours::accent2));

    voiceSec = section ("VOICING");
    voiceSec->addRow().add (choice ("polyMode", "Mode"), 1.3f).add (knob ("voices", "Voices", colours::accent2))
        .add (knob ("glide", "Glide")).add (knob ("bendRange", "Bend", colours::accent2));

    // --- Row 1
    oscA = section ("OSC A");
    buildOscSection (oscA, "oscA", true);
    oscB = section ("OSC B");
    buildOscSection (oscB, "oscB", false);

    subNoise = section ("SUB / NOISE");
    subNoise->addRow (0.8f).add (choice ("subOct", "Sub Octave"));
    subNoise->addRow (0.8f).add (choice ("subShape", "Sub Shape"));
    subNoise->addRow (1.5f).add (knob ("subLevel", "Sub Level"));
    subNoise->addRow (1.5f).add (knob ("noiseLevel", "Noise")).add (knob ("noiseColor", "Color", colours::accent2));

    filter = section ("FILTER");
    filter->setHeaderWidget (toggle ("fltOn", "On"));
    filter->addRow (0.8f).add (choice ("fltType", "Type"));
    filter->addRow (1.6f).add (knob ("fltCutoff", "Cutoff")).add (knob ("fltReso", "Resonance")).add (knob ("fltDrive", "Drive"));
    filter->addRow (1.6f).add (knob ("fltEnvAmt", "Env Amt", colours::accent2)).add (knob ("fltKey", "Key Track", colours::accent2))
        .add (knob ("fltVel", "Velocity", colours::accent2));

    // --- Row 2
    ampEnv = section ("AMP ENVELOPE");
    ampEnv->addRow().add (knob ("ampA", "Attack")).add (knob ("ampD", "Decay")).add (knob ("ampS", "Sustain")).add (knob ("ampR", "Release"));
    modEnv = section ("MOD ENVELOPE");
    modEnv->addRow().add (knob ("envA", "Attack", colours::accent2)).add (knob ("envD", "Decay", colours::accent2))
        .add (knob ("envS", "Sustain", colours::accent2)).add (knob ("envR", "Release", colours::accent2));

    for (int l = 0; l < 2; ++l)
    {
        const juce::String id = l == 0 ? "lfo1" : "lfo2";
        auto* s = section (l == 0 ? "LFO 1" : "LFO 2");
        s->addRow (0.8f).add (choice (id + "Shape", {}), 1.2f).add (toggle (id + "Sync", "Sync"), 0.6f).add (toggle (id + "Retrig", "Retrig"), 0.7f);
        s->addRow (1.6f).add (knob (id + "Rate", "Rate (Hz)")).add (choice (id + "Div", "Sync Rate"));
        (l == 0 ? lfo1 : lfo2) = s;
    }

    // --- Tabs: matrix, sequencer/arp, effects
    {
        auto* matrix = make<Section> ("MODULATION MATRIX");
        for (int r = 0; r < 3; ++r)
        {
            auto& row = matrix->addRow();
            for (int c = 0; c < 2; ++c)
            {
                const int slot = r * 2 + c;
                row.add (choice (ws::matrixId (slot, "Src"), "Source " + juce::String (slot + 1)), 1.0f)
                   .add (choice (ws::matrixId (slot, "Dst"), "Destination"), 1.2f)
                   .add (knob (ws::matrixId (slot, "Amt"), "Amount"), 0.55f);
                if (c == 0) row.add (make<juce::Component>(), 0.15f);
            }
        }
        tabs.addTab ("MODULATION", colours::panel, matrix, false);

        auto* seqTab = make<juce::Component>();
        auto* seq = make<Section> ("STEP SEQUENCER  (Mod-Quelle \"Step Seq\")");
        seq->addRow (3.0f).add (make<StepSeqView> (proc));
        seq->addRow (1.4f).add (choice ("seqDiv", "Rate")).add (knob ("seqLen", "Length", colours::accent2))
            .add (knob ("seqSmooth", "Smooth", colours::accent2)).add (make<juce::Component>(), 2.0f);
        auto* arp = make<Section> ("ARPEGGIATOR");
        arp->setHeaderWidget (toggle ("arpOn", "On"));
        arp->addRow().add (choice ("arpMode", "Mode")).add (choice ("arpDiv", "Rate"));
        arp->addRow (1.6f).add (knob ("arpOct", "Octaves", colours::accent2)).add (knob ("arpGate", "Gate"));
        seqTab->addAndMakeVisible (seq);
        seqTab->addAndMakeVisible (arp);
        struct Split : juce::ComponentListener
        {
            juce::Component *a, *b;
            void componentMovedOrResized (juce::Component& c, bool, bool) override
            {
                auto r = c.getLocalBounds().reduced (4);
                a->setBounds (r.removeFromLeft ((int) (r.getWidth() * 0.68f)));
                r.removeFromLeft (6);
                b->setBounds (r);
            }
        };
        auto* split = new Split();
        split->a = seq; split->b = arp;
        seqTab->addComponentListener (split);
        splitListeners.emplace_back (split);
        tabs.addTab ("SEQUENCER / ARP", colours::panel, seqTab, false);

        auto* fx = make<juce::Component>();
        auto addFx = [&] (Section* s) { fx->addAndMakeVisible (s); return s; };
        auto* dist = addFx (make<Section> ("DISTORTION"));
        dist->setHeaderWidget (toggle ("distOn", "On"));
        dist->addRow().add (knob ("distDrive", "Drive")).add (knob ("distMix", "Mix"));
        auto* chorus = addFx (make<Section> ("CHORUS"));
        chorus->setHeaderWidget (toggle ("chorusOn", "On"));
        chorus->addRow().add (knob ("chorusRate", "Rate")).add (knob ("chorusDepth", "Depth")).add (knob ("chorusMix", "Mix"));
        auto* delay = addFx (make<Section> ("DELAY"));
        delay->setHeaderWidget (toggle ("delayOn", "On"));
        delay->addRow (0.7f).add (choice ("delayDiv", "Time"));
        delay->addRow().add (knob ("delayFb", "Feedback")).add (knob ("delayTone", "Tone")).add (knob ("delayMix", "Mix"));
        auto* reverb = addFx (make<Section> ("REVERB"));
        reverb->setHeaderWidget (toggle ("reverbOn", "On"));
        reverb->addRow().add (knob ("reverbSize", "Size")).add (knob ("reverbDamp", "Damp"))
            .add (knob ("reverbWidth", "Width")).add (knob ("reverbMix", "Mix"));
        struct FxLayout : juce::ComponentListener
        {
            std::vector<std::pair<juce::Component*, float>> items;
            void componentMovedOrResized (juce::Component& c, bool, bool) override
            {
                auto r = c.getLocalBounds().reduced (4);
                float total = 0; for (auto& i : items) total += i.second;
                const float avail = (float) r.getWidth() - 6.0f * (float) (items.size() - 1);
                float x = (float) r.getX();
                for (auto& i : items) { const float w = avail * i.second / total; i.first->setBounds ((int) x, r.getY(), (int) w, r.getHeight()); x += w + 6; }
            }
        };
        auto* fl = new FxLayout();
        fl->items = { { dist, 2.0f }, { chorus, 3.0f }, { delay, 3.0f }, { reverb, 4.0f } };
        fx->addComponentListener (fl);
        splitListeners.emplace_back (fl);
        tabs.addTab ("EFFECTS", colours::panel, fx, false);
    }
    tabs.setTabBarDepth (28);
    tabs.setOutline (0);
    content.addAndMakeVisible (tabs);

    keyboard.setKeyWidth (16.0f);
    keyboard.setLowestVisibleKey (36);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::accent.withAlpha (0.6f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::accent.withAlpha (0.25f));
    content.addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (baseWidth * 2 / 3, baseHeight * 2 / 3, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseWidth / baseHeight);
    setSize (baseWidth, baseHeight);

    refreshPresetLists();
    startTimerHz (10);
}

WavesmithEditor::~WavesmithEditor()
{
    stopTimer();
    tabs.clearTabs();
    sections.clear();
    owned.clear();
    splitListeners.clear();
    setLookAndFeel (nullptr);
}

void WavesmithEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void WavesmithEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, baseWidth, baseHeight);

    auto r = juce::Rectangle<int> (0, 0, baseWidth, baseHeight).reduced (8);

    // Top bar
    auto top = r.removeFromTop (34);
    struct Logo : juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            g.setColour (colours::accent);
            g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
            g.drawText ("WAVESMITH", getLocalBounds(), juce::Justification::centredLeft);
        }
    };
    if (logo == nullptr) { logo = std::make_unique<Logo>(); content.addAndMakeVisible (*logo); }
    logo->setBounds (top.removeFromLeft (170));
    top.removeFromLeft (10);
    categoryBox.setBounds (top.removeFromLeft (130).reduced (0, 4));
    top.removeFromLeft (6);
    prevButton.setBounds (top.removeFromLeft (28).reduced (0, 4));
    top.removeFromLeft (4);
    presetBox.setBounds (top.removeFromLeft (290).reduced (0, 4));
    top.removeFromLeft (4);
    nextButton.setBounds (top.removeFromLeft (28).reduced (0, 4));
    top.removeFromLeft (6);
    saveButton.setBounds (top.removeFromLeft (60).reduced (0, 4));
    r.removeFromTop (6);

    keyboard.setBounds (r.removeFromBottom (62));
    r.removeFromBottom (6);

    auto placeRow = [] (juce::Rectangle<int> area, std::initializer_list<std::pair<juce::Component*, float>> items)
    {
        float total = 0; for (auto& i : items) total += i.second;
        const float avail = (float) area.getWidth() - 6.0f * (float) (items.size() - 1);
        float x = (float) area.getX();
        for (auto& i : items) { const float w = avail * i.second / total; i.first->setBounds ((int) x, area.getY(), (int) w, area.getHeight()); x += w + 6; }
    };

    placeRow (r.removeFromTop (250), { { oscA, 3.3f }, { oscB, 3.3f }, { subNoise, 1.35f }, { filter, 2.7f } });
    r.removeFromTop (6);
    placeRow (r.removeFromTop (128), { { ampEnv, 2.6f }, { modEnv, 2.6f }, { lfo1, 1.8f }, { lfo2, 1.8f } });
    r.removeFromTop (6);
    auto bottom = r;
    auto side = bottom.removeFromRight (250);
    bottom.removeFromRight (6);
    tabs.setBounds (bottom);
    masterSec->setBounds (side.removeFromTop (side.getHeight() / 2 - 3));
    side.removeFromTop (6);
    voiceSec->setBounds (side);
}

//==============================================================================
void WavesmithEditor::refreshPresetLists()
{
    const auto cats = proc.presets.getCategories();
    categoryBox.clear (juce::dontSendNotification);
    categoryBox.addItemList (cats, 1);

    const int current = proc.presets.getCurrentIndex();
    juce::String cat = current >= 0 ? proc.presets.getPreset (current).category : cats[0];
    categoryBox.setSelectedItemIndex (std::max (0, cats.indexOf (cat)), juce::dontSendNotification);
    selectCategory (cat);
}

void WavesmithEditor::selectCategory (const juce::String& cat)
{
    presetIndicesInCategory.clear();
    presetBox.clear (juce::dontSendNotification);
    const int current = proc.presets.getCurrentIndex();
    int selected = -1;
    for (int i = 0; i < proc.presets.getNumPresets(); ++i)
    {
        const auto& p = proc.presets.getPreset (i);
        if (p.category != cat) continue;
        if (i == current) selected = (int) presetIndicesInCategory.size();
        presetIndicesInCategory.push_back (i);
        presetBox.addItem (p.name, (int) presetIndicesInCategory.size());
    }
    if (selected >= 0)
        presetBox.setSelectedItemIndex (selected, juce::dontSendNotification);
    else
        presetBox.setText ("- " + juce::String ((int) presetIndicesInCategory.size()) + " Presets -", juce::dontSendNotification);
}

void WavesmithEditor::stepPreset (int delta)
{
    if (presetIndicesInCategory.empty()) return;
    int sel = presetBox.getSelectedItemIndex();
    const int n = (int) presetIndicesInCategory.size();
    sel = sel < 0 ? 0 : (sel + delta + n) % n;
    proc.setCurrentProgram (presetIndicesInCategory[(size_t) sel]);
    presetBox.setSelectedItemIndex (sel, juce::dontSendNotification);
}

void WavesmithEditor::showSavePrompt()
{
    auto* w = new juce::AlertWindow ("Preset speichern", "Name fuer das neue Preset:", juce::MessageBoxIconType::NoIcon, this);
    const int cur = proc.presets.getCurrentIndex();
    w->addTextEditor ("name", cur >= 0 ? proc.presets.getPreset (cur).name + " Copy" : "My Preset");
    w->addButton ("Speichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<WavesmithEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result == 1 && safe != nullptr)
        {
            const auto name = w->getTextEditorContents ("name").trim();
            if (safe->proc.presets.saveUserPreset (name))
            {
                ++safe->proc.presetChangeCounter;
                safe->refreshPresetLists();
            }
        }
    }), true);
}

void WavesmithEditor::timerCallback()
{
    const int c = proc.presetChangeCounter.load();
    if (c != lastPresetCounter)
    {
        lastPresetCounter = c;
        refreshPresetLists();
    }
}
