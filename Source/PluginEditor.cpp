#include "PluginEditor.h"

namespace
{
    constexpr int editorW   = 1210;
    constexpr int topH      = 92;
    constexpr int headerH   = 22;
    constexpr int rowH      = 64;
    constexpr int leftW     = 450;
    constexpr int footerH   = 26;

    const juce::Colour bg       { 0xff15171c };
    const juce::Colour panel    { 0xff1e2129 };
    const juce::Colour textCol  { 0xffd8dce6 };
    const juce::Colour dimText  { 0xff7c8496 };

    const juce::Colour trackColours[PolyRhythmProcessor::numTracks] = {
        juce::Colour (0xffff5a5f), juce::Colour (0xffffa24a), juce::Colour (0xfff5d547),
        juce::Colour (0xff5fd68a), juce::Colour (0xff4bb8ff), juce::Colour (0xffb57bff)
    };
}

//==============================================================================
juce::Rectangle<float> StepGrid::cellBounds (int track, int step) const
{
    const float cellW = (float) getWidth() / (float) PolyRhythmProcessor::maxSteps;
    const float y = (float) (track * rowHeight);
    return { step * cellW + 2.0f, y + 10.0f, cellW - 4.0f, (float) rowHeight - 20.0f };
}

bool StepGrid::cellAt (juce::Point<int> pos, int& track, int& step) const
{
    if (! getLocalBounds().contains (pos))
        return false;
    track = pos.y / rowHeight;
    step  = (int) ((float) pos.x / ((float) getWidth() / (float) PolyRhythmProcessor::maxSteps));
    return track >= 0 && track < PolyRhythmProcessor::numTracks && step >= 0 && step < PolyRhythmProcessor::maxSteps;
}

void StepGrid::paint (juce::Graphics& g)
{
    for (int t = 0; t < PolyRhythmProcessor::numTracks; ++t)
    {
        const int len = proc.getTrackLength (t);
        const int cur = proc.getCurrentStep (t);
        const auto col = trackColours[t];
        const bool muted = proc.apvts.getRawParameterValue (PolyRhythmProcessor::id ("mute", t))->load() > 0.5f;

        if (t % 2 == 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.02f));
            g.fillRect (0, t * rowHeight, getWidth(), rowHeight);
        }

        for (int s = 0; s < PolyRhythmProcessor::maxSteps; ++s)
        {
            const auto r = cellBounds (t, s);
            const bool on = proc.getStep (t, s);
            const bool active = s < len;

            juce::Colour c;
            if (active)
                c = on ? col : ((s / 4) % 2 == 0 ? juce::Colour (0xff343946) : juce::Colour (0xff2b2f3a));
            else
                c = on ? col.withAlpha (0.18f) : juce::Colour (0xff1b1d23);

            if (muted && on && active)
                c = c.withSaturation (0.15f).withMultipliedBrightness (0.6f);

            g.setColour (c);
            g.fillRoundedRectangle (r, 3.0f);

            if (s == cur && active)
            {
                g.setColour (juce::Colours::white.withAlpha (on ? 0.95f : 0.6f));
                g.drawRoundedRectangle (r.expanded (1.0f), 3.0f, 2.0f);
            }
        }

        // loop end marker
        const float cellW = (float) getWidth() / (float) PolyRhythmProcessor::maxSteps;
        g.setColour (col.withAlpha (0.8f));
        g.fillRect ((float) len * cellW - 1.0f, (float) (t * rowHeight) + 6.0f, 2.0f, (float) rowHeight - 12.0f);
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    int t, s;
    if (! cellAt (e.getPosition(), t, s))
        return;
    paintValue = ! proc.getStep (t, s);
    proc.setStep (t, s, paintValue);
    lastTrack = t; lastStep = s;
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    int t, s;
    if (! cellAt (e.getPosition(), t, s) || (t == lastTrack && s == lastStep))
        return;
    proc.setStep (t, s, paintValue);
    lastTrack = t; lastStep = s;
    repaint();
}

//==============================================================================
void MidiDragArea::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (hover ? juce::Colour (0xff3a4152) : juce::Colour (0xff2a2f3b));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colour (0xff4bb8ff));
    g.drawRoundedRectangle (r, 5.0f, 1.5f);
    g.setColour (textCol);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("\xe2\x86\x93 MIDI in DAW ziehen"), getLocalBounds(), juce::Justification::centred);
}

void MidiDragArea::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 4)
        return;

    dragging = true;

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PolyRhythm32");
    dir.createDirectory();
    auto file = dir.getChildFile ("PolyRhythm32 Loop " + juce::Time::getCurrentTime().formatted ("%H-%M-%S") + ".mid");

    juce::String info;
    if (! proc.writeMidiLoop (file, info))
    {
        if (onInfo) onInfo (info);
        dragging = false;
        return;
    }

    if (onInfo) onInfo ("Drag: " + info);

    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                [safe = juce::Component::SafePointer<MidiDragArea> (this)]
                                                                {
                                                                    if (safe != nullptr)
                                                                        safe->dragging = false;
                                                                });
}

//==============================================================================
PolyRhythmEditor::PolyRhythmEditor (PolyRhythmProcessor& p)
    : AudioProcessorEditor (&p), proc (p), grid (p), dragArea (p)
{
    laf.setColourScheme (juce::LookAndFeel_V4::getMidnightColourScheme());
    laf.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2c313d));
    laf.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4bb8ff));
    laf.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2c313d));
    laf.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff2c313d));
    laf.setColour (juce::Slider::trackColourId, juce::Colour (0xff4bb8ff).withAlpha (0.6f));
    laf.setColour (juce::Label::textColourId, textCol);
    setLookAndFeel (&laf);

    auto& apvts = proc.apvts;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // ---- top bar
    title.setText ("PolyRhythm32", juce::dontSendNotification);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (title);

    randomAll.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff8a3ffc));
    randomAll.onClick = [this] { proc.randomizeAll(); grid.repaint(); };
    addAndMakeVisible (randomAll);

    densityLabel.setText ("Dichte", juce::dontSendNotification);
    addAndMakeVisible (densityLabel);
    density.setSliderStyle (juce::Slider::LinearHorizontal);
    density.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 22);
    addAndMakeVisible (density);
    densityA = std::make_unique<SA> (apvts, "density", density);

    randLen.setButtonText (juce::String::fromUTF8 ("inkl. L\xc3\xa4nge"));
    addAndMakeVisible (randLen);
    randLenA = std::make_unique<BA> (apvts, "randLen", randLen);

    channelLabel.setText ("MIDI-Kanal", juce::dontSendNotification);
    addAndMakeVisible (channelLabel);
    channel.setSliderStyle (juce::Slider::IncDecButtons);
    channel.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 22);
    addAndMakeVisible (channel);
    channelA = std::make_unique<SA> (apvts, "channel", channel);

    virtOut.setTooltip ("Eigener MIDI-Port (CoreMIDI). In der DAW als MIDI-Eingang waehlbar.");
    addAndMakeVisible (virtOut);
    virtOutA = std::make_unique<BA> (apvts, "virtOut", virtOut);
    virtOut.setEnabled (PolyRhythmProcessor::virtualPortSupported());
    portLabel.setColour (juce::Label::textColourId, juce::Colour (0xff5fd68a));
    portLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (portLabel);

    playButton.setClickingTogglesState (true);
    playButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff36b56a));
    addAndMakeVisible (playButton);
    playA = std::make_unique<BA> (apvts, "play", playButton);

    bpmLabel.setText ("BPM", juce::dontSendNotification);
    addAndMakeVisible (bpmLabel);
    bpm.setSliderStyle (juce::Slider::IncDecButtons);
    bpm.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 56, 22);
    addAndMakeVisible (bpm);
    bpmA = std::make_unique<SA> (apvts, "bpm", bpm);
    bpm.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);

    syncLabel.setColour (juce::Label::textColourId, dimText);
    addAndMakeVisible (syncLabel);

    exportLen.addItemList (apvts.getParameter ("exportLen")->getAllValueStrings(), 1);
    addAndMakeVisible (exportLen);
    exportLenA = std::make_unique<CA> (apvts, "exportLen", exportLen);

    exportButton.onClick = [this] { exportMidi(); };
    addAndMakeVisible (exportButton);

    dragArea.onInfo = [this] (const juce::String& s) { infoLabel.setText (s, juce::dontSendNotification); };
    addAndMakeVisible (dragArea);

    infoLabel.setColour (juce::Label::textColourId, dimText);
    addAndMakeVisible (infoLabel);

    // ---- tracks
    const auto rateNames = PolyRhythmProcessor::getRateNames();
    for (int t = 0; t < PolyRhythmProcessor::numTracks; ++t)
    {
        auto& tc = tracks[(size_t) t];

        tc.name.setText ("Spur " + juce::String (t + 1), juce::dontSendNotification);
        tc.name.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        tc.name.setColour (juce::Label::textColourId, trackColours[t]);
        addAndMakeVisible (tc.name);

        for (auto* s : { &tc.note, &tc.length })
        {
            s->setSliderStyle (juce::Slider::IncDecButtons);
            s->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 24);
            s->setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
            addAndMakeVisible (*s);
        }
        tc.note.setTooltip ("Note (C3 = 60)");
        tc.length.setTooltip ("Spurlaenge 1/32 - 32/32");

        tc.rate.addItemList (rateNames, 1);
        addAndMakeVisible (tc.rate);

        for (auto* s : { &tc.vel, &tc.gate })
        {
            s->setSliderStyle (juce::Slider::LinearBar);
            s->setColour (juce::Slider::trackColourId, trackColours[t].withAlpha (0.45f));
            addAndMakeVisible (*s);
        }

        tc.mute.setClickingTogglesState (true);
        tc.mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd04545));
        addAndMakeVisible (tc.mute);

        tc.rnd.setColour (juce::TextButton::buttonColourId, trackColours[t].withAlpha (0.35f));
        tc.rnd.onClick = [this, t] { proc.randomizeTrack (t); grid.repaint(); };
        addAndMakeVisible (tc.rnd);

        tc.clr.onClick = [this, t] { proc.clearTrack (t); grid.repaint(); };
        addAndMakeVisible (tc.clr);

        tc.noteA = std::make_unique<SA> (apvts, PolyRhythmProcessor::id ("note", t), tc.note);
        tc.lenA  = std::make_unique<SA> (apvts, PolyRhythmProcessor::id ("len", t), tc.length);
        tc.velA  = std::make_unique<SA> (apvts, PolyRhythmProcessor::id ("vel", t), tc.vel);
        tc.gateA = std::make_unique<SA> (apvts, PolyRhythmProcessor::id ("gate", t), tc.gate);
        tc.rateA = std::make_unique<CA> (apvts, PolyRhythmProcessor::id ("rate", t), tc.rate);
        tc.muteA = std::make_unique<BA> (apvts, PolyRhythmProcessor::id ("mute", t), tc.mute);

        tc.vel.textFromValueFunction  = [] (double v) { return "Vel " + juce::String ((int) v); };
        tc.gate.textFromValueFunction = [] (double v) { return "Gate " + juce::String (juce::roundToInt (v * 100.0)) + "%"; };
        tc.vel.updateText();
        tc.gate.updateText();
    }

    grid.rowHeight = rowH;
    addAndMakeVisible (grid);

    setSize (editorW, topH + headerH + rowH * PolyRhythmProcessor::numTracks + footerH);
    startTimerHz (30);
    timerCallback();
}

PolyRhythmEditor::~PolyRhythmEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PolyRhythmEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    g.setColour (panel);
    g.fillRect (0, 0, getWidth(), topH);
    g.fillRect (0, topH + headerH, leftW, rowH * PolyRhythmProcessor::numTracks);

    // grid header: step numbers
    const float cellW = (float) (getWidth() - leftW - 10) / (float) PolyRhythmProcessor::maxSteps;
    g.setFont (juce::FontOptions (11.0f));
    for (int s = 0; s < PolyRhythmProcessor::maxSteps; ++s)
    {
        g.setColour (s % 8 == 0 ? textCol : dimText);
        g.drawText (juce::String (s + 1), juce::Rectangle<float> ((float) leftW + s * cellW, (float) topH + 2.0f, cellW, (float) headerH - 4.0f),
                    juce::Justification::centred);
    }

    g.setColour (dimText);
    const int hy = topH + 2, hh = headerH - 4;
    g.drawText ("Spur", 12, hy, 60, hh, juce::Justification::centredLeft);
    g.drawText ("Note", 72, hy, 110, hh, juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("L\xc3\xa4nge (x/32)"), 190, hy, 110, hh, juce::Justification::centredLeft);
    g.drawText ("Rate", 308, hy, 84, hh, juce::Justification::centredLeft);

    // row separators
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int t = 1; t < PolyRhythmProcessor::numTracks; ++t)
        g.fillRect (0, topH + headerH + t * rowH, getWidth(), 1);

    g.setColour (dimText);
    g.drawText (juce::String::fromUTF8 ("Klick/Ziehen: Steps setzen  |  L\xc3\xa4nge 1/32 \xe2\x80\x93 32/32 pro Spur  |  Noten: C1 abw\xc3\xa4rts (C3 = 60)"),
                juce::Rectangle<int> (12, getHeight() - footerH, getWidth() - 24, footerH), juce::Justification::centredLeft);
}

void PolyRhythmEditor::resized()
{
    // line 1
    auto line1 = juce::Rectangle<int> (12, 10, getWidth() - 24, 30);
    title.setBounds (line1.removeFromLeft (180));
    randomAll.setBounds (line1.removeFromLeft (130).reduced (2));
    line1.removeFromLeft (16);
    densityLabel.setBounds (line1.removeFromLeft (52));
    density.setBounds (line1.removeFromLeft (170));
    line1.removeFromLeft (10);
    randLen.setBounds (line1.removeFromLeft (120));
    line1.removeFromLeft (20);
    channelLabel.setBounds (line1.removeFromLeft (76));
    channel.setBounds (line1.removeFromLeft (100));
    line1.removeFromLeft (16);
    virtOut.setBounds (line1.removeFromLeft (130));
    portLabel.setBounds (line1);

    // line 2
    auto line2 = juce::Rectangle<int> (12, 50, getWidth() - 24, 30);
    playButton.setBounds (line2.removeFromLeft (80).reduced (2));
    line2.removeFromLeft (8);
    bpmLabel.setBounds (line2.removeFromLeft (36));
    bpm.setBounds (line2.removeFromLeft (120));
    line2.removeFromLeft (6);
    syncLabel.setBounds (line2.removeFromLeft (170));
    exportLen.setBounds (line2.removeFromLeft (120).reduced (2));
    exportButton.setBounds (line2.removeFromLeft (160).reduced (2));
    line2.removeFromLeft (6);
    dragArea.setBounds (line2.removeFromLeft (180).reduced (1));
    line2.removeFromLeft (8);
    infoLabel.setBounds (line2);

    // tracks
    for (int t = 0; t < PolyRhythmProcessor::numTracks; ++t)
    {
        auto& tc = tracks[(size_t) t];
        const int y = topH + headerH + t * rowH;

        auto r1 = juce::Rectangle<int> (10, y + 4, leftW - 20, 26);
        tc.name.setBounds (r1.removeFromLeft (62));
        tc.note.setBounds (r1.removeFromLeft (110));
        r1.removeFromLeft (8);
        tc.length.setBounds (r1.removeFromLeft (110));
        r1.removeFromLeft (8);
        tc.rate.setBounds (r1.removeFromLeft (84));

        auto r2 = juce::Rectangle<int> (72, y + 34, leftW - 82, 26);
        tc.vel.setBounds (r2.removeFromLeft (100).reduced (0, 2));
        r2.removeFromLeft (6);
        tc.gate.setBounds (r2.removeFromLeft (100).reduced (0, 2));
        r2.removeFromLeft (8);
        tc.mute.setBounds (r2.removeFromLeft (34));
        r2.removeFromLeft (4);
        tc.rnd.setBounds (r2.removeFromLeft (50));
        r2.removeFromLeft (4);
        tc.clr.setBounds (r2.removeFromLeft (48));
    }

    grid.setBounds (leftW, topH + headerH, getWidth() - leftW - 10, rowH * PolyRhythmProcessor::numTracks);
}

void PolyRhythmEditor::timerCallback()
{
    const bool synced = proc.isHostSynced();
    playButton.setEnabled (! synced);
    bpm.setEnabled (! synced);
    const auto port = proc.getVirtualPortName();
    portLabel.setText (port.isNotEmpty() ? juce::String::fromUTF8 ("\xe2\x97\x8f ") + port : juce::String(), juce::dontSendNotification);
    syncLabel.setText (synced ? "Sync: Host-Transport" : "Sync: intern (Standalone)", juce::dontSendNotification);
    grid.repaint();
}

void PolyRhythmEditor::exportMidi()
{
    auto start = juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("PolyRhythm32 Loop.mid");
    chooser = std::make_unique<juce::FileChooser> ("MIDI-Loop speichern", start, "*.mid");

    const auto chooserFlags = juce::FileBrowserComponent::saveMode
                     | juce::FileBrowserComponent::canSelectFiles
                     | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File())
            return;

        f = f.withFileExtension ("mid");
        juce::String info;
        if (proc.writeMidiLoop (f, info))
            infoLabel.setText ("Gespeichert: " + f.getFileName() + " (" + info + ")", juce::dontSendNotification);
        else
            infoLabel.setText (info, juce::dontSendNotification);
    });
}
