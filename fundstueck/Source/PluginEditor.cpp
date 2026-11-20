#include "PluginEditor.h"
#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

using namespace juce;

static const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H" };
static const char* kClsNames[4] = { "tief", "mittel", "hell", "tonal" };
static const char* kLaneNames[5] = { "Kick", "Snare", "Hat", "Perc", "Melodie" };

static Font fnt (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }

static void drawWave (Graphics& g, const std::vector<float>& d, Rectangle<float> r, Colour c)
{
    if (d.empty() || r.getWidth() < 1) return;
    g.setColour (c);
    const int w = (int) r.getWidth();
    const size_t n = d.size();
    for (int x = 0; x < w; ++x)
    {
        const size_t a = (size_t) ((double) x / w * n), b = juce::jmax (a + 1, (size_t) ((double) (x + 1) / w * n));
        float mn = 0, mx = 0;
        for (size_t i = a; i < b && i < n; ++i) { mn = jmin (mn, d[i]); mx = jmax (mx, d[i]); }
        const float y1 = r.getCentreY() - mx * r.getHeight() * .5f, y2 = r.getCentreY() - mn * r.getHeight() * .5f;
        g.fillRect (r.getX() + (float) x, y1, 1.0f, jmax (1.0f, y2 - y1));
    }
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
}

void FsLook::drawButtonBackground (Graphics& g, Button& b, const Colour& bgc, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (.5f);
    auto c = b.getToggleState() ? b.findColour (TextButton::buttonOnColourId) : bgc;
    if (down) c = c.brighter (.1f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 7.f);
    g.setColour (b.getToggleState() ? c : (over ? Colour (0xff5a5141) : col::line));
    g.drawRoundedRectangle (r, 7.f, 1.f);
}

Font FsLook::getTextButtonFont (TextButton&, int h) { return fnt (jmin (14.f, h * .55f)); }

void FsLook::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle st, Slider& s)
{
    if (st != Slider::LinearBar) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, st, s); return; }
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    g.setColour (s.findColour (Slider::backgroundColourId));
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (s.findColour (Slider::trackColourId).withAlpha (.55f));
    if (s.getMinimum() < 0 && s.getMaximum() > 0) // bipolar (Tune)
    {
        const float zero = (float) s.getPositionOfValue (0.0);
        g.fillRoundedRectangle (Rectangle<float>::leftTopRightBottom (jmin (zero, pos), r.getY(), jmax (zero, pos), r.getBottom()), 4.f);
    }
    else
        g.fillRoundedRectangle (r.withRight (pos), 5.f);
}

Label* FsLook::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::textColourId, col::ink);
    l->setFont (fnt (s.getHeight() < 20 ? 11.f : 13.f));
    l->setJustificationType (Justification::centred);
    return l;
}

void PlayButton::paintButton (Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (2);
    g.setColour (on ? col::ink : col::lane[0]);
    g.fillEllipse (r);
    if (over) { g.setColour (Colours::white.withAlpha (.12f)); g.fillEllipse (r); }
    g.setColour (col::dark);
    const auto c = r.getCentre();
    if (on) g.fillRect (Rectangle<float> (14, 14).withCentre (c));
    else
    {
        Path p; p.addTriangle (c.x - 6, c.y - 9, c.x - 6, c.y + 9, c.x + 10, c.y);
        g.fillPath (p);
    }
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

void OverviewView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panel2);
    g.fillRoundedRectangle (r, 10.f);
    if (proc.source.empty() || peaks.empty())
    {
        g.setColour (col::dim); g.setFont (fnt (14));
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
        drawWave (g, s.data, r.reduced (5, 4).withTrimmedBottom (12), col::cls (s.cls));
        String lab (kClsNames[s.cls]);
        if (s.cls == fs::clsTonal) lab << " " << kNoteNames[((int) std::round (s.midi) % 12 + 12) % 12];
        g.setColour (col::dim); g.setFont (fnt (10));
        g.drawText (lab, r.withTrimmedTop (r.getHeight() - 14).toNearestInt(), Justification::centred);
    }
}

void ChipsView::mouseDown (const MouseEvent& e)
{
    const auto set = proc.msgSet;
    if (! set) return;
    for (int i = 0; i < (int) set->slices.size() && i < 32; ++i)
        if (chipRect (i).contains (e.position)) { content.chooseSlice (i); return; }
}

// ---------------------------------------------------------------- LaneRow
LaneRow::LaneRow (FundstueckProcessor& p, Content& c, int l) : proc (p), content (c), lane (l)
{
    const String id[5] = { "kick", "snare", "hat", "perc", "mel" };
    for (auto* s : { &vol, &tune, &len })
    {
        s->setSliderStyle (Slider::LinearBar);
        s->setTextBoxStyle (Slider::TextBoxLeft, false, 50, 18);
        s->setColour (Slider::trackColourId, col::lane[l]);
        s->setColour (Slider::backgroundColourId, Colour (0x22000000));
        addAndMakeVisible (*s);
    }
    aVol = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id[l] + "_vol", vol);
    aTune = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id[l] + "_tune", tune);
    aLen = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id[l] + "_len", len);
    mute.setClickingTogglesState (true);
    mute.setColour (TextButton::buttonOnColourId, col::dim);
    mute.setTooltip ("Stumm");
    addAndMakeVisible (mute);
    aMute = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, id[l] + "_mute", mute);
}

void LaneRow::resized()
{
    const int h = getHeight();
    nameR = { 10, 0, 74, h };
    thumbR = { 86, (h - 30) / 2, 66, 30 };
    upR = { 156, h / 2 - 13, 16, 13 };
    downR = { 156, h / 2, 16, 13 };
    vol.setBounds (180, 21, 52, 18);
    tune.setBounds (238, 21, 52, 18);
    len.setBounds (296, 21, 52, 18);
    mute.setBounds (356, (h - 26) / 2, 26, 26);
    stepsR = { 394, 5, getWidth() - 394 - 8, h - 10 };
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
        drawWave (g, set->slices[(size_t) si].data, thumbR.toFloat().reduced (3, 2), c);

    g.setColour (col::dim);
    Path up; up.addTriangle ((float) upR.getCentreX() - 4, (float) upR.getBottom() - 3, (float) upR.getCentreX() + 4, (float) upR.getBottom() - 3, (float) upR.getCentreX(), (float) upR.getY() + 3);
    Path dn; dn.addTriangle ((float) downR.getCentreX() - 4, (float) downR.getY() + 3, (float) downR.getCentreX() + 4, (float) downR.getY() + 3, (float) downR.getCentreX(), (float) downR.getBottom() - 3);
    g.fillPath (up); g.fillPath (dn);

    g.setFont (fnt (9)); g.setColour (col::dim);
    g.drawText ("VOL", vol.getBounds().withY (5).withHeight (14), Justification::centred);
    g.drawText ("TUNE", tune.getBounds().withY (5).withHeight (14), Justification::centred);
    g.drawText (U8 ("L\xc3\x84NGE"), len.getBounds().withY (5).withHeight (14), Justification::centred);

    const int now = proc.currentStep.load();
    for (int s = 0; s < 16; ++s)
    {
        auto sr = stepRect (s);
        const int v = proc.steps[lane][s].load();
        const bool on = lane == fs::kMel ? v >= 0 : v != 0;
        g.setColour (on ? c : Colours::white.withAlpha (s % 4 == 0 ? .09f : .05f));
        g.fillRoundedRectangle (sr, 6.f);
        if (s == now) { g.setColour (col::ink); g.drawRoundedRectangle (sr.reduced (1), 6.f, 2.f); }
        g.setColour (col::dark);
        if (lane == fs::kMel && on)
        {
            g.setFont (fnt (11, true));
            g.drawText (kNoteNames[v % 12], sr.toNearestInt(), Justification::centred);
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
    if (nameR.contains (p.toInt()))
    {
        content.armed = content.armed == lane ? -1 : lane;
        if (auto* par = getParentComponent()) par->repaint();
        for (auto* sib : getParentComponent()->getChildren()) if (dynamic_cast<LaneRow*> (sib) != nullptr) sib->repaint();
        return;
    }
    if (thumbR.contains (p.toInt())) { proc.audition (proc.laneSlice[lane].load(), lane); return; }
    if (upR.contains (p.toInt())) { proc.cycleSlice (lane, -1); return; }
    if (downR.contains (p.toInt())) { proc.cycleSlice (lane, 1); return; }
    const int s = stepAt (p);
    if (s < 0) return;
    int mod = 0;
    if (e.mods.isShiftDown()) mod = 1;
    else if (e.mods.isPopupMenu() || e.mods.isAltDown()) mod = -1;
    proc.stepClick (lane, s, mod);
    repaint();
}

void LaneRow::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const int s = stepAt (e.position);
    if (lane != fs::kMel || s < 0 || proc.steps[lane][s].load() < 0) { Component::mouseWheelMove (e, w); return; }
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

Content::Content (FundstueckProcessor& p) : proc (p), overview (p, *this), chips (p, *this)
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
    btn (demoBtn, U8 ("Demo-Ger\xc3\xa4usche"), [this] { proc.loadDemo(); });
    btn (loadBtn, U8 ("Datei laden\xe2\x80\xa6"), [this] { openFileChooser(); });
    btn (recBtn, U8 ("\xe2\x97\x8f  Aufnehmen"), [this] { if (proc.recording.load()) proc.stopRecording(); else proc.startRecording(); });
    recBtn.setColour (TextButton::textColourOffId, col::lane[0]);
    recBtn.setTooltip (U8 ("Nimmt den Eingang auf. Ableton: im Ger\xc3\xa4t \xe2\x80\x9eSidechain\xe2\x80\x9c aufklappen und die Mikrofon-Spur w\xc3\xa4hlen."));
    syncBtn.setButtonText ("HOST-SYNC");
    syncBtn.setClickingTogglesState (true);
    syncBtn.setColour (TextButton::buttonOnColourId, col::lane[4]);
    syncBtn.onClick = [this] { if (syncBtn.getToggleState()) proc.internalPlay = false; };
    addAndMakeVisible (syncBtn);
    aSync = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "sync", syncBtn);
    btn (rollAll, U8 ("Alles w\xc3\xbcrfeln"), [this] { proc.autoAssign (true); proc.rollDrums(); proc.rollMel(); });
    btn (rollDrums, "Drums", [this] { proc.rollDrums(); });
    btn (rollMel, "Melodie", [this] { proc.rollMel(); });
    btn (rollSounds, "Sounds", [this] { proc.autoAssign (true); });
    btn (clearBtn, "Leeren", [this] { proc.clearPattern(); });
    rollAll.setColour (TextButton::buttonColourId, col::ink);
    rollAll.setColour (TextButton::textColourOffId, col::dark);
    demoBtn.setColour (TextButton::buttonColourId, col::ink);
    demoBtn.setColour (TextButton::textColourOffId, col::dark);

    for (int i = 0; i < 6; ++i)
    {
        auto* b = styleBtns.add (new TextButton (fs::kStyles[i].name));
        b->setRadioGroupId (101);
        b->setClickingTogglesState (true);
        b->onClick = [this, i] { if (proc.style != i || ! styleBtns[i]->getToggleState()) {} proc.setStyle (i); syncControls(); };
        addAndMakeVisible (b);
    }
    for (int i = 0; i < 12; ++i) rootBox.addItem (kNoteNames[i], i + 1);
    for (int i = 0; i < 5; ++i) scaleBox.addItem (fs::kScales[i].name, i + 1);
    rootBox.onChange = [this] { proc.root = rootBox.getSelectedId() - 1; proc.rollMel(); };
    scaleBox.onChange = [this] { proc.scale = scaleBox.getSelectedId() - 1; proc.rollMel(); };
    addAndMakeVisible (rootBox); addAndMakeVisible (scaleBox);

    addAndMakeVisible (overview);
    addAndMakeVisible (chips);
    for (int l = 0; l < fs::kLanes; ++l) addAndMakeVisible (lanes.add (new LaneRow (proc, *this, l)));

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
    int x = 430;
    for (auto* b : { &rollAll, &rollDrums, &rollMel, &rollSounds, &clearBtn })
    {
        const int w = b == &rollAll ? 130 : 86;
        b->setBounds (x, 356, w, 28); x += w + 6;
    }

    // 3 Sequencer
    for (int l = 0; l < lanes.size(); ++l) lanes[l]->setBounds (26, 436 + l * 51, 1048, 47);
    overview.refresh();
}

void Content::paint (Graphics& g)
{
    g.fillAll (col::bg);
    // Kopf
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
    // Panels
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
    // Eingangspegel
    {
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
    g.drawText (U8 ("Spurnamen anklicken, dann Schnipsel w\xc3\xa4hlen \xc2\xb7 Hats: mehrfach klicken = Wirbel \xc2\xb7 Melodie: Mausrad / Shift / Rechtsklick"),
                250, 412, 824, 20, Justification::centredRight, true);
}

void Content::chooseSlice (int i)
{
    if (armed >= 0) { proc.setLaneSlice (armed, i); proc.audition (i, armed); }
    else proc.audition (i, -1);
}

void Content::openFileChooser()
{
    chooser = std::make_unique<FileChooser> (U8 ("Aufnahmen w\xc3\xa4hlen"), File(), "*.wav;*.aif;*.aiff;*.mp3;*.m4a;*.aac;*.flac;*.ogg;*.caf");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
                          [this] (const FileChooser& fc)
                          {
                              StringArray paths;
                              for (auto& f : fc.getResults()) paths.add (f.getFullPathName());
                              if (! paths.isEmpty()) proc.loadFiles (paths);
                          });
}

void Content::timerCallback()
{
    if (sensDirty && Time::getMillisecondCounter() - sensChangedAt > 250)
    {
        sensDirty = false;
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
        overview.repaint();
    }
    // Aufnahme
    if (proc.recFull.load()) proc.stopRecording();
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
    const bool host = proc.hostPlaying.load();
    play.on = host || proc.internalPlay.load();
    play.repaint();
    if (host != seenHost) { seenHost = host; repaint (Rectangle<int> (240, 0, 860, 70)); }
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
    proc.loadFiles (files);
}
