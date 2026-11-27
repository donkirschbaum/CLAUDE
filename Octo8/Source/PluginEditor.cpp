#include "PluginEditor.h"

using namespace juce;
using namespace octo;

static Font uiFont (float h, bool bold = false)
{
    auto o = FontOptions (h);
    return Font (bold ? o.withStyle ("Bold") : o);
}

// ======================================================================== LookAndFeel
OctoLookAndFeel::OctoLookAndFeel()
{
    setColour (TextButton::buttonColourId, colours::panel2);
    setColour (TextButton::buttonOnColourId, colours::ink);
    setColour (TextButton::textColourOffId, colours::ink);
    setColour (TextButton::textColourOnId, Colour (0xff111111));
    setColour (ComboBox::backgroundColourId, colours::panel2);
    setColour (ComboBox::textColourId, colours::ink);
    setColour (ComboBox::outlineColourId, colours::line);
    setColour (ComboBox::arrowColourId, colours::mute);
    setColour (PopupMenu::backgroundColourId, colours::panel);
    setColour (PopupMenu::textColourId, colours::ink);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
}

void OctoLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s)
{
    const float sz = (float) jmin (w, h) - 4.0f;
    auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).withSizeKeepingCentre (sz, sz);
    const float rad = sz * 0.5f - sz * 0.06f;
    const float cx = c.getCentreX(), cy = c.getCentreY();
    const float ang = a0 + pos * (a1 - a0);
    const PathStrokeType stroke (sz * 0.09f, PathStrokeType::curved, PathStrokeType::rounded);

    Path track;
    track.addCentredArc (cx, cy, rad, rad, 0.0f, a0, a1, true);
    g.setColour (colours::line);
    g.strokePath (track, stroke);

    if (pos > 0.003f)
    {
        Path v;
        v.addCentredArc (cx, cy, rad, rad, 0.0f, a0, ang, true);
        g.setColour (s.findColour (Slider::rotarySliderFillColourId));
        g.strokePath (v, stroke);
    }

    const float cap = rad * 0.74f;
    g.setColour (colours::knob);
    g.fillEllipse (cx - cap, cy - cap, cap * 2, cap * 2);
    const float len = cap * 0.82f;
    g.setColour (Colours::white);
    g.drawLine (cx, cy, cx + std::sin (ang) * len, cy - std::cos (ang) * len, sz * 0.065f);
}

void OctoLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto fill = b.findColour (on ? TextButton::buttonOnColourId : TextButton::buttonColourId);
    if (over) fill = fill.brighter (0.08f);
    if (down) fill = fill.darker (0.12f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 6.0f);
    if (! on)
    {
        g.setColour (colours::line);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }
}

Font OctoLookAndFeel::getTextButtonFont (TextButton&, int h) { return uiFont (jmin (14.0f, (float) h * 0.55f), true); }
Font OctoLookAndFeel::getComboBoxFont (ComboBox&) { return uiFont (14.0f, true); }
Font OctoLookAndFeel::getPopupMenuFont() { return uiFont (15.0f); }

void OctoLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    Path p;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (colours::mute);
    g.fillPath (p);
}

// ======================================================================== Knob
Knob::Knob (AudioProcessorValueTreeState& s, const String& id, const String& label, Colour col, int lh)
    : name (label), labelH (lh)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setColour (Slider::rotarySliderFillColourId, col);
    slider.setMouseDragSensitivity (180);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    slider.setBounds (r.removeFromTop (r.getHeight() - labelH));
}

void Knob::paint (Graphics& g)
{
    auto r = getLocalBounds();
    auto t = r.removeFromBottom (labelH);
    g.setColour (colours::ink);
    g.setFont (uiFont (12.5f, true));
    g.drawFittedText (slider.getTextFromValue (slider.getValue()), t.removeFromTop (15), Justification::centred, 1, 0.8f);
    g.setColour (colours::mute);
    g.setFont (uiFont (11.5f));
    g.drawFittedText (name, t, Justification::centredTop, 1, 0.8f);
}

// ======================================================================== ChoiceButtons
ChoiceButtons::ChoiceButtons (RangedAudioParameter& p, const StringArray& labels, Colour on)
    : att (p, [this] (float v)
           {
               const int idx = roundToInt (v);
               for (int i = 0; i < buttons.size(); ++i)
                   buttons[i]->setToggleState (i == idx, dontSendNotification);
           })
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new TextButton (labels[i]));
        b->setColour (TextButton::buttonOnColourId, on);
        b->setColour (TextButton::textColourOnId, on == colours::ink ? colours::bg : Colour (0xff111111));
        b->onClick = [this, i] { att.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    att.sendInitialUpdate();
}

void ChoiceButtons::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / jmax (1, buttons.size());
    for (int i = 0; i < buttons.size(); ++i)
    {
        if (i == buttons.size() - 1) { buttons[i]->setBounds (r); break; }
        buttons[i]->setBounds (r.removeFromLeft (w - 2));
        r.removeFromLeft (2);
    }
}

// ======================================================================== Scope
void Scope::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::screen);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Colours::white.withAlpha (0.05f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (r.getHeight() * i / 4), 0, r.getWidth());
    for (int i = 1; i < 10; ++i) g.drawVerticalLine ((int) (r.getWidth() * i / 10), 0, r.getHeight());

    const int N = Octo8Processor::SCOPE;
    const int w = proc.scopeWrite.load();
    std::vector<float> buf ((size_t) N);
    for (int i = 0; i < N; ++i) buf[(size_t) i] = proc.scope[(size_t) ((w + i) & (N - 1))];
    int s0 = N / 2;
    for (int j = N / 2; j > 1; --j)
        if (buf[(size_t) j - 1] < 0.0f && buf[(size_t) j] >= 0.0f) { s0 = j; break; }
    s0 = jmin (s0, N - 1024);

    Path p;
    const int n = 1024;
    for (int j = 0; j < n; ++j)
    {
        const float x = r.getWidth() * (float) j / (n - 1);
        const float y = r.getCentreY() - buf[(size_t) (s0 + j)] * r.getHeight() * 0.45f;
        if (j == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (Colour (0xffff5a1f).withAlpha (0.35f));
    g.strokePath (p, PathStrokeType (4.0f));
    g.setColour (Colour (0xffff8a55));
    g.strokePath (p, PathStrokeType (1.6f));

    g.setColour (Colour (0xffff9a6e));
    g.setFont (uiFont (12.0f, true));
    g.drawText (status, getLocalBounds().reduced (10, 6), Justification::topLeft);
}

// ======================================================================== Strip
Strip::Strip (Octo8Processor& p, int c)
    : proc (p), ch (c), col (channelColour (c)),
      wave (p.apvts, Octo8Processor::pid (c, "wave"), "Waveform", col, 30),
      ntype (p.apvts, Octo8Processor::pid (c, "ntype"), "Noise-Typ", col, 30),
      gain (p.apvts, Octo8Processor::pid (c, "gain"), "Gain", col),
      tune (p.apvts, Octo8Processor::pid (c, "tune"), "Tune", col),
      fine (p.apvts, Octo8Processor::pid (c, "fine"), "Fine", col)
{
    wave.slider.setMouseDragSensitivity (240);
    ntype.slider.setMouseDragSensitivity (160);
    for (auto* k : { &wave, &ntype, &gain, &tune, &fine }) addAndMakeVisible (k);
    for (auto* b : { &noiseBtn, &rnd, &mute, &solo }) addAndMakeVisible (b);
    for (auto* b : { &noiseBtn, &mute, &solo }) b->setClickingTogglesState (true);
    noiseBtn.setColour (TextButton::buttonOnColourId, col);
    mute.setColour (TextButton::buttonOnColourId, colours::muteOn);
    solo.setColour (TextButton::buttonOnColourId, colours::soloOn);
    noiseBtn.setTooltip ("Waveform-Regler auf Rauscharten umschalten");
    aNoise = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, Octo8Processor::pid (c, "noise"), noiseBtn);
    aMute = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, Octo8Processor::pid (c, "mute"), mute);
    aSolo = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, Octo8Processor::pid (c, "solo"), solo);
    rnd.onClick = [this] { proc.randomChannel (ch); };
    tick();
}

void Strip::resized()
{
    const int w = getWidth();
    wave.setBounds (0, 28, w, 86);
    ntype.setBounds (0, 28, w, 86);
    noiseBtn.setBounds (8, 118, w - 16, 26);
    const int kw = (w - 8) / 3;
    gain.setBounds (4, 150, kw, 66);
    tune.setBounds (4 + kw, 150, kw, 66);
    fine.setBounds (4 + 2 * kw, 150, kw, 66);
    const int bw = (w - 16 - 8) / 3;
    rnd.setBounds (8, 220, bw, 24);
    mute.setBounds (12 + bw, 220, bw, 24);
    solo.setBounds (16 + 2 * bw, 220, bw, 24);
}

void Strip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (r, 12.0f, 1.0f);
    g.setColour (col);
    g.fillEllipse (8, 6, 20, 20);
    g.setColour (Colour (0xff111111));
    g.setFont (uiFont (13.0f, true));
    g.drawText (String (ch + 1), 8, 6, 20, 20, Justification::centred);
    g.setColour (colours::mute);
    g.setFont (uiFont (12.5f));
    g.drawText ("Kanal " + String (ch + 1), 34, 6, 80, 20, Justification::centredLeft);
    g.setColour (colours::line.interpolatedWith (col, flash));
    g.fillEllipse ((float) getWidth() - 18, 11, 9, 9);
}

void Strip::tick()
{
    const bool nz = proc.param (Octo8Processor::pid (ch, "noise")) > 0.5f;
    wave.setVisible (! nz);
    ntype.setVisible (nz);
    setAlpha (proc.audible (ch) ? 1.0f : 0.45f);
    const int h = proc.hitCount[ch].load();
    if (h != lastHits) { lastHits = h; flash = 1.0f; repaint(); }
    else if (flash > 0) { flash = jmax (0.0f, flash - 0.18f); repaint (getWidth() - 22, 6, 18, 20); }
}

// ======================================================================== SeqGrid
Rectangle<float> SeqGrid::cell (int r, int c) const
{
    const float cw = (float) getWidth() / NSTEPS, rh = (float) getHeight() / NCH;
    return { c * cw + 1.5f, r * rh + 2.0f, cw - 3.0f, rh - 4.0f };
}

void SeqGrid::paint (Graphics& g)
{
    const int ps = proc.playStep.load();
    g.setFont (uiFont (12.0f, true));
    for (int r = 0; r < NCH; ++r)
    {
        const bool active = proc.trackOn (r);
        for (int c = 0; c < NSTEPS; ++c)
        {
            auto rc = cell (r, c);
            const bool on = proc.stepOn[r][c].load();
            Colour fill = on ? channelColour (r) : (c % 4 == 0 ? colours::panel2.brighter (0.12f) : colours::panel2);
            if (! active) fill = fill.withMultipliedAlpha (0.4f);
            g.setColour (fill);
            g.fillRoundedRectangle (rc, 4.0f);
            if (c == ps)
            {
                g.setColour (colours::ink);
                g.drawRoundedRectangle (rc.expanded (0.5f), 4.0f, 1.8f);
            }
            const int d = proc.stepDeg[r][c].load();
            if (on && d != 0)
            {
                g.setColour (Colour (0xff111111));
                g.drawText ((d > 0 ? "+" : "") + String (d), rc, Justification::centred);
            }
        }
    }
}

void SeqGrid::mouseDown (const MouseEvent& e)
{
    dc = jlimit (0, NSTEPS - 1, (int) (e.position.x / ((float) getWidth() / NSTEPS)));
    dr = jlimit (0, NCH - 1, (int) (e.position.y / ((float) getHeight() / NCH)));
    startDeg = proc.stepDeg[dr][dc].load();
    dragging = false;
}

void SeqGrid::mouseDrag (const MouseEvent& e)
{
    if (dr < 0 || ! proc.stepOn[dr][dc].load()) return;
    const int dy = e.getDistanceFromDragStartY();
    if (std::abs (dy) > 6) dragging = true;
    if (dragging)
    {
        proc.stepDeg[dr][dc] = jlimit (-7, 14, startDeg + roundToInt (-dy / 12.0f));
        repaint();
    }
}

void SeqGrid::mouseUp (const MouseEvent&)
{
    if (dr >= 0 && ! dragging)
    {
        const bool on = ! proc.stepOn[dr][dc].load();
        proc.stepOn[dr][dc] = on;
        if (! on) proc.stepDeg[dr][dc] = 0;
        repaint();
    }
    dr = dc = -1;
}

// ======================================================================== FilterCard
FilterCard::FilterCard (Octo8Processor& p, int c)
    : ch (c), col (channelColour (c)),
      type (*p.apvts.getParameter (Octo8Processor::pid (c, "ftype")), { "LP", "HP", "BP" }, colours::ink),
      cut (p.apvts, Octo8Processor::pid (c, "cutoff"), "Cutoff", col),
      res (p.apvts, Octo8Processor::pid (c, "reso"), "Reso", col),
      a (p.apvts, Octo8Processor::pid (c, "attack"), "Attack", col),
      d (p.apvts, Octo8Processor::pid (c, "decay"), "Decay", col),
      s (p.apvts, Octo8Processor::pid (c, "sustain"), "Sustain", col),
      r (p.apvts, Octo8Processor::pid (c, "release"), "Release", col)
{
    addAndMakeVisible (type);
    for (auto* k : { &cut, &res, &a, &d, &s, &r }) addAndMakeVisible (k);
}

void FilterCard::resized()
{
    const int w = getWidth();
    type.setBounds (w - 8 - 120, 5, 120, 22);
    const int kw = (w - 12) / 6;
    int x = 6;
    for (auto* k : { &cut, &res, &a, &d, &s, &r }) { k->setBounds (x, 32, kw, 80); x += kw; }
}

void FilterCard::paint (Graphics& g)
{
    auto rr = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (rr, 12.0f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (rr, 12.0f, 1.0f);
    g.setColour (col);
    g.fillEllipse (8, 6, 20, 20);
    g.setColour (Colour (0xff111111));
    g.setFont (uiFont (13.0f, true));
    g.drawText (String (ch + 1), 8, 6, 20, 20, Justification::centred);
    g.setColour (colours::mute);
    g.setFont (uiFont (12.5f));
    g.drawText ("Spur " + String (ch + 1) + "  Filter / ADSR", 34, 6, 150, 20, Justification::centredLeft);
    g.setColour (colours::line);
    const float sx = 6 + (float) (getWidth() - 12) / 6 * 2;
    g.drawVerticalLine ((int) sx, 40.0f, 96.0f);
}

// ======================================================================== Editor
namespace L
{
    constexpr int stripY = 112, stripH = 250, gridX = 164, gridY = 440, rowH = 28, cardY = 706, cardH = 116;
}

void Octo8Editor::Content::paint (Graphics& g)
{
    g.fillAll (colours::bg);
    g.setColour (colours::ink);
    g.setFont (uiFont (32.0f, true));
    g.drawText ("Octo 8", 12, 10, 140, 40, Justification::centredLeft);
    g.setColour (colours::mute);
    g.setFont (uiFont (12.5f));
    g.drawText ("8-Kanal-Synth", 14, 48, 140, 18, Justification::centredLeft);
    g.drawText ("Tonart", 448, 12, 70, 18, Justification::centredLeft);
    g.drawText ("Skala", 528, 12, 100, 18, Justification::centredLeft);

    g.setColour (colours::ink);
    g.setFont (uiFont (21.0f, true));
    g.drawText ("Oszillatoren", 12, 84, 300, 26, Justification::centredLeft);
    g.drawText ("Sequencer", 12, 374, 300, 26, Justification::centredLeft);
    g.drawText (String::fromUTF8 ("Filter und H\xc3\xbcllkurve"), 12, 678, 400, 26, Justification::centredLeft);

    g.setColour (colours::mute);
    g.setFont (uiFont (12.5f));
    g.drawText (String::fromUTF8 ("Klick = Step an/aus \xc2\xb7 Auf aktivem Step hoch/runter ziehen = Tonh\xc3\xb6he in Skalenstufen \xc2\xb7 "
                                  "l\xc3\xa4uft synchron, wenn Ableton spielt"),
                12, 402, 760, 18, Justification::centredLeft);

    for (int i = 0; i < NCH; ++i)
    {
        const float y = (float) (L::gridY + i * L::rowH) + 4.0f;
        g.setColour (channelColour (i));
        g.fillEllipse (12, y, 20, 20);
        g.setColour (Colour (0xff111111));
        g.setFont (uiFont (13.0f, true));
        g.drawText (String (i + 1), 12, (int) y, 20, 20, Justification::centred);
    }
}

Octo8Editor::Octo8Editor (Octo8Processor& p)
    : AudioProcessorEditor (&p), proc (p), scope (p), grid (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);

    // ---- top bar
    playBtn.setClickingTogglesState (true);
    playBtn.setColour (TextButton::buttonOnColourId, colours::accent);
    playBtn.setColour (TextButton::buttonColourId, colours::accent.withAlpha (0.25f));
    playBtn.setColour (TextButton::textColourOnId, Colours::white);
    playBtn.setTooltip ("Interner Start; spielt Ableton, folgt der Sequencer automatisch dem Host");
    aPlay = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "play", playBtn);
    playBtn.onStateChange = [this] { playBtn.setButtonText (playBtn.getToggleState() ? "Stop" : "Play"); };

    modeBtns = std::make_unique<ChoiceButtons> (*p.apvts.getParameter ("mode"), StringArray { "Drone", "Sequencer" }, colours::ink);

    for (int i = 0; i < 12; ++i) keyBox.addItem (noteNames()[i], i + 1);
    for (int i = 0; i < (int) scales().size(); ++i) scaleBox.addItem (scales()[(size_t) i].name, i + 1);
    aKey = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "key", keyBox);
    aScale = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "scale", scaleBox);
    keyBox.onChange = [this] { proc.requantize(); for (auto* s : strips) s->repaint(); };
    scaleBox.onChange = [this] { proc.requantize(); };

    masterRnd.setColour (TextButton::buttonColourId, colours::ink);
    masterRnd.setColour (TextButton::textColourOffId, colours::bg);
    masterRnd.onClick = [this] { proc.masterRandom(); };
    master = std::make_unique<Knob> (p.apvts, "master", "Master", colours::accent, 26);

    for (auto* c : std::initializer_list<Component*> { &playBtn, modeBtns.get(), &keyBox, &scaleBox, &masterRnd, master.get(), &scope })
        content.addAndMakeVisible (c);

    // ---- oscillators
    for (int i = 0; i < NCH; ++i) content.addAndMakeVisible (strips.add (new Strip (p, i)));

    // ---- sequencer
    bpm = std::make_unique<Knob> (p.apvts, "bpm", "BPM intern", colours::accent, 26);
    gate = std::make_unique<Knob> (p.apvts, "gate", "Gate", colours::accent, 26);
    seqRnd.onClick = [this] { proc.randomAllTracks(); grid.repaint(); };
    clearBtn.onClick = [this] { proc.clearAll(); grid.repaint(); };
    for (auto* c : std::initializer_list<Component*> { bpm.get(), gate.get(), &seqRnd, &clearBtn, &grid })
        content.addAndMakeVisible (c);

    for (int i = 0; i < NCH; ++i)
    {
        auto* r = trackRnd.add (new TextButton ("Rnd"));
        auto* m = trackMute.add (new TextButton ("M"));
        auto* s = trackSolo.add (new TextButton ("S"));
        m->setClickingTogglesState (true);
        s->setClickingTogglesState (true);
        m->setColour (TextButton::buttonOnColourId, colours::muteOn);
        s->setColour (TextButton::buttonOnColourId, colours::soloOn);
        r->onClick = [this, i] { proc.randomTrack (i); grid.repaint(); };
        trackAtt.add (new AudioProcessorValueTreeState::ButtonAttachment (p.apvts, Octo8Processor::pid (i, "smute"), *m));
        trackAtt.add (new AudioProcessorValueTreeState::ButtonAttachment (p.apvts, Octo8Processor::pid (i, "ssolo"), *s));
        for (auto* b : { r, m, s }) content.addAndMakeVisible (b);
    }

    // ---- filter / envelope
    for (int i = 0; i < NCH; ++i) content.addAndMakeVisible (cards.add (new FilterCard (p, i)));

    // ---- layout in design coordinates
    playBtn.setBounds (150, 28, 86, 34);
    modeBtns->setBounds (246, 28, 190, 34);
    keyBox.setBounds (448, 32, 70, 30);
    scaleBox.setBounds (528, 32, 190, 30);
    masterRnd.setBounds (730, 28, 130, 34);
    master->setBounds (866, 10, 58, 68);
    scope.setBounds (932, 12, 256, 66);

    const float sw = (1176.0f - 7 * 6.0f) / NCH;
    for (int i = 0; i < NCH; ++i)
        strips[i]->setBounds (roundToInt (12 + i * (sw + 6)), L::stripY, roundToInt (sw), L::stripH);

    bpm->setBounds (800, 368, 58, 66);
    gate->setBounds (862, 368, 58, 66);
    seqRnd.setBounds (930, 386, 140, 30);
    clearBtn.setBounds (1078, 386, 110, 30);
    grid.setBounds (L::gridX, L::gridY, 1188 - L::gridX, NCH * L::rowH);
    for (int i = 0; i < NCH; ++i)
    {
        const int y = L::gridY + i * L::rowH + 2;
        trackRnd[i]->setBounds (40, y, 46, 24);
        trackMute[i]->setBounds (90, y, 32, 24);
        trackSolo[i]->setBounds (126, y, 32, 24);
    }

    const float cw = (1176.0f - 3 * 6.0f) / 4;
    for (int i = 0; i < NCH; ++i)
        cards[i]->setBounds (roundToInt (12 + (i % 4) * (cw + 6)), L::cardY + (i / 4) * (L::cardH + 6), roundToInt (cw), L::cardH);

    setResizable (true, true);
    setResizeLimits (720, 570, 1920, 1520);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setSize (960, 760);
    startTimerHz (30);
}

Octo8Editor::~Octo8Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Octo8Editor::paint (Graphics& g) { g.fillAll (colours::bg); }

void Octo8Editor::resized()
{
    const float s = (float) getWidth() / (float) W;
    content.setTransform (AffineTransform::scale (s));
}

void Octo8Editor::timerCallback()
{
    const bool seq = proc.param ("mode") > 0.5f;
    auto u = [] (const char* t) { return String::fromUTF8 (t); };
    String st;
    if (proc.hostPlaying.load())
        st = u (seq ? "Host-Sync \xc2\xb7 " : "Drone \xc2\xb7 Host \xc2\xb7 ") + String (proc.hostBpm.load(), 1) + " BPM";
    else if (proc.param ("play") > 0.5f)
        st = seq ? u ("Sequencer \xc2\xb7 ") + String ((int) proc.param ("bpm")) + " BPM" : u ("Drone l\xc3\xa4uft");
    else
        st = seq ? u ("Bereit (Play oder Ableton starten)") : u ("Drone: Play oder MIDI-Note");
    scope.status = st;
    scope.repaint();
    grid.repaint();
    for (auto* s : strips) s->tick();
    for (int i = 0; i < NCH; ++i) cards[i]->setAlpha (proc.audible (i) ? 1.0f : 0.45f);
}
