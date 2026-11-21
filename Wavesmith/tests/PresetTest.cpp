// Headless test: loads every factory preset, plays notes and checks the output.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_events/juce_events.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
struct PlayHead : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (124.0);
        info.setIsPlaying (false);
        return info;
    }
};
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const bool calibrate = argc > 2 && juce::String (argv[1]) == "--calibrate";
    const bool writeWavs = argc > 1 && ! calibrate;
    juce::DynamicObject::Ptr calib = new juce::DynamicObject();

    WavesmithProcessor proc;
    PlayHead ph;
    proc.setPlayHead (&ph);
    const double sr = 44100.0;
    const int block = 256;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    if (! proc.presets.unknownParameterIds.isEmpty())
    {
        std::printf ("UNKNOWN PARAMS: %s\n", proc.presets.unknownParameterIds.joinIntoString (", ").toRawUTF8());
        return 1;
    }

    const int n = proc.getNumPrograms();
    std::printf ("presets: %d\n", n);
    int problems = 0;
    double totalCpu = 0.0, audioSeconds = 0.0;

    for (int i = 0; i < n; ++i)
    {
        proc.setCurrentProgram (i);
        proc.prepareToPlay (sr, block);
        const auto& pre = proc.presets.getPreset (i);
        const bool chord = (pre.category == "Pluck" || pre.category == "Lead") && ! calibrate;
        if (calibrate)
            if (auto* v = proc.apvts.getParameter ("masterVol"))
                v->setValueNotifyingHost (v->convertTo0to1 (-24.0f));

        juce::AudioBuffer<float> buf (2, block);
        juce::AudioBuffer<float> rec (2, (int) (sr * 3.0));
        rec.clear();
        double peak = 0.0, sumSq = 0.0;
        bool bad = false;
        const int totalBlocks = (int) (sr * 3.0) / block;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();

        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
            {
                const int base = pre.category == "Bass" ? 36 : 60;
                midi.addEvent (juce::MidiMessage::noteOn (1, base, (juce::uint8) 100), 10);
                if (chord)
                {
                    midi.addEvent (juce::MidiMessage::noteOn (1, base + 4, (juce::uint8) 100), 12);
                    midi.addEvent (juce::MidiMessage::noteOn (1, base + 7, (juce::uint8) 100), 14);
                }
            }
            if (b == (int) (sr * 2.0) / block)
                midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);

            proc.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
            {
                const float* d = buf.getReadPointer (c);
                for (int s = 0; s < block; ++s)
                {
                    if (! std::isfinite (d[s])) bad = true;
                    peak = std::max (peak, (double) std::abs (d[s]));
                    sumSq += (double) d[s] * d[s];
                }
                rec.copyFrom (c, b * block, buf, c, 0, block);
            }
        }
        totalCpu += juce::Time::getMillisecondCounterHiRes() - t0;
        audioSeconds += 3.0;
        const double rms = std::sqrt (sumSq / (2.0 * totalBlocks * block));
        const double rmsDb = juce::Decibels::gainToDecibels (rms, -120.0);

        const bool quiet = rmsDb < -40.0, loud = rmsDb > -6.0;
        if (bad || quiet || loud)
        {
            ++problems;
            std::printf ("[%4d] %-10s %-24s peak %.3f rms %.1f dB %s\n", i, pre.category.toRawUTF8(), pre.name.toRawUTF8(),
                         peak, rmsDb, bad ? "NaN!" : (quiet ? "QUIET" : "LOUD"));
        }
        if (calibrate)
        {
            const double target = (pre.category == "Pluck" || pre.category == "Lead") ? 0.42 : 0.6;
            const double vol = juce::jlimit (-30.0, 6.0, -24.0 + juce::Decibels::gainToDecibels (target / std::max (peak, 1.0e-4)));
            calib->setProperty (pre.name, std::round (vol * 10.0) / 10.0);
        }
        if (writeWavs && (i % 50 == 0))
        {
            juce::File f (juce::String (argv[1]) + "/" + juce::String (i) + "_" + pre.name.replaceCharacter (' ', '_') + ".wav");
            f.deleteFile();
            juce::WavAudioFormat fmt;
            if (auto out = std::unique_ptr<juce::AudioFormatWriter> (fmt.createWriterFor (new juce::FileOutputStream (f), sr, 2, 16, {}, 0)))
                out->writeFromAudioSampleBuffer (rec, 0, rec.getNumSamples());
        }
        std::printf ("%s%4d %-9s %-24s rms %6.1f dB peak %.2f\n", "", i, pre.category.toRawUTF8(), pre.name.toRawUTF8(), rmsDb, peak);
    }

    if (calibrate)
        juce::File (juce::String (argv[2])).replaceWithText (juce::JSON::toString (juce::var (calib.get())));
    std::printf ("\nproblems: %d / %d\n", problems, n);
    std::printf ("realtime factor: %.1fx (single core)\n", audioSeconds * 1000.0 / totalCpu);

    // Editor smoke test: render UI to a PNG
    if (writeWavs)
    {
        proc.setCurrentProgram (0);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (WavesmithEditor::baseWidth, WavesmithEditor::baseHeight);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::String (argv[1]) + "/ui.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("ui snapshot written\n");
    }
    return problems > 0 ? 2 : 0;
}
