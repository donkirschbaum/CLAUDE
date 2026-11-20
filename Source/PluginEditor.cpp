#include "PluginEditor.h"

namespace ui
{
juce::Font mono (float h, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h,
                                          bold ? juce::Font::bold : juce::Font::plain));
}
juce::Font sans (float h, bool bold)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

static void drawSpaced (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r,
                        juce::Justification j, float h = 10.f, float kern = .12f)
{
    g.setFont (mono (h).withExtraKerningFactor (kern));
    g.drawText (t, r, j, false);
}

void drawLearnFrame (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (col::glow.withAlpha (.18f));
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (col::glow);
    g.drawRoundedRectangle (r.reduced (.75f), 5.f, 1.5f);
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, col::plate2);
    setColour (juce::PopupMenu::textColourId, col::ink);
    setColour (juce::PopupMenu::headerTextColourId, col::copper);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::copper);
    setColour (juce::PopupMenu::highlightedTextColourId, col::scope);
    setColour (juce::TooltipWindow::backgroundColourId, col::plate2);
    setColour (juce::TooltipWindow::textColourId, col::ink);
    setColour (juce::TooltipWindow::outlineColourId, col::line);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (col::plate2);
    g.setColour (col::line);
    g.drawRect (0, 0, w, h);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float a0, float a1, juce::Slider& s)
{
    const auto  b  = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.f);
    const float r  = juce::jmin (b.getWidth(), b.getHeight()) * .5f;
    const auto  c  = b.getCentre();
    const float ar = r - 1.5f;
    const float a  = a0 + pos * (a1 - a0);
    const bool  bi = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float from = bi ? (a0 + a1) * .5f : a0;

    juce::Path track;
    track.addCentredArc (c.x, c.y, ar, ar, 0.f, a0, a1, true);
    g.setColour (col::line);
    g.strokePath (track, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (std::abs (a - from) > .01f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, ar, ar, 0.f, juce::jmin (from, a), juce::jmax (from, a), true);
        g.setColour (col::copper);
        g.strokePath (val, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float cr = r * .68f;
    g.setColour (col::plate2);
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
    g.setColour (col::line);
    g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, 1.f);

    const juce::Point<float> tip (c.x + std::sin (a) * cr * .82f, c.y - std::cos (a) * cr * .82f);
    g.setColour (s.hasKeyboardFocus (false) ? col::glow : col::ink);
    g.drawLine ({ c, tip }, 2.2f);
}

//==============================================================================
juce::String MidiLink::ccText() const
{
    const int cc = proc.ccForTarget (target);
    return cc >= 0 ? "CC " + juce::String (cc) : "kein CC";
}

void MidiLink::showMenu (juce::Component& owner) const
{
    const int cc = proc.ccForTarget (target);
    const bool isLearning = learning();

    juce::PopupMenu m;
    m.addSectionHeader (proc.targetName (target) + (cc >= 0 ? juce::String::fromUTF8 ("  \xc2\xb7  CC ") + juce::String (cc) : juce::String()));
    m.addItem (1, isLearning ? juce::String ("MIDI Learn abbrechen")
                             : juce::String::fromUTF8 ("MIDI Learn: jetzt einen Regler am Controller bewegen"));
    m.addItem (2, "Zuordnung entfernen", cc >= 0);

    auto& p = proc;
    const int t = target;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&owner),
                     [&p, t, isLearning] (int r)
                     {
                         if (r == 1) { if (isLearning) p.cancelLearn(); else p.startLearn (t); }
                         if (r == 2) p.clearTarget (t);
                     });
}

//==============================================================================
Knob::Knob (DroneAchtProcessor& p, const juce::String& id, const juce::String& n,
            Formatter f, bool sm, bool bipolar)
    : link { p, p.targetForParam (id) }, name (n), fmt (std::move (f)), small (sm)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (170);
    slider.getProperties().set ("bipolar", bipolar);
    slider.onValueChange = [this] { repaint(); };
    slider.onMenu  = [this] { link.showMenu (slider); };
    slider.onHover = [this] { repaint(); };
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
    if (auto* prm = p.apvts.getParameter (id))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    slider.setTitle (n);
}

void Knob::resized()
{
    const int d = small ? 36 : 48;
    slider.setBounds ((getWidth() - d) / 2, 0, d, d);
}

void Knob::paint (juce::Graphics& g)
{
    const int d = small ? 36 : 48;
    const bool learn = link.learning();
    if (learn) drawLearnFrame (g, getLocalBounds().toFloat());

    auto r = getLocalBounds().toFloat().withTrimmedTop ((float) d + 2.f);
    g.setColour (col::ink);
    drawSpaced (g, name.toUpperCase(), r.removeFromTop (13.f), juce::Justification::centred, 9.5f, .08f);

    const bool hover = slider.isMouseOver() && ! slider.isMouseButtonDown();
    const juce::String val = learn ? juce::String::fromUTF8 ("LERNT\xe2\x80\xa6") : hover ? link.ccText() : fmt ((float) slider.getValue());
    g.setColour (learn ? col::glow : hover ? col::copper : col::muted);
    drawSpaced (g, val, r.removeFromTop (13.f), juce::Justification::centred, 9.5f, 0.f);
}

//==============================================================================
Segmented::Segmented (DroneAchtProcessor& p, const juce::String& id, juce::StringArray l, const juce::String& t)
    : link { p, p.targetForParam (id) }, labels (std::move (l)), title (t),
      attachment (*p.apvts.getParameter (id), [this] (float v) { index = juce::roundToInt (v); repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
    setTitle (t);
}

juce::Rectangle<float> Segmented::segArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (17.f).withHeight (28.f).reduced (.5f);
}

void Segmented::paint (juce::Graphics& g)
{
    auto top = getLocalBounds().toFloat().withHeight (14.f);
    g.setColour (col::muted);
    drawSpaced (g, title.toUpperCase(), top, juce::Justification::centredLeft, 9.5f);
    const bool learn = link.learning();
    g.setColour (learn ? col::glow : col::line.brighter (.35f));
    drawSpaced (g, learn ? juce::String::fromUTF8 ("LERNT\xe2\x80\xa6") : link.ccText().toUpperCase(),
                top, juce::Justification::centredRight, 9.f, 0.f);

    auto area = segArea();
    const float w = area.getWidth() / (float) labels.size();
    juce::Path clip;
    clip.addRoundedRectangle (area, 4.f);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (clip);
        for (int i = 0; i < labels.size(); ++i)
        {
            auto seg = juce::Rectangle<float> (area.getX() + w * (float) i, area.getY(), w, area.getHeight());
            if (i == index) { g.setColour (col::copper); g.fillRect (seg); }
            if (i > 0)      { g.setColour (col::line);   g.fillRect (seg.withWidth (1.f)); }
            g.setColour (i == index ? col::scope : col::muted);
            drawSpaced (g, labels[i].toUpperCase(), seg, juce::Justification::centred, 10.f, .06f);
        }
    }
    g.setColour (learn ? col::glow : col::line);
    g.drawRoundedRectangle (area, 4.f, learn ? 1.5f : 1.f);
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { link.showMenu (*this); return; }
    auto area = segArea();
    if (! area.contains (e.position)) return;
    const int i = juce::jlimit (0, labels.size() - 1,
                                (int) ((e.position.x - area.getX()) / area.getWidth() * (float) labels.size()));
    attachment.setValueAsCompleteGesture ((float) i);
}

//==============================================================================
FastToggle::FastToggle (DroneAchtProcessor& p, const juce::String& id)
    : link { p, p.targetForParam (id) },
      attachment (*p.apvts.getParameter (id), [this] (float v) { on = v > .5f; repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
}

void FastToggle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (.5f);
    if (link.learning()) { drawLearnFrame (g, r); }
    g.setColour (on ? col::copper : col::line);
    g.drawRoundedRectangle (r, 3.f, 1.f);
    g.setColour (on ? col::glow : col::muted);
    drawSpaced (g, link.learning() ? juce::String::fromUTF8 ("LERNT\xe2\x80\xa6") : juce::String (on ? "SCHNELL" : "LANGSAM"),
                r, juce::Justification::centred, 9.f, .08f);
}

void FastToggle::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { link.showMenu (*this); return; }
    attachment.setValueAsCompleteGesture (on ? 0.f : 1.f);
}

//==============================================================================
ActionButton::ActionButton (DroneAchtProcessor& p, int a, juce::String l)
    : link { p, DroneAchtProcessor::actionBase + a }, action (a), label (std::move (l))
{
    pips = a % 6 + 1;
    setTitle (DroneAchtProcessor::actionName (a));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    refreshTooltip();
}

void ActionButton::refreshTooltip()
{
    setTooltip (DroneAchtProcessor::actionName (action) + juce::String::fromUTF8 ("  \xc2\xb7  ") + link.ccText()
                + juce::String::fromUTF8 ("  \xc2\xb7  Rechtsklick: MIDI Learn"));
}

void ActionButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (.5f);
    const bool over  = isMouseOver();
    const bool learn = link.learning();

    if (label.isEmpty())
    {
        // Würfel
        auto d = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight()));
        g.setColour (down ? col::copper : over ? col::line.brighter (.15f) : col::plate2);
        g.fillRoundedRectangle (d, 4.f);
        g.setColour (learn ? col::glow : over ? col::copper : col::line.brighter (.25f));
        g.drawRoundedRectangle (d, 4.f, learn ? 1.6f : 1.f);

        static const float layout[6][6][2] = {
            { { .5f, .5f } },
            { { .28f, .28f }, { .72f, .72f } },
            { { .26f, .26f }, { .5f, .5f }, { .74f, .74f } },
            { { .28f, .28f }, { .72f, .28f }, { .28f, .72f }, { .72f, .72f } },
            { { .26f, .26f }, { .74f, .26f }, { .5f, .5f }, { .26f, .74f }, { .74f, .74f } },
            { { .28f, .24f }, { .72f, .24f }, { .28f, .5f }, { .72f, .5f }, { .28f, .76f }, { .72f, .76f } } };
        const float pr = d.getWidth() * .085f;
        g.setColour (down ? col::scope : learn ? col::glow : over ? col::glow : col::ink.withAlpha (.85f));
        for (int i = 0; i < pips; ++i)
        {
            const float px = d.getX() + layout[pips - 1][i][0] * d.getWidth();
            const float py = d.getY() + layout[pips - 1][i][1] * d.getHeight();
            g.fillEllipse (px - pr, py - pr, pr * 2, pr * 2);
        }
        return;
    }

    g.setColour (down ? col::copper : col::plate2);
    g.fillRoundedRectangle (r, 4.f);
    if (learn) drawLearnFrame (g, r);
    g.setColour (learn ? col::glow : over ? col::muted : col::line);
    g.drawRoundedRectangle (r, 4.f, 1.f);
    g.setColour (down ? col::scope : col::ink);
    drawSpaced (g, learn ? juce::String::fromUTF8 ("LERNT\xe2\x80\xa6") : label.toUpperCase(), r, juce::Justification::centred, 11.f, .1f);
}

void ActionButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { link.showMenu (*this); return; }
    down = true;
    repaint();
}

void ActionButton::mouseUp (const juce::MouseEvent& e)
{
    if (! down) return;
    down = false;
    if (getLocalBounds().contains (e.getPosition()))
    {
        link.proc.performAction (action);
        pips = juce::Random::getSystemRandom().nextInt (6) + 1;
    }
    repaint();
}

//==============================================================================
void MidiMenuButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (.5f);
    const bool learning = proc.getLearnTarget() >= 0;
    g.setColour (col::plate2);
    g.fillRoundedRectangle (r, 4.f);
    g.setColour (learning ? col::glow : isMouseOver() ? col::muted : col::line);
    g.drawRoundedRectangle (r, 4.f, learning ? 1.5f : 1.f);
    g.setColour (learning ? col::glow : col::ink);
    drawSpaced (g, "MIDI", r, juce::Justification::centred, 11.f, .1f);
}

void MidiMenuButton::mouseDown (const juce::MouseEvent&)
{
    juce::PopupMenu m;
    m.addSectionHeader (juce::String::fromUTF8 ("MIDI-Zuordnung \xc2\xb7 Rechtsklick auf jeden Regler f\xc3\xbcr MIDI Learn"));
    m.addItem (1, "Lernmodus abbrechen", proc.getLearnTarget() >= 0);
    m.addItem (2, "Standard-Zuordnung laden (CC 14-31, 70-90, 102-119)");
    m.addItem (3, juce::String::fromUTF8 ("Alle Zuordnungen l\xc3\xb6schen"));
    auto& p = proc;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [&p] (int r)
    {
        if (r == 1) p.cancelLearn();
        if (r == 2) p.loadDefaultMidiMap();
        if (r == 3) p.clearMidiMap();
    });
}

//==============================================================================
Pad::Pad (DroneAchtProcessor& p, int v, juce::String key)
    : proc (p), voice (v), keyLabel (std::move (key))
{
    noteLabel = juce::MidiMessage::getMidiNoteName (DroneAchtProcessor::midiNotes[v], true, true, 3);
    hold = proc.apvts.getRawParameterValue ("hold" + juce::String (v >> 2));
    setTitle ("Stimme " + juce::String (v + 1));
}

juce::Rectangle<float> Pad::circle() const
{
    return getLocalBounds().toFloat().reduced ((float) margin);
}

bool Pad::hitTest (int x, int y)
{
    auto c = circle();
    return c.getCentre().getDistanceFrom ({ (float) x, (float) y }) <= c.getWidth() * .5f;
}

void Pad::mouseDown (const juce::MouseEvent&) { proc.padTouch[(size_t) voice].store (true); }
void Pad::mouseUp (const juce::MouseEvent&)   { proc.padTouch[(size_t) voice].store (false); }

void Pad::paint (juce::Graphics& g)
{
    const auto  c   = circle();
    const auto  cen = c.getCentre();
    const float rad = c.getWidth() * .5f;
    const float lvl = juce::jlimit (0.f, 1.f, proc.engine.level[(size_t) voice].load (std::memory_order_relaxed));
    const float hl  = hold->load();

    juce::Path disc;
    disc.addEllipse (c);

    if (lvl > .02f)
        juce::DropShadow (col::copper.withAlpha (lvl * .9f), (int) (margin * 2), {}).drawForPath (g, disc);

    if (hl > .01f)
    {
        g.setColour (col::copper.withAlpha (hl * .85f));
        g.drawEllipse (c.expanded (5.f), 2.f);
    }

    const juce::Point<float> hi (cen.x - rad * .32f, cen.y - rad * .44f);
    g.setGradientFill (juce::ColourGradient (col::padHi, hi, col::plate, { hi.x + rad * 1.45f, hi.y }, true));
    g.fillPath (disc);

    if (lvl > .02f)
    {
        juce::ColourGradient lit (col::glow.withAlpha (lvl), hi, col::copper.withAlpha (lvl * .9f), { hi.x + rad * 1.3f, hi.y }, true);
        lit.addColour (.85, col::copper.withAlpha (lvl * .5f));
        g.setGradientFill (lit);
        g.fillPath (disc);
    }

    g.setColour (col::line);
    g.drawEllipse (c, 1.f);

    g.setColour (lvl > .5f ? col::scope : col::muted);
    drawSpaced (g, keyLabel, c.withSizeKeepingCentre (c.getWidth(), 16.f).translated (0, -6.f), juce::Justification::centred, 12.f, 0.f);
    drawSpaced (g, noteLabel, c.withSizeKeepingCentre (c.getWidth(), 14.f).translated (0, 9.f), juce::Justification::centred, 9.f, 0.f);
}

//==============================================================================
void Scope::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::scope);
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (.5f), 6.f, 1.f);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 1, r.getRight() - 1);
    g.setColour (col::muted);
    drawSpaced (g, "OSZILLOSKOP", r.reduced (12.f, 8.f).withHeight (14.f), juce::Justification::centredLeft, 10.f);

    const int size = DroneAchtProcessor::scopeSize;
    const int w = proc.scopeWrite.load (std::memory_order_acquire);
    const int n = (int) snap.size();
    for (int i = 0; i < n; ++i)
        snap[(size_t) i] = proc.scopeBuf[(size_t) ((w - n + i + size) & (size - 1))];

    int start = 0;
    for (int i = 1; i < n / 2; ++i)
        if (snap[(size_t) i - 1] < 0.f && snap[(size_t) i] >= 0.f) { start = i; break; }

    const int   len = 1024;
    const float h   = r.getHeight() * .42f;
    juce::Path p;
    for (int i = 0; i < len; ++i)
    {
        const float x = r.getX() + (float) i / (float) (len - 1) * r.getWidth();
        const float y = r.getCentreY() - juce::jlimit (-1.f, 1.f, snap[(size_t) (start + i)]) * h;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (r.reduced (1.f).toNearestInt());
    g.setColour (col::copper.withAlpha (.25f));
    g.strokePath (p, juce::PathStrokeType (5.f));
    g.setColour (col::glow);
    g.strokePath (p, juce::PathStrokeType (1.6f));
}
} // namespace ui

//==============================================================================
namespace
{
juce::String pct  (float v) { return juce::String (juce::roundToInt (v * 100)) + "%"; }
juce::String hz   (float v) { return v < 1 ? juce::String (v, 2) + " Hz" : v < 10 ? juce::String (v, 1) + " Hz" : juce::String (juce::roundToInt (v)) + " Hz"; }
juce::String ms   (float v) { return juce::String (juce::roundToInt (v * 1000)) + " ms"; }
juce::String semi (float v) { return (v >= 0 ? "+" : "") + juce::String (v, std::abs (v) < 10 ? 1 : 0) + " HT"; }

juce::String noteOf (float f)
{
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H" };
    const float m = 69.f + 12.f * std::log2 (f / 440.f);
    const int   n = juce::roundToInt (m);
    const int   c = juce::roundToInt ((m - (float) n) * 100.f);
    juce::String s = juce::String (names[((n % 12) + 12) % 12]) + juce::String ((int) std::floor (n / 12.0) - 1);
    if (c != 0) s << (c > 0 ? " +" : " ") << c;
    return s;
}
}

DroneAchtEditor::DroneAchtEditor (DroneAchtProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    scope = std::make_unique<ui::Scope> (p);
    addAndMakeVisible (*scope);

    addKnob ("vol", "Volume", pct);
    addKnob ("drive", "Drive", pct);
    addKnob ("vib", "Vibrato", pct);
    addKnob ("vibRate", "Vib-Rate", hz);

    addKnob ("lfoA", "Rate A", hz);
    addKnob ("lfoB", "Rate B", hz);
    addSeg ("lfoMode", { "A+B", juce::String::fromUTF8 ("A\xc3\x97" "B") }, juce::String::fromUTF8 ("VERKN\xc3\x9c" "PFUNG"));
    addSeg ("lfoWave", { "Dreieck", "Rechteck" }, "Form");

    addKnob ("t1", "Zeit L", ms);
    addKnob ("t2", "Zeit R", ms);
    addKnob ("fb", "Feedback", pct);
    addKnob ("dmod", "Mod", pct);
    addKnob ("dmix", "Mix", pct);

    for (int q = 0; q < 2; ++q)
    {
        addKnob ("hold" + juce::String (q), "Hold", pct);
        addKnob ("pitch" + juce::String (q), "Pitch", semi, false, true);
        addSeg ("src" + juce::String (q), { "Paar", "Nachbar", "LFO" }, "Mod-Quelle");
    }
    for (int pr = 0; pr < 4; ++pr)
    {
        addKnob ("mod" + juce::String (pr), "Mod", pct);
        addKnob ("sharp" + juce::String (pr), "Sharp", pct);
    }
    for (int i = 0; i < drone::numVoices; ++i)
    {
        auto* tune  = proc.apvts.getRawParameterValue ("tune" + juce::String (i));
        auto* pitch = proc.apvts.getRawParameterValue ("pitch" + juce::String (i >> 2));
        addKnob ("tune" + juce::String (i), "Tune", [i, tune, pitch] (float)
        {
            return noteOf (55.f * std::pow (2.f, (drone::baseSemis[(size_t) i] + tune->load() + pitch->load()) / 12.f));
        }, true, true);

        pads.push_back (std::make_unique<ui::Pad> (p, i, juce::String::charToString ((juce::juce_wchar) keyChars[i])));
        addAndMakeVisible (*pads.back());

        toggles.push_back (std::make_unique<ui::FastToggle> (p, "fast" + juce::String (i)));
        addAndMakeVisible (*toggles.back());
    }

    // Kopfzeile
    addAction (actAll, "Zufall");
    addAction (actMutate, "Mutieren");
    addAction (actChaos, "Chaos");
    addAction (actTune, "Stimmung");
    addAction (actReset, juce::String::fromUTF8 ("ZUR\xc3\x9c" "CKSETZEN"));
    addAndMakeVisible (midiBtn);

    // Würfel auf den Platten
    for (int a : { (int) actMaster, (int) actLfo, (int) actDelay, (int) actQuadA, (int) actQuadB,
                   (int) actPair0, (int) actPair1, (int) actPair2, (int) actPair3 })
        addAction (a);

    setWantsKeyboardFocus (true);
    setSize (1060, 776);
    startTimerHz (30);
}

DroneAchtEditor::~DroneAchtEditor()
{
    stopTimer();
    for (int i = 0; i < drone::numVoices; ++i) { proc.padTouch[(size_t) i].store (false); proc.keyTouch[(size_t) i].store (false); }
    setLookAndFeel (nullptr);
}

ui::Knob* DroneAchtEditor::addKnob (const juce::String& id, const juce::String& name, ui::Knob::Formatter fmt, bool small, bool bipolar)
{
    auto k = std::make_unique<ui::Knob> (proc, id, name, std::move (fmt), small, bipolar);
    addAndMakeVisible (*k);
    return (knobs[id] = std::move (k)).get();
}

ui::Segmented* DroneAchtEditor::addSeg (const juce::String& id, juce::StringArray labels, const juce::String& title)
{
    auto s = std::make_unique<ui::Segmented> (proc, id, std::move (labels), title);
    addAndMakeVisible (*s);
    return (segs[id] = std::move (s)).get();
}

ui::ActionButton* DroneAchtEditor::addAction (int action, juce::String label)
{
    auto b = std::make_unique<ui::ActionButton> (proc, action, std::move (label));
    addAndMakeVisible (*b);
    return (actions[action] = std::move (b)).get();
}

void DroneAchtEditor::timerCallback()
{
    scope->repaint();
    for (auto& pd : pads) pd->repaint();
    for (int i = 0; i < drone::numVoices; ++i) knobs["tune" + juce::String (i)]->repaint();

    const int v = proc.getMapVersion();
    if (v != lastMapVersion)
    {
        lastMapVersion = v;
        for (auto& [id, k] : knobs) k->repaint();
        for (auto& [id, s] : segs) s->repaint();
        for (auto& [a, b] : actions) { b->refreshTooltip(); b->repaint(); }
        for (auto& t : toggles) t->repaint();
        midiBtn.repaint();
    }
}

bool DroneAchtEditor::keyStateChanged (bool)
{
    bool used = false;
    for (int i = 0; i < drone::numVoices; ++i)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown (keyChars[i]) || juce::KeyPress::isKeyCurrentlyDown (keyChars[i] + 32);
        if (down) used = true;
        proc.keyTouch[(size_t) i].store (down);
    }
    if (juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::escapeKey) && proc.getLearnTarget() >= 0)
    {
        proc.cancelLearn();
        used = true;
    }
    return used;
}

void DroneAchtEditor::focusLost (FocusChangeType)
{
    for (auto& k : proc.keyTouch) k.store (false);
}

//==============================================================================
void DroneAchtEditor::resized()
{
    plates.clear();
    const int W = getWidth(), m = 20, gap = 14;
    const int knobW = 62, kH = ui::Knob::heightFor (false), kSH = ui::Knob::heightFor (true);
    const int dice = 24;

    // Kopfzeile, von rechts nach links
    {
        int x = W - m;
        auto place = [&] (juce::Component& c, int w) { x -= w; c.setBounds (x, m + 2, w, 32); x -= 8; };
        place (midiBtn, 64);
        place (*actions[actReset], 128);
        place (*actions[actTune], 100);
        place (*actions[actChaos], 80);
        place (*actions[actMutate], 104);
        place (*actions[actAll], 88);
    }

    auto placeRow = [&] (std::initializer_list<const char*> ids, int x, int y)
    {
        for (auto* id : ids) { knobs[id]->setBounds (x, y, knobW, kH); x += knobW + 4; }
        return x;
    };
    auto plate = [&] (juce::Rectangle<int> r, juce::String l, juce::String rt, bool inner, int action)
    {
        plates.push_back ({ r, l, rt, inner, action });
        if (action >= 0)
        {
            const int inset = inner ? 7 : 9;
            actions[action]->setBounds (r.getRight() - inset - dice, r.getY() + inset - 2, dice, dice);
        }
    };

    // Reihe 1: Oszilloskop + Master
    const int y1 = 94, h1 = 172, masterW = 4 * (knobW + 4) + 24;
    juce::Rectangle<int> master (W - m - masterW, y1, masterW, h1);
    scope->setBounds (m, y1, master.getX() - gap - m, h1);
    plate (master, "MASTER", "", false, actMaster);
    placeRow ({ "vol", "drive", "vib", "vibRate" }, master.getX() + 12, y1 + 56);

    // Reihe 2: LFO + Delay
    const int y2 = y1 + h1 + gap, h2 = 124;
    const int lfoW = (W - 2 * m - gap) * 45 / 100;
    juce::Rectangle<int> lfo (m, y2, lfoW, h2), del (m + lfoW + gap, y2, W - 2 * m - gap - lfoW, h2);
    plate (lfo, "HYPER-LFO", juce::String::fromUTF8 ("A \xc3\x97 B"), false, actLfo);
    plate (del, "MOD-DELAY", "PING-PONG", false, actDelay);
    int x = placeRow ({ "lfoA", "lfoB" }, lfo.getX() + 12, y2 + 38);
    segs["lfoMode"]->setBounds (x + 8, y2 + 50, 108, 46);
    segs["lfoWave"]->setBounds (x + 8 + 108 + 12, y2 + 50, 150, 46);
    placeRow ({ "t1", "t2", "fb", "dmod", "dmix" }, del.getX() + 12, y2 + 38);

    // Reihe 3: zwei Quads
    const int y3 = y2 + h2 + gap, h3 = getHeight() - m - y3;
    const int qW = (W - 2 * m - gap) / 2;
    for (int q = 0; q < 2; ++q)
    {
        juce::Rectangle<int> quad (m + q * (qW + gap), y3, qW, h3);
        const juce::String qs (q);
        plate (quad, "STIMMEN " + juce::String (q * 4 + 1) + "-" + juce::String (q * 4 + 4),
               q ? "QUAD B" : "QUAD A", false, q ? actQuadB : actQuadA);
        knobs["hold" + qs]->setBounds (quad.getX() + 12, y3 + 34, knobW, kH);
        knobs["pitch" + qs]->setBounds (quad.getX() + 12 + knobW + 4, y3 + 34, knobW, kH);
        segs["src" + qs]->setBounds (quad.getX() + 12 + 2 * (knobW + 4) + 12, y3 + 46, 220, 46);

        const int py = y3 + 120, ph = h3 - 120 - 14;
        const int pW = (qW - 24 - 10) / 2;
        for (int k = 0; k < 2; ++k)
        {
            const int pr = q * 2 + k;
            juce::Rectangle<int> pair (quad.getX() + 12 + k * (pW + 10), py, pW, ph);
            plate (pair, "PAAR " + juce::String (pr * 2 + 1) + "/" + juce::String (pr * 2 + 2), "", true, actPair0 + pr);

            const int cy = py + 34;
            knobs["mod" + juce::String (pr)]->setBounds (pair.getCentreX() - knobW / 2, cy - 4, knobW, kH);
            knobs["sharp" + juce::String (pr)]->setBounds (pair.getCentreX() - knobW / 2, cy - 4 + kH + 4, knobW, kH);

            for (int s = 0; s < 2; ++s)
            {
                const int v  = pr * 2 + s;
                const int cx = s == 0 ? pair.getX() + 10 + 39 : pair.getRight() - 10 - 39;
                const int d  = 64, pm = ui::Pad::margin;
                pads[(size_t) v]->setBounds (cx - d / 2 - pm, cy - pm, d + 2 * pm, d + 2 * pm);
                knobs["tune" + juce::String (v)]->setBounds (cx - knobW / 2, cy + d + 10, knobW, kSH);
                toggles[(size_t) v]->setBounds (cx - 36, cy + d + 10 + kSH + 4, 72, 20);
            }
        }
    }
}

void DroneAchtEditor::paint (juce::Graphics& g)
{
    using namespace ui;
    g.fillAll (col::panel);

    juce::AttributedString title;
    const auto tf = sans (40.f, true).withHorizontalScale (.86f).withExtraKerningFactor (.02f);
    title.append ("DRONE ", tf, col::ink);
    title.append ("ACHT", tf, col::copper);
    title.draw (g, { 20.f, 14.f, 400.f, 46.f });
    g.setColour (col::muted);
    g.setFont (mono (10.5f).withExtraKerningFactor (.12f));
    g.drawText (juce::String::fromUTF8 ("8 STIMMEN \xc2\xb7 KREUZ-FM \xc2\xb7 HYPER-LFO \xc2\xb7 MOD-DELAY \xc2\xb7 NOTEN C3\xe2\x80\x93" "C4"),
                juce::Rectangle<int> (20, 62, 520, 16), juce::Justification::centredLeft, false);
    g.setColour (col::line.brighter (.35f));
    g.setFont (mono (9.5f).withExtraKerningFactor (.1f));
    g.drawText (juce::String::fromUTF8 ("RECHTSKLICK AUF JEDEN REGLER ODER W\xc3\x9c" "RFEL: MIDI LEARN"),
                juce::Rectangle<int> (getWidth() - 20 - 420, 62, 420, 16), juce::Justification::centredRight, false);

    for (auto& p : plates)
    {
        auto r = p.r.toFloat();
        g.setColour (p.inner ? col::plate2 : col::plate);
        g.fillRoundedRectangle (r, p.inner ? 5.f : 6.f);
        g.setColour (col::line);
        g.drawRoundedRectangle (r.reduced (.5f), p.inner ? 5.f : 6.f, 1.f);

        auto lbl = r.reduced (p.inner ? 10.f : 14.f, 0).withY (r.getY() + (p.inner ? 8.f : 12.f)).withHeight (14.f);
        if (p.action >= 0) lbl.removeFromRight (p.inner ? 22.f : 28.f);
        g.setFont (mono (10.f).withExtraKerningFactor (.14f));
        g.setColour (col::muted);
        g.drawText (p.left, lbl, juce::Justification::centredLeft, false);
        g.setColour (col::copper);
        g.drawText (p.right, lbl, juce::Justification::centredRight, false);
    }
}
