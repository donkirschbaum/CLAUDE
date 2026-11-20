#include "PluginEditor.h"

namespace col
{
const juce::Colour bg      { 0xff1c1916 };
const juce::Colour panel   { 0xff292420 };
const juce::Colour panelHi { 0xff332d28 };
const juce::Colour cream   { 0xffeadfc8 };
const juce::Colour amber   { 0xfff2a33a };
const juce::Colour red     { 0xffe5493b };
const juce::Colour green   { 0xff7cc576 };
const juce::Colour dim     { 0xff7a6e63 };
const juce::Colour dark    { 0xff14120f };
} // namespace col

//============================================================================ LookAndFeel
LoopTapeLookAndFeel::LoopTapeLookAndFeel()
{
    setColour (juce::Label::textColourId, col::cream);
    setColour (juce::ComboBox::textColourId, col::cream);
    setColour (juce::ComboBox::backgroundColourId, col::dark);
    setColour (juce::ComboBox::outlineColourId, col::dim.withAlpha (0.5f));
    setColour (juce::ComboBox::arrowColourId, col::amber);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::amber);
    setColour (juce::PopupMenu::highlightedTextColourId, col::dark);
    setColour (juce::Slider::textBoxTextColourId, col::cream);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::backgroundColourId, col::dark);
    setColour (juce::TextButton::textColourOffId, col::cream);
    setColour (juce::TextButton::textColourOnId, col::dark);
}

void LoopTapeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                            float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const bool enabled = s.isEnabled();
    const auto accent = enabled ? col::amber : col::dim;

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, start, end, true);
    g.setColour (col::dark);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, start, angle, true);
    g.setColour (accent);
    g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body
    const float kr = r - 9.0f;
    g.setGradientFill (juce::ColourGradient (col::cream.brighter (0.1f), c.x, c.y - kr,
                                             col::cream.darker (0.45f), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (col::dark.withAlpha (0.6f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

    // pointer
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.6f, -kr + 3.0f, 3.2f, kr * 0.55f, 1.5f);
    g.setColour (enabled ? col::dark : col::dim);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c));
}

void LoopTapeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? col::amber.withAlpha (0.18f) : col::dark);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? col::amber : (hl ? col::dim.brighter() : col::dim.withAlpha (0.6f)));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    auto led = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.33f);
    g.setColour (on ? col::amber : col::dim.withAlpha (0.4f));
    g.fillEllipse (led);
    if (on)
    {
        g.setColour (col::amber.withAlpha (0.3f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (on ? col::cream : col::cream.withAlpha (0.65f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r.withTrimmedRight (6.0f), juce::Justification::centred);
}

void LoopTapeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base,
                                                bool hl, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto c = base;
    if (down) c = c.darker (0.25f);
    else if (hl) c = c.brighter (0.12f);
    g.setGradientFill (juce::ColourGradient (c.brighter (0.08f), r.getX(), r.getY(), c.darker (0.15f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (col::dark);
    g.drawRoundedRectangle (r, 7.0f, 1.2f);
}

void LoopTapeLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (col::dark);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.isEnabled() ? col::dim.withAlpha (0.7f) : col::dim.withAlpha (0.3f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.isEnabled() ? col::amber : col::dim);
    g.fillPath (arrow);
}

//============================================================================ Knob
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title)
    : attachment (s, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, col::cream);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setDoubleClickReturnValue (true, s.getParameter (id)->convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (slider);

    label.setText (title.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, col::cream.withAlpha (0.75f));
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//============================================================================ TapeDisplay
void TapeDisplay::tick()
{
    const auto st = (lt::Looper::State) proc.info.state.load();
    const bool moving = st == lt::Looper::State::Playing || st == lt::Looper::State::Overdubbing
                        || st == lt::Looper::State::Recording;
    const bool rev = proc.apvts.getRawParameterValue ("reverse")->load() > 0.5f;
    const bool half = proc.apvts.getRawParameterValue ("halfSpeed")->load() > 0.5f;
    if (moving) reelAngle += (rev && st != lt::Looper::State::Recording ? -1.0f : 1.0f) * (half ? 0.06f : 0.12f);
    blink = (blink + 1) % 30;
    repaint();
}

static void drawReel (juce::Graphics& g, juce::Point<float> c, float r, float angle, float tapeFrac)
{
    // tape pack
    const float pack = r * (0.45f + 0.5f * tapeFrac);
    g.setColour (juce::Colour (0xff3a2a1e));
    g.fillEllipse (c.x - pack, c.y - pack, pack * 2, pack * 2);
    // reel flange
    g.setColour (col::cream.withAlpha (0.12f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 2.0f);
    // hub with spokes
    const float hub = r * 0.38f;
    g.setColour (col::cream.withAlpha (0.85f));
    g.fillEllipse (c.x - hub, c.y - hub, hub * 2, hub * 2);
    g.setColour (col::dark);
    for (int i = 0; i < 3; ++i)
    {
        juce::Path p;
        p.addRoundedRectangle (-hub * 0.18f, -hub * 0.92f, hub * 0.36f, hub * 0.5f, 2.0f);
        g.fillPath (p, juce::AffineTransform::rotation (angle + (float) i * juce::MathConstants<float>::twoPi / 3.0f).translated (c));
    }
    g.fillEllipse (c.x - hub * 0.22f, c.y - hub * 0.22f, hub * 0.44f, hub * 0.44f);
}

void TapeDisplay::paint (juce::Graphics& g)
{
    using S = lt::Looper::State;
    auto r = getLocalBounds().toFloat();
    g.setColour (col::dark);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (col::dim.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    const auto st = (S) proc.info.state.load();
    const double len = proc.info.loopSeconds.load();
    const double posS = proc.info.position.load();
    const double recS = proc.info.recSeconds.load();
    const double recTarget = proc.info.recTargetSeconds.load();
    const int layers = proc.info.layers.load();
    const int stored = proc.info.stored.load();
    const bool pending = proc.info.pendingDub.load();

    auto inner = r.reduced (12.0f);

    // --- reels
    auto reelArea = inner.removeFromTop (inner.getHeight() * 0.52f);
    const float rr = juce::jmin (reelArea.getHeight() * 0.46f, reelArea.getWidth() * 0.2f);
    const juce::Point<float> c1 (reelArea.getX() + reelArea.getWidth() * 0.27f, reelArea.getCentreY());
    const juce::Point<float> c2 (reelArea.getX() + reelArea.getWidth() * 0.73f, reelArea.getCentreY());
    float frac = len > 0.0 ? (float) (posS / len) : 0.0f;
    if (st == S::Recording) frac = (float) juce::jlimit (0.0, 1.0, recS / (double) lt::Looper::kMaxSeconds);

    // tape path
    g.setColour (juce::Colour (0xff3a2a1e));
    g.drawLine (c1.x, c1.y + rr * 0.9f, c2.x, c2.y + rr * 0.9f, 2.5f);
    drawReel (g, c1, rr, reelAngle, 1.0f - frac);
    drawReel (g, c2, rr, reelAngle * 0.8f, frac);

    // state badge
    juce::String stText; juce::Colour stCol = col::dim;
    switch (st)
    {
        case S::Empty:       stText = "EMPTY"; break;
        case S::Recording:   stText = recTarget > 0.0 ? "REC > GRID" : "RECORDING"; stCol = col::red; break;
        case S::Playing:     stText = pending ? "WAIT..." : "PLAYING"; stCol = col::green; break;
        case S::Overdubbing: stText = pending ? "WAIT..." : "OVERDUB"; stCol = col::amber; break;
        case S::Stopped:     stText = "STOPPED"; stCol = col::cream.withAlpha (0.7f); break;
    }
    const bool lit = ! (st == S::Recording || st == S::Overdubbing) || blink < 20;
    auto badge = juce::Rectangle<float> (reelArea.getCentreX() - 48.0f, reelArea.getY() + 2.0f, 96.0f, 20.0f);
    g.setColour (stCol.withAlpha (lit ? 0.22f : 0.08f));
    g.fillRoundedRectangle (badge, 4.0f);
    g.setColour (lit ? stCol : stCol.withAlpha (0.4f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (stText, badge, juce::Justification::centred);

    // --- progress bar
    inner.removeFromTop (8.0f);
    auto bar = inner.removeFromTop (10.0f);
    g.setColour (col::panelHi);
    g.fillRoundedRectangle (bar, 3.0f);
    if (st == S::Recording)
    {
        g.setColour (col::red);
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (2.0f, bar.getWidth() * frac)), 3.0f);
    }
    else if (len > 0.0)
    {
        g.setColour (st == S::Overdubbing ? col::amber : col::cream.withAlpha (0.8f));
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (2.0f, bar.getWidth() * frac)), 3.0f);
    }

    // --- time readout
    inner.removeFromTop (6.0f);
    auto txt = inner.removeFromTop (18.0f);
    auto fmt = [] (double s) { return juce::String (s, 2) + " s"; };
    g.setColour (col::cream.withAlpha (0.85f));
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    juce::String left = st == S::Recording ? fmt (recS) : (len > 0 ? fmt (posS) + " / " + fmt (len) : juce::String ("--"));
    g.drawText (left, txt, juce::Justification::centredLeft);
    g.drawText ("LAYERS " + juce::String (layers) + "/" + juce::String (lt::Looper::kMaxLayers), txt, juce::Justification::centredRight);

    // --- layer cells
    inner.removeFromTop (6.0f);
    auto cells = inner.removeFromTop (16.0f);
    const float cw = cells.getWidth() / (float) lt::Looper::kMaxLayers;
    for (int i = 0; i < lt::Looper::kMaxLayers; ++i)
    {
        auto cell = juce::Rectangle<float> (cells.getX() + cw * (float) i, cells.getY(), cw, cells.getHeight()).reduced (2.0f, 0.0f);
        const bool recNow = st == S::Overdubbing && i == layers - 1 && ! pending;
        if (i < layers)
        {
            g.setColour (recNow ? (blink < 20 ? col::red : col::red.withAlpha (0.4f)) : (i == 0 ? col::cream : col::amber));
            g.fillRoundedRectangle (cell, 2.5f);
        }
        else if (i < stored)
        {
            g.setColour (col::amber.withAlpha (0.55f));
            g.drawRoundedRectangle (cell, 2.5f, 1.2f);
        }
        else
        {
            g.setColour (col::panelHi);
            g.fillRoundedRectangle (cell, 2.5f);
        }
    }
}

//============================================================================ DragTile
void DragTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = isEnabled();
    const bool hl = on && isMouseOver();
    g.setColour (hl ? col::amber.withAlpha (0.16f) : col::dark);
    g.fillRoundedRectangle (r, 7.0f);
    const float dash[] = { 4.0f, 3.0f };
    juce::Path outline; outline.addRoundedRectangle (r, 7.0f);
    juce::Path dashed; juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dash, 2);
    g.setColour (on ? col::amber.withAlpha (hl ? 1.0f : 0.7f) : col::dim.withAlpha (0.4f));
    g.fillPath (dashed);

    // little waveform icon
    auto icon = r.removeFromLeft (r.getHeight()).reduced (8.0f);
    g.setColour (on ? col::amber : col::dim);
    const float heights[] = { 0.35f, 0.8f, 0.55f, 1.0f, 0.45f, 0.7f };
    const float bw = icon.getWidth() / 6.0f;
    for (int i = 0; i < 6; ++i)
    {
        const float h = icon.getHeight() * heights[i];
        g.fillRoundedRectangle (icon.getX() + bw * (float) i + 0.5f, icon.getCentreY() - h * 0.5f, bw - 1.5f, h, 1.0f);
    }
    g.setColour (on ? col::cream : col::dim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("LOOP INS SET ZIEHEN", r.withTrimmedRight (8.0f), juce::Justification::centredLeft);
}

void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || ! isEnabled() || e.getDistanceFromDragStart() < 6) return;
    dragStarted = true;
    const auto file = proc.renderToExportFolder();
    if (! file.existsAsFile()) { if (onStatus) onStatus ("Kein Loop zum Ziehen"); return; }
    if (onStatus) onStatus ("Gespeichert: " + file.getFileName());
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

//============================================================================ Editor
LoopTapeEditor::LoopTapeEditor (LoopTapeProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      time (p.apvts, "time", "Time"), feedback (p.apvts, "feedback", "Feedback"), mix (p.apvts, "mix", "Mix"),
      tone (p.apvts, "tone", "Tone"), lowCut (p.apvts, "lowCut", "Low Cut"), wow (p.apvts, "wow", "Wow"),
      flutter (p.apvts, "flutter", "Flutter"), saturation (p.apvts, "saturation", "Saturation"),
      hiss (p.apvts, "hiss", "Hiss"), loopLevel (p.apvts, "loopLevel", "Loop Level"), display (p), dragTile (p)
{
    setLookAndFeel (&lnf);

    for (auto* k : { &time, &feedback, &mix, &tone, &lowCut, &wow, &flutter, &saturation, &hiss, &loopLevel })
        addAndMakeVisible (*k);

    auto setupCombo = [this] (juce::ComboBox& box, juce::Label& lbl, const juce::String& text, const juce::String& id,
                              std::unique_ptr<ComboAttachment>& att) {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
            box.addItemList (choice->choices, 1);
        addAndMakeVisible (box);
        att = std::make_unique<ComboAttachment> (proc.apvts, id, box);
        lbl.setText (text, juce::dontSendNotification);
        lbl.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        lbl.setColour (juce::Label::textColourId, col::cream.withAlpha (0.6f));
        addAndMakeVisible (lbl);
    };
    setupCombo (divisionBox, divisionLbl, "DIVISION", "division", divisionAtt);
    setupCombo (modeBox, modeLbl, "STEREO", "mode", modeAtt);
    setupCombo (routingBox, routingLbl, "LOOP ROUTING", "routing", routingAtt);
    setupCombo (quantizeBox, quantizeLbl, "LOOP LENGTH", "quantize", quantizeAtt);

    for (auto* b : { &syncBtn, &killDryBtn, &reverseBtn, &halfBtn }) addAndMakeVisible (*b);
    syncAtt    = std::make_unique<ButtonAttachment> (proc.apvts, "sync", syncBtn);
    killDryAtt = std::make_unique<ButtonAttachment> (proc.apvts, "killDry", killDryBtn);
    reverseAtt = std::make_unique<ButtonAttachment> (proc.apvts, "reverse", reverseBtn);
    halfAtt    = std::make_unique<ButtonAttachment> (proc.apvts, "halfSpeed", halfBtn);

    killDryBtn.setTooltip ("Entfernt das direkte Eingangssignal (fuer Return-Spuren)");
    syncBtn.setTooltip ("Delay-Zeit folgt dem Ableton-Tempo (Notenwert unter DIVISION)");

    hostInfo.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    hostInfo.setColour (juce::Label::textColourId, col::cream.withAlpha (0.8f));
    hostInfo.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (hostInfo);

    exportBtn.setColour (juce::TextButton::buttonColourId, col::panelHi);
    exportBtn.setTooltip ("Loop als WAV speichern (Mix oder alle Layer einzeln)");
    exportBtn.onClick = [this] { showExportMenu(); };
    addAndMakeVisible (exportBtn);
    dragTile.setTooltip ("Ziehen und auf eine Audiospur in Ableton fallen lassen");
    dragTile.onStatus = [this] (const juce::String& t) { setStatus (t); };
    addAndMakeVisible (dragTile);
    exportStatus.setFont (juce::FontOptions (11.0f));
    exportStatus.setColour (juce::Label::textColourId, col::amber);
    exportStatus.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (exportStatus);

    recBtn.setColour (juce::TextButton::buttonColourId, col::red.darker (0.25f));
    playBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3d5a3a));
    for (auto* b : { &undoBtn, &redoBtn, &clearBtn }) b->setColour (juce::TextButton::buttonColourId, col::panelHi);

    recBtn.onClick   = [this] { proc.sendCommand (LoopTapeProcessor::CmdRecord); };
    playBtn.onClick  = [this] { proc.sendCommand (LoopTapeProcessor::CmdPlay); };
    undoBtn.onClick  = [this] { proc.sendCommand (LoopTapeProcessor::CmdUndo); };
    redoBtn.onClick  = [this] { proc.sendCommand (LoopTapeProcessor::CmdRedo); };
    clearBtn.onClick = [this] { proc.sendCommand (LoopTapeProcessor::CmdClear); };
    for (auto* b : { &recBtn, &playBtn, &undoBtn, &redoBtn, &clearBtn }) addAndMakeVisible (*b);

    addAndMakeVisible (display);

    setSize (980, 566);
    startTimerHz (30);
}

LoopTapeEditor::~LoopTapeEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void LoopTapeEditor::timerCallback()
{
    display.tick();

    const bool sync = syncBtn.getToggleState();
    time.slider.setEnabled (! sync);
    divisionBox.setEnabled (sync);

    {
        const double bpm = proc.info.hostBpm.load();
        const float ms = proc.info.delayMs.load();
        const auto msText = ms >= 1000.0f ? juce::String (ms / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (ms)) + " ms";
        const auto bpmText = proc.info.hostBpmKnown.load() ? juce::String (bpm, 1) + " BPM" : juce::String ("kein Tempo");
        hostInfo.setText (sync ? "HOST " + bpmText + "  " + divisionBox.getText() + " = " + msText
                               : "FREI " + msText + "   (Host " + bpmText + ")",
                          juce::dontSendNotification);
        hostInfo.setColour (juce::Label::textColourId, sync ? col::amber : col::cream.withAlpha (0.6f));
    }

    const bool hasLoop = proc.info.loopSeconds.load() > 0.0;
    exportBtn.setEnabled (hasLoop);
    if (dragTile.isEnabled() != hasLoop) { dragTile.setEnabled (hasLoop); dragTile.repaint(); }
    if (statusTicks > 0 && --statusTicks == 0) exportStatus.setText ({}, juce::dontSendNotification);

    using S = lt::Looper::State;
    const auto st = (S) proc.info.state.load();
    switch (st)
    {
        case S::Empty:       recBtn.setButtonText ("REC"); break;
        case S::Recording:   recBtn.setButtonText ("PLAY / DUB"); break;
        case S::Playing:
        case S::Stopped:     recBtn.setButtonText ("DUB"); break;
        case S::Overdubbing: recBtn.setButtonText ("END DUB"); break;
    }
    playBtn.setButtonText (st == S::Playing || st == S::Overdubbing || st == S::Recording ? "STOP" : "PLAY");
    undoBtn.setEnabled (st != S::Empty);
    redoBtn.setEnabled (proc.info.stored.load() > proc.info.layers.load());
    clearBtn.setEnabled (st != S::Empty);
}

void LoopTapeEditor::setStatus (const juce::String& text)
{
    exportStatus.setText (text, juce::dontSendNotification);
    statusTicks = 30 * 6;
}

void LoopTapeEditor::showExportMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Loop (Mix) als WAV speichern...");
    m.addItem (2, "Alle Layer einzeln + Mix speichern...");
    m.addSeparator();
    m.addItem (3, "Export-Ordner im Finder zeigen");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportBtn), [this] (int choice) {
        auto folder = LoopTapeProcessor::defaultExportFolder();
        folder.createDirectory();

        if (choice == 1)
        {
            chooser = std::make_unique<juce::FileChooser> ("Loop als WAV speichern",
                                                           folder.getChildFile (proc.suggestedFileName ("Mix")), "*.wav");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc) {
                                      auto f = fc.getResult();
                                      if (f == juce::File()) return;
                                      if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
                                      const auto res = proc.writeWav (f, -1);
                                      setStatus (res.wasOk() ? "Gespeichert: " + f.getFileName() : res.getErrorMessage());
                                  });
        }
        else if (choice == 2)
        {
            chooser = std::make_unique<juce::FileChooser> ("Ordner fuer die Layer waehlen", folder);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this] (const juce::FileChooser& fc) {
                                      const auto dir = fc.getResult();
                                      if (dir == juce::File()) return;
                                      const int n = proc.writeAllLayers (dir);
                                      setStatus (n > 0 ? juce::String (n) + " Dateien gespeichert in " + dir.getFileName()
                                                       : juce::String ("Kein Loop vorhanden"));
                                  });
        }
        else if (choice == 3)
        {
            folder.revealToUser();
        }
    });
}

void LoopTapeEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    // header
    auto header = getLocalBounds().removeFromTop (58).toFloat();
    g.setColour (col::cream);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText ("LOOPTAPE", header.withTrimmedLeft (24.0f), juce::Justification::centredLeft);
    g.setColour (col::amber);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("TAPE ECHO  +  LAYER LOOPER", header.withTrimmedLeft (182.0f), juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("v" JucePlugin_VersionString, header.withTrimmedRight (24.0f), juce::Justification::centredRight);

    auto drawPanel = [&g] (juce::Rectangle<int> r, const juce::String& title) {
        auto f = r.toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (f, 10.0f);
        g.setColour (col::dark.withAlpha (0.8f));
        g.drawRoundedRectangle (f.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (col::amber.withAlpha (0.9f));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (title, f.removeFromTop (28.0f).withTrimmedLeft (16.0f), juce::Justification::centredLeft);
    };
    drawPanel (echoPanel, "TAPE ECHO");
    drawPanel (loopPanel, "LOOPER");
}

void LoopTapeEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    r.removeFromTop (44);
    echoPanel = r.removeFromLeft (510);
    r.removeFromLeft (14);
    loopPanel = r;

    // ---------- echo panel
    {
        auto a = echoPanel.reduced (14);
        a.removeFromTop (34);
        const int kw = a.getWidth() / 5;
        auto row1 = a.removeFromTop (150);
        for (auto* k : { &time, &feedback, &mix, &tone, &lowCut })
            k->setBounds (row1.removeFromLeft (kw).reduced (4, 6));

        a.removeFromTop (20);
        auto row2 = a.removeFromTop (150);
        for (auto* k : { &wow, &flutter, &saturation, &hiss })
            k->setBounds (row2.removeFromLeft (kw).reduced (4, 6));

        auto ctl = row2.reduced (4, 8);
        syncBtn.setBounds (ctl.removeFromTop (26));
        ctl.removeFromTop (4);
        divisionLbl.setBounds (ctl.removeFromTop (14));
        divisionBox.setBounds (ctl.removeFromTop (24));
        ctl.removeFromTop (6);
        modeLbl.setBounds (ctl.removeFromTop (14));
        modeBox.setBounds (ctl.removeFromTop (24));

        auto bottom = a.removeFromBottom (30);
        killDryBtn.setBounds (bottom.removeFromLeft (130));
        bottom.removeFromLeft (10);
        hostInfo.setBounds (bottom.reduced (4, 0));
    }

    // ---------- looper panel
    {
        auto a = loopPanel.reduced (14);
        a.removeFromTop (22);
        display.setBounds (a.removeFromTop (190));
        a.removeFromTop (12);

        auto transport = a.removeFromTop (54);
        recBtn.setBounds (transport.removeFromLeft (132).reduced (3));
        playBtn.setBounds (transport.removeFromLeft (100).reduced (3));
        const int sw = transport.getWidth() / 3;
        undoBtn.setBounds (transport.removeFromLeft (sw).reduced (3, 9));
        redoBtn.setBounds (transport.removeFromLeft (sw).reduced (3, 9));
        clearBtn.setBounds (transport.reduced (3, 9));

        auto exportRow = a.removeFromBottom (36);
        exportBtn.setBounds (exportRow.removeFromLeft (132).reduced (3));
        dragTile.setBounds (exportRow.removeFromLeft (200).reduced (3));
        exportStatus.setBounds (exportRow.reduced (4, 0));
        a.removeFromBottom (8);

        a.removeFromTop (10);
        auto lower = a;
        loopLevel.setBounds (lower.removeFromLeft (110).reduced (4, 0));
        lower.removeFromLeft (8);

        auto col1 = lower.removeFromLeft (lower.getWidth() / 2).reduced (4, 4);
        auto col2 = lower.reduced (4, 4);
        routingLbl.setBounds (col1.removeFromTop (14));
        routingBox.setBounds (col1.removeFromTop (26));
        col1.removeFromTop (10);
        reverseBtn.setBounds (col1.removeFromTop (28));

        quantizeLbl.setBounds (col2.removeFromTop (14));
        quantizeBox.setBounds (col2.removeFromTop (26));
        col2.removeFromTop (10);
        halfBtn.setBounds (col2.removeFromTop (28));
    }
}
