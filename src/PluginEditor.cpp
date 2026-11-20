#include "PluginEditor.h"
#include <cmath>
#include <numeric>

using namespace shaker;

namespace
{
const int W = 1060, H = 880;
const int rowsY = 166, rowH = 100, detailY = 570;

void fillSampleBox (juce::ComboBox& box, SampleLibrary& lib)
{
    box.clear (juce::dontSendNotification);
    for (auto* cat : { "Shaker", "Cabasa", "Maracas", "Eigene" })
    {
        std::vector<int> ids;
        for (int i = 0; i < lib.size(); ++i)
            if (lib.get (i)->cat == cat && ! lib.get (i)->hidden.load()) ids.push_back (i);
        if (ids.empty()) continue;
        std::sort (ids.begin(), ids.end(), [&] (int a, int b) { return lib.get (a)->name.compareNatural (lib.get (b)->name) < 0; });
        box.addSectionHeading (cat);
        for (int i : ids) box.addItem (lib.get (i)->name, i + 1);
    }
}

juce::String fracText (long n, long d)
{
    const long g = std::gcd (n, d);
    n /= g; d /= g;
    return d == 1 ? juce::String (n) : juce::String (n) + "/" + juce::String (d);
}

juce::String msText (double s) { return s < 1.0 ? juce::String (s * 1000.0, s < 0.01 ? 1 : 0) + " ms" : juce::String (s, 2) + " s"; }

void styleLabel (juce::Label& l, float size, juce::Colour c, juce::Justification j = juce::Justification::centredLeft)
{
    l.setFont (ui::font (size));
    l.setColour (juce::Label::textColourId, c);
    l.setJustificationType (j);
    l.setInterceptsMouseClicks (false, false);
}
} // namespace

// ================= LookAndFeel =================
ShakerLook::ShakerLook()
{
    setColour (juce::ResizableWindow::backgroundColourId, ui::bg);
    setColour (juce::TextButton::buttonColourId, ui::panel2);
    setColour (juce::TextButton::buttonOnColourId, ui::track[0]);
    setColour (juce::TextButton::textColourOffId, ui::text);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1a1410));
    setColour (juce::ComboBox::backgroundColourId, ui::panel2);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::outlineColourId, ui::line);
    setColour (juce::ComboBox::arrowColourId, ui::mute);
    setColour (juce::PopupMenu::backgroundColourId, ui::panel);
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::headerTextColourId, ui::track[0]);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::panel2);
    setColour (juce::PopupMenu::highlightedTextColourId, ui::track[0]);
    setColour (juce::Label::textColourId, ui::text);
    setColour (juce::ToggleButton::textColourId, ui::mute);
    setColour (juce::ToggleButton::tickColourId, ui::track[0]);
    setColour (juce::ToggleButton::tickDisabledColourId, ui::mute);
    setColour (juce::Slider::textBoxTextColourId, ui::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void ShakerLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto acc = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float size = (float) juce::jmin (w, h) - 4.0f;
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f;
    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, a1, true);
    g.setColour (ui::line);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bip = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float zeroPos = bip ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
    const float az = a0 + zeroPos * (a1 - a0), av = a0 + pos * (a1 - a0);
    if (std::abs (av - az) > 0.01f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, r - 2, r - 2, 0, juce::jmin (az, av), juce::jmax (az, av), true);
        g.setColour (acc);
        g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float kr = r * 0.66f;
    g.setColour (s.isMouseButtonDown() ? juce::Colour (0xff2f2a24) : ui::panel2);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (juce::Colour (0xff0d0c0a));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.2f);
    juce::Point<float> tip (c.x + std::sin (av) * kr * 0.8f, c.y - std::cos (av) * kr * 0.8f);
    g.setColour (ui::text);
    g.drawLine (c.x, c.y, tip.x, tip.y, 2.2f);
}

void ShakerLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto base = on ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
    if (over && ! on) base = base.brighter (0.08f);
    if (down) base = base.darker (0.15f);
    g.setColour (base);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (on ? base : ui::line);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
}

juce::Label* ShakerLook::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (ui::font (11.5f));
    l->setColour (juce::Label::textColourId, ui::text);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setJustificationType (juce::Justification::centred);
    return l;
}

void ShakerLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (ui::line);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    juce::Path p;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (ui::mute);
    g.fillPath (p);
}

// ================= Knob =================
Knob::Knob (const juce::String& label, juce::Colour accent)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 78, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setMouseDragSensitivity (180);
    slider.setVelocityBasedMode (false);
    addAndMakeVisible (slider);
    name.setText (label, juce::dontSendNotification);
    styleLabel (name, 11.5f, ui::mute, juce::Justification::centred);
    addAndMakeVisible (name);
}

void Knob::attach (juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    att.reset();
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    name.setBounds (r.removeFromBottom (16));
    slider.setBounds (r);
}

// ================= StepGrid =================
StepGrid::StepGrid (ShakerProcessor& p, int track) : proc (p), t (track) {}

juce::Rectangle<float> StepGrid::gridArea() const
{
    auto r = getLocalBounds().toFloat();
    return r.withWidth (juce::jmax (r.getWidth() * 0.18f, r.getWidth() * widthFrac));
}

int StepGrid::idxAt (float x) const
{
    const int steps = juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "steps")));
    auto a = gridArea();
    return juce::jlimit (0, steps - 1, (int) std::floor ((x - a.getX()) / a.getWidth() * steps));
}

float StepGrid::velAt (float y) const
{
    auto a = gridArea();
    return std::round (juce::jlimit (0.05f, 1.0f, 1.0f - (y - a.getY()) / a.getHeight()) * 100.0f) / 100.0f;
}

void StepGrid::paint (juce::Graphics& g)
{
    const int steps = juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "steps")));
    const int mode = juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "mode")));
    const int rate = juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "rate")));
    const double sb = cycleBeats (steps, mode, rate) / steps;
    auto a = gridArea();
    const float gap = steps > 20 ? 2.0f : 3.0f;
    const float cw = (a.getWidth() - gap * (steps - 1)) / steps;
    const auto acc = ui::track[t];
    for (int i = 0; i < steps; ++i)
    {
        juce::Rectangle<float> c (a.getX() + i * (cw + gap), a.getY(), cw, a.getHeight());
        const double pos = i * sb;
        const bool beat = std::abs (pos - std::round (pos)) < 1e-6;
        g.setColour (beat ? juce::Colour (0xff2d2823) : ui::panel2);
        g.fillRoundedRectangle (c, 4.0f);
        const float v = proc.engine.shared[t].pat[(size_t) i].load();
        if (v > 0)
        {
            auto f = c.withTop (c.getBottom() - c.getHeight() * v);
            g.setColour (acc.withAlpha (v < 0.35f ? 0.45f : (i == shownStep ? 1.0f : 0.85f)));
            g.fillRoundedRectangle (f, 3.0f);
        }
        if (i == shownStep)
        {
            g.setColour (ui::text);
            g.drawRoundedRectangle (c.reduced (0.5f), 4.0f, 1.2f);
        }
    }
    if (juce::Time::getMillisecondCounterHiRes() < infoUntil)
    {
        g.setFont (ui::font (11.0f));
        auto r = juce::Rectangle<float> (getWidth() - 130.0f, 0, 128, 15);
        g.setColour (ui::bg.withAlpha (0.85f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (ui::text);
        g.drawText (info, r, juce::Justification::centred);
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    downPos = e.position;
    downIdx = idxAt (e.position.x);
    levelMode = false;
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (! levelMode && (std::abs (e.position.y - downPos.y) > 5 || std::abs (e.position.x - downPos.x) > 10))
        levelMode = true;
    if (! levelMode) return;
    const int i = idxAt (e.position.x);
    const float v = velAt (e.position.y);
    proc.engine.shared[t].pat[(size_t) i].store (v);
    lastVel = v;
    info = "Schritt " + juce::String (i + 1) + ": " + juce::String (juce::roundToInt (v * 100)) + "%";
    infoUntil = juce::Time::getMillisecondCounterHiRes() + 900;
    repaint();
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    if (! levelMode)
    {
        auto& a = proc.engine.shared[t].pat[(size_t) downIdx];
        const float v = a.load() > 0 ? 0.0f : lastVel;
        a.store (v);
        info = "Schritt " + juce::String (downIdx + 1) + ": " + (v > 0 ? juce::String (juce::roundToInt (v * 100)) + "%" : juce::String ("aus"));
        infoUntil = juce::Time::getMillisecondCounterHiRes() + 900;
    }
    levelMode = false;
    repaint();
}

// ================= TrackRow =================
TrackRow::TrackRow (ShakerProcessor& p, int track, std::function<void (int)> sel, std::function<void (int)> ed)
    : grid (p, track), proc (p), t (track), onSelect (std::move (sel)), onEdit (std::move (ed))
{
    const auto acc = ui::track[t];
    addAndMakeVisible (grid);
    addAndMakeVisible (sampleBox);
    sampleBox.onChange = [this] { if (sampleBox.getSelectedId() > 0) { proc.setSample (t, sampleBox.getSelectedId() - 1); onSelect (t); } };
    for (auto* b : { &edit, &aud, &muteB, &soloB, &dice, &minus, &plus, &pm, &pr })
    {
        addAndMakeVisible (*b);
        b->setColour (juce::TextButton::buttonOnColourId, acc);
    }
    muteB.setClickingTogglesState (true);
    soloB.setClickingTogglesState (true);
    muteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ShakerProcessor::tid (t, "mute"), muteB);
    soloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ShakerProcessor::tid (t, "solo"), soloB);
    edit.onClick = [this] { onEdit (t); };
    aud.onClick = [this] { proc.engine.shared[t].audition.store (1); onSelect (t); };
    dice.onClick = [this] { proc.genTrack (t); grid.repaint(); };
    minus.onClick = [this] { proc.setSteps (t, juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "steps"))) - 1); };
    plus.onClick = [this] { proc.setSteps (t, juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "steps"))) + 1); };
    pm.onClick = [this] { proc.setParam (ShakerProcessor::tid (t, "mode"), 0.0f); };
    pr.onClick = [this] { proc.setParam (ShakerProcessor::tid (t, "mode"), 1.0f); };
    edit.setTooltip ("Sample-Editor");
    dice.setTooltip ("Pattern dieser Spur neu wuerfeln");

    for (int i = 0; i < kNumRates; ++i) rateBox.addItem (kRates[i].label, i + 1);
    addAndMakeVisible (rateBox);
    rateAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ShakerProcessor::tid (t, "rate"), rateBox);

    styleLabel (stepsLbl, 13.0f, ui::text, juce::Justification::centred);
    styleLabel (cycLbl, 12.0f, ui::mute);
    styleLabel (roleLbl, 10.5f, ui::mute, juce::Justification::centred);
    roleLbl.setText (Generator::roleNames[t], juce::dontSendNotification);
    for (auto* l : { &stepsLbl, &cycLbl, &roleLbl }) addAndMakeVisible (*l);
    refreshSamples();
    refresh();
}

void TrackRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (selected ? ui::track[t].withAlpha (0.9f) : ui::line);
    g.drawRoundedRectangle (r, 12.0f, selected ? 1.6f : 1.0f);
    g.setColour (ui::track[t]);
    g.fillRoundedRectangle (r.withWidth (5.0f), 2.5f);
    // LED + Nummer
    const bool led = juce::Time::getMillisecondCounterHiRes() < ledUntil;
    g.setColour (led ? ui::track[t] : ui::line);
    g.fillEllipse (14, 11, 11, 11);
    g.setColour (ui::track[t]);
    g.setFont (ui::font (15.0f, true));
    g.drawText (juce::String (t + 1), 28, 4, 16, 26, juce::Justification::centredLeft);
}

void TrackRow::resized()
{
    auto r = getLocalBounds().reduced (10, 4);
    auto top = r.removeFromTop (28);
    top.removeFromLeft (36);
    sampleBox.setBounds (top.removeFromLeft (230).reduced (0, 1));
    top.removeFromLeft (6);
    edit.setBounds (top.removeFromLeft (46).reduced (0, 1)); top.removeFromLeft (4);
    aud.setBounds (top.removeFromLeft (46).reduced (0, 1)); top.removeFromLeft (4);
    muteB.setBounds (top.removeFromLeft (30).reduced (0, 1)); top.removeFromLeft (4);
    soloB.setBounds (top.removeFromLeft (30).reduced (0, 1)); top.removeFromLeft (4);
    dice.setBounds (top.removeFromLeft (46).reduced (0, 1)); top.removeFromLeft (16);
    minus.setBounds (top.removeFromLeft (26).reduced (0, 1));
    stepsLbl.setBounds (top.removeFromLeft (30));
    plus.setBounds (top.removeFromLeft (26).reduced (0, 1)); top.removeFromLeft (10);
    pm.setBounds (top.removeFromLeft (38).reduced (0, 1)); top.removeFromLeft (2);
    pr.setBounds (top.removeFromLeft (38).reduced (0, 1)); top.removeFromLeft (8);
    rateBox.setBounds (top.removeFromLeft (72).reduced (0, 1)); top.removeFromLeft (8);
    roleLbl.setBounds (top.removeFromRight (64));
    cycLbl.setBounds (top);
    r.removeFromTop (6);
    grid.setBounds (r.withTrimmedLeft (4));
}

void TrackRow::mouseDown (const juce::MouseEvent&) { onSelect (t); }

void TrackRow::refreshSamples()
{
    fillSampleBox (sampleBox, proc.lib);
    refresh();
}

void TrackRow::refresh()
{
    sampleBox.setSelectedId (proc.engine.shared[t].sample.load() + 1, juce::dontSendNotification);
    lastSteps = -1;
    tick();
    grid.repaint();
}

void TrackRow::tick()
{
    const int steps = juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "steps")));
    const int mode = juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "mode")));
    const int rate = juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "rate")));
    if (steps != lastSteps || mode != lastMode || rate != lastRate)
    {
        lastSteps = steps; lastMode = mode; lastRate = rate;
        stepsLbl.setText (juce::String (steps), juce::dontSendNotification);
        pm.setToggleState (mode == 0, juce::dontSendNotification);
        pr.setToggleState (mode == 1, juce::dontSendNotification);
        long n, d;
        cycleFraction (steps, mode, rate, n, d);
        cycLbl.setText ("= " + fracText (n, d * 4) + (n <= d * 4 ? " Takt" : " Takte"), juce::dontSendNotification);
        grid.repaint();
    }
    auto& sh = proc.engine.shared[t];
    const int cur = sh.curStep.load();
    const unsigned int hits = sh.hits.load();
    if (hits != lastHits) { lastHits = hits; ledUntil = juce::Time::getMillisecondCounterHiRes() + 70; repaint (0, 0, 50, 34); }
    else if (ledUntil > 0 && juce::Time::getMillisecondCounterHiRes() > ledUntil) { ledUntil = 0; repaint (0, 0, 50, 34); }
    if (cur != grid.shownStep || sh.patternChanged.exchange (false)) { grid.shownStep = cur; grid.repaint(); }
}

// ================= DetailPanel =================
DetailPanel::DetailPanel (ShakerProcessor& p) : proc (p)
{
    struct Def { const char* id; const char* label; int tab; int group; bool bip; };
    static const Def defs[] = {
        { "gain", "Gain", 0, 0, true }, { "tune", "Tune", 0, 0, true }, { "fine", "Finetune", 0, 0, true },
        { "attack", "Attack", 0, 1, false }, { "decay", "Decay", 0, 1, false }, { "sustain", "Sustain", 0, 1, false }, { "release", "Release", 0, 1, false },
        { "lowcut", "Low Cut", 0, 2, false }, { "highcut", "High Cut", 0, 2, false }, { "reso", "Resonanz", 0, 2, false }, { "bodyf", "Korpus-Freq.", 0, 2, false },
        { "body", "Korpus", 0, 2, true }, { "air", "Glanz", 0, 2, true }, { "drive", "Saettigung", 0, 2, false }, { "comp", "Kompressor", 0, 2, false },
        { "width", "Stereo-Breite", 0, 2, false },
        { "vol", "Lautstaerke", 1, 0, false }, { "pan", "Panorama", 1, 0, true }, { "rev", "Hall-Send", 1, 0, false },
        { "rprob", "Wuerfel-Hits", 2, 0, false }, { "rtime", "Timing", 2, 0, false }, { "rvel", "Dynamik", 2, 0, false }, { "rpitch", "Pitch-Zufall", 2, 0, false },
        { "rlen", "Huellk.-Zufall", 2, 0, false }, { "rstart", "Start-Zufall", 2, 0, false }, { "rrev", "Reverse-Chance", 2, 0, false },
        { "rpan", "Stereo-Streuung", 2, 0, false }, { "rratchet", "Ratchets", 2, 0, false },
    };
    for (auto& d : defs)
    {
        Item it { std::make_unique<Knob> (d.label, ui::track[0]), d.id, d.tab, d.group };
        it.knob->slider.getProperties().set ("bipolar", d.bip);
        addChildComponent (*it.knob);
        items.push_back (std::move (it));
    }
    for (int i = 0; i < 3; ++i)
    {
        addAndMakeVisible (tabs[i]);
        tabs[i].onClick = [this, i] { showTab (i); };
    }
    addAndMakeVisible (resetB);
    resetB.onClick = [this] { proc.resetSound (track); };
    setTrack (0);
    showTab (0);
}

void DetailPanel::setTrack (int t)
{
    track = t;
    for (auto& it : items)
    {
        it.knob->slider.setColour (juce::Slider::rotarySliderFillColourId, ui::track[t]);
        it.knob->attach (proc.apvts, ShakerProcessor::tid (t, it.id));
    }
    for (auto& b : tabs) b.setColour (juce::TextButton::buttonOnColourId, ui::track[t]);
    repaint();
}

void DetailPanel::showTab (int i)
{
    tab = i;
    for (int k = 0; k < 3; ++k) tabs[k].setToggleState (k == i, juce::dontSendNotification);
    for (auto& it : items) it.knob->setVisible (it.tab == i);
    resetB.setVisible (i == 0);
    resized();
    repaint();
}

void DetailPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (ui::line);
    g.drawRoundedRectangle (r, 12.0f, 1.0f);
    g.setColour (ui::track[track]);
    g.setFont (ui::font (14.0f, true));
    g.drawText ("Spur " + juce::String (track + 1), getWidth() - 330, 8, 100, 24, juce::Justification::centredRight);
    if (tab == 0)
    {
        g.setFont (ui::font (11.0f, true));
        g.drawText ("SAMPLE", 20, 44, 200, 14, juce::Justification::left);
        g.drawText ("HUELLKURVE (ADSR)", 20 + 3 * 82 + 30, 44, 200, 14, juce::Justification::left);
        g.drawText ("KLANGFORMUNG", 20, 160, 200, 14, juce::Justification::left);
    }
}

void DetailPanel::resized()
{
    for (int i = 0; i < 3; ++i) tabs[i].setBounds (14 + i * 86, 8, 80, 26);
    resetB.setBounds (getWidth() - 214, 8, 200, 26);
    const int kw = 82, kh = 96;
    int col[3] = { 0, 0, 0 };
    for (auto& it : items)
    {
        if (it.tab != tab) continue;
        int x, y;
        if (tab == 0)
        {
            if (it.group == 0) { x = 14 + col[0]++ * kw; y = 58; }
            else if (it.group == 1) { x = 14 + 3 * kw + 30 + col[1]++ * kw; y = 58; }
            else { x = 14 + col[2]++ * kw; y = 174; }
        }
        else { x = 14 + col[0]++ * kw; y = 50; }
        it.knob->setBounds (x, y, kw - 4, kh);
    }
}

// ================= Sample-Editor =================
SampleEditorView::SampleEditorView (ShakerProcessor& p) : proc (p)
{
    styleLabel (title, 18.0f, ui::text);
    styleLabel (sVal, 13.0f, ui::track[2], juce::Justification::centred);
    styleLabel (eVal, 13.0f, ui::track[1], juce::Justification::centred);
    styleLabel (meta, 12.0f, ui::mute);
    for (auto* l : { &title, &sVal, &eVal, &meta }) addAndMakeVisible (*l);
    addAndMakeVisible (sampleBox);
    for (auto* b : { &load, &del, &closeB, &zSel, &zAll, &zIn, &zOut, &autoB, &resetB, &prev, &sL, &sR, &eL, &eR })
        addAndMakeVisible (*b);

    sampleBox.onChange = [this]
    {
        if (sampleBox.getSelectedId() <= 0) return;
        proc.setSample (t, sampleBox.getSelectedId() - 1);
        zoomAll();
        proc.engine.shared[t].audition.store (2);
    };
    closeB.onClick = [this] { if (onClose) onClose(); };
    zAll.onClick = [this] { zoomAll(); };
    zSel.onClick = [this]
    {
        auto& sh = proc.engine.shared[t];
        const double a = sh.start.load(), b = sh.end.load(), pad = (b - a) * 0.08;
        if (auto* s = sample()) { v0 = juce::jmax (0.0, a - pad); v1 = juce::jmin (s->lengthSeconds(), b + pad); }
        repaint();
    };
    auto zoomBy = [this] (double f)
    {
        auto* s = sample(); if (! s) return;
        auto& sh = proc.engine.shared[t];
        const double c = (sh.start.load() + sh.end.load()) * 0.5, D = s->lengthSeconds();
        const double w = juce::jlimit (0.004, D, (v1 - v0) * f);
        v0 = juce::jlimit (0.0, D - w, c - w / 2); v1 = v0 + w;
        repaint();
    };
    zIn.onClick = [zoomBy] { zoomBy (0.5); };
    zOut.onClick = [zoomBy] { zoomBy (2.0); };
    autoB.onClick = [this]
    {
        if (auto* s = sample())
        {
            auto& sh = proc.engine.shared[t];
            proc.setRegion (t, juce::jmin (s->onset, sh.end.load() - 0.003f), sh.end.load());
            repaint();
        }
    };
    resetB.onClick = [this] { proc.setSample (t, proc.engine.shared[t].sample.load()); zoomAll(); };
    prev.onClick = [this] { proc.engine.shared[t].audition.store (2); };
    auto nudge = [this] (bool startMarker, int dir)
    {
        auto* s = sample(); if (! s) return;
        auto& sh = proc.engine.shared[t];
        const float d = dir * (float) juce::jmax (0.0005, (v1 - v0) / 200.0);
        float a = sh.start.load(), b = sh.end.load();
        if (startMarker) a = juce::jlimit (0.0f, b - 0.003f, a + d);
        else b = juce::jlimit (a + 0.003f, (float) s->lengthSeconds(), b + d);
        proc.setRegion (t, a, b);
        repaint();
    };
    sL.onClick = [nudge] { nudge (true, -1); };
    sR.onClick = [nudge] { nudge (true, 1); };
    eL.onClick = [nudge] { nudge (false, -1); };
    eR.onClick = [nudge] { nudge (false, 1); };
    load.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Eigene Samples laden", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.caf");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto files = fc.getResults();
                                  if (files.isEmpty()) return;
                                  const int idx = proc.loadCustomFiles (files);
                                  if (idx >= 0)
                                  {
                                      proc.setSample (t, idx);
                                      zoomAll();
                                      proc.engine.shared[t].audition.store (2);
                                  }
                              });
    };
    del.onClick = [this]
    {
        proc.removeCustom (proc.engine.shared[t].sample.load());
        zoomAll();
    };
}

const SampleData* SampleEditorView::sample() const { return proc.lib.get (proc.engine.shared[t].sample.load()); }

void SampleEditorView::open (int track)
{
    t = track;
    title.setText ("Sample-Editor  -  Spur " + juce::String (t + 1), juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, ui::track[t]);
    for (auto* b : { &prev })
        b->setColour (juce::TextButton::buttonColourId, ui::track[t].withAlpha (0.35f));
    refreshSamples();
    zoomAll();
}

void SampleEditorView::refreshSamples()
{
    fillSampleBox (sampleBox, proc.lib);
    sampleBox.setSelectedId (proc.engine.shared[t].sample.load() + 1, juce::dontSendNotification);
}

void SampleEditorView::zoomAll()
{
    if (auto* s = sample()) { v0 = 0; v1 = s->lengthSeconds(); }
    sampleBox.setSelectedId (proc.engine.shared[t].sample.load() + 1, juce::dontSendNotification);
    repaint();
}

juce::Rectangle<float> SampleEditorView::waveArea() const { return { 16.0f, 80.0f, (float) getWidth() - 32.0f, 220.0f }; }

void SampleEditorView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (ui::track[t]);
    g.drawRoundedRectangle (r, 14.0f, 1.5f);

    auto wa = waveArea();
    g.setColour (ui::wave);
    g.fillRoundedRectangle (wa, 8.0f);
    auto* s = sample();
    if (s == nullptr) return;
    auto& sh = proc.engine.shared[t];
    const double start = sh.start.load(), end = sh.end.load();
    const auto X = [&] (double tt) { return wa.getX() + (float) ((tt - v0) / (v1 - v0)) * wa.getWidth(); };

    // Zeitraster
    const double span = v1 - v0;
    double stepT = 1.0;
    for (double c : { 0.001, 0.005, 0.01, 0.05, 0.1, 0.25, 0.5, 1.0 }) if (span / c < 12) { stepT = c; break; }
    g.setFont (ui::font (10.0f));
    for (double gt = std::ceil (v0 / stepT) * stepT; gt < v1; gt += stepT)
    {
        const float x = X (gt);
        g.setColour (juce::Colour (0xff5c544a));
        g.fillRect (x, wa.getY(), 1.0f, 6.0f);
        g.drawText (juce::String (juce::roundToInt (gt * 1000)) + "ms", (int) x + 3, (int) wa.getY() + 2, 60, 12, juce::Justification::left);
    }
    g.setColour (juce::Colour (0xff2a2621));
    g.fillRect (wa.getX(), wa.getCentreY(), wa.getWidth(), 1.0f);

    // Wellenform (Wurzel-Skalierung, damit leise Ausklänge sichtbar sind)
    const int n = s->buf.getNumSamples(), nc = s->buf.getNumChannels();
    const double spp = (v1 - v0) * s->sr / wa.getWidth();
    const float gain = 1.0f / juce::jmax (0.05f, s->peak);
    g.setColour (ui::track[t]);
    for (int px = 0; px < (int) wa.getWidth(); ++px)
    {
        const int a = (int) (v0 * s->sr + px * spp), b = juce::jmax (a + 1, (int) (v0 * s->sr + (px + 1) * spp));
        const int stride = juce::jmax (1, (b - a) / 400);
        float lo = 1, hi = -1;
        for (int i = juce::jmax (0, a); i < juce::jmin (n, b); i += stride)
        {
            float v = 0;
            for (int c = 0; c < nc; ++c) v += s->buf.getSample (c, i);
            v /= (float) nc;
            lo = juce::jmin (lo, v); hi = juce::jmax (hi, v);
        }
        if (hi < lo) continue;
        auto sc = [gain] (float v) { return (v < 0 ? -1.0f : 1.0f) * std::sqrt (juce::jmin (1.0f, std::abs (v) * gain)); };
        const float y0 = wa.getCentreY() - sc (hi) * wa.getHeight() * 0.46f;
        const float y1 = wa.getCentreY() - sc (lo) * wa.getHeight() * 0.46f;
        g.fillRect (wa.getX() + px, y0, 1.0f, juce::jmax (1.0f, y1 - y0));
    }
    // außerhalb der Auswahl abdunkeln
    const float xs = juce::jlimit (wa.getX(), wa.getRight(), X (start)), xe = juce::jlimit (wa.getX(), wa.getRight(), X (end));
    g.setColour (juce::Colour (0xb80a0908));
    g.fillRect (wa.getX(), wa.getY(), xs - wa.getX(), wa.getHeight());
    g.fillRect (xe, wa.getY(), wa.getRight() - xe, wa.getHeight());
    auto marker = [&] (float x, juce::Colour c, bool left)
    {
        g.setColour (c);
        g.fillRect (x - 1.0f, wa.getY(), 2.0f, wa.getHeight());
        juce::Path tri;
        if (left) tri.addTriangle (x, wa.getBottom(), x + 13, wa.getBottom(), x, wa.getBottom() - 15);
        else tri.addTriangle (x, wa.getBottom(), x - 13, wa.getBottom(), x, wa.getBottom() - 15);
        g.fillPath (tri);
    };
    marker (X (start), ui::track[2], true);
    marker (X (end), ui::track[1], false);

    // Abspielposition beim Vorhören
    const double el = (juce::Time::getMillisecondCounterHiRes() - sh.previewStartMs.load()) / 1000.0 * sh.previewRate.load();
    if (el >= 0 && el < end - start)
    {
        g.setColour (ui::text);
        g.fillRect (X (start + el), wa.getY(), 1.5f, wa.getHeight());
    }
    sVal.setText (msText (start), juce::dontSendNotification);
    eVal.setText (msText (end), juce::dontSendNotification);
    meta.setText (s->name + "  -  " + msText (s->lengthSeconds()) + "  -  " + (nc == 1 ? "mono" : "stereo") + "  -  Ausschnitt " + msText (end - start),
                  juce::dontSendNotification);
    del.setVisible (s->custom);
}

void SampleEditorView::resized()
{
    title.setBounds (18, 10, 500, 26);
    closeB.setBounds (getWidth() - 110, 10, 94, 28);
    sampleBox.setBounds (16, 44, 320, 28);
    load.setBounds (344, 44, 120, 28);
    del.setBounds (470, 44, 90, 28);
    meta.setBounds (16, 304, getWidth() - 32, 18);
    int x = 16;
    const int y = 330;
    auto place = [&x, y] (juce::Component& c, int w) { c.setBounds (x, y, w, 28); x += w + 4; };
    place (sL, 28); place (sVal, 80); place (sR, 28); x += 12;
    place (eL, 28); place (eVal, 80); place (eR, 28); x += 20;
    place (zSel, 76); place (zAll, 60); place (zIn, 32); place (zOut, 32); x += 20;
    place (autoB, 92); place (resetB, 70);
    prev.setBounds (16, 372, 220, 34);
}

void SampleEditorView::mouseDown (const juce::MouseEvent& e)
{
    if (! waveArea().contains (e.position)) { drag = 0; return; }
    auto& sh = proc.engine.shared[t];
    auto wa = waveArea();
    const double tt = v0 + (e.position.x - wa.getX()) / wa.getWidth() * (v1 - v0);
    drag = std::abs (tt - sh.start.load()) <= std::abs (tt - sh.end.load()) ? 1 : 2;
    moveMarker (e.position.x);
}

void SampleEditorView::mouseDrag (const juce::MouseEvent& e) { if (drag) moveMarker (e.position.x); }

void SampleEditorView::moveMarker (float x)
{
    auto* s = sample(); if (! s) return;
    auto wa = waveArea();
    auto& sh = proc.engine.shared[t];
    const double tt = v0 + juce::jlimit (0.0f, 1.0f, (x - wa.getX()) / wa.getWidth()) * (v1 - v0);
    float a = sh.start.load(), b = sh.end.load();
    if (drag == 1) a = (float) juce::jlimit (0.0, (double) b - 0.003, tt);
    else b = (float) juce::jlimit ((double) a + 0.003, s->lengthSeconds(), tt);
    proc.setRegion (t, a, b);
    repaint();
}

void SampleEditorView::tick()
{
    auto& sh = proc.engine.shared[t];
    const double el = (juce::Time::getMillisecondCounterHiRes() - sh.previewStartMs.load()) / 1000.0;
    if (el > -0.1 && el < 5.0) repaint (waveArea().toNearestInt().expanded (2));
}

// ================= Export-Kachel =================
ExportTile::ExportTile (ShakerProcessor& p) : proc (p)
{
    addChildComponent (finder);
    finder.onClick = [this] { if (proc.exportFile.existsAsFile()) proc.exportFile.revealToUser(); };
}

void ExportTile::resized() { finder.setBounds (10, getHeight() - 30, getWidth() - 20, 24); }

void ExportTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const int st = proc.exportState.load();
    const auto acc = ui::track[2];
    g.setColour (st == 2 ? acc.withAlpha (0.16f) : ui::panel2);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (st == 2 ? acc : ui::line);
    if (st == 2)
    {
        juce::Path border;
        border.addRoundedRectangle (r, 10.0f);
        const float dash[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (border, border, dash, 2);
        g.fillPath (border);
    }
    else g.drawRoundedRectangle (r, 10.0f, 1.0f);

    auto txt = r.reduced (10, 6);
    if (st == 0 || st == 3)
    {
        g.setColour (ui::text);
        g.setFont (ui::font (13.0f, true));
        g.drawText (st == 3 ? "Export fehlgeschlagen" : "Loop-Export", txt.removeFromTop (18), juce::Justification::left);
        g.setColour (ui::mute);
        g.setFont (ui::font (11.5f));
        const double beats = proc.loopBeats (true), bpm = proc.exportBpm();
        g.drawFittedText ("Export rendert " + juce::String (beats / 4.0, std::abs (beats / 4.0 - std::round (beats / 4.0)) < 1e-6 ? 0 : 2)
                          + " Takte bei " + juce::String (bpm, 1) + " BPM als WAV, bis sich alle Spuren wiederholen.",
                          txt.toNearestInt(), juce::Justification::topLeft, 4);
    }
    else if (st == 1)
    {
        g.setColour (ui::text);
        g.setFont (ui::font (13.0f, true));
        g.drawText ("Rendere ...", txt.removeFromTop (18), juce::Justification::left);
        g.setColour (ui::mute);
        g.setFont (ui::font (11.5f));
        g.drawText (proc.exportInfo, txt.removeFromTop (16), juce::Justification::left);
        auto bar = txt.removeFromTop (26).withSizeKeepingCentre (txt.getWidth(), 8);
        g.setColour (ui::line);
        g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (acc);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * proc.exportProgress.load()), 4.0f);
    }
    else
    {
        g.setColour (acc);
        g.setFont (ui::font (14.0f, true));
        g.drawText ("WAV in Ableton ziehen", txt.removeFromTop (20), juce::Justification::left);
        g.setColour (ui::text);
        g.setFont (ui::font (11.5f));
        g.drawText (proc.exportInfo, txt.removeFromTop (16), juce::Justification::left);
        g.setColour (ui::mute);
        g.drawText (proc.exportFile.getFileName(), txt.removeFromTop (16), juce::Justification::left, true);
    }
}

void ExportTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || proc.exportState.load() != 2 || e.getDistanceFromDragStart() < 6) return;
    if (! proc.exportFile.existsAsFile()) return;
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ proc.exportFile.getFullPathName() }, false, this, [this] { dragging = false; });
}

void ExportTile::tick()
{
    const int st = proc.exportState.load();
    const float pr = proc.exportProgress.load();
    finder.setVisible (st == 2);
    setMouseCursor (st == 2 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    if (st != lastState || (st == 1 && std::abs (pr - lastProg) > 0.005f) || st == 0)
    {
        lastState = st; lastProg = pr;
        repaint();
    }
}

// ================= Editor =================
ShakerEditor::ShakerEditor (ShakerProcessor& p) : AudioProcessorEditor (&p), proc (p), detail (p), sampleEd (p)
{
    setLookAndFeel (&look);
    for (auto* b : { &playB, &randB, &patB, &smpB, &mutB, &expB }) addAndMakeVisible (*b);
    expB.setColour (juce::TextButton::buttonColourId, ui::track[2].withAlpha (0.3f));
    expB.setTooltip ("Kompletten Loop als WAV rendern");
    expB.onClick = [this] { proc.startExport(); };
    addAndMakeVisible (exportTile);
    playB.setClickingTogglesState (true);
    playB.setColour (juce::TextButton::buttonOnColourId, ui::track[1]);
    playB.setTooltip ("Interner Play, wenn der Host gestoppt ist");
    playB.onClick = [this] { proc.internalPlay.store (playB.getToggleState()); };
    randB.onClick = [this] { proc.randomAll (false); refreshAll(); };
    patB.onClick = [this] { proc.patternsOnly(); refreshAll(); };
    smpB.onClick = [this] { proc.samplesOnly(); refreshAll(); };
    mutB.onClick = [this] { proc.mutateAll(); refreshAll(); };
    randB.setColour (juce::TextButton::buttonColourId, ui::track[0].withAlpha (0.3f));

    addAndMakeVisible (polyDice);
    polyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "polydice", polyDice);

    struct G { const char* id; const char* label; };
    for (auto& gd : { G { "swing", "Swing" }, G { "chaos", "Chaos" }, G { "evolve", "Evolution" }, G { "drift", "Pitch-Drift" }, G { "density", "Dichte" },
                      G { "ghost", "Ghosts" }, G { "room", "Raum" }, G { "roomsize", "Raumgroesse" }, G { "master", "Master" }, G { "tempo", "Tempo intern" } })
    {
        auto k = std::make_unique<Knob> (gd.label, ui::track[0]);
        k->attach (proc.apvts, gd.id);
        addAndMakeVisible (*k);
        globals.push_back (std::move (k));
    }
    styleLabel (status, 12.0f, ui::mute, juce::Justification::centredRight);
    styleLabel (ratio, 12.0f, ui::mute);
    addAndMakeVisible (status);
    addAndMakeVisible (ratio);

    for (int t = 0; t < kTracks; ++t)
    {
        rows[t] = std::make_unique<TrackRow> (proc, t, [this] (int i) { select (i); },
                                              [this] (int i) { select (i); sampleEd.open (i); sampleEd.setVisible (true); sampleEd.toFront (false); });
        addAndMakeVisible (*rows[t]);
    }
    addAndMakeVisible (detail);
    addChildComponent (sampleEd);
    sampleEd.onClose = [this] { sampleEd.setVisible (false); refreshAll(); };

    setSize (W, H);
    select (0);
    refreshAll();
    startTimerHz (30);
}

ShakerEditor::~ShakerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ShakerEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    g.setFont (ui::font (26.0f, true));
    g.setColour (ui::text);
    g.drawText ("Shaker", 16, 6, 90, 34, juce::Justification::centredLeft);
    g.setColour (ui::track[0]);
    g.drawText ("Lab", 104, 6, 60, 34, juce::Justification::centredLeft);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (8.0f, 46.0f, (float) W - 16.0f, 116.0f, 12.0f);
    g.setColour (ui::line);
    g.drawRoundedRectangle (8.5f, 46.5f, (float) W - 17.0f, 115.0f, 12.0f, 1.0f);
}

void ShakerEditor::resized()
{
    playB.setBounds (180, 8, 70, 32);
    randB.setBounds (262, 8, 90, 32);
    patB.setBounds (356, 8, 90, 32);
    smpB.setBounds (450, 8, 90, 32);
    mutB.setBounds (544, 8, 90, 32);
    expB.setBounds (638, 8, 90, 32);
    polyDice.setBounds (738, 10, 200, 28);
    status.setBounds (W - 116, 10, 100, 28);
    exportTile.setBounds (848, 50, 198, 108);
    for (size_t i = 0; i < globals.size(); ++i)
        globals[i]->setBounds (18 + (int) i * 82, 50, 78, 92);
    ratio.setBounds (20, 140, 820, 18);
    for (int t = 0; t < kTracks; ++t)
        rows[t]->setBounds (8, rowsY + t * rowH, W - 16, rowH - 6);
    detail.setBounds (8, detailY, W - 16, H - detailY - 8);
    sampleEd.setBounds (8, rowsY, W - 16, H - rowsY - 8);
}

void ShakerEditor::select (int t)
{
    for (int i = 0; i < kTracks; ++i)
    {
        rows[i]->selected = i == t;
        rows[i]->repaint();
    }
    detail.setTrack (t);
}

void ShakerEditor::refreshAll()
{
    for (auto& r : rows) r->refresh();
    if (sampleEd.isVisible()) sampleEd.refreshSamples();
}

void ShakerEditor::timerCallback()
{
    if (proc.libVersion.load() != lastLib)
    {
        lastLib = proc.libVersion.load();
        for (auto& r : rows) r->refreshSamples();
        if (sampleEd.isVisible()) sampleEd.refreshSamples();
    }
    if (proc.uiVersion.load() != lastUi)
    {
        lastUi = proc.uiVersion.load();
        refreshAll();
    }
    for (auto& r : rows) r->tick();
    if (sampleEd.isVisible()) sampleEd.tick();
    exportTile.tick();
    expB.setEnabled (proc.exportState.load() != 1);

    // Status und gemeinsamer Zyklus
    const bool hp = proc.hostPlaying.load();
    status.setText (hp ? "Host: " + juce::String (proc.hostBpm.load(), 1) + " BPM" : (proc.internalPlay.load() ? "intern: " + juce::String (juce::roundToInt (proc.getParam ("tempo"))) + " BPM" : "gestoppt"),
                    juce::dontSendNotification);
    long N = 1, D = 0;
    for (int t = 0; t < kTracks; ++t)
    {
        long n, d;
        cycleFraction (juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "steps"))), juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "mode"))),
                       juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "rate"))), n, d);
        N = N / std::gcd (N, n) * n;
        D = D == 0 ? d : std::gcd (D, d);
    }
    const auto bars = fracText (N, D * 4);
    ratio.setText ("Alle vier Spuren treffen sich wieder nach " + bars + (N <= D * 4 ? " Takt" : " Takten"), juce::dontSendNotification);

    // Rasterbreite = Loop-Länge im Verhältnis zur längsten Spur
    double maxC = 0, cyc[kTracks];
    for (int t = 0; t < kTracks; ++t)
    {
        cyc[t] = cycleBeats (juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "steps"))), juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "mode"))),
                             juce::roundToInt (proc.getParam (ShakerProcessor::tid (t, "rate"))));
        maxC = juce::jmax (maxC, cyc[t]);
    }
    for (int t = 0; t < kTracks; ++t)
    {
        const float f = (float) (cyc[t] / maxC);
        if (std::abs (f - rows[t]->grid.widthFrac) > 1e-4f) { rows[t]->grid.widthFrac = f; rows[t]->grid.repaint(); }
    }
}
