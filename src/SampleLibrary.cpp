#include "SampleLibrary.h"
#include "SampleNames.h"
#include <BinaryData.h>
#include <cmath>

SampleLibrary::SampleLibrary()
{
    items.resize ((size_t) capacity);
    formatManager.registerBasicFormats();

    juce::WavAudioFormat wav;
    for (int i = 0; i < kNumBuiltinSamples; ++i)
    {
        const auto& info = kBuiltinSamples[i];
        // BinaryData-Namen: Dateiname mit '_' statt Sonderzeichen
        int dataSize = 0;
        const char* data = nullptr;
        for (int r = 0; r < BinaryData::namedResourceListSize; ++r)
        {
            if (juce::String (BinaryData::originalFilenames[r]) == info.file)
            {
                data = BinaryData::getNamedResource (BinaryData::namedResourceList[r], dataSize);
                break;
            }
        }
        if (data == nullptr || dataSize <= 0)
            continue;

        auto stream = std::make_unique<juce::MemoryInputStream> (data, (size_t) dataSize, false);
        std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (stream.release(), true));
        if (reader == nullptr)
            continue;

        auto s = std::make_unique<SampleData>();
        s->key = juce::String ("b:") + info.file;
        s->name = info.name;
        s->cat = info.cat;
        s->sr = reader->sampleRate;
        const int ch = (int) juce::jmin ((unsigned int) 2, reader->numChannels);
        s->buf.setSize (ch, (int) reader->lengthInSamples);
        reader->read (&s->buf, 0, (int) reader->lengthInSamples, 0, true, ch > 1);
        s->fam = familyOf (s->name);
        analyse (*s);
        add (std::move (s));
    }
}

int SampleLibrary::add (std::unique_ptr<SampleData> s)
{
    const int i = count.load();
    if (i >= capacity)
        return -1;
    items[(size_t) i] = std::move (s);
    count.store (i + 1);
    return i;
}

int SampleLibrary::findKey (const juce::String& key) const
{
    for (int i = 0; i < size(); ++i)
        if (items[(size_t) i]->key == key && ! items[(size_t) i]->hidden.load())
            return i;
    return -1;
}

int SampleLibrary::findName (const juce::String& name) const
{
    for (int i = 0; i < size(); ++i)
        if (items[(size_t) i]->name == name && ! items[(size_t) i]->hidden.load())
            return i;
    return -1;
}

int SampleLibrary::loadFile (const juce::File& f)
{
    const auto key = "u:" + f.getFullPathName();
    for (int i = 0; i < size(); ++i)
        if (items[(size_t) i]->key == key)
        {
            items[(size_t) i]->hidden.store (false);
            return i;
        }

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (f));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return -1;

    // eigene Samples auf höchstens 30 s begrenzen
    const auto len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));
    auto s = std::make_unique<SampleData>();
    s->key = key;
    s->name = f.getFileNameWithoutExtension();
    s->cat = "Eigene";
    s->custom = true;
    s->file = f;
    s->sr = reader->sampleRate;
    const int ch = (int) juce::jmin ((unsigned int) 2, reader->numChannels);
    s->buf.setSize (ch, len);
    reader->read (&s->buf, 0, len, 0, true, ch > 1);
    s->fam = familyOf (s->name);
    analyse (*s);
    return add (std::move (s));
}

int SampleLibrary::familyOf (const juce::String& nameIn)
{
    const auto n = nameIn.toLowerCase();
    auto any = [&n] (std::initializer_list<const char*> words)
    {
        for (auto* w : words)
            if (n.contains (w))
                return true;
        return false;
    };
    if (any ({ "vinyl", " sp", "60s", "70s", "mpc", "prommer", "burned", "fried", "decap", "dmx", "rattle" }))
        return 1;
    if (any ({ "808", "electr", "noise", "alien", "source", "sweep", "build", "transient", "rktd", "drop", "ironman", "chop" }))
        return 2;
    return 0;
}

void SampleLibrary::analyse (SampleData& s)
{
    const int n = s.buf.getNumSamples();
    const int nc = s.buf.getNumChannels();
    if (n <= 0 || nc <= 0)
        return;

    float pk = 0.0f;
    for (int c = 0; c < nc; ++c)
        pk = juce::jmax (pk, s.buf.getMagnitude (c, 0, n));
    s.peak = pk > 0.0f ? pk : 1.0f;

    // Auto-Start: erster Wert über 1,2 % der Spitze, 1 ms davor
    const float thr = pk * 0.012f;
    int on = 0;
    for (int i = 0; i < n && on == 0; ++i)
        for (int c = 0; c < nc; ++c)
            if (std::abs (s.buf.getSample (c, i)) > thr) { on = juce::jmax (1, i); break; }
    on = juce::jmax (0, on - (int) (s.sr * 0.001));
    s.onset = (float) (on / s.sr);

    // Lautheit der ersten 100 ms auf -20 dB angleichen
    const int w = juce::jmin (n - on, (int) (s.sr * 0.1));
    double sum = 0.0;
    for (int c = 0; c < nc; ++c)
        for (int i = on; i < on + w; ++i)
            sum += (double) s.buf.getSample (c, i) * s.buf.getSample (c, i);
    const double rms = std::sqrt (sum / juce::jmax (1, w * nc));
    float autoDb = rms > 1e-6 ? (float) (-20.0 - 20.0 * std::log10 (rms)) : 0.0f;
    if (pk > 0.0f)
        autoDb = juce::jmin (autoDb, (float) (-1.0 - 20.0 * std::log10 (pk) + 6.0));
    s.autoDb = juce::jlimit (-24.0f, 24.0f, autoDb);

    // Helligkeit: Nulldurchgänge in den ersten 100 ms
    const float* d0 = s.buf.getReadPointer (0);
    int zc = 0;
    for (int i = on + 1; i < on + w; ++i)
        if ((d0[i] >= 0.0f) != (d0[i - 1] >= 0.0f))
            ++zc;
    s.zcr = (float) zc / (float) juce::jmax (1, w);

    // hörbare Länge: letzter 10-ms-Block über 5 % der Spitze
    const int blk = juce::jmax (1, (int) (s.sr * 0.01));
    int last = on;
    for (int b = on; b < n; b += blk)
    {
        float m = 0.0f;
        for (int i = b; i < juce::jmin (n, b + blk); ++i)
            m = juce::jmax (m, std::abs (d0[i]));
        if (m > pk * 0.05f)
            last = b;
    }
    s.dur = juce::jmax (0.01f, (float) ((last - on) / s.sr));
}
