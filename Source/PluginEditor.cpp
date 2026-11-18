// Shruthi XT editor. GPL v3.0.
#include "PluginEditor.h"
#include "BinaryData.h"

ShruthiEditor::ShruthiEditor (ShruthiProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    auto options = juce::WebBrowserComponent::Options {}
        .withBackend (juce::WebBrowserComponent::Options::Backend::defaultBackend)
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
        .withEventListener ("shruthi", [this] (const juce::var& m) { onUIEvent (m); });

    web = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*web);
    web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    setResizeLimits (420, 500, 1600, 1400);
    setSize (1000, 780);
    proc.readParamValues (lastParams); proc.readMtxValues (lastMtx);
    startTimerHz (30);
}

ShruthiEditor::~ShruthiEditor() { stopTimer(); }

void ShruthiEditor::resized() { if (web) web->setBounds (getLocalBounds()); }

std::optional<juce::WebBrowserComponent::Resource> ShruthiEditor::getResource (const juce::String& url)
{
    const auto path = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    if (path.isEmpty() || path == "/" || path.startsWith ("index"))
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) BinaryData::index_htmlSize);
        std::memcpy (r.data.data(), BinaryData::index_html, r.data.size());
        r.mimeType = "text/html";
        return r;
    }
    return std::nullopt;
}

void ShruthiEditor::emit (const juce::var& m)
{
    if (web) web->emitEventIfBrowserIsVisible ("native", m);
}

void ShruthiEditor::onUIEvent (const juce::var& m)
{
    const juce::String t = m["t"].toString();
    if (t == "ready")
    {
        ready = true;
        auto st = proc.stateAsVar();
        st.getDynamicObject()->setProperty ("t", "state");
        emit (st);
        proc.readParamValues (lastParams); proc.readMtxValues (lastMtx);
        lastVersion = proc.stateVersion.load();
        lastMapVersion = -1;
    }
    else if (t == "patch")
    {
        proc.applyPatchFromUI (m["p"]);
        proc.readParamValues (lastParams); proc.readMtxValues (lastMtx);
    }
    else if (t == "sys") proc.applySysFromUI (m["g"]);
    else if (t == "store") proc.saveStore (m["data"]);
    else if (t == "importSyx")
    {
        chooser = std::make_unique<juce::FileChooser> ("SysEx-Datei wählen", juce::File(), "*.syx;*.SYX");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                juce::MemoryBlock mb; f.loadFileAsData (mb);
                juce::Array<juce::var> bytes;
                const auto* d = static_cast<const uint8_t*> (mb.getData());
                for (size_t i = 0; i < mb.getSize(); ++i) bytes.add ((int) d[i]);
                auto* o = new juce::DynamicObject(); o->setProperty ("t", "syxData"); o->setProperty ("bytes", bytes);
                emit (juce::var (o));
            });
    }
    else if (t == "learn") proc.setLearn (m["id"].toString());
    else if (t == "unmap") proc.unmap ((int) m["cc"]);
    else if (t == "mapmode") proc.setMapMode ((int) m["cc"], (int) m["mode"]);
    else if (t == "mapreset") proc.resetMap (true);
    else if (t == "mapclear") proc.resetMap (false);
    else if (t == "data") {}
    else proc.postUIEvent (m);
}

void ShruthiEditor::timerCallback()
{
    if (! ready) return;
    if (proc.stateVersion.load() != lastVersion)
    {
        lastVersion = proc.stateVersion.load();
        auto st = proc.stateAsVar(); st.getDynamicObject()->setProperty ("t", "state"); emit (st);
        proc.readParamValues (lastParams); proc.readMtxValues (lastMtx);
        return;
    }
    int now[shruthi::kNumParams]; proc.readParamValues (now);
    auto* changed = new juce::DynamicObject(); bool any = false;
    for (int i = 0; i < shruthi::kNumParams; ++i)
        if (now[i] != lastParams[i]) { changed->setProperty (shruthi::kParamInfo[i].id, now[i]); lastParams[i] = now[i]; any = true; }
    int mnow[36]; proc.readMtxValues (mnow);
    for (int i = 0; i < 36; ++i)
        if (mnow[i] != lastMtx[i]) { changed->setProperty ("m" + juce::String (i / 3 + 1) + "sda"[i % 3], mnow[i]); lastMtx[i] = mnow[i]; any = true; }
    if (any) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "params"); o->setProperty ("v", juce::var (changed)); emit (juce::var (o)); }
    else delete changed;

    if (proc.mapVersion.load() != lastMapVersion)
    {
        lastMapVersion = proc.mapVersion.load();
        auto* o = new juce::DynamicObject(); o->setProperty ("t", "map"); o->setProperty ("list", proc.mapAsVar());
        emit (juce::var (o));
    }
    const int s = proc.currentStep();
    if (s != lastStep) { lastStep = s; auto* o = new juce::DynamicObject(); o->setProperty ("t", "step"); o->setProperty ("s", s); emit (juce::var (o)); }
    const bool j = proc.jamState();
    if (j != lastJam) { lastJam = j; auto* o = new juce::DynamicObject(); o->setProperty ("t", "jam"); o->setProperty ("on", j); emit (juce::var (o)); }
}
