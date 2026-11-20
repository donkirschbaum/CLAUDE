#include "SampleLibrary.h"
#include "BinaryData.h"

namespace
{
    // Reihenfolge und Namen der Werks-Samples (wie in der Browser-Version)
    const std::vector<std::pair<const char*, const char*>> factoryNames {
        { "drums", "Clean Break" }, { "vox", "Aah" }, { "pad", "Warm Pad" }, { "keys", "Tape Keys" },
        { "bell", "FM Bell" }, { "bass", "Acid Line" }, { "pluck", "Pluck Arp" }, { "choir", "Choir" },
        { "strings", "Strings" }, { "speech", "Speech" }, { "808", "808 Boom" }, { "organ", "Organ Chord" },
        { "mallet", "Mallet Melody" }, { "metal", "Metal Hits" }, { "wind", "Wind" }, { "glitch", "Bytebeat" }
    };

    const void* findResource (const juce::String& originalName, int& size)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const char* resName = BinaryData::namedResourceList[i];
            if (originalName == BinaryData::getNamedResourceOriginalFilename (resName))
                return BinaryData::getNamedResource (resName, size);
        }
        size = 0;
        return nullptr;
    }

    // In den Papierkorb, sonst direkt löschen
    bool removeFile (const juce::File& f)
    {
        if (! f.exists()) return true;
        return f.moveToTrash() || f.deleteFile();
    }

    float varToFloat (const juce::var& v)
    {
        if (v.isBool()) return (bool) v ? 1.0f : 0.0f;
        return (float) (double) v;
    }
}

SampleLibrary::SampleLibrary()
{
    formats.registerBasicFormats();

    // Werks-Samples aus BinaryData laden
    juce::WavAudioFormat wav;
    for (auto& [id, name] : factoryNames)
    {
        int size = 0;
        if (auto* data = findResource (juce::String ("s_") + id + ".wav", size))
        {
            auto stream = std::make_unique<juce::MemoryInputStream> (data, (size_t) size, false);
            std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (stream.release(), true));
            if (auto s = loadFromReader (std::move (reader), juce::String ("factory:") + id, name))
                factory.push_back (s);
        }
    }

    // Werks-Presets aus JSON
    int size = 0;
    if (auto* data = findResource ("presets.json", size))
    {
        auto json = juce::JSON::parse (juce::String::fromUTF8 ((const char*) data, size));
        if (auto* arr = json.getArray())
        {
            for (auto& item : *arr)
            {
                PresetData p;
                p.category  = item["cat"].toString();
                p.name      = item["name"].toString();
                p.sampleRef = "factory:" + item["src"].toString();
                if (auto* obj = item["p"].getDynamicObject())
                {
                    for (auto& prop : obj->getProperties())
                    {
                        auto key = prop.name.toString();
                        auto val = varToFloat (prop.value);
                        if (key == "bits") p.values["crush"] = 16.0f - val;
                        else p.values[key] = val;
                    }
                }
                presets.push_back (p);
            }
        }
    }
    numFactoryPresets = (int) presets.size();

    samplesDir().createDirectory();
    presetsDir().createDirectory();
    rescanUserPresets();
}

juce::File SampleLibrary::rootDir()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("MIKROKORN");
}
juce::File SampleLibrary::samplesDir() { return rootDir().getChildFile ("Samples"); }
juce::File SampleLibrary::presetsDir() { return rootDir().getChildFile ("Presets"); }

bool SampleLibrary::isAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;mp3;m4a;ogg;caf");
}

SamplePtr SampleLibrary::loadFromReader (std::unique_ptr<juce::AudioFormatReader> reader, const juce::String& ref, const juce::String& name)
{
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return nullptr;

    const auto maxLen = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
    const int numCh = (int) juce::jlimit (1u, 2u, reader->numChannels);
    juce::AudioBuffer<float> tmp (numCh, maxLen);
    reader->read (&tmp, 0, maxLen, 0, true, numCh > 1);

    auto s = std::make_shared<SampleData>();
    s->ref = ref;
    s->name = name;
    s->sampleRate = reader->sampleRate;
    s->buffer.setSize (1, maxLen);
    s->buffer.clear();
    for (int c = 0; c < numCh; ++c)
        s->buffer.addFrom (0, 0, tmp, c, 0, maxLen, 1.0f / (float) numCh);

    // Eigene Samples werden wie im Browser auf 0.95 normalisiert
    if (ref.startsWith ("user:"))
    {
        const float peak = s->buffer.getMagnitude (0, 0, maxLen);
        if (peak > 0.0001f) s->buffer.applyGain (0.95f / peak);
    }
    return s;
}

juce::StringArray SampleLibrary::getUserSampleFiles() const
{
    juce::StringArray names;
    for (auto& f : samplesDir().findChildFiles (juce::File::findFiles, false))
        if (isAudioFile (f) && ! f.getFileName().startsWith ("."))
            names.add (f.getFileName());
    names.sortNatural();
    return names;
}

juce::String SampleLibrary::displayName (const juce::String& ref)
{
    if (ref.startsWith ("user:"))
        return juce::File::createFileWithoutCheckingPath (ref.fromFirstOccurrenceOf ("user:", false, false)).getFileNameWithoutExtension();
    auto id = ref.fromFirstOccurrenceOf ("factory:", false, false);
    for (auto& [fid, name] : factoryNames)
        if (id == fid) return name;
    return id;
}

SamplePtr SampleLibrary::find (const juce::String& ref)
{
    const juce::ScopedLock sl (lock);
    for (auto& s : factory)
        if (s->ref == ref) return s;

    if (ref.startsWith ("user:"))
    {
        auto it = userCache.find (ref);
        if (it != userCache.end()) return it->second;

        auto file = samplesDir().getChildFile (ref.fromFirstOccurrenceOf ("user:", false, false));
        if (! file.existsAsFile()) return nullptr;

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        auto s = loadFromReader (std::move (reader), ref, file.getFileNameWithoutExtension());
        if (s) userCache[ref] = s;
        return s;
    }
    return nullptr;
}

SamplePtr SampleLibrary::importFile (const juce::File& file, juce::String& error)
{
    {
        std::unique_ptr<juce::AudioFormatReader> test (formats.createReaderFor (file));
        if (test == nullptr) { error = "Dateiformat wird nicht unterstuetzt: " + file.getFileName(); return nullptr; }
    }

    samplesDir().createDirectory();
    juce::File target = file;
    if (file.getParentDirectory() != samplesDir())
    {
        target = samplesDir().getNonexistentChildFile (file.getFileNameWithoutExtension(), file.getFileExtension(), false);
        if (! file.copyFileTo (target)) { error = "Konnte Datei nicht in die Library kopieren."; return nullptr; }
    }
    auto s = find ("user:" + target.getFileName());
    if (s == nullptr) error = "Konnte Datei nicht lesen.";
    return s;
}

bool SampleLibrary::deleteUserSample (const juce::String& ref)
{
    if (! ref.startsWith ("user:")) return false;
    const juce::ScopedLock sl (lock);
    auto file = samplesDir().getChildFile (ref.fromFirstOccurrenceOf ("user:", false, false));
    userCache.erase (ref);

    // Presets, die dieses Sample benutzen, ebenfalls entfernen
    for (auto& p : presets)
        if (p.isUser && p.sampleRef == ref)
            removeFile (p.file);

    const bool ok = removeFile (file);
    rescanUserPresets();
    return ok;
}

int SampleLibrary::getNumPresets() const
{
    const juce::ScopedLock sl (lock);
    return (int) presets.size();
}

PresetData SampleLibrary::getPreset (int index) const
{
    const juce::ScopedLock sl (lock);
    if (index < 0 || index >= (int) presets.size()) return {};
    return presets[(size_t) index];
}

void SampleLibrary::rescanUserPresets()
{
    const juce::ScopedLock sl (lock);
    presets.resize ((size_t) numFactoryPresets);

    auto files = presetsDir().findChildFiles (juce::File::findFiles, false, "*.mkpreset");
    files.sort();
    std::vector<PresetData> user;
    for (auto& f : files)
    {
        auto json = juce::JSON::parse (f);
        if (! json.isObject()) continue;
        PresetData p;
        p.category = "Eigene Presets";
        p.name = json["name"].toString();
        p.sampleRef = json["sample"].toString();
        p.isUser = true;
        p.file = f;
        if (auto* obj = json["params"].getDynamicObject())
            for (auto& prop : obj->getProperties())
                p.values[prop.name.toString()] = varToFloat (prop.value);
        if (p.name.isEmpty()) p.name = f.getFileNameWithoutExtension();
        user.push_back (p);
    }
    std::sort (user.begin(), user.end(), [] (const PresetData& a, const PresetData& b)
               { return a.file.getCreationTime() < b.file.getCreationTime(); });
    for (auto& p : user) presets.push_back (p);
}

int SampleLibrary::findUserPresetByFile (const juce::File& f) const
{
    const juce::ScopedLock sl (lock);
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].isUser && presets[i].file == f) return (int) i;
    return -1;
}

int SampleLibrary::saveUserPreset (const juce::String& name, const juce::String& sampleRef, const std::map<juce::String, float>& values)
{
    presetsDir().createDirectory();
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("name", name);
    obj->setProperty ("sample", sampleRef);
    obj->setProperty ("version", 1);
    auto params = std::make_unique<juce::DynamicObject>();
    for (auto& [k, v] : values) params->setProperty (k, v);
    obj->setProperty ("params", juce::var (params.release()));

    auto safe = juce::File::createLegalFileName (name).trim();
    if (safe.isEmpty()) safe = "Preset";
    auto file = presetsDir().getNonexistentChildFile (safe, ".mkpreset", false);
    if (! file.replaceWithText (juce::JSON::toString (juce::var (obj.release()))))
        return -1;

    rescanUserPresets();
    return findUserPresetByFile (file);
}

bool SampleLibrary::deleteUserPreset (int index)
{
    const juce::ScopedLock sl (lock);
    if (index < numFactoryPresets || index >= (int) presets.size()) return false;
    removeFile (presets[(size_t) index].file);
    rescanUserPresets();
    return true;
}
