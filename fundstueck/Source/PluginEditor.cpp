#include "PluginEditor.h"
#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

using namespace juce;

static const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H" };
static const char* kClsNames[4] = { "tief", "mittel", "hell", "tonal" };
static const char* kLaneNames[5] = { "Kick", "Snare", "Hat", "Perc", "Melodie" };
static const char* kLaneIds[5] = { "kick", "snare", "hat", "perc", "mel" };

static Font fnt (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }

static void drawWave (Graphics& g, const std::vector<float>& d, Rectangle<float> r, Colour c, size_t from = 0, size_t to = 0)
{
    if (to == 0 || to > d.size()) to = d.size();
    if (to <= from || r.getWidth() < 1) return;
    g.setColour (c);
    const int w = (int) r.getWidth();
    const size_t n = to - from;
    for (int x = 0; x < w; ++x)
    {
        const size_t a = from + (size_t) ((double) x / w * n), b = jmax (a + 1, from + (size_t) ((double) (x + 1) / w * n));
        float mn = 0, mx = 0;
        for (size_t i = a; i < b && i < to; ++i) { mn = jmin (mn, d[i]); mx = jmax (mx, d[i]); }
        const float y1 = r.getCentreY() - mx * r.getHeight() * .5f, y2 = r.getCentreY() - mn * r.getHeight() * .5f;
        g.fillRect (r.getX() + (float) x, y1, 1.0f, jmax (1.0f, y2 - y1));
    }
}

static String clsLabel (const fs::Slice& s)
{
    String lab (kClsNames[s.cls]);
    if (s.cls == fs::clsTonal) lab << " " << kNoteNames[((int) std::round (s.midi) % 12 + 12) % 12];
    return lab;
}

// ---------------------------------------------------------------- Look
FsLook::FsLook()
{
    setColour (TextButton::buttonColourId, col::panel2);
    setColour (TextButton::buttonOnColourId, col::ink);
    setColour (TextButton::textColourOffId, col::ink);
    setColour (TextButton::textColourOnId, col::dark);
    setColour (Slider::trackColourId, col::lane[0]);
    setColour (Slider::backgroundColourId, col::panel2);
    setColour (Slider::textBoxTextColourId, col::ink);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (ComboBox::backgroundColourId, col::panel2);
    setColour (ComboBox::outlineColourId, col::line);
    setColour (ComboBox::textColourId, col::ink);
    setColour (ComboBox::arrowColourId, col::dim);
    setColour (PopupMenu::backgroundColourId, col::panel2);
    setColour (PopupMenu::textColourId, col::ink);
    setColour (PopupMenu::highlightedBackgroundColourId, col::ink);
    setColour (PopupMenu::highlightedTextColourId, col::dark);
    setColour (Label::textColourId, col::ink);
    setColour (BubbleComponent::backgroundColourId, col::bg);
    setColour (BubbleComponent::outlineColourId, col::line);
    setColour (TooltipWindow::backgroundColourId, col::bg);
    setColour (TooltipWindow::textColourId, col::ink);
    setColour (TooltipWindow::outlineColourId, col::line);
}

void FsLook::drawButtonBackground (Graphics& g, Button& b, const Colour& bgc, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (.5f);
    auto c = b.getToggleState() ? b.findColour (TextButton::buttonOnColourId) : bgc;
    if (down) c = c.brighter (.1f);
    const float rad = jmin (7.f, r.getHeight() * .3f);
    g.setColour (c);
    g.fillRoundedRectangle (r, rad);
    g.setColour (b.getToggleState() ? c : (over ? Colour (0xff5a5141) : col::line));
    g.drawRoundedRectangle (r, rad, 1.f);
}

Font FsLook::getTextButtonFont (TextButton& b, int h) { return fnt (jmin (14.f, h * .55f), b.getHeight() < 26); }
Font FsLook::getSliderPopupFont (Slider&) { return fnt (12, true); }

void FsLook::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle st, Slider& s)
{
    if (st != Slider::LinearBar) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, st, s); return; }
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    g.setColour (s.findColour (Slider::backgroundColourId));
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (s.findColour (Slider::trackColourId).withAlpha (.55f));
    g.fillRoundedRectangle (r.withRight (pos), 5.f);
}

void FsLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s)
{
    const auto c = s.findColour (Slider::trackColourId).withMultipliedAlpha (s.isEnabled() ? 1.f : .4f);
    const float alpha = (float) s.getProperties().getWithDefault ("dimmed", false) ? .35f : 1.f;
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2);
    const float d = jmin (r.getWidth(), r.getHeight()), rad = d * .5f;
    const auto ctr = r.getCentre();
    const float ang = a0 + pos * (a1 - a0);
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? (a0 + a1) * .5f : a0;

    Path track; track.addCentredArc (ctr.x, ctr.y, rad - 2, rad - 2, 0, a0, a1, true);
    g.setColour (col::line); g.strokePath (track, PathStrokeType (3.f, PathStrokeType::curved, PathStrokeType::rounded));
    Path val; val.addCentredArc (ctr.x, ctr.y, rad - 2, rad - 2, 0, jmin (from, ang), jmax (from, ang), true);
    g.setColour (c.withMultipliedAlpha (alpha)); g.strokePath (val, PathStrokeType (3.f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour (col::panel.brighter (.05f)); g.fillEllipse (Rectangle<float> (d - 10, d - 10).withCentre (ctr));
    Path ptr; ptr.addRoundedRectangle (-1.2f, -(rad - 6), 2.4f, rad * .55f, 1.f);
    g.setColour (col::ink.withMultipliedAlpha (alpha)); g.fillPath (ptr, AffineTransform::rotation (ang).translated (ctr));
}

Label* FsLook::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::textColourId, col::ink);
    l->setFont (fnt (13.f));
    l->setJustificationType (Justification::centred);
    return l;
}

void PlayButton::paintButton (Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (2);
    g.setColour ((on ? col::ink : col::lane[0]).withMultipliedAlpha (isEnabled() ? 1.f : .5f));
    g.fillEllipse (r);
    if (over && isEnabled()) { g.setColour (Colours::white.withAlpha (.12f)); g.fillEllipse (r); }
    g.setColour (col::dark);
    const auto c = r.getCentre();
    if (on) g.fillRect (Rectangle<float> (14, 14).withCentre (c));
    else
    {
        Path p; p.addTriangle (c.x - 6, c.y - 9, c.x - 6, c.y + 9, c.x + 10, c.y);
        g.fillPath (p);
    }
}

// ---------------------------------------------------------------- Export
void ExportButton::mouseDrag (const MouseEvent& e)
{
    if (dragged || e.getDistanceFromDragStart() < 8 || ! render) return;
    dragged = true;
    const auto f = render();
    if (f.existsAsFile())
        DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}

void ExportButton::clicked()
{
    if (dragged || ! render) return;
    const auto f = render();
    if (onClickExport) onClickExport (f);
}

// ---------------------------------------------------------------- Overview
void OverviewView::refresh()
{
    const int w = jmax (1, getWidth());
    peaks.assign ((size_t) w, { 0.f, 0.f });
    const auto& d = proc.source;
    if (d.empty()) { repaint(); return; }
    const size_t n = d.size();
    for (int x = 0; x < w; ++x)
    {
        const size_t a = (size_t) ((double) x / w * n), b = jmax (a + 1, (size_t) ((double) (x + 1) / w * n));
        float mn = 0, mx = 0;
        for (size_t i = a; i < b && i < n; ++i) { mn = jmin (mn, d[i]); mx = jmax (mx, d[i]); }
        peaks[(size_t) x] = { mn, mx };
    }
    repaint();
}

void OverviewView::drawStatus (Graphics& g)
{
    const auto f = fnt (11);
    const String t = proc.statusText;
    if (t.isEmpty()) return;
    const float w = GlyphArrangement::getStringWidth (f, t) + 16;
    const Rectangle<float> box (6, 6, jmin (w, (float) getWidth() - 12), 18);
    g.setColour (col::bg.withAlpha (.8f)); g.fillRoundedRectangle (box, 5.f);
    g.setColour (proc.recording.load() ? col::lane[0] : col::ink); g.setFont (f);
    g.drawText (t, box.reduced (8, 0).toNearestInt(), Justification::centredLeft, true);
}

void OverviewView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panel2);
    g.fillRoundedRectangle (r, 10.f);
    if (proc.source.empty() || peaks.empty())
    {
        drawStatus (g);
        g.setColour (col::dim); g.setFont (fnt (14));
        g.drawText (U8 ("Audiodateien hierher ziehen \xe2\x80\x93 Klopfen, Schl\xc3\xbc" "ssel, Gl\xc3\xa4ser, Stimme"), getLocalBounds(), Justification::centred);
        return;
    }
    const double n = (double) proc.source.size();
    const auto set = proc.msgSet;
    if (set)
        for (auto& s : set->slices)
        {
            const float x1 = (float) (s.start / n) * r.getWidth(), x2 = (float) (s.end / n) * r.getWidth();
            g.setColour (col::cls (s.cls).withAlpha (.13f));
            g.fillRect (x1, 0.f, x2 - x1, r.getHeight());
        }
    g.setColour (Colour (0xff8d8471));
    const float cy = r.getCentreY(), hh = r.getHeight() * .45f;
    for (size_t x = 0; x < peaks.size(); ++x)
    {
        const float y1 = cy - peaks[x].second * hh, y2 = cy - peaks[x].first * hh;
        g.fillRect ((float) x, y1, 1.f, jmax (1.f, y2 - y1));
    }
    if (set)
        for (auto& s : set->slices)
        {
            g.setColour (col::cls (s.cls));
            g.fillRect ((float) (s.start / n) * r.getWidth(), 0.f, 2.f, r.getHeight());
        }
    drawStatus (g);
}

void OverviewView::mouseDown (const MouseEvent& e)
{
    const auto set = proc.msgSet;
    if (! set || proc.source.empty()) return;
    const double pos = e.position.x / (double) getWidth() * (double) proc.source.size();
    for (size_t i = 0; i < set->slices.size(); ++i)
        if (pos >= set->slices[i].start && pos < set->slices[i].end) { content.chooseSlice ((int) i); return; }
}

// ---------------------------------------------------------------- Chips
Rectangle<float> ChipsView::chipRect (int i) const
{
    const float gap = 6.f, w = ((float) getWidth() - 15 * gap) / 16.f, h = ((float) getHeight() - gap) / 2.f;
    return { (float) (i % 16) * (w + gap), (float) (i / 16) * (h + gap), w, h };
}

void ChipsView::paint (Graphics& g)
{
    const auto set = proc.msgSet;
    if (! set) return;
    int used[5]; for (int l = 0; l < 5; ++l) used[l] = proc.laneSlice[l].load();
    for (size_t i = 0; i < set->slices.size() && i < 32; ++i)
    {
        const auto& s = set->slices[i];
        auto r = chipRect ((int) i);
        g.setColour (col::panel2); g.fillRoundedRectangle (r, 7.f);
        bool inUse = false; for (int l = 0; l < 5; ++l) inUse |= used[l] == (int) i;
        g.setColour (inUse ? col::ink : col::line); g.drawRoundedRectangle (r.reduced (.5f), 7.f, inUse ? 1.6f : 1.f);
        const size_t n = s.data.size();
        drawWave (g, s.data, r.reduced (5, 4).withTrimmedBottom (12), col::cls (s.cls),
                  (size_t) (proc.getTrimStart ((int) i) * (float) n), (size_t) (proc.getTrimEnd ((int) i) * (float) n));
        g.setColour (col::dim); g.setFont (fnt (10));
        g.drawText (clsLabel (s), r.withTrimmedTop (r.getHeight() - 14).toNearestInt(), Justification::centred);
    }
}

void ChipsView::mouseDown (const MouseEvent& e)
{
    const auto set = proc.msgSet;
    if (! set) return;
    for (int i = 0; i < (int) set->slices.size() && i < 32; ++i)
        if (chipRect (i).contains (e.position)) { content.chooseSlice (i); return; }
}

// ---------------------------------------------------------------- SampleEditView
SampleEditView::SampleEditView (FundstueckProcessor& p, Content& c) : proc (p), content (c)
{
    closeBtn.setTooltip (U8 ("Bearbeitung verlassen"));
    closeBtn.onClick = [this] { setVisible (false); index = -1; };
    playBtn.setButtonText (U8 ("\xe2\x96\xb6  Anh\xc3\xb6ren"));
    playBtn.onClick = [this] { proc.audition (index, -1); };
    resetBtn.setButtonText (U8 ("Zur\xc3\xbc" "cksetzen"));
    resetBtn.onClick = [this] { proc.pushUndo(); ts = 0; te = 1; proc.setTrim (index, ts, te); proc.audition (index, -1); repaint(); };
    prevBtn.setButtonText (U8 ("\xe2\x97\x80"));
    nextBtn.setButtonText (U8 ("\xe2\x96\xb6"));
    prevBtn.onClick = [this] { const int n = proc.msgSet ? (int) proc.msgSet->slices.size() : 0; if (n > 0) { open ((index - 1 + n) % n); proc.audition (index, -1); } };
    nextBtn.onClick = [this] { const int n = proc.msgSet ? (int) proc.msgSet->slices.size() : 0; if (n > 0) { open ((index + 1) % n); proc.audition (index, -1); } };
    for (auto* b : { &closeBtn, &playBtn, &resetBtn, &prevBtn, &nextBtn }) addAndMakeVisible (b);
    for (int l = 0; l < fs::kLanes; ++l)
    {
        auto* b = laneBtns.add (new TextButton (kLaneNames[l]));
        b->setColour (TextButton::buttonOnColourId, col::lane[l]);
        b->setColour (TextButton::textColourOffId, col::lane[l]);
        b->onClick = [this, l] { proc.pushUndo(); proc.setLaneSlice (l, index); proc.audition (index, l); open (index); };
        addAndMakeVisible (b);
    }
}

void SampleEditView::open (int idx)
{
    index = idx;
    ts = proc.getTrimStart (idx); te = proc.getTrimEnd (idx);
    for (int l = 0; l < laneBtns.size(); ++l) laneBtns[l]->setToggleState (proc.laneSlice[l].load() == idx, dontSendNotification);
    setVisible (true);
    toFront (false);
    repaint();
}

void SampleEditView::resized()
{
    const int w = getWidth();
    closeBtn.setBounds (w - 50, 12, 36, 30);
    nextBtn.setBounds (w - 96, 12, 36, 30);
    prevBtn.setBounds (w - 136, 12, 36, 30);
    waveR = { 20, 66, w - 40, 390 };
    playBtn.setBounds (20, 482, 140, 32);
    resetBtn.setBounds (166, 482, 140, 32);
    for (int l = 0; l < laneBtns.size(); ++l) laneBtns[l]->setBounds (430 + l * 124, 482, 118, 32);
}

float SampleEditView::xToFrac (float x) const { return jlimit (0.f, 1.f, (x - (float) waveR.getX()) / (float) waveR.getWidth()); }
float SampleEditView::fracToX (float f) const { return (float) waveR.getX() + f * (float) waveR.getWidth(); }

void SampleEditView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panel); g.fillRoundedRectangle (r, 14.f);
    g.setColour (col::line); g.drawRoundedRectangle (r.reduced (.5f), 14.f, 1.f);
    const auto set = proc.msgSet;
    if (! set || index < 0 || index >= (int) set->slices.size()) return;
    const auto& s = set->slices[(size_t) index];
    const double sr = set->sampleRate;
    const double dur = (double) s.data.size() / sr;

    g.setColour (col::ink); g.setFont (fnt (16, true));
    g.drawText ("Schnipsel " + String (index + 1) + " / " + String ((int) set->slices.size()), 20, 12, 220, 30, Justification::centredLeft);
    g.setColour (col::cls (s.cls)); g.setFont (fnt (14));
    g.drawText (clsLabel (s) + U8 (" \xc2\xb7 ") + String (roundToInt (dur * 1000.0)) + " ms", 220, 12, 300, 30, Justification::centredLeft);

    g.setColour (col::panel2); g.fillRoundedRectangle (waveR.toFloat(), 10.f);
    drawWave (g, s.data, waveR.toFloat().reduced (0, 12), col::cls (s.cls));
    const float xs = fracToX (ts), xe = fracToX (te);
    g.setColour (Colours::black.withAlpha (.6f));
    g.fillRect (Rectangle<float>::leftTopRightBottom ((float) waveR.getX(), (float) waveR.getY(), xs, (float) waveR.getBottom()));
    g.fillRect (Rectangle<float>::leftTopRightBottom (xe, (float) waveR.getY(), (float) waveR.getRight(), (float) waveR.getBottom()));

    auto handle = [&] (float x, const String& lab, Colour c, bool left)
    {
        g.setColour (c); g.fillRect (x - 1.f, (float) waveR.getY(), 2.f, (float) waveR.getHeight());
        const Rectangle<float> flag (left ? x : x - 52.f, (float) waveR.getY(), 52.f, 20.f);
        g.fillRoundedRectangle (flag, 4.f);
        g.setColour (col::dark); g.setFont (fnt (10, true));
        g.drawText (lab, flag.toNearestInt(), Justification::centred);
    };
    handle (xs, "START", col::lane[4], true);
    handle (xe, "ENDE", col::lane[0], false);

    g.setFont (fnt (12)); g.setColour (col::dim);
    g.drawText (String (roundToInt (ts * dur * 1000.0)) + " ms", jlimit (waveR.getX(), waveR.getRight() - 80, (int) xs - 40), waveR.getBottom() + 4, 80, 16, Justification::centred);
    g.drawText (String (roundToInt (te * dur * 1000.0)) + " ms", jlimit (waveR.getX(), waveR.getRight() - 80, (int) xe - 40), waveR.getBottom() + 4, 80, 16, Justification::centred);
    g.setColour (col::dim); g.setFont (fnt (11));
    g.drawText (U8 ("AUF SPUR LEGEN"), 320, 482, 104, 32, Justification::centredRight);
    g.setColour (col::ink); g.setFont (fnt (13));
    g.drawText (U8 ("L\xc3\xa4nge: ") + String (roundToInt ((te - ts) * dur * 1000.0)) + " ms", 20, 530, 300, 20, Justification::centredLeft);
    g.setColour (col::dim); g.setFont (fnt (12));
    g.drawText (U8 ("Griffe ziehen oder in die Wellenform klicken \xc2\xb7 Doppelklick setzt zur\xc3\xbc" "ck \xc2\xb7 X schlie\xc3\x9f" "t die Bearbeitung"),
                20, 556, getWidth() - 40, 20, Justification::centredLeft);
}

void SampleEditView::mouseMove (const MouseEvent& e)
{
    const bool near = waveR.contains (e.position.toInt())
                      && (std::abs (e.position.x - fracToX (ts)) < 10 || std::abs (e.position.x - fracToX (te)) < 10);
    setMouseCursor (near ? MouseCursor::LeftRightResizeCursor : MouseCursor::NormalCursor);
}

void SampleEditView::mouseDown (const MouseEvent& e)
{
    if (! waveR.expanded (0, 22).contains (e.position.toInt())) return;
    const float x = e.position.x;
    proc.pushUndo();
    dragging = std::abs (x - fracToX (ts)) <= std::abs (x - fracToX (te)) ? 1 : 2;
    mouseDrag (e);
}

void SampleEditView::mouseDrag (const MouseEvent& e)
{
    if (dragging == 0) return;
    const float f = xToFrac (e.position.x);
    if (dragging == 1) ts = jlimit (0.f, te - 0.02f, f);
    else te = jlimit (ts + 0.02f, 1.f, f);
    repaint();
}

void SampleEditView::mouseUp (const MouseEvent&)
{
    if (dragging == 0) return;
    dragging = 0;
    proc.setTrim (index, ts, te);
    proc.audition (index, -1);
}

void SampleEditView::mouseDoubleClick (const MouseEvent& e)
{
    if (! waveR.contains (e.position.toInt())) return;
    proc.pushUndo();
    ts = 0; te = 1;
    proc.setTrim (index, ts, te);
    repaint();
}

// ---------------------------------------------------------------- LaneRow
LaneRow::LaneRow (FundstueckProcessor& p, Content& c, int l) : proc (p), content (c), lane (l)
{
    const String id (kLaneIds[l]);
    auto knob = [&] (Slider& s, const String& pid, std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>& att)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setColour (Slider::trackColourId, col::lane[l]);
        s.setPopupDisplayEnabled (true, true, &content);
        s.setMouseDragSensitivity (160);
        addAndMakeVisible (s);
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id + pid, s);
        if (auto* prm = p.apvts.getParameter (id + pid))
            s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    };
    knob (vol, "_vol", aVol);
    knob (tune, "_tune", aTune);
    knob (atk, "_atk", aAtk);
    knob (dec, "_len", aDec);
    knob (len, "_steps", aLen);
    vol.setTooltip (U8 ("Lautst\xc3\xa4rke")); tune.setTooltip (U8 ("Tonh\xc3\xb6he (Halbt\xc3\xb6ne)"));
    atk.setTooltip ("Attack: Einschwingzeit"); dec.setTooltip (U8 ("Decay: Ausklingen (Anteil der Schnipsel-L\xc3\xa4nge)"));
    len.setTooltip (U8 ("Spurl\xc3\xa4nge 1/16 \xe2\x80\xa6 16/16 (wirkt, wenn P = Polymeter an ist)"));

    auto toggle = [&] (TextButton& b, const String& pid, Colour on, const String& tip, std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>& att)
    {
        b.setClickingTogglesState (true);
        b.setColour (TextButton::buttonOnColourId, on);
        b.setTooltip (tip);
        addAndMakeVisible (b);
        att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, id + pid, b);
    };
    toggle (solo, "_solo", col::lane[1], "Solo", aSolo);
    toggle (mute, "_mute", col::dim, "Mute", aMute);
    toggle (poly, "_poly", col::lane[l], U8 ("Polymeter: Spur l\xc3\xa4uft mit eigener L\xc3\xa4nge (Regler rechts)"), aPoly);
    poly.onStateChange = [this] { len.getProperties().set ("dimmed", ! poly.getToggleState()); len.repaint(); repaint(); };
    len.onValueChange = [this] { repaint(); };
    len.getProperties().set ("dimmed", ! poly.getToggleState());
}

void LaneRow::resized()
{
    const int h = getHeight();
    nameR = { 10, 0, 68, h };
    thumbR = { 78, (h - 30) / 2, 58, 30 };
    upR = { 139, h / 2 - 13, 14, 13 };
    downR = { 139, h / 2, 14, 13 };
    const int kx[4] = { 158, 196, 234, 272 };
    Slider* ks[4] = { &vol, &tune, &atk, &dec };
    for (int i = 0; i < 4; ++i) ks[i]->setBounds (kx[i], 1, 36, 32);
    solo.setBounds (314, 11, 24, 24);
    mute.setBounds (340, 11, 24, 24);
    poly.setBounds (366, 11, 24, 24);
    len.setBounds (396, 1, 36, 32);
    stepsR = { 440, 5, getWidth() - 440 - 8, h - 10 };
}

Rectangle<float> LaneRow::stepRect (int s) const
{
    const float gap = 3.f, ggap = 7.f;
    const float w = ((float) stepsR.getWidth() - 15 * gap - 3 * ggap) / 16.f;
    return { (float) stepsR.getX() + (float) s * (w + gap) + (float) (s / 4) * ggap, (float) stepsR.getY(), w, (float) stepsR.getHeight() };
}

int LaneRow::stepAt (Point<float> p) const
{
    for (int s = 0; s < 16; ++s) if (stepRect (s).contains (p)) return s;
    return -1;
}

void LaneRow::paint (Graphics& g)
{
    const auto c = col::lane[lane];
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panel2); g.fillRoundedRectangle (r, 10.f);
    if (content.armed == lane) { g.setColour (c); g.drawRoundedRectangle (r.reduced (.75f), 10.f, 1.5f); }

    g.setColour (c); g.setFont (fnt (15, true));
    g.drawText (kLaneNames[lane], nameR, Justification::centredLeft);

    g.setColour (Colour (0x33000000)); g.fillRoundedRectangle (thumbR.toFloat(), 5.f);
    const auto set = proc.msgSet;
    const int si = proc.laneSlice[lane].load();
    if (set && si >= 0 && si < (int) set->slices.size())
    {
        const auto& sl = set->slices[(size_t) si];
        const size_t n = sl.data.size();
        drawWave (g, sl.data, thumbR.toFloat().reduced (3, 2), c, (size_t) (proc.getTrimStart (si) * (float) n), (size_t) (proc.getTrimEnd (si) * (float) n));
    }

    g.setColour (col::dim);
    Path up; up.addTriangle ((float) upR.getCentreX() - 4, (float) upR.getBottom() - 3, (float) upR.getCentreX() + 4, (float) upR.getBottom() - 3, (float) upR.getCentreX(), (float) upR.getY() + 3);
    Path dn; dn.addTriangle ((float) downR.getCentreX() - 4, (float) downR.getY() + 3, (float) downR.getCentreX() + 4, (float) downR.getY() + 3, (float) downR.getCentreX(), (float) downR.getBottom() - 3);
    g.fillPath (up); g.fillPath (dn);

    const int L = proc.laneLength (lane);
    g.setFont (fnt (8.5f)); g.setColour (col::dim);
    const char* labs[4] = { "VOL", "TUNE", "ATTACK", "DECAY" };
    Slider* ks[4] = { &vol, &tune, &atk, &dec };
    for (int i = 0; i < 4; ++i) g.drawText (labs[i], ks[i]->getX() - 4, 33, 44, 12, Justification::centred);
    g.setColour (poly.getToggleState() ? c : col::dim);
    g.drawText (poly.getToggleState() ? String (L) + "/16" : String ("STEPS"), len.getX() - 4, 33, 44, 12, Justification::centred);

    const int now = proc.currentStep.load();
    const int cur = now >= 0 ? now % L : -1;
    for (int s = 0; s < 16; ++s)
    {
        auto sr = stepRect (s);
        const int v = proc.steps[lane][s].load();
        const bool on = lane == fs::kMel ? v >= 0 : v != 0;
        const bool inside = s < L;
        Colour fill = on ? c : Colours::white.withAlpha (s % 4 == 0 ? .09f : .05f);
        if (! inside) fill = on ? c.withAlpha (.22f) : Colours::white.withAlpha (.015f);
        g.setColour (fill);
        g.fillRoundedRectangle (sr, 6.f);
        if (! inside) { g.setColour (col::line); g.drawRoundedRectangle (sr.reduced (.5f), 6.f, 1.f); }
        if (s == cur) { g.setColour (col::ink); g.drawRoundedRectangle (sr.reduced (1), 6.f, 2.f); }
        g.setColour (inside ? col::dark : col::dim);
        if (lane == fs::kMel && on)
        {
            g.setFont (fnt (sr.getWidth() < 34 ? 10.5f : 11.5f, true));
            g.drawText (FundstueckProcessor::noteName (v), sr.toNearestInt(), Justification::centred);
        }
        else if (lane == 2 && v > 1)
        {
            g.setFont (fnt (12, true));
            g.drawText (String::repeatedString (U8 ("\xe2\x80\xa2"), v), sr.withTrimmedTop (sr.getHeight() * .5f).toNearestInt(), Justification::centred);
        }
    }
}

void LaneRow::mouseDown (const MouseEvent& e)
{
    const auto p = e.position;
    dragStep = -1;
    if (nameR.contains (p.toInt()))
    {
        content.armed = content.armed == lane ? -1 : lane;
        for (auto* sib : getParentComponent()->getChildren()) if (dynamic_cast<LaneRow*> (sib) != nullptr) sib->repaint();
        return;
    }
    if (thumbR.contains (p.toInt())) { proc.audition (proc.laneSlice[lane].load(), lane); return; }
    if (upR.contains (p.toInt())) { proc.pushUndo(); proc.cycleSlice (lane, -1); return; }
    if (downR.contains (p.toInt())) { proc.pushUndo(); proc.cycleSlice (lane, 1); return; }
    const int s = stepAt (p);
    if (s < 0) return;
    if (lane == fs::kMel)
    {
        if (e.mods.isPopupMenu()) { showNoteMenu (s); return; }
        const int v = proc.steps[lane][s].load();
        if (v >= 0) { dragStep = s; dragBaseNote = v; dragStartY = p.y; dragMoved = false; return; }  // Ziehen oder Loeschen beim Loslassen
    }
    proc.pushUndo();
    proc.stepClick (lane, s, 0);
    repaint();
}

void LaneRow::mouseDrag (const MouseEvent& e)
{
    if (dragStep < 0) return;
    const int d = roundToInt ((dragStartY - e.position.y) / 8.0f);
    if (d == 0 && ! dragMoved) return;
    const auto N = proc.scaleNotes();
    if (N.empty()) return;
    int base = 0, best = 1000;
    for (int i = 0; i < (int) N.size(); ++i) { const int dist = std::abs (N[(size_t) i] - dragBaseNote); if (dist < best) { best = dist; base = i; } }
    const int note = N[(size_t) jlimit (0, (int) N.size() - 1, base + d)];
    if (note != proc.steps[lane][dragStep].load())
    {
        if (! dragMoved) proc.pushUndo();
        dragMoved = true;
        proc.setMelNote (dragStep, note, true);
        repaint();
    }
}

void LaneRow::mouseUp (const MouseEvent&)
{
    if (dragStep >= 0 && ! dragMoved)
    {
        proc.pushUndo();
        proc.setMelNote (dragStep, -1, false);
        repaint();
    }
    dragStep = -1;
}

void LaneRow::mouseMove (const MouseEvent& e)
{
    const int s = stepAt (e.position);
    const bool noteHere = lane == fs::kMel && s >= 0 && proc.steps[lane][s].load() >= 0;
    setMouseCursor (noteHere ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor);
}

void LaneRow::showNoteMenu (int s)
{
    const auto N = proc.scaleNotes();
    const int cur = proc.steps[fs::kMel][s].load();
    PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());
    m.addSectionHeader (U8 ("Note \xc2\xb7 ") + String (kNoteNames[proc.root]) + " " + fs::kScales[proc.scale].name);
    for (int i = (int) N.size() - 1; i >= 0; --i)
    {
        String name = FundstueckProcessor::noteName (N[(size_t) i]);
        if ((N[(size_t) i] - proc.root) % 12 == 0) name << U8 ("  \xc2\xb7 Grundton");
        m.addItem (i + 1, name, true, N[(size_t) i] == cur);
    }
    m.addSeparator();
    m.addItem (1000, U8 ("L\xc3\xb6schen"), cur >= 0);
    const auto area = localAreaToGlobal (stepRect (s).toNearestInt());
    SafePointer<LaneRow> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (area).withMaximumNumColumns (1),
                     [safe, s, N] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         safe->proc.pushUndo();
                         if (r == 1000) safe->proc.setMelNote (s, -1, false);
                         else safe->proc.setMelNote (s, N[(size_t) r - 1], true);
                         safe->repaint();
                     });
}

void LaneRow::mouseDoubleClick (const MouseEvent& e)
{
    if (thumbR.contains (e.position.toInt()) && proc.laneSlice[lane].load() >= 0)
        content.openEditor (proc.laneSlice[lane].load());
}

void LaneRow::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const int s = stepAt (e.position);
    if (lane != fs::kMel || s < 0 || proc.steps[lane][s].load() < 0) { Component::mouseWheelMove (e, w); return; }
    const auto now = Time::getMillisecondCounter();
    if (now - lastWheelUndo > 700) proc.pushUndo();
    lastWheelUndo = now;
    proc.shiftNote (lane, s, w.deltaY > 0 ? 1 : -1);
    repaint();
}

// ---------------------------------------------------------------- Content
namespace L
{
const int sliderY = 30, sliderW = 86, sliderX0 = 412, sliderGap = 6;
const Rectangle<int> sec1 { 16, 70, 1068, 232 }, sec2 { 16, 310, 1068, 84 }, sec3 { 16, 402, 1068, 290 };
}

static const char* kHeaderLabels[7] = { "TEMPO", "SWING", "FEEL", "RAUM", "TAPE", "SUB-KICK", "MASTER" };

void Content::bigSlider (Slider& s, const char* id, std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>& att)
{
    s.setSliderStyle (Slider::LinearBar);
    s.setTextBoxStyle (Slider::TextBoxLeft, false, 80, 22);
    addAndMakeVisible (s);
    if (id != nullptr) att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s);
}

Content::Content (FundstueckProcessor& p) : proc (p), overview (p, *this), chips (p, *this), editor (p, *this)
{
    setLookAndFeel (&look);
    addAndMakeVisible (play);
    play.onClick = [this] { if (! proc.syncOn.load()) proc.internalPlay = ! proc.internalPlay.load(); };

    bigSlider (bpm, "bpm", aBpm); bigSlider (swing, "swing", aSwing); bigSlider (feel, "feel", aFeel);
    bigSlider (rev, "rev", aRev); bigSlider (tape, "tape", aTape); bigSlider (sub, "sub", aSub); bigSlider (master, "master", aMaster);
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> none;
    bigSlider (sens, nullptr, none); bigSlider (dens, nullptr, none);
    for (auto* s : { &sens, &dens })
    {
        s->setRange (0, 100, 1);
        s->textFromValueFunction = [] (double v) { return String (roundToInt (v)) + " %"; };
    }
    sens.setValue (proc.sens * 100, dontSendNotification);
    dens.setValue (proc.density * 100, dontSendNotification);
    sens.onValueChange = [this] { sensDirty = true; sensChangedAt = Time::getMillisecondCounter(); };
    dens.onValueChange = [this] { proc.density = (float) dens.getValue() / 100.f; };
    for (auto* s : { &bpm, &swing, &feel }) s->setColour (Slider::trackColourId, col::lane[0]);
    for (auto* s : { &rev, &tape, &sub, &master }) s->setColour (Slider::trackColourId, col::lane[2]);
    sens.setColour (Slider::trackColourId, col::lane[1]);
    dens.setColour (Slider::trackColourId, col::lane[4]);

    auto btn = [this] (TextButton& b, const String& t, std::function<void()> fn)
    { b.setButtonText (t); b.onClick = std::move (fn); addAndMakeVisible (b); };
    btn (demoBtn, U8 ("Demo-Ger\xc3\xa4usche"), [this] { proc.pushUndo(); proc.loadDemo(); });
    btn (loadBtn, U8 ("Datei laden\xe2\x80\xa6"), [this] { openFileChooser(); });
    btn (recBtn, U8 ("\xe2\x97\x8f  Aufnehmen"), [this] { if (proc.recording.load()) { proc.pushUndo(); proc.stopRecording(); } else proc.startRecording(); });
    recBtn.setColour (TextButton::textColourOffId, col::lane[0]);
    recBtn.setTooltip (U8 ("Nimmt den Eingang auf. Ableton: im Ger\xc3\xa4t \xe2\x80\x9eSidechain\xe2\x80\x9c aufklappen und die Mikrofon-Spur w\xc3\xa4hlen."));
    syncBtn.setButtonText ("HOST-SYNC");
    syncBtn.setClickingTogglesState (true);
    syncBtn.setColour (TextButton::buttonOnColourId, col::lane[4]);
    syncBtn.onClick = [this] { if (syncBtn.getToggleState()) proc.internalPlay = false; };
    addAndMakeVisible (syncBtn);
    aSync = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "sync", syncBtn);
    btn (rollAll, U8 ("Alles w\xc3\xbcrfeln"), [this] { proc.pushUndo(); proc.autoAssign (true); proc.rollDrums(); proc.rollMel(); });
    btn (rollDrums, "Drums", [this] { proc.pushUndo(); proc.rollDrums(); });
    btn (rollMel, "Melodie", [this] { proc.pushUndo(); proc.rollMel(); });
    btn (rollSounds, "Sounds", [this] { proc.pushUndo(); proc.autoAssign (true); });
    btn (clearBtn, "Leeren", [this] { proc.pushUndo(); proc.clearPattern(); });
    btn (undoBtn, U8 ("\xe2\x86\xb6  R\xc3\xbc" "ckg\xc3\xa4ngig"), [this] { proc.undo(); syncControls(); });
    btn (redoBtn, U8 ("\xe2\x86\xb7  Wiederholen"), [this] { proc.redo(); syncControls(); });
    undoBtn.setTooltip (U8 ("Letzte \xc3\x84nderung an Pattern, Schnipseln, Tonart oder Aufnahme zur\xc3\xbc" "cknehmen"));
    redoBtn.setTooltip (U8 ("Zur\xc3\xbc" "ckgenommene \xc3\x84nderung wiederherstellen"));
    rollAll.setColour (TextButton::buttonColourId, col::ink);
    rollAll.setColour (TextButton::textColourOffId, col::dark);
    demoBtn.setColour (TextButton::buttonColourId, col::ink);
    demoBtn.setColour (TextButton::textColourOffId, col::dark);

    exportBtn.setButtonText (U8 ("\xe2\x86\x93  Export  (ziehen \xe2\x86\x92 Ableton)"));
    exportBtn.setTooltip (U8 ("Rendert den kompletten Loop bis zur Wiederholung als WAV. Klicken: speichern + im Finder zeigen. Ziehen: direkt auf eine Ableton-Spur ablegen."));
    exportBtn.setColour (TextButton::buttonColourId, col::lane[2].withAlpha (.25f));
    exportBtn.render = [this]
    {
        String info;
        const auto f = proc.exportLoop (info);
        proc.statusText = info;
        proc.bump();
        return f;
    };
    exportBtn.onClickExport = [] (const File& f) { if (f.existsAsFile()) f.revealToUser(); };
    addAndMakeVisible (exportBtn);

    for (int i = 0; i < 6; ++i)
    {
        auto* b = styleBtns.add (new TextButton (fs::kStyles[i].name));
        b->setRadioGroupId (101);
        b->setClickingTogglesState (true);
        b->onClick = [this, i] { proc.pushUndo(); proc.setStyle (i); syncControls(); };
        addAndMakeVisible (b);
    }
    for (int i = 0; i < 12; ++i) rootBox.addItem (kNoteNames[i], i + 1);
    for (int i = 0; i < 5; ++i) scaleBox.addItem (fs::kScales[i].name, i + 1);
    // Tonart-Wechsel transponiert die vorhandene Melodie (statt neu zu wuerfeln)
    rootBox.onChange = [this] { proc.pushUndo(); const int o = proc.root; proc.root = rootBox.getSelectedId() - 1; proc.remapMelody (o, proc.scale); };
    scaleBox.onChange = [this] { proc.pushUndo(); const int o = proc.scale; proc.scale = scaleBox.getSelectedId() - 1; proc.remapMelody (proc.root, o); };
    addAndMakeVisible (rootBox); addAndMakeVisible (scaleBox);

    addAndMakeVisible (overview);
    addAndMakeVisible (chips);
    for (int l = 0; l < fs::kLanes; ++l) addAndMakeVisible (lanes.add (new LaneRow (proc, *this, l)));
    addChildComponent (editor);

   #if JucePlugin_Build_Standalone
    // Standalone: Eingang nicht stummschalten (das Plugin gibt den Eingang nie auf den Ausgang)
    if (proc.wrapperType == AudioProcessor::wrapperType_Standalone)
        if (auto* h = StandalonePluginHolder::getInstance()) h->getMuteInputValue().setValue (false);
   #endif
    setSize (W, H);
    sendLookAndFeelChange();
    syncControls();
    startTimerHz (30);
}

Content::~Content() { setLookAndFeel (nullptr); }

void Content::syncControls()
{
    for (int i = 0; i < styleBtns.size(); ++i) styleBtns[i]->setToggleState (i == proc.style, dontSendNotification);
    rootBox.setSelectedId (proc.root + 1, dontSendNotification);
    scaleBox.setSelectedId (proc.scale + 1, dontSendNotification);
    sens.setValue (proc.sens * 100, dontSendNotification);
    dens.setValue (proc.density * 100, dontSendNotification);
}

void Content::resized()
{
    play.setBounds (252, 12, 48, 48);
    Slider* hs[7] = { &bpm, &swing, &feel, &rev, &tape, &sub, &master };
    for (int i = 0; i < 7; ++i)
        hs[i]->setBounds (L::sliderX0 + i * (L::sliderW + L::sliderGap) + (i >= 3 ? 16 : 0), L::sliderY, L::sliderW, 24);
    syncBtn.setBounds (310, 30, 90, 24);

    // 1 Geraeusche
    demoBtn.setBounds (262, 78, 136, 28);
    loadBtn.setBounds (404, 78, 124, 28);
    recBtn.setBounds (534, 78, 150, 28);
    sens.setBounds (860, 80, 130, 24);
    overview.setBounds (30, 114, 1040, 94);
    chips.setBounds (30, 214, 1040, 80);

    // 2 Groove
    for (int i = 0; i < 6; ++i) styleBtns[i]->setBounds (150 + i * 94, 318, 88, 28);
    dens.setBounds (860, 320, 140, 24);
    rootBox.setBounds (216, 356, 62, 28);
    scaleBox.setBounds (284, 356, 124, 28);
    int x = 420;
    for (auto* b : { &rollAll, &rollDrums, &rollMel, &rollSounds, &clearBtn })
    {
        const int w = b == &rollAll ? 112 : 66;
        b->setBounds (x, 356, w, 28); x += w + 5;
    }
    exportBtn.setBounds (x + 8, 356, 1070 - x - 8, 28);
    undoBtn.setBounds (150, 409, 116, 22);
    redoBtn.setBounds (272, 409, 116, 22);

    // 3 Sequencer
    for (int l = 0; l < lanes.size(); ++l) lanes[l]->setBounds (26, 436 + l * 51, 1048, 47);

    // Schnipsel-Bearbeitung liegt ueber den Bereichen 1-3
    editor.setBounds (16, 70, 1068, 622);
    overview.refresh();
}

void Content::paint (Graphics& g)
{
    g.fillAll (col::bg);
    {
        const auto f = fnt (30, true);
        const int wFund = (int) std::ceil (GlyphArrangement::getStringWidth (f, "Fund"));
        g.setFont (f);
        g.setColour (col::ink);
        g.drawText ("Fund", 24, 10, wFund + 4, 36, Justification::centredLeft);
        g.setColour (col::lane[0]);
        g.drawText (U8 ("st\xc3\xbc" "ck"), 24 + wFund, 10, 140, 36, Justification::centredLeft);
    }
    g.setColour (col::dim); g.setFont (fnt (13));
    g.drawText (U8 ("Alltagsger\xc3\xa4usche rein, Beats raus."), 24, 42, 220, 18, Justification::centredLeft);

    g.setFont (fnt (10));
    for (int i = 0; i < 7; ++i)
    {
        const int x = L::sliderX0 + i * (L::sliderW + L::sliderGap) + (i >= 3 ? 16 : 0);
        String t (kHeaderLabels[i]);
        if (i == 0 && syncBtn.getToggleState()) t = "TEMPO (HOST)";
        g.setColour (col::dim);
        g.drawText (t, x, 12, L::sliderW + 40, 14, Justification::centredLeft);
    }
    g.setColour (col::dim);
    g.drawText ("DAW", 310, 12, 90, 14, Justification::centredLeft);
    if (syncBtn.getToggleState())
    {
        auto r = bpm.getBounds().toFloat();
        g.setColour (col::lane[4].withAlpha (.25f)); g.fillRoundedRectangle (r, 5.f);
        g.setColour (col::lane[4]); g.drawRoundedRectangle (r.reduced (.5f), 5.f, 1.f);
        const double hb = proc.hostBpm.load();
        g.setColour (col::ink); g.setFont (fnt (13, true));
        g.drawText (hb > 0 ? String (hb, hb == std::floor (hb) ? 0 : 1) + " BPM" : U8 ("\xe2\x80\x93 BPM"), r.toNearestInt(), Justification::centred);
    }

    auto panel = [&g] (Rectangle<int> r, const char* num, const String& title)
    {
        g.setColour (col::panel); g.fillRoundedRectangle (r.toFloat(), 14.f);
        g.setColour (col::line); g.drawRoundedRectangle (r.toFloat().reduced (.5f), 14.f, 1.f);
        g.setColour (col::panel2); g.fillEllipse ((float) r.getX() + 14, (float) r.getY() + 12, 20, 20);
        g.setColour (col::ink); g.setFont (fnt (11, true));
        g.drawText (num, r.getX() + 14, r.getY() + 12, 20, 20, Justification::centred);
        g.setColour (col::dim); g.setFont (fnt (12));
        g.drawText (title, r.getX() + 42, r.getY() + 12, 200, 20, Justification::centredLeft);
    };
    panel (L::sec1, "1", U8 ("GER\xc3\x84USCHE SAMMELN"));
    panel (L::sec2, "2", "GROOVE");
    panel (L::sec3, "3", "SEQUENCER");

    {   // Eingangspegel
        auto m = meterR.toFloat();
        g.setColour (col::panel2); g.fillRoundedRectangle (m, 4.f);
        const float lvl = jlimit (0.f, 1.f, shownLevel);
        const float db = lvl > 1.0e-5f ? jlimit (0.f, 1.f, (20.f * std::log10 (lvl) + 60.f) / 60.f) : 0.f;
        g.setColour (lvl > .95f ? col::lane[0] : col::lane[4]);
        g.fillRoundedRectangle (m.withWidth (m.getWidth() * db), 4.f);
        g.setColour (col::dim); g.setFont (fnt (9));
        g.drawText ("IN", meterR.translated (meterR.getWidth() + 4, 0).withWidth (20), Justification::centredLeft);
    }

    g.setFont (fnt (10)); g.setColour (col::dim);
    g.drawText ("EMPFINDLICHKEIT", 744, 80, 110, 24, Justification::centredRight);
    g.drawText ("DICHTE", 790, 320, 64, 24, Justification::centredRight);
    g.drawText ("TONART", 150, 356, 60, 28, Justification::centredLeft);
    g.setFont (fnt (12));
    g.drawText (U8 ("Spurname + Schnipsel = zuweisen \xc2\xb7 Hats: mehrfach klicken = Wirbel \xc2\xb7 Melodie: Note ziehen \xe2\x86\x95 / Rechtsklick = Notenmen\xc3\xbc \xc2\xb7 P = Polymeter"),
                400, 410, 674, 20, Justification::centredRight, true);
}

void Content::chooseSlice (int i)
{
    if (armed >= 0) { proc.pushUndo(); proc.setLaneSlice (armed, i); proc.audition (i, armed); }
    else proc.audition (i, -1);
    openEditor (i);
}

void Content::openEditor (int i)
{
    editor.open (i);
}

void Content::openFileChooser()
{
    chooser = std::make_unique<FileChooser> (U8 ("Aufnahmen w\xc3\xa4hlen"), File(), "*.wav;*.aif;*.aiff;*.mp3;*.m4a;*.aac;*.flac;*.ogg;*.caf");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
                          [this] (const FileChooser& fc)
                          {
                              StringArray paths;
                              for (auto& f : fc.getResults()) paths.add (f.getFullPathName());
                              if (! paths.isEmpty()) { proc.pushUndo(); proc.loadFiles (paths); }
                          });
}

void Content::timerCallback()
{
    if (sensDirty && Time::getMillisecondCounter() - sensChangedAt > 250)
    {
        sensDirty = false;
        proc.pushUndo();
        proc.sens = (float) sens.getValue() / 100.f;
        proc.reanalyze (FundstueckProcessor::Mode::Resens);
    }
    const int v = proc.uiVersion.load();
    if (v != seenVersion)
    {
        seenVersion = v;
        overview.refresh();
        chips.repaint();
        for (auto* l : lanes) l->repaint();
        syncControls();
        if (editor.isVisible())
        {
            const int n = proc.msgSet ? (int) proc.msgSet->slices.size() : 0;
            if (editor.index >= n) { editor.setVisible (false); editor.index = -1; }
            else editor.repaint();
        }
    }

    // Aufnahme
    if (proc.recFull.load()) { proc.pushUndo(); proc.stopRecording(); }
    undoBtn.setEnabled (proc.canUndo());
    redoBtn.setEnabled (proc.canRedo());
    const bool rec = proc.recording.load();
    const int tenths = rec ? (int) (proc.recPos.load() / jmax (1.0, proc.getRecSampleRate()) * 10.0) : -1;
    if (rec != seenRec || tenths != seenRecTenths)
    {
        seenRec = rec; seenRecTenths = tenths;
        recBtn.setButtonText (rec ? U8 ("\xe2\x96\xa0  Stopp  ") + String (tenths / 10.0, 1) + " s" : U8 ("\xe2\x97\x8f  Aufnehmen"));
        recBtn.setColour (TextButton::buttonColourId, rec ? col::lane[0] : col::panel2);
        recBtn.setColour (TextButton::textColourOffId, rec ? col::dark : col::lane[0]);
        if (rec && tenths > 25 && proc.inLevel.load() < 1.0e-4f)
        {
            proc.statusText = U8 ("Nimmt auf \xe2\x80\x93 aber kein Eingangssignal! Ableton: Sidechain im Ger\xc3\xa4t aktivieren und Mikrofon-Spur w\xc3\xa4hlen.");
            overview.repaint();
        }
    }
    const float lvl = proc.inLevel.load();
    if (std::abs (lvl - shownLevel) > 0.002f) { shownLevel = lvl; repaint (meterR.expanded (2, 2).withWidth (meterR.getWidth() + 30)); }
    const bool sync = syncBtn.getToggleState();
    const double hb = proc.hostBpm.load();
    if (sync != seenSync || (sync && hb != shownHostBpm))
    {
        seenSync = sync; shownHostBpm = hb;
        bpm.setVisible (! sync);
        play.setEnabled (! sync);
        repaint (Rectangle<int> (240, 0, 860, 70));
    }

    const int st = proc.currentStep.load();
    if (st != seenStep) { seenStep = st; for (auto* l : lanes) l->repaint(); }
    play.on = proc.hostPlaying.load() || proc.internalPlay.load();
    play.repaint();
}

// ---------------------------------------------------------------- Editor
FundstueckEditor::FundstueckEditor (FundstueckProcessor& p) : AudioProcessorEditor (p), proc (p), content (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (Content::W * 6 / 10, Content::H * 6 / 10, Content::W * 3 / 2, Content::H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) Content::W / Content::H);
    setSize (Content::W, Content::H);
}

void FundstueckEditor::resized()
{
    const float s = (float) getWidth() / (float) Content::W;
    content.setBounds (0, 0, Content::W, Content::H);
    content.setTransform (AffineTransform::scale (s));
}

void FundstueckEditor::filesDropped (const StringArray& files, int, int)
{
    proc.pushUndo();
    proc.loadFiles (files);
}
