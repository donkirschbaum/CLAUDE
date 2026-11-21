#include "PresetManager.h"
#include "BinaryData.h"

namespace ws
{

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    loadFactoryPresets();
    rescanUserPresets();
}

juce::File PresetManager::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
               .getChildFile ("Wavesmith").getChildFile ("User Presets");
}

void PresetManager::loadFactoryPresets()
{
    const auto json = juce::String::fromUTF8 (BinaryData::factory_presets_json, BinaryData::factory_presets_jsonSize);
    const auto root = juce::JSON::parse (json);
    if (auto* list = root.getProperty ("presets", {}).getArray())
    {
        for (const auto& p : *list)
        {
            Preset preset;
            preset.name = p.getProperty ("name", "Init").toString();
            preset.category = p.getProperty ("category", "Misc").toString();
            preset.params = p.getProperty ("params", {});
            presets.push_back (preset);

            // sanity check for tests
            if (auto* obj = preset.params.getDynamicObject())
                for (const auto& prop : obj->getProperties())
                    if (apvts.getParameter (prop.name.toString()) == nullptr)
                        unknownParameterIds.addIfNotAlreadyThere (prop.name.toString());
        }
    }
    numFactory = (int) presets.size();
}

void PresetManager::rescanUserPresets()
{
    presets.resize ((size_t) numFactory);
    const auto folder = getUserPresetFolder();
    if (! folder.isDirectory()) return;

    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wspreset");
    files.sort();
    for (const auto& f : files)
    {
        const auto v = juce::JSON::parse (f);
        if (! v.isObject()) continue;
        Preset preset;
        preset.name = v.getProperty ("name", f.getFileNameWithoutExtension()).toString();
        preset.category = "User";
        preset.params = v.getProperty ("params", {});
        preset.file = f;
        presets.push_back (preset);
    }
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray cats;
    for (const auto& p : presets)
        cats.addIfNotAlreadyThere (p.category);
    return cats;
}

void PresetManager::applyParams (const juce::var& params)
{
    // Reset everything to defaults first so presets are self-contained
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    if (auto* obj = params.getDynamicObject())
    {
        for (const auto& prop : obj->getProperties())
        {
            if (auto* rp = apvts.getParameter (prop.name.toString()))
            {
                const float real = (float) (double) prop.value;
                rp->setValueNotifyingHost (rp->convertTo0to1 (real));
            }
        }
    }
}

void PresetManager::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets.size()) return;
    currentIndex = index;
    applyParams (presets[(size_t) index].params);
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return false;

    auto* params = new juce::DynamicObject();
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            params->setProperty (rp->paramID, rp->convertFrom0to1 (rp->getValue()));

    auto* root = new juce::DynamicObject();
    root->setProperty ("name", name);
    root->setProperty ("params", juce::var (params));

    const auto safeName = juce::File::createLegalFileName (name.isEmpty() ? "Untitled" : name);
    auto file = folder.getChildFile (safeName + ".wspreset");
    if (! file.replaceWithText (juce::JSON::toString (juce::var (root))))
        return false;

    rescanUserPresets();
    for (int i = numFactory; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].file == file)
            currentIndex = i;
    return true;
}

} // namespace ws
