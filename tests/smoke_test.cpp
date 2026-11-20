// Kurzer Funktionstest: alle Presets laden, Noten spielen, auf Stille/NaN prüfen, State speichern/laden.
#include "../Source/PluginProcessor.h"
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    MikrokornProcessor p;
    p.setPlayConfigDetails (0, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    std::cout << "Presets: " << p.library.getNumPresets() << ", Werks-Samples: " << p.library.getFactorySamples().size() << "\n";

    int quiet = 0, bad = 0;
    juce::AudioBuffer<float> buf (2, 512);
    for (int i = 0; i < p.library.getNumFactoryPresets(); ++i)
    {
        p.loadPreset (i);
        double sum = 0; long cnt = 0;
        for (int b = 0; b < 140; ++b)
        {
            juce::MidiBuffer m;
            if (b == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 10);
            if (b == 120) m.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            p.processBlock (buf, m);
            for (int s = 0; s < 512; ++s) { float v = buf.getSample (0, s); if (! std::isfinite (v)) ++bad; sum += v * v; ++cnt; }
        }
        const double rms = std::sqrt (sum / cnt);
        if (rms < 0.005) { ++quiet; std::cout << "  leise: " << i + 1 << " " << p.library.getPreset (i).name << " rms=" << rms << "\n"; }
    }
    std::cout << "leise Presets: " << quiet << ", NaN-Samples: " << bad << "\n";

    // State-Roundtrip
    p.selectSlot (3); p.loadPreset (42);
    juce::MemoryBlock mb; p.getStateInformation (mb);
    MikrokornProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
    std::cout << "State: slot " << q.getCurrentSlot() << " preset " << q.getCurrentPreset() << " (erwartet 3 / 42), Name: " << q.getSlotName (3) << "\n";

    // Eigenes Preset speichern + löschen
    const int idx = p.saveCurrentAsPreset ("Smoke Test Preset");
    std::cout << "User-Preset gespeichert an Index " << idx << ", Anzahl jetzt " << p.library.getNumPresets() << "\n";
    std::cout << "Loeschen: " << (p.deleteCurrentPreset() ? "ok" : "FEHLER") << ", Anzahl " << p.library.getNumPresets() << "\n";
    return (bad == 0 && q.getCurrentPreset() == 42) ? 0 : 1;
}
