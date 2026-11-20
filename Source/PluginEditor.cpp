#include "PluginEditor.h"

namespace Col
{
    const juce::Colour bg     { 0xff121110 };
    const juce::Colour panel  { 0xff1f1c18 };
    const juce::Colour panel2 { 0xff2a2620 };
    const juce::Colour line   { 0xff3a352d };
    const juce::Colour ink    { 0xffefe6d2 };
    const juce::Colour dim    { 0xff9a907d };
    const juce::Colour faint  { 0xff6f665a };
    const juce::Colour accent { 0xffff6a2a };
    const juce::Colour accent2{ 0xffffb347 };
    const juce::Colour well   { 0xff0c0b0a };
    const juce::Colour dark   { 0xff1a1208 };
}

static juce::Font uiFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

// ============================================================================
class MikrokornLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MikrokornLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, Col::bg);
        setColour (juce::TextButton::buttonColourId, Col::panel2);
        setColour (juce::TextButton::buttonOnColourId, Col::accent);
        setColour (juce::TextButton::textColourOffId, Col::ink);
        setColour (juce::TextButton::textColourOnId, Col::dark);
        setColour (juce::ComboBox::backgroundColourId, Col::well);
        setColour (juce::ComboBox::textColourId, Col::ink);
        setColour (juce::ComboBox::outlineColourId, Col::line);
        setColour (juce::ComboBox::arrowColourId, Col::dim);
        setColour (juce::PopupMenu::backgroundColourId, Col::panel);
        setColour (juce::PopupMenu::textColourId, Col::ink);
        setColour (juce::PopupMenu::headerTextColourId, Col::accent);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Col::accent);
        setColour (juce::PopupMenu::highlightedTextColourId, Col::dark);
        setColour (juce::Slider::textBoxTextColourId, Col::ink);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, Col::ink);
        setColour (juce::ListBox::backgroundColourId, Col::well);
        setColour (juce::ListBox::outlineColourId, Col::line);
        setColour (juce::AlertWindow::backgroundColourId, Col::panel);
        setColour (juce::AlertWindow::textColourId, Col::ink);
        setColour (juce::AlertWindow::outlineColourId, Col::line);
        setColour (juce::TextEditor::backgroundColourId, Col::well);
        setColour (juce::TextEditor::textColourId, Col::ink);
        setColour (juce::TextEditor::outlineColourId, Col::line);
        setColour (juce::TextEditor::focusedOutlineColourId, Col::accent);
        setColour (juce::ScrollBar::thumbColourId, Col::line);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider&) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        const juce::PathStrokeType stroke (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track; track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, a1, true);
        g.setColour (Col::line); g.strokePath (track, stroke);
        if (pos > 0.001f)
        {
            juce::Path v; v.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, ang, true);
            g.setColour (Col::accent); g.strokePath (v, stroke);
        }
        const float kr = r - 8.0f;
        g.setColour (juce::Colour (0xff2e2a23)); g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (juce::Colour (0xff4a4338)); g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);
        g.setColour (Col::ink);
        g.drawLine (juce::Line<float> (c, c.getPointOnCircumference (kr - 3.0f, ang)), 2.2f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        auto fill = on ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
        if (down) fill = fill.brighter (0.1f);
        g.setColour (fill); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? fill : (over ? juce::Colour (0xff5a5246) : Col::line));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return uiFont (std::min (13.0f, h * 0.5f)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (19.0f, true); }
    juce::Font getPopupMenuFont() override { return uiFont (14.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
};

// ============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 72, 16);
        slider.setMouseDragSensitivity (180);
        slider.setVelocityBasedMode (false);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setDoubleClickReturnValue (true, 0.0); // wird unten durch Default ersetzt
        addAndMakeVisible (slider);
        label.setText (title, juce::dontSendNotification);
        label.setFont (uiFont (10.5f));
        label.setColour (juce::Label::textColourId, Col::dim);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
        if (auto* p = s.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (14));
        slider.setBounds (r);
    }
private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// ============================================================================
class WaveformView : public juce::Component
{
public:
    explicit WaveformView (MikrokornProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Col::well); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Col::line); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        auto sample = proc.getCurrentSample();
        const int W = getWidth(), H = getHeight();
        if (sample.get() != peakSample || W != peakWidth) buildPeaks (sample, W);

        const float st = proc.apvts.getRawParameterValue ("start")->load();
        const float en = proc.apvts.getRawParameterValue ("end")->load();
        const bool rev = en < st;
        const float s = std::min (st, en) * W, e = std::max (st, en) * W;
        g.setColour (Col::accent.withAlpha (0.07f)); g.fillRect (s, 0.0f, e - s, (float) H);

        const float mid = H * 0.5f, amp = H * 0.44f;
        for (int x = 0; x < (int) peaks.size() / 2; ++x)
        {
            const bool inside = x >= s && x <= e;
            g.setColour (inside ? juce::Colour (0xffe9dcc0) : juce::Colour (0xff4a4338));
            const float y0 = mid - peaks[(size_t) x * 2 + 1] * amp, y1 = mid - peaks[(size_t) x * 2] * amp;
            g.fillRect ((float) x, y0, 1.0f, std::max (1.0f, y1 - y0));
        }

        auto marker = [&] (float x, const char* l, bool left)
        {
            g.setColour (Col::accent); g.fillRect (x - 1.0f, 0.0f, 2.0f, (float) H);
            const float bx = left ? x : x - 14.0f;
            g.fillRect (bx, 0.0f, 14.0f, 14.0f);
            g.setColour (Col::dark); g.setFont (uiFont (10.0f, true));
            g.drawText (l, juce::Rectangle<float> (bx, 0, 14, 14), juce::Justification::centred);
        };
        marker (st * W, "S", true);
        marker (en * W, "E", false);
        if (rev) { g.setColour (Col::accent2); g.setFont (uiFont (10.0f)); g.drawText (juce::String::fromUTF8 ("\xe2\x97\x80 rueckwaerts"), (int) s + 18, H - 18, 120, 14, juce::Justification::left); }

        if (proc.uiActive.load())
        {
            const float x = proc.uiPos.load() * W;
            const float gw = proc.uiGrain.load() * W;
            const bool backwards = (proc.apvts.getRawParameterValue ("reverse")->load() > 0.5f) != rev;
            if (gw > 0) { g.setColour (Col::accent2.withAlpha (0.22f)); g.fillRect (backwards ? x - std::max (2.0f, gw) : x, 0.0f, std::max (2.0f, gw), (float) H); }
            g.setColour (Col::accent2); g.fillRect (x - 1.0f, 0.0f, 2.0f, (float) H);
        }

        g.setColour (juce::Colour (0xff5f574b)); g.setFont (uiFont (10.0f));
        g.drawText (sample ? sample->name : juce::String(), 22, 3, 300, 14, juce::Justification::left);
        g.drawText ("Marker ziehen  |  Audiodatei hier ablegen", W - 270, 3, 252, 14, juce::Justification::right);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const float x = juce::jlimit (0.0f, 1.0f, e.position.x / (float) getWidth());
        const float st = proc.apvts.getRawParameterValue ("start")->load();
        const float en = proc.apvts.getRawParameterValue ("end")->load();
        dragParam = proc.apvts.getParameter (std::abs (x - st) <= std::abs (x - en) ? "start" : "end");
        if (dragParam) { dragParam->beginChangeGesture(); dragParam->setValueNotifyingHost (x); }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragParam) dragParam->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, e.position.x / (float) getWidth()));
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragParam) dragParam->endChangeGesture();
        dragParam = nullptr;
    }

private:
    void buildPeaks (const SamplePtr& s, int W)
    {
        peakSample = s.get(); peakWidth = W;
        peaks.assign ((size_t) std::max (1, W) * 2, 0.0f);
        if (s == nullptr || W <= 0) return;
        const float* d = s->buffer.getReadPointer (0);
        const int n = s->buffer.getNumSamples();
        const double per = (double) n / W;
        for (int i = 0; i < W; ++i)
        {
            float mn = 0, mx = 0;
            const int a = (int) (i * per), b = std::min (n, (int) ((i + 1) * per) + 1);
            for (int j = a; j < b; ++j) { mn = std::min (mn, d[j]); mx = std::max (mx, d[j]); }
            peaks[(size_t) i * 2] = mn; peaks[(size_t) i * 2 + 1] = mx;
        }
    }

    MikrokornProcessor& proc;
    std::vector<float> peaks;
    const SampleData* peakSample = nullptr;
    int peakWidth = 0;
    juce::RangedAudioParameter* dragParam = nullptr;
};

// ============================================================================
class UserSampleList : public juce::ListBox, private juce::ListBoxModel
{
public:
    explicit UserSampleList (MikrokornProcessor& p) : proc (p)
    {
        setModel (this);
        setRowHeight (22);
        setOutlineThickness (1);
    }
    void refresh()
    {
        auto now = proc.library.getUserSampleFiles();
        if (now != files) { files = now; updateContent(); }
        repaint();
    }
    juce::String selectedRef() const
    {
        const int r = getSelectedRow();
        return juce::isPositiveAndBelow (r, files.size()) ? "user:" + files[r] : juce::String();
    }
    int getNumRows() override { return files.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, files.size())) return;
        const bool current = proc.getCurrentSampleRef() == "user:" + files[row];
        if (current) { g.setColour (Col::accent2); g.fillRect (0, 0, w, h); }
        else if (selected) { g.setColour (Col::panel2); g.fillRect (0, 0, w, h); }
        g.setColour (current ? Col::dark : Col::ink);
        g.setFont (uiFont (12.0f));
        g.drawText (juce::File::createFileWithoutCheckingPath (files[row]).getFileNameWithoutExtension(), 8, 0, w - 12, h, juce::Justification::centredLeft);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (juce::isPositiveAndBelow (row, files.size())) proc.useSample ("user:" + files[row]);
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        juce::ListBox::paintOverChildren (g);
        if (files.isEmpty())
        {
            g.setColour (Col::faint); g.setFont (uiFont (11.0f));
            g.drawFittedText ("Noch leer - importiere eigene Samples oder zieh Audiodateien auf das Plugin.", getLocalBounds().reduced (10), juce::Justification::centred, 3);
        }
    }
private:
    MikrokornProcessor& proc;
    juce::StringArray files;
};

// ============================================================================
class MikrokornContent : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    MikrokornContent (MikrokornProcessor& p, juce::LookAndFeel& lf)
        : proc (p), lnf (lf), wave (p), userList (p),
          keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        // Kopfzeile
        for (auto* b : { &randBtn, &initBtn }) addAndMakeVisible (b);
        randBtn.onClick = [this] { proc.randomize(); };
        initBtn.onClick = [this] { proc.initSlot(); };

        // Slots
        for (int i = 0; i < MikrokornProcessor::numSlots; ++i)
        {
            auto* b = slotBtns.add (new juce::TextButton());
            b->onClick = [this, i] { proc.selectSlot (i); };
            addAndMakeVisible (b);
        }

        // Preset-Leiste
        addAndMakeVisible (prevBtn); addAndMakeVisible (nextBtn);
        prevBtn.onClick = [this] { proc.stepPreset (-1); };
        nextBtn.onClick = [this] { proc.stepPreset (1); };
        addAndMakeVisible (presetBox);
        presetBox.setJustificationType (juce::Justification::centredLeft);
        presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        presetBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        presetBox.setColour (juce::ComboBox::focusedOutlineColourId, juce::Colours::transparentBlack);
        presetBox.onChange = [this]
        {
            const int id = presetBox.getSelectedId();
            if (id > 0 && id - 1 != proc.getCurrentPreset()) proc.loadPreset (id - 1);
        };
        catLabel.setFont (uiFont (10.5f, true));
        catLabel.setColour (juce::Label::textColourId, Col::accent);
        catLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (catLabel);
        numLabel.setFont (uiFont (17.0f, true));
        numLabel.setColour (juce::Label::textColourId, Col::accent2);
        numLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (numLabel);
        addAndMakeVisible (saveBtn); addAndMakeVisible (delBtn);
        saveBtn.setToggleState (true, juce::dontSendNotification);
        saveBtn.onClick = [this] { askSavePreset(); };
        delBtn.onClick = [this] { askDeletePreset(); };

        addAndMakeVisible (wave);

        // Library
        for (auto& fs : proc.library.getFactorySamples())
        {
            auto* b = factoryBtns.add (new juce::TextButton (fs->name));
            b->setColour (juce::TextButton::buttonOnColourId, Col::accent2);
            const auto ref = fs->ref;
            b->onClick = [this, ref] { proc.useSample (ref); };
            addAndMakeVisible (b);
        }
        addAndMakeVisible (userList);
        for (auto* b : { &importBtn, &folderBtn, &delSampleBtn }) addAndMakeVisible (b);
        importBtn.onClick = [this] { chooseFiles(); };
        folderBtn.onClick = [] { SampleLibrary::samplesDir().createDirectory(); SampleLibrary::samplesDir().startAsProcess(); };
        delSampleBtn.onClick = [this] { askDeleteSample(); };

        // Regler
        auto knob = [this] (const char* id, const char* title)
        {
            auto* k = knobs.add (new Knob (proc.apvts, id, title));
            addAndMakeVisible (k);
            return k;
        };
        kTune = knob ("tune", "PITCH"); kStart = knob ("start", "START"); kEnd = knob ("end", "ENDE");
        kGrain = knob ("grain", "GRAIN"); kShift = knob ("shift", "SHIFT"); kSmooth = knob ("smooth", "SMOOTH");
        kCrush = knob ("crush", "CRUSH"); kRate = knob ("srate", "RATE");
        kAttack = knob ("attack", "ATTACK"); kRelease = knob ("release", "RELEASE");
        kCutoff = knob ("cutoff", "FILTER"); kReso = knob ("reso", "RESO"); kDrive = knob ("drive", "DRIVE"); kVol = knob ("vol", "VOLUME");

        auto toggle = [this] (juce::TextButton& b, const char* id)
        {
            b.setClickingTogglesState (true);
            addAndMakeVisible (b);
            buttonAtts.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (proc.apvts, id, b));
        };
        toggle (loopBtn, "loop"); toggle (revBtn, "reverse"); toggle (legatoBtn, "legato"); toggle (holdBtn, "hold");

        // Klaviatur
        keyboard.setAvailableRange (24, 108);
        keyboard.setLowestVisibleKey (48);
        keyboard.setKeyPressBaseOctave (5);
        keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe9e1cf));
        keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1b1916));
        keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff000000));
        keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Col::accent);
        keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Col::accent2.withAlpha (0.3f));
        keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, Col::panel2);
        keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, Col::ink);
        addAndMakeVisible (keyboard);

        refreshAll();
        startTimerHz (30);
    }

    ~MikrokornContent() override { stopTimer(); }

    // ---------------------------------------------------------------- Layout
    void resized() override
    {
        const int W = getWidth(), pad = 20, inner = W - pad * 2;
        initBtn.setBounds (W - pad - 80, 20, 80, 30);
        randBtn.setBounds (W - pad - 170, 20, 82, 30);

        const int sw = (inner - 5 * 8) / 6;
        for (int i = 0; i < slotBtns.size(); ++i)
            slotBtns[i]->setBounds (pad + i * (sw + 8), 68, sw, 42);

        juce::Rectangle<int> bar (pad, 118, inner, 48);
        prevBtn.setBounds (bar.removeFromLeft (48)); bar.removeFromLeft (8);
        delBtn.setBounds (bar.removeFromRight (120)); bar.removeFromRight (8);
        saveBtn.setBounds (bar.removeFromRight (160)); bar.removeFromRight (8);
        numLabel.setBounds (bar.removeFromRight (90)); bar.removeFromRight (8);
        nextBtn.setBounds (bar.removeFromRight (48)); bar.removeFromRight (8);
        presetArea = bar;
        catLabel.setBounds (bar.getX() + 10, bar.getY() + 3, bar.getWidth() - 20, 13);
        presetBox.setBounds (bar.getX() + 2, bar.getY() + 15, bar.getWidth() - 4, 32);

        wave.setBounds (pad, 174, inner, 140);

        libArea = { pad, 322, inner, 170 };
        const int gx = pad + 10, gy = libArea.getY() + 40, gw = 620, cols = 4;
        const int bw = (gw - (cols - 1) * 6) / cols, bh = 26;
        for (int i = 0; i < factoryBtns.size(); ++i)
            factoryBtns[i]->setBounds (gx + (i % cols) * (bw + 6), gy + (i / cols) * (bh + 6), bw, bh);
        const int ux = gx + gw + 16, uw = libArea.getRight() - 10 - ux;
        userList.setBounds (ux, gy, uw, 84);
        const int ubw = (uw - 12) / 3;
        importBtn.setBounds (ux, gy + 92, ubw + 20, 26);
        folderBtn.setBounds (ux + ubw + 26, gy + 92, ubw - 14, 26);
        delSampleBtn.setBounds (ux + ubw * 2 + 18, gy + 92, uw - (ubw * 2 + 18), 26);

        // Regler-Reihen
        const int kw = 68, kh = 92, ry1 = 500, ry2 = 628, sh = 120;
        int x = pad;
        auto section = [&] (const char* title, int y, std::initializer_list<Knob*> ks, std::initializer_list<juce::TextButton*> ts)
        {
            const int w = 10 + (int) ks.size() * kw + (ts.size() ? 100 : 0) + 10;
            sections.push_back ({ title, { x, y, w, sh } });
            int kx = x + 10;
            for (auto* k : ks) { k->setBounds (kx, y + 22, kw, kh); kx += kw; }
            int ty = y + 34;
            for (auto* t : ts) { t->setBounds (kx + 8, ty, 88, 28); ty += 36; }
            x += w + 8;
        };
        sections.clear();
        section ("SAMPLE", ry1, { kTune, kStart, kEnd }, { &loopBtn, &revBtn });
        section ("GRAIN", ry1, { kGrain, kShift, kSmooth }, {});
        section ("LO-FI", ry1, { kCrush, kRate }, {});
        section ("HUELLKURVE", ry1, { kAttack, kRelease }, { &legatoBtn, &holdBtn });
        x = pad;
        section ("AUSGANG", ry2, { kCutoff, kReso, kDrive, kVol }, {});
        keyboard.setBounds (x, ry2, W - pad - x, sh);
        keyboard.setKeyWidth (std::max (16.0f, (W - pad - x) / 29.0f));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        juce::ColourGradient grad (juce::Colour (0xff25211c), 0, 0, juce::Colour (0xff1b1915), 0, r.getHeight(), false);
        g.setGradientFill (grad); g.fillRect (r);

        // Logo
        g.setFont (uiFont (30.0f, true));
        g.setColour (Col::ink); g.drawText ("MIKRO", 20, 14, 120, 34, juce::Justification::left);
        const int mw = juce::GlyphArrangement::getStringWidthInt (uiFont (30.0f, true), "MIKRO");
        g.setColour (Col::accent); g.drawText ("KORN", 20 + mw, 14, 120, 34, juce::Justification::left);
        g.setFont (uiFont (10.5f)); g.setColour (Col::dim);
        g.drawText (juce::String::fromUTF8 ("GRANULAR-SAMPLER  \xc2\xb7  V") + JucePlugin_VersionString, 21, 46, 300, 14, juce::Justification::left);

        // LED
        const float en = proc.uiActive.load() ? proc.uiEnv.load() : 0.0f;
        const juce::Rectangle<float> led ((float) getWidth() - 214, 29, 11, 11);
        if (en > 0.01f)
        {
            g.setColour (Col::accent.withAlpha (0.25f * en)); g.fillEllipse (led.expanded (5));
            g.setColour (Col::accent.withAlpha (0.35f + en * 0.65f));
        }
        else g.setColour (juce::Colour (0xff3b1a0d));
        g.fillEllipse (led);

        // Preset-Anzeige
        g.setColour (Col::well); g.fillRoundedRectangle (presetArea.toFloat(), 8.0f);
        g.setColour (Col::line); g.drawRoundedRectangle (presetArea.toFloat().reduced (0.5f), 8.0f, 1.0f);
        auto nl = numLabel.getBounds().toFloat();
        g.setColour (Col::well); g.fillRoundedRectangle (nl, 8.0f);
        g.setColour (Col::line); g.drawRoundedRectangle (nl.reduced (0.5f), 8.0f, 1.0f);

        // Library + Sektionen
        auto box = [&] (juce::Rectangle<int> b, const juce::String& title)
        {
            g.setColour (juce::Colours::black.withAlpha (0.18f)); g.fillRoundedRectangle (b.toFloat(), 10.0f);
            g.setColour (Col::line); g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 10.0f, 1.0f);
            g.setColour (Col::dim); g.setFont (uiFont (10.0f));
            g.drawText (title, b.getX() + 10, b.getY() + 7, b.getWidth() - 20, 12, juce::Justification::left);
        };
        box (libArea, "SAMPLE-LIBRARY");
        g.setColour (Col::faint); g.setFont (uiFont (10.0f));
        g.drawText ("WERKS-SAMPLES", libArea.getX() + 10, libArea.getY() + 24, 200, 12, juce::Justification::left);
        g.drawText ("EIGENE SAMPLES  (" + SampleLibrary::samplesDir().getFullPathName().replace (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName(), "~") + ")",
                    userList.getX(), libArea.getY() + 24, userList.getWidth(), 12, juce::Justification::left);
        for (auto& s : sections) box (s.second, s.first);

        if (dragOver)
        {
            g.setColour (Col::accent2); g.drawRoundedRectangle (wave.getBounds().toFloat().expanded (3), 10.0f, 2.0f);
            g.drawRoundedRectangle (libArea.toFloat().expanded (3), 12.0f, 2.0f);
        }
    }

    // ---------------------------------------------------------------- Drag & Drop
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files) if (SampleLibrary::isAudioFile (juce::File (f))) return true;
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dragOver = false; repaint();
        juce::Array<juce::File> list;
        for (auto& f : files) if (SampleLibrary::isAudioFile (juce::File (f))) list.add (juce::File (f));
        importFiles (list);
    }

private:
    void timerCallback() override
    {
        wave.repaint();
        repaint (getWidth() - 225, 18, 30, 30);
        const int v = proc.stateVersion.load();
        if (v != lastVersion) refreshAll();
        if (++rescanCounter >= 60) { rescanCounter = 0; userList.refresh(); }   // neue Dateien im Ordner erkennen
    }

    void refreshAll()
    {
        lastVersion = proc.stateVersion.load();
        const int cur = proc.getCurrentSlot();
        for (int i = 0; i < slotBtns.size(); ++i)
        {
            slotBtns[i]->setButtonText (juce::String (i + 1) + "   " + proc.getSlotName (i));
            slotBtns[i]->setToggleState (i == cur, juce::dontSendNotification);
        }

        const int count = proc.library.getNumPresets();
        if (count != presetCount) rebuildPresetBox();
        const int pi = proc.getCurrentPreset();
        if (pi >= 0)
        {
            presetBox.setSelectedId (pi + 1, juce::dontSendNotification);
            auto pr = proc.library.getPreset (pi);
            catLabel.setText ((pr.category + (proc.isCurrentEdited() ? "  -  bearbeitet" : "")).toUpperCase(), juce::dontSendNotification);
            numLabel.setText (juce::String (pi + 1).paddedLeft ('0', 3) + "/" + juce::String (count), juce::dontSendNotification);
        }
        else
        {
            presetBox.setSelectedId (0, juce::dontSendNotification);
            presetBox.setText (proc.getSlotName (cur), juce::dontSendNotification);
            catLabel.setText ("NICHT GESPEICHERT  -  EIGENE EINSTELLUNG", juce::dontSendNotification);
            numLabel.setText ("-/" + juce::String (count), juce::dontSendNotification);
        }
        delBtn.setVisible (pi >= proc.library.getNumFactoryPresets());

        const auto ref = proc.getCurrentSampleRef();
        const auto& fs = proc.library.getFactorySamples();
        for (int i = 0; i < factoryBtns.size() && i < (int) fs.size(); ++i)
            factoryBtns[i]->setToggleState (fs[(size_t) i]->ref == ref, juce::dontSendNotification);
        userList.refresh();
        repaint();
    }

    void rebuildPresetBox()
    {
        presetCount = proc.library.getNumPresets();
        presetBox.clear (juce::dontSendNotification);
        juce::String lastCat;
        for (int i = 0; i < presetCount; ++i)
        {
            auto pr = proc.library.getPreset (i);
            if (pr.category != lastCat) { presetBox.addSectionHeading (pr.category); lastCat = pr.category; }
            presetBox.addItem (juce::String (i + 1).paddedLeft ('0', 3) + "  " + pr.name, i + 1);
        }
    }

    void askSavePreset()
    {
        auto* w = new juce::AlertWindow ("Preset speichern", "Name fuer das neue Preset:", juce::MessageBoxIconType::NoIcon, this);
        w->setLookAndFeel (&lnf);
        const int pi = proc.getCurrentPreset();
        auto def = proc.getSlotName (proc.getCurrentSlot());
        if (pi >= 0 && pi < proc.library.getNumFactoryPresets() && ! proc.isCurrentEdited()) def << " 2";
        w->addTextEditor ("name", def);
        w->addButton ("Speichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
        {
            if (result != 1) return;
            auto name = w->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;
            if (proc.saveCurrentAsPreset (name) < 0) showMessage ("Konnte Preset nicht speichern.");
        }), true);
    }

    void askDeletePreset()
    {
        const int pi = proc.getCurrentPreset();
        if (pi < proc.library.getNumFactoryPresets()) return;
        confirm ("Preset loeschen?", "\"" + proc.library.getPreset (pi).name + "\" wird in den Papierkorb verschoben.",
                 [this] { proc.deleteCurrentPreset(); });
    }

    void askDeleteSample()
    {
        auto ref = userList.selectedRef();
        if (ref.isEmpty()) ref = proc.getCurrentSampleRef().startsWith ("user:") ? proc.getCurrentSampleRef() : juce::String();
        if (ref.isEmpty()) { showMessage ("Waehle zuerst ein eigenes Sample in der Liste aus."); return; }
        confirm ("Sample loeschen?", "\"" + SampleLibrary::displayName (ref) + "\" und alle eigenen Presets, die es benutzen, werden in den Papierkorb verschoben.",
                 [this, ref] { proc.deleteUserSample (ref); });
    }

    void confirm (const juce::String& title, const juce::String& msg, std::function<void()> onOk)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle (title).withMessage (msg)
                                          .withButton ("Loeschen").withButton ("Abbrechen")
                                          .withAssociatedComponent (this),
                                      [onOk] (int r) { if (r == 1 && onOk) onOk(); });
    }

    void showMessage (const juce::String& msg)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::InfoIcon)
                                          .withTitle ("MIKROKORN").withMessage (msg).withButton ("OK")
                                          .withAssociatedComponent (this), nullptr);
    }

    void chooseFiles()
    {
        chooser = std::make_unique<juce::FileChooser> ("Samples in die Library importieren", juce::File(),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.ogg;*.caf");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc) { importFiles (fc.getResults()); });
    }

    void importFiles (const juce::Array<juce::File>& files)
    {
        if (files.isEmpty()) return;
        juce::String err;
        if (! proc.importSamples (files, err)) showMessage (err.isNotEmpty() ? err : juce::String ("Import fehlgeschlagen."));
        else if (err.isNotEmpty()) showMessage (err);
    }

    MikrokornProcessor& proc;
    juce::LookAndFeel& lnf;

    juce::TextButton randBtn { "Zufall" }, initBtn { "Init" };
    juce::OwnedArray<juce::TextButton> slotBtns;
    juce::TextButton prevBtn { juce::String::fromUTF8 ("\xe2\x97\x80") }, nextBtn { juce::String::fromUTF8 ("\xe2\x96\xb6") };
    juce::ComboBox presetBox;
    juce::Label catLabel, numLabel;
    juce::TextButton saveBtn { "Als Preset speichern" }, delBtn { "Preset loeschen" };
    juce::Rectangle<int> presetArea, libArea;

    WaveformView wave;
    juce::OwnedArray<juce::TextButton> factoryBtns;
    UserSampleList userList;
    juce::TextButton importBtn { "+ Importieren" }, folderBtn { "Ordner" }, delSampleBtn { "Loeschen" };

    juce::OwnedArray<Knob> knobs;
    Knob *kTune {}, *kStart {}, *kEnd {}, *kGrain {}, *kShift {}, *kSmooth {}, *kCrush {}, *kRate {},
         *kAttack {}, *kRelease {}, *kCutoff {}, *kReso {}, *kDrive {}, *kVol {};
    juce::TextButton loopBtn { "LOOP" }, revBtn { "REVERSE" }, legatoBtn { "LEGATO" }, holdBtn { "HOLD" };
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAtts;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sections;

    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;

    int lastVersion = -1, presetCount = -1, rescanCounter = 0;
    bool dragOver = false;
};

// ============================================================================
MikrokornEditor::MikrokornEditor (MikrokornProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    lnf = std::make_unique<MikrokornLookAndFeel>();
    setLookAndFeel (lnf.get());
    content = std::make_unique<MikrokornContent> (p, *lnf);
    addAndMakeVisible (*content);
    content->setSize (baseWidth, baseHeight);

    setResizable (true, true);
    setResizeLimits (baseWidth * 6 / 10, baseHeight * 6 / 10, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseWidth / baseHeight);
    setSize (baseWidth, baseHeight);
    setWantsKeyboardFocus (false);
}

MikrokornEditor::~MikrokornEditor()
{
    content = nullptr;
    setLookAndFeel (nullptr);
}

void MikrokornEditor::paint (juce::Graphics& g) { g.fillAll (Col::bg); }

void MikrokornEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;
    content->setTransform (juce::AffineTransform::scale (scale));
    content->setBounds (0, 0, baseWidth, baseHeight);
}
