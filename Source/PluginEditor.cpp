#include "PluginEditor.h"

namespace g8
{
juce::Font fnt (float h, bool bold) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }

static juce::String pct (double v) { return juce::String (juce::roundToInt (v * 100)); }

// ======================= LookAndFeel =======================
LnF::LnF()
{
    setColour (juce::ComboBox::textColourId, col::ink);
    setColour (juce::ComboBox::arrowColourId, col::ink);
    setColour (juce::PopupMenu::backgroundColourId, col::btn);
    setColour (juce::PopupMenu::textColourId, col::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::ink);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, col::ink);
    setColour (juce::ScrollBar::thumbColourId, col::ink2);
}

void LnF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = y + h * .5f;
    g.setColour (col::ink);
    g.fillRoundedRectangle ((float) x, cy - 1.5f, (float) w, 3.f, 1.5f);
    juce::Rectangle<float> th (pos - 5.f, cy - 7.f, 10.f, 14.f);
    g.setColour (juce::Colour (0xfff6f7f8)); g.fillRoundedRectangle (th, 2.f);
    g.setColour (col::ink); g.drawRoundedRectangle (th, 2.f, 1.5f);
}

void LnF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (.75f);
    juce::Colour fill = b.getToggleState() ? col::ink : (over ? juce::Colours::white : col::btn);
    if (b.getProperties().contains ("onColour") && b.getToggleState()) fill = juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["onColour"]);
    if (down) fill = fill.darker (.1f);
    g.setColour (fill); g.fillRoundedRectangle (r, 3.f);
    g.setColour (col::ink); g.drawRoundedRectangle (r, 3.f, 1.5f);
}

void LnF::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    juce::Colour tc = on ? juce::Colours::white : col::ink;
    if (b.getProperties().contains ("onText") && on) tc = juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["onText"]);
    g.setColour (tc);
    g.setFont (fnt (juce::jmin (13.f, b.getHeight() * .55f), true));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (3, 0), juce::Justification::centred, 1);
}

void LnF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    juce::Rectangle<float> r (0, 0, (float) w, (float) h);
    r = r.reduced (.75f);
    g.setColour (col::btn); g.fillRoundedRectangle (r, 3.f);
    g.setColour (col::ink); g.drawRoundedRectangle (r, 3.f, 1.5f);
    juce::Path p; const float ax = w - 11.f, ay = h * .5f;
    p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.fillPath (p);
}

void LnF::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

void LnF::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    juce::Rectangle<float> box (2.f, b.getHeight() * .5f - 7.f, 14.f, 14.f);
    g.setColour (b.getToggleState() ? col::led : col::btn); g.fillRoundedRectangle (box, 2.f);
    g.setColour (col::ink); g.drawRoundedRectangle (box, 2.f, 1.3f);
    if (b.getToggleState())
    {
        juce::Path tick; tick.startNewSubPath (box.getX() + 3, box.getCentreY()); tick.lineTo (box.getX() + 6, box.getBottom() - 3); tick.lineTo (box.getRight() - 3, box.getY() + 3);
        g.setColour (juce::Colours::white); g.strokePath (tick, juce::PathStrokeType (1.8f));
    }
    g.setColour (col::ink); g.setFont (fnt (13.f));
    g.drawText (b.getButtonText(), 22, 0, b.getWidth() - 22, b.getHeight(), juce::Justification::centredLeft);
}

static void label (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, juce::Justification j = juce::Justification::centredLeft)
{
    g.setColour (col::ink2); g.setFont (fnt (11.f, true));
    g.drawText (t.toUpperCase(), r, j);
}

static void fillCombo (juce::ComboBox& c, const char* const* items, int n) { for (int i = 0; i < n; ++i) c.addItem (items[i], i + 1); }
static void fillCombo (juce::ComboBox& c, juce::StringArray items) { for (int i = 0; i < items.size(); ++i) c.addItem (items[i], i + 1); }

// ======================= XYPad =======================
XYPad::XYPad (Grids8Processor& p, const juce::String& xId, const juce::String& yId)
    : proc (p), px (p.apvts.getParameter (xId)), py (p.apvts.getParameter (yId))
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void XYPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float w = r.getWidth(), h = r.getHeight();
    const bool big = w > 150;
    g.setColour (col::well); g.fillRoundedRectangle (r, 4.f);
    g.setColour (col::wellInk.withAlpha (.22f));
    const float m = big ? 10.f : 6.f;
    for (int j = 0; j < 5; ++j) for (int i = 0; i < 5; ++i)
        g.fillEllipse (m + i * (w - 2 * m) / 4 - 1.5f, m + j * (h - 2 * m) / 4 - 1.5f, 3.f, 3.f);
    const float bx = px->getValue() * w, by = (1 - py->getValue()) * h;
    g.setColour (col::wellInk.withAlpha (.25f));
    g.drawVerticalLine ((int) bx, 0, h); g.drawHorizontalLine ((int) by, 0, w);
    const float ex = effX * w, ey = (1 - effY) * h;
    if (std::abs (ex - bx) + std::abs (ey - by) > 1) { g.setColour (col::led.withAlpha (.5f)); g.drawLine (bx, by, ex, ey, 1.f); }
    const float rr = big ? 9.f : 6.f;
    g.setColour (col::wellInk); g.drawEllipse (bx - rr, by - rr, rr * 2, rr * 2, big ? 2.f : 1.5f);
    const float dr = big ? 6.f : 4.f;
    g.setColour (hot ? juce::Colours::white : col::led); g.fillEllipse (ex - dr, ey - dr, dr * 2, dr * 2);
}

void XYPad::setFrom (juce::Point<float> p)
{
    px->setValueNotifyingHost (juce::jlimit (0.f, 1.f, p.x / getWidth()));
    py->setValueNotifyingHost (juce::jlimit (0.f, 1.f, 1.f - p.y / getHeight()));
    if (onUserMove) onUserMove();
    repaint();
}
void XYPad::mouseDown (const juce::MouseEvent& e)
{
    px->beginChangeGesture(); py->beginChangeGesture();
    if (e.mods.isAltDown()) { juce::Random r; px->setValueNotifyingHost (r.nextFloat()); py->setValueNotifyingHost (r.nextFloat()); repaint(); return; }
    setFrom (e.position);
}
void XYPad::mouseDrag (const juce::MouseEvent& e) { if (! e.mods.isAltDown()) setFrom (e.position); }
void XYPad::mouseUp (const juce::MouseEvent&) { px->endChangeGesture(); py->endChangeGesture(); }
void XYPad::mouseDoubleClick (const juce::MouseEvent&)
{
    px->beginChangeGesture(); py->beginChangeGesture();
    px->setValueNotifyingHost (.5f); py->setValueNotifyingHost (.5f);
    px->endChangeGesture(); py->endChangeGesture(); repaint();
}

// ======================= StepView =======================
void StepView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::well); g.fillRoundedRectangle (r, 4.f);
    const float W = r.getWidth(), H = r.getHeight(), cw = W / NSTEP, gap = juce::jmax (1.f, cw * .14f);
    const int len = juce::jlimit (1, 32, (int) proc.pv (Grids8Processor::tid (track, "len")));
    const int64_t gg = juce::jmax<int64_t> (0, step);
    const auto e = proc.effParamsUI (track, gg);
    const float th = 255.f * (1.f - e.d);
    const int cur = step >= 0 ? (int) posmod (step, len) : -1;
    const auto& m = proc.map();
    for (int s = 0; s < NSTEP; ++s)
    {
        const float x = s * cw + gap * .5f, w = cw - gap;
        if (s >= len) { g.setColour (juce::Colours::white.withAlpha (.04f)); g.fillRect (x, 2.f, w, H - 4); continue; }
        const float L = m.level (track, s, e.x, e.y);
        const bool hit = e.d > .004f && L > th;
        g.setColour (juce::Colours::white.withAlpha (s % 4 == 0 ? .10f : .05f)); g.fillRect (x, 2.f, w, H - 4);
        const float lh = clamp01 (L / 255.f) * (H - 14);
        g.setColour (hit ? col::led : col::wellInk.withAlpha (.28f)); g.fillRect (x, H - 6 - lh, w, lh);
        if (hit) { g.setColour (L > 205 ? col::ledPale : col::led); g.fillRect (x, H - 5, w, 3.f); }
        if (s == cur) { g.setColour (juce::Colours::white); g.drawRect (x + 1, 2.f, w - 2, H - 4, 2.f); }
    }
    const float y = H - 6 - clamp01 (th / 255.f) * (H - 14);
    g.setColour (col::ledPale.withAlpha (.55f));
    const float dashes[] = { 3.f, 3.f };
    g.drawDashedLine ({ 0, y, W, y }, dashes, 2, 1.f);
}

// ======================= LabeledSlider =======================
LabeledSlider::LabeledSlider (APVTS& s, const juce::String& id, const juce::String& l, std::function<juce::String (double)> f)
    : label (l), fmt (std::move (f))
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setDoubleClickReturnValue (true, s.getParameter (id)->convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    slider.onValueChange = [this] { repaint(); };
}
void LabeledSlider::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().removeFromTop (13);
    g.setColour (col::ink2); g.setFont (fnt (10.5f, true));
    g.drawText (label.toUpperCase(), r, juce::Justification::centredLeft);
    g.setColour (col::ink); g.setFont (fnt (11.f));
    g.drawText (fmt (slider.getValue()), r, juce::Justification::centredRight);
}
void LabeledSlider::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (12)); }

// ======================= TrackRow =======================
TrackRow::TrackRow (Grids8Processor& p, int t)
    : proc (p), track (t),
      pad (p, Grids8Processor::tid (t, "x"), Grids8Processor::tid (t, "y")),
      dens (p.apvts, Grids8Processor::tid (t, "dens"), "Dichte", pct),
      chaos (p.apvts, Grids8Processor::tid (t, "chaos"), "Chaos", pct),
      len (p.apvts, Grids8Processor::tid (t, "len"), juce::String::fromUTF8 ("Länge"), [] (double v) { return juce::String ((int) v); }),
      vol (p.apvts, Grids8Processor::tid (t, "vol"), "Vol", pct),
      pitch (p.apvts, Grids8Processor::tid (t, "pitch"), "Pitch", [] (double v) { int i = (int) std::lround (v); return (i > 0 ? "+" : "") + juce::String (i); }),
      decay (p.apvts, Grids8Processor::tid (t, "decay"), "Decay", pct),
      mdx (p.apvts, Grids8Processor::tid (t, "mDx"), "Tiefe X", pct),
      mdy (p.apvts, Grids8Processor::tid (t, "mDy"), "Tiefe Y", pct),
      mdd (p.apvts, Grids8Processor::tid (t, "mDd"), "Tiefe D", pct),
      steps (p, t)
{
    for (auto* c : std::initializer_list<juce::Component*> { &fileBtn, &mute, &solo, &dice, &pad, &dens, &chaos, &len, &vol, &pitch, &decay, &mdx, &mdy, &mdd, &steps, &shape, &rate })
        addAndMakeVisible (c);
    mute.setClickingTogglesState (true); solo.setClickingTogglesState (true);
    mute.getProperties().set ("onColour", (juce::int64) col::ink2.getARGB());
    solo.getProperties().set ("onColour", (juce::int64) col::solo.getARGB()); solo.getProperties().set ("onText", (juce::int64) col::ink.getARGB());
    aMute = std::make_unique<APVTS::ButtonAttachment> (p.apvts, Grids8Processor::tid (t, "mute"), mute);
    aSolo = std::make_unique<APVTS::ButtonAttachment> (p.apvts, Grids8Processor::tid (t, "solo"), solo);
    fillCombo (shape, SHAPE_NAMES, 6); fillCombo (rate, RATE_NAMES, 7);
    aShape = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, Grids8Processor::tid (t, "mShape"), shape);
    aRate = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, Grids8Processor::tid (t, "mRate"), rate);
    dice.setButtonText (juce::String::fromUTF8 ("Würfel"));
    dice.onClick = [this] { proc.diceTrack (track, juce::ModifierKeys::currentModifiers.isShiftDown()); };
    fileBtn.setButtonText (p.sampleName (t));
    fileBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (juce::String::fromUTF8 ("Sample für Spur ") + juce::String (track + 1), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile() && proc.loadUserSample (track, f)) fileBtn.setButtonText (proc.sampleName (track));
        });
    };
}

void TrackRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (.75f);
    g.setColour (juce::Colours::white.withAlpha (.18f)); g.fillRoundedRectangle (r, 6.f);
    g.setColour (col::ink); g.drawRoundedRectangle (r, 6.f, 1.5f);
    juce::Rectangle<float> idx (10, 8, 22, 22);
    g.setColour (flash > .3f ? col::led : col::ink); g.fillRoundedRectangle (idx, 3.f);
    g.setColour (juce::Colours::white); g.setFont (fnt (12.f, true));
    g.drawText (juce::String (track + 1), idx, juce::Justification::centred);
    g.setColour (col::ink); g.setFont (fnt (18.f, true));
    g.drawText (juce::String (TRACKS[track].name).toUpperCase(), 38, 6, 96, 26, juce::Justification::centredLeft);
    label (g, "Mod", { modX, 9, 34, 14 });
}

void TrackRow::resized()
{
    // Spalten: Name 136 | Pad 72 | Regler 240 | Steps flex | Mod 220 | Wuerfel 44
    const int H = getHeight();
    fileBtn.setBounds (10, 34, 120, 18);
    mute.setBounds (10, 56, 34, 20); solo.setBounds (48, 56, 34, 20);
    pad.setBounds (144, 4, H - 8, H - 8);
    const int kx = 144 + H - 8 + 10, kw = 76;
    LabeledSlider* ks[] = { &dens, &chaos, &len, &vol, &pitch, &decay };
    for (int i = 0; i < 6; ++i) ks[i]->setBounds (kx + (i % 3) * (kw + 6), 6 + (i / 3) * 34, kw, 30);
    const int sx = kx + 3 * (kw + 6) + 4;
    const int modW = 230, diceW = 64;
    const int sw = getWidth() - sx - modW - diceW - 20;
    steps.setBounds (sx, 8, sw, H - 16);
    const int mx = sx + sw + 10;
    modX = mx;
    shape.setBounds (mx + 34, 6, 92, 20); rate.setBounds (mx + 132, 6, 92, 20);
    mdx.setBounds (mx, 38, 70, 30); mdy.setBounds (mx + 77, 38, 70, 30); mdd.setBounds (mx + 154, 38, 70, 30);
    dice.setBounds (getWidth() - diceW - 8, H / 2 - 14, diceW, 28);
}

void TrackRow::tick (int64_t g)
{
    const auto e = proc.effParamsUI (track, juce::jmax<int64_t> (0, g));
    pad.effX = e.x; pad.effY = e.y;
    const auto hc = proc.hitCount[track].load();
    if (hc != lastHits) { lastHits = hc; flash = 1.f; }
    pad.hot = flash > .5f;
    flash *= .82f;
    steps.step = g;
    pad.repaint(); steps.repaint(); repaint (juce::Rectangle<int> (8, 6, 26, 26));
    if (fileBtn.getButtonText() != proc.sampleName (track)) fileBtn.setButtonText (proc.sampleName (track));
}

// ======================= Sections =======================
void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (.75f);
    g.setColour (juce::Colours::white.withAlpha (.18f)); g.fillRoundedRectangle (r, 6.f);
    g.setColour (col::ink); g.drawRoundedRectangle (r, 6.f, 1.5f);
    g.setFont (fnt (15.f, true));
    g.drawText (title.toUpperCase(), 40, 6, getWidth() - 50, 22, juce::Justification::centredLeft);
}
void Section::resized()
{
    toggle.setBounds (10, 7, 22, 20);
    if (! collapsed) layoutContent (getLocalBounds().reduced (10, 0).withTrimmedTop (34).withTrimmedBottom (8));
}

MasterSection::MasterSection (Grids8Processor& p)
    : Section ("Master X / Y"), pad (p, "mX", "mY"),
      dens (p.apvts, "mDens", juce::String::fromUTF8 ("Dichte ±"), [] (double v) { return (v > 0 ? "+" : "") + pct (v); }),
      chaos (p.apvts, "mChaos", "Chaos", pct), vol (p.apvts, "mVol", "Volume", pct),
      dx (p.apvts, "mDx", "Mod Tiefe X", pct), dy (p.apvts, "mDy", "Mod Tiefe Y", pct), dd (p.apvts, "mDd", "Mod Tiefe Dichte", pct)
{
    for (auto* c : std::initializer_list<juce::Component*> { &pad, &dens, &chaos, &vol, &dx, &dy, &dd, &shape, &rate }) addAndMakeVisible (c);
    fillCombo (shape, SHAPE_NAMES, 6); fillCombo (rate, RATE_NAMES, 7);
    aShape = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "mShape", shape);
    aRate = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "mRate", rate);
    contentHeight = 270 + 8 + 3 * 32 + 30 + 3 * 32;
}
void MasterSection::layoutContent (juce::Rectangle<int> r)
{
    pad.setBounds (r.removeFromTop (270)); r.removeFromTop (8);
    for (auto* s : { &dens, &chaos, &vol }) s->setBounds (r.removeFromTop (32).withTrimmedBottom (4));
    auto row = r.removeFromTop (30).withTrimmedBottom (6);
    shape.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3)); row.removeFromLeft (6); rate.setBounds (row);
    for (auto* s : { &dx, &dy, &dd }) s->setBounds (r.removeFromTop (32).withTrimmedBottom (4));
}

RandomSection::RandomSection (Grids8Processor& p)
    : Section ("Zufall"), proc (p),
      amt (p.apvts, "rndAmt", juce::String::fromUTF8 ("Stärke"), pct), hTime (p.apvts, "hTime", "Humanize Timing", pct), hVel (p.apvts, "hVel", "Humanize Velocity", pct)
{
    const char* names[][2] = { { "Alles würfeln", "all" }, { "Mutieren", "mutate" }, { "Neue Map", "map" }, { "XY würfeln", "xy" },
                               { "Dichten", "dens" }, { "Chaos", "chaos" }, { "Mutes", "mutes" }, { "Sound", "sound" },
                               { "Modulation", "mods" }, { "XY-Glide", "glide" }, { "Längen", "len" }, { "Tempo/Swing", "tempo" } };
    for (auto& n : names)
    {
        auto* b = buttons.add (new juce::TextButton (juce::String::fromUTF8 (n[0])));
        const juce::String kind (n[1]);
        b->onClick = [this, kind] { proc.randomize (kind); repaint(); };
        addAndMakeVisible (b);
    }
    for (auto* c : std::initializer_list<juce::Component*> { &amt, &hTime, &hVel, &evOn, &afOn, &evEvery, &afEvery }) addAndMakeVisible (c);
    fillCombo (evEvery, { "1 Takt", "2 Takte", "4 Takte", "8 Takte" }); fillCombo (afEvery, { "2 Takte", "4 Takte", "8 Takte" });
    aEv = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "evOn", evOn);
    aAf = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "afOn", afOn);
    aEvE = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "evEvery", evEvery);
    aAfE = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "afEvery", afEvery);
    contentHeight = 6 * 30 + 6 + 3 * 32 + 2 * 28 + 20;
}
void RandomSection::layoutContent (juce::Rectangle<int> r)
{
    for (int i = 0; i < buttons.size(); i += 2)
    {
        auto row = r.removeFromTop (30).withTrimmedBottom (4);
        buttons[i]->setBounds (row.removeFromLeft (row.getWidth() / 2 - 3)); row.removeFromLeft (6);
        buttons[i + 1]->setBounds (row);
    }
    r.removeFromTop (6);
    for (auto* s : { &amt, &hTime, &hVel }) s->setBounds (r.removeFromTop (32).withTrimmedBottom (4));
    auto row = r.removeFromTop (28).withTrimmedBottom (4); evOn.setBounds (row.removeFromLeft (150)); evEvery.setBounds (row);
    row = r.removeFromTop (28).withTrimmedBottom (4); afOn.setBounds (row.removeFromLeft (150)); afEvery.setBounds (row);
}
void RandomSection::paint (juce::Graphics& g)
{
    Section::paint (g);
    if (! collapsed) { label (g, "Map-Seed " + juce::String (proc.getSeed()), { 10, getHeight() - 26, getWidth() - 20, 16 }); }
}

OptionsSection::OptionsSection (Grids8Processor& p) : Section ("MIDI & Optionen")
{
    for (auto* c : std::initializer_list<juce::Component*> { &noteMap, &midiCh, &choke, &sound }) addAndMakeVisible (c);
    fillCombo (noteMap, { juce::String::fromUTF8 ("Drum Rack C1–G1 (36–43)"), "General MIDI" });
    fillCombo (midiCh, { "MIDI-Out Kanal 10", juce::String::fromUTF8 ("MIDI-Out Kanal 1–8 je Spur") });
    aNote = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "noteMap", noteMap);
    aCh = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "midiCh", midiCh);
    aChoke = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "choke", choke);
    aSound = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "sound", sound);
    contentHeight = 4 * 28 + 120;
}
void OptionsSection::layoutContent (juce::Rectangle<int> r)
{
    for (juce::Component* c : { (juce::Component*) &noteMap, (juce::Component*) &midiCh, (juce::Component*) &choke, (juce::Component*) &sound })
        c->setBounds (r.removeFromTop (28).withTrimmedBottom (4));
    infoArea = r;
}
void OptionsSection::paint (juce::Graphics& g)
{
    Section::paint (g);
    if (collapsed) return;
    g.setColour (col::ink2); g.setFont (fnt (12.f));
    g.drawFittedText (juce::String::fromUTF8 ("Slots: Chip „Drum Rack ziehen“ auf eine MIDI-Spur mit Drum Rack ziehen. Live-MIDI: neue MIDI-Spur, „MIDI From“ = Grids-8-Spur. "
                      "CC: Master X/Y 16/17, Spur X 20–27, Y 30–37, Dichte 40–47."),
                      infoArea, juce::Justification::topLeft, 8);
}

LeftPanel::LeftPanel (Grids8Processor& p) : master (p), random (p), options (p)
{
    for (Section* s : { (Section*) &master, (Section*) &random, (Section*) &options })
    {
        addAndMakeVisible (s);
        s->onLayout = [this] { relayout(); };
    }
}
void LeftPanel::relayout()
{
    int y = 0;
    for (Section* s : { (Section*) &master, (Section*) &random, (Section*) &options }) y += s->preferredHeight() + 10;
    setSize (getWidth(), y);
    resized();
}
void LeftPanel::resized()
{
    int y = 0;
    for (Section* s : { (Section*) &master, (Section*) &random, (Section*) &options })
    {
        s->setBounds (0, y, getWidth(), s->preferredHeight());
        y += s->preferredHeight() + 10;
    }
}

// ======================= Slots =======================
void DragChip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (.75f);
    g.setColour (enabledChip ? col::ink : col::panel2); g.fillRoundedRectangle (r, 3.f);
    g.setColour (enabledChip ? juce::Colours::white : col::ink2); g.setFont (fnt (11.5f, true));
    g.drawFittedText (text.toUpperCase(), getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}
void DragChip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || ! enabledChip || e.getDistanceFromDragStart() < 4) return;
    dragging = true;
    auto f = proc.writeSlotMidi (slot, multi);
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
    else dragging = false;
}

SlotView::SlotView (Grids8Processor& p, int i)
    : proc (p), idx (i), chipRack (p, i, false, "Drum Rack ziehen"), chipMulti (p, i, true, "8 Spuren ziehen")
{
    for (auto* c : std::initializer_list<juce::Component*> { &lenBox, &hold, &loop, &clear, &saveBtn, &chipRack, &chipMulti }) addAndMakeVisible (c);
    const int lens[] = { 1, 2, 4, 8, 16 };
    for (int k = 0; k < 5; ++k) lenBox.addItem (juce::String (lens[k]) + (lens[k] > 1 ? " Takte" : " Takt"), lens[k]);
    lenBox.onChange = [this] { proc.setSlotLen (idx, lenBox.getSelectedId()); };
    hold.onClick = [this] { proc.toggleHold (idx); };
    loop.onClick = [this] { proc.toggleLoop (idx); };
    clear.onClick = [this] { proc.clearSlot (idx); };
    loop.getProperties().set ("onColour", (juce::int64) col::ok.getARGB());
    saveBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Slot als MIDI sichern",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Grids8_Slot" + juce::String (idx + 1) + ".mid"), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f != juce::File()) proc.writeSlotMidi (idx, false, f.withFileExtension ("mid"));
        });
    };
    refresh();
}

void SlotView::refresh()
{
    bool has, lock, isLoop; int len;
    {
        const juce::ScopedLock sl (proc.slotLock);
        has = proc.slots[(size_t) idx].has_value(); lock = proc.cfg[(size_t) idx].lock; len = proc.cfg[(size_t) idx].len; isLoop = proc.loopSlot == idx;
    }
    lenBox.setSelectedId (len, juce::dontSendNotification);
    hold.setToggleState (lock, juce::dontSendNotification); hold.setButtonText (lock ? "Gehalten" : "Halten");
    loop.setToggleState (isLoop, juce::dontSendNotification); loop.setButtonText (isLoop ? "Loop an" : "Loop");
    chipRack.enabledChip = chipMulti.enabledChip = has;
    for (juce::Component* c : { (juce::Component*) &loop, (juce::Component*) &clear, (juce::Component*) &saveBtn }) c->setEnabled (has);
    meta = proc.slotMeta (idx);
    repaint(); chipRack.repaint(); chipMulti.repaint();
}

void SlotView::paint (juce::Graphics& g)
{
    bool lock;
    { const juce::ScopedLock sl (proc.slotLock); lock = proc.cfg[(size_t) idx].lock; }
    auto r = getLocalBounds().toFloat().reduced (.75f);
    g.setColour (juce::Colours::white.withAlpha (.18f)); g.fillRoundedRectangle (r, 6.f);
    g.setColour (col::ink); g.drawRoundedRectangle (r, 6.f, 1.5f);
    if (! lock) { g.setColour (col::rec); g.fillRect (1.5f, 6.f, 3.f, r.getHeight() - 12); }
    g.setColour (col::ink); g.setFont (fnt (15.f, true));
    g.drawText ("SLOT " + juce::String (idx + 1), 12, 4, 80, 22, juce::Justification::centredLeft);
    g.setColour (col::ink2); g.setFont (fnt (11.f));
    g.drawText (meta, 12, 27, getWidth() - 20, 14, juce::Justification::centredLeft);
    juce::Rectangle<float> roll (10, 43, getWidth() - 20.f, 24);
    g.setColour (col::well); g.fillRoundedRectangle (roll, 3.f);
    const juce::ScopedLock sl (proc.slotLock);
    if (const auto& d = proc.slots[(size_t) idx])
    {
        const float cw = roll.getWidth() / (float) (d->bars * 16), rh = roll.getHeight() / NT;
        for (int b = 1; b < d->bars; ++b) { g.setColour (juce::Colours::white.withAlpha (.08f)); g.fillRect (roll.getX() + b * 16 * cw, roll.getY(), 1.f, roll.getHeight()); }
        for (const auto& e : d->ev)
        {
            g.setColour (col::led.withAlpha (.35f + e.v / 127.f * .65f));
            g.fillRect (roll.getX() + (e.s + e.o) * cw, roll.getY() + e.t * rh + .5f, juce::jmax (1.2f, cw * .7f), rh - 1.f);
        }
    }
}

void SlotView::resized()
{
    const int w = getWidth();
    hold.setBounds (w - 82, 5, 72, 20);
    lenBox.setBounds (w - 160, 5, 72, 20);
    chipRack.setBounds (10, 71, (w - 26) / 2, 22); chipMulti.setBounds (16 + (w - 26) / 2, 71, (w - 26) / 2, 22);
    loop.setBounds (10, 97, 66, 20); clear.setBounds (80, 97, 66, 20); saveBtn.setBounds (150, 97, w - 160, 20);
}

// ======================= Content =======================
Content::Content (Grids8Processor& p)
    : proc (p),
      bpm (p.apvts, "bpm", "Tempo ohne Host", [] (double v) { return juce::String (v, 0); }),
      swing (p.apvts, "swing", "Swing", [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; }),
      left (p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &play, &fill, &holdAll, &bpm, &swing, &leftView }) addAndMakeVisible (c);
    leftView.setViewedComponent (&left, false);
    leftView.setScrollBarsShown (true, false);
    leftView.setScrollBarThickness (8);
    play.getProperties().set ("onColour", (juce::int64) col::ok.getARGB());
    play.onClick = [this] { proc.internalPlay = ! proc.internalPlay.load(); };
    fill.onStateChange = [this] { proc.fillHeld = fill.isDown(); fill.setToggleState (fill.isDown(), juce::dontSendNotification); };
    holdAll.onClick = [this] { proc.holdAll(); };
    for (int t = 0; t < NT; ++t) addAndMakeVisible (rows.add (new TrackRow (p, t)));
    for (int i = 0; i < 8; ++i) addAndMakeVisible (slotViews.add (new SlotView (p, i)));
    left.master.pad.onUserMove = [] {};
}

void Content::paint (juce::Graphics& g)
{
    g.fillAll (col::panel);
    g.setColour (juce::Colours::white.withAlpha (.08f));
    for (int x = 0; x < W; x += 3) g.drawVerticalLine (x, 0, (float) H);
    g.setColour (col::ink); g.setFont (fnt (30.f, true));
    g.drawText ("GRIDS 8", 16, 8, 160, 40, juce::Justification::centredLeft);
    g.setColour (col::ink2); g.setFont (fnt (11.f));
    g.drawText (juce::String::fromUTF8 ("Topografischer Drum-Sequencer · 8 Spuren"), 160, 22, 260, 16, juce::Justification::centredLeft);
    g.fillRect (16, 56, W - 32, 2);
    // Capture-Status
    const bool live = capText.startsWith (juce::String::fromUTF8 ("Capture läuft"));
    g.setColour (live ? col::rec : col::panel2); g.fillEllipse (842, 24, 10, 10);
    g.setColour (col::ink); g.drawEllipse (842, 24, 10, 10, 1.3f);
    g.setFont (fnt (12.f)); g.drawText (capText, 860, 18, 300, 22, juce::Justification::centredLeft);
    g.setFont (fnt (18.f, true));
    g.drawText ("SPEICHER", 330, 746, 140, 22, juce::Justification::centredLeft);
    g.setColour (col::ink2); g.setFont (fnt (12.f));
    g.drawText (juce::String::fromUTF8 ("Alle Slots nehmen beim Spielen automatisch die letzten 1–16 Takte auf. „Halten“ friert ein, Chip in Ableton ziehen."), 470, 746, 900, 22, juce::Justification::centredLeft);
}

void Content::resized()
{
    play.setBounds (430, 14, 90, 32);
    bpm.setBounds (536, 14, 130, 30);
    swing.setBounds (680, 14, 140, 30);
    fill.setBounds (1170, 16, 60, 28);
    holdAll.setBounds (1240, 16, 120, 28);
    leftView.setBounds (12, 66, 312, H - 76);
    left.setSize (300, 10); left.relayout();
    for (int t = 0; t < NT; ++t) rows[t]->setBounds (330, 66 + t * 84, W - 342, 80);
    const int sw = (W - 342 - 30) / 4;
    for (int i = 0; i < 8; ++i) slotViews[i]->setBounds (330 + (i % 4) * (sw + 10), 772 + (i / 4) * 124, sw, 120);
}

void Content::tick()
{
    const auto g = proc.uiStep.load();
    for (auto* r : rows) r->tick (g);
    float mx, my; proc.masterEffUI (juce::jmax<int64_t> (0, g), mx, my);
    left.master.pad.effX = mx; left.master.pad.effY = my; left.master.pad.repaint();

    const bool hostRun = proc.hostPlaying.load();
    const bool running = hostRun || proc.internalPlay.load();
    play.setToggleState (proc.internalPlay.load(), juce::dontSendNotification);
    play.setButtonText (hostRun ? "Host" : (proc.internalPlay.load() ? "Stop" : "Play"));
    int live = 0; size_t hist = 0; bool anyLive = false;
    {
        const juce::ScopedLock sl (proc.slotLock);
        for (int i = 0; i < 8; ++i) { if (! proc.cfg[(size_t) i].lock) { ++live; if (proc.slots[(size_t) i]) anyLive = true; } }
        hist = proc.history.size();
    }
    juce::String t = running ? juce::String::fromUTF8 ("Capture läuft · Puffer ") + juce::String ((int) hist) + juce::String::fromUTF8 ("/16 Takte · ") + juce::String (live) + juce::String::fromUTF8 (" Slots live")
                             : juce::String::fromUTF8 ("Capture startet mit Play · ") + juce::String (live) + juce::String::fromUTF8 (" Slots live");
    t << juce::String::fromUTF8 (" · ") << juce::String (proc.hostBpm.load(), 1) << " BPM";
    if (t != capText) { capText = t; repaint (830, 10, 340, 40); }
    holdAll.setButtonText (anyLive ? "Alle halten" : "Alle live");
    const int v = proc.slotsVersion.load();
    if (v != lastVersion) { lastVersion = v; for (auto* s : slotViews) s->refresh(); left.random.repaint(); }
}

// ======================= Editor =======================
Grids8Editor::Grids8Editor (Grids8Processor& p) : AudioProcessorEditor (p), content (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (Content::W / 2, Content::H / 2, Content::W * 2, Content::H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) Content::W / Content::H);
    setSize ((int) (Content::W * .85), (int) (Content::H * .85));
    startTimerHz (30);
}
Grids8Editor::~Grids8Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}
void Grids8Editor::resized()
{
    const float s = (float) getWidth() / (float) Content::W;
    content.setBounds (0, 0, Content::W, Content::H);
    content.setTransform (juce::AffineTransform::scale (s));
}
} // namespace g8
