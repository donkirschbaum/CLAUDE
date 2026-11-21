// VOLTA·4 – Editor.
#include "PluginEditor.h"
#include "BinaryData.h"

VoltaEditor::VoltaEditor (VoltaProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    auto options = juce::WebBrowserComponent::Options {}
        .withBackend (juce::WebBrowserComponent::Options::Backend::defaultBackend)
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
        .withEventListener ("volta", [this] (const juce::var& m) { onUIEvent (m); });

    web = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*web);
    web->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    last.assign ((size_t) proc.numParams(), -12345.f);
    setResizable (true, true);
    setResizeLimits (400, 500, 1800, 1600);
    setSize (1060, 860);
    startTimerHz (30);
}

VoltaEditor::~VoltaEditor() { stopTimer(); }

void VoltaEditor::resized() { if (web) web->setBounds (getLocalBounds()); }

std::optional<juce::WebBrowserComponent::Resource> VoltaEditor::getResource (const juce::String& url)
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

void VoltaEditor::emit (const juce::var& m)
{
    if (web) web->emitEventIfBrowserIsVisible ("native", m);
}

void VoltaEditor::sendFullState()
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("t", "state");
    auto* pv = new juce::DynamicObject();
    for (int i = 0; i < proc.numParams(); ++i)
    {
        last[(size_t) i] = proc.rawValue (i);
        pv->setProperty (juce::Identifier (volta::paramList()[(size_t) i].id), last[(size_t) i]);
    }
    o->setProperty ("params", juce::var (pv));
    o->setProperty ("cables", proc.getCablesVar());
    o->setProperty ("pi", proc.presetIndex);
    o->setProperty ("map", proc.mapAsVar());
    emit (juce::var (o));
    lastVersion = proc.stateVersion.load();
    lastMapVersion = proc.mapVersion.load();
}

void VoltaEditor::onUIEvent (const juce::var& m)
{
    const juce::String t = m["t"].toString();
    if (t == "ready") { ready = true; sendFullState(); }
    else if (t == "p")
    {
        const int i = proc.indexOf (m["id"].toString());
        if (i < 0) return;
        const float v = (float) (double) m["v"];
        proc.setFromUI (i, v);
        last[(size_t) i] = proc.rawValue (i);
    }
    else if (t == "gb" || t == "ge")
    {
        const int i = proc.indexOf (m["id"].toString());
        if (i < 0) return;
        if (t == "gb") proc.param (i)->beginChangeGesture(); else proc.param (i)->endChangeGesture();
    }
    else if (t == "cables") proc.setCablesFromUI (m["list"]);
    else if (t == "pi") proc.presetIndex = (int) m["v"];
    else if (t == "pad") proc.padFromUI (juce::jlimit (0, 3, (int) m["c"]));
    else if (t == "learn") proc.setLearn (m["id"].toString());
    else if (t == "learnoff") proc.setLearn ({});
    else if (t == "unlearn") proc.unlearn (m["id"].toString());
    else if (t == "mapreset") proc.resetMap();
}

void VoltaEditor::timerCallback()
{
    if (! ready) return;
    if (proc.stateVersion.load() != lastVersion) { sendFullState(); return; }

    auto* changed = new juce::DynamicObject(); bool any = false;
    for (int i = 0; i < proc.numParams(); ++i)
    {
        const float v = proc.rawValue (i);
        if (v != last[(size_t) i]) { last[(size_t) i] = v; changed->setProperty (juce::Identifier (volta::paramList()[(size_t) i].id), v); any = true; }
    }
    if (any) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "params"); o->setProperty ("v", juce::var (changed)); emit (juce::var (o)); }
    else delete changed;

    if (proc.mapVersion.load() != lastMapVersion)
    {
        lastMapVersion = proc.mapVersion.load();
        auto* o = new juce::DynamicObject(); o->setProperty ("t", "map"); o->setProperty ("m", proc.mapAsVar());
        const int cc = proc.lastLearnedCC.exchange (-1);
        if (cc >= 0) o->setProperty ("learned", cc);
        emit (juce::var (o));
    }

    auto* o = new juce::DynamicObject();
    o->setProperty ("t", "tick");
    o->setProperty ("s", (juce::int64) proc.currentStep());
    o->setProperty ("lit", (int) proc.takeLit());
    juce::Array<juce::var> cv; for (int i = 0; i < 7; ++i) cv.add (proc.cvShow (i));
    o->setProperty ("cv", cv);
    o->setProperty ("playing", proc.transportPlaying.load());
    o->setProperty ("hb", proc.hostBpm.load());
    emit (juce::var (o));
}
