# VOLTA·4 – Patch-Drumsynth (AU/VST3 für macOS)

Semimodularer Drum-Synth nach dem Pulsar-23-Prinzip: vier Stimmen (Kick, Snare, Bass, Hats), 4-Spur-Gate-Sequenzer mit Mute/Solo und Polymetrik, Patchfeld mit Takt-, Sequenzer-, LFO-, Zufalls-, Teiler- und Hüllkurven-Ausgängen, Echo und Kompressor. 204 Presets in 12 Stilen, globaler Zufall, Rückgängig/Wiederholen.

## Plugin bekommen
Jeder Push in `Volta4/` baut das Plugin über GitHub Actions:
1. Reiter **Actions** → Lauf „Build VOLTA-4 macOS (AU + VST3)“ öffnen, unten unter **Artifacts** `VOLTA-4-macOS` laden – oder unter **Releases** `volta4-latest`.
2. Anleitung in `INSTALLIEREN.txt` folgen.

Der erste Build dauert etwa 10–15 Minuten (JUCE wird mitgebaut).

## Funktionen im Plugin
- **HOST**-Knopf neben BPM: Tempo und Transport kommen von der DAW (Ableton), schritt- und samplegenau an der Song-Position.
- **Alle Parameter MIDI-fähig**: 118 automatisierbare Parameter, Standard-CC-Belegung, eigenes MIDI-Learn, MIDI-Noten für Probeschläge und Bass-Tonhöhe.
- Kabel, Preset und CC-Belegung werden im DAW-Projekt gespeichert.

## Aufbau
- `Source/VoltaEngine.*` – Klangerzeugung und Patch-Logik (C++-Port der Web-Audio-Version)
- `Source/Params.h` – Parameterliste mit Standard-CCs
- `Source/PluginProcessor.*` – Parameter, Host-Sync, MIDI, Zustand
- `Source/PluginEditor.*` + `Resources/index.html` – Oberfläche (WebView, dieselbe Datei wie die Web-Version)

## Selbst bauen (optional)
```
cd Volta4
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release -j 8
```
Benötigt Xcode-Kommandozeilenwerkzeuge und CMake ≥ 3.22.

Eigenständiges Instrument, inspiriert vom Arbeitsprinzip des SOMA Pulsar-23; kein Nachbau von dessen Schaltung oder Panel.
