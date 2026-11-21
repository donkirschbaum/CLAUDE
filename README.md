# Shruthi XT – AU/VST3-Plugin

Emulation des Mutable Instruments Shruthi-1 mit XT-Bedienfeld als Plugin für Ableton Live (macOS, Apple Silicon und Intel).

## Plugin bekommen
Jeder Push baut das Plugin automatisch über GitHub Actions:
1. Reiter **Actions** öffnen → letzten Lauf „Build macOS (AU + VST3)" wählen.
2. Unten unter **Artifacts** `ShruthiXT-macOS` herunterladen.
3. Anleitung in `INSTALLIEREN.txt` folgen.

Der erste Build dauert etwa 10–15 Minuten (JUCE wird mitgebaut).

## Aufbau
- `Source/ShruthiEngine.*` – Klangerzeugung (Firmware-Logik: Modmatrix, Hüllkurven, LFOs, Sequencer, Arpeggiator, 8-Bit-Oszillatoren, Filterboards)
- `Source/ShruthiBank.*`, `ShruthiBankData.h` – 128 Werksprogramme und Patch-Dekodierung (92-Byte-Format)
- `Source/ShruthiData.h` – Wavetables und Tabellen der Original-Firmware
- `Source/PluginProcessor.*` – Parameter (alle Patch-Werte automatisierbar), MIDI, Host-Tempo, Zustand
- `Source/PluginEditor.*` + `Resources/index.html` – Bedienoberfläche (WebView, identisch zur Web-Version)

## Selbst bauen (optional)
```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release -j 8
```
Benötigt Xcode-Kommandozeilenwerkzeuge und CMake ≥ 3.22.

## Presets
128 Werks-Presets der Shruthi-1-Firmware und 228 Community-Patches aus dem Mutable-Instruments-Forum (gesammelt in https://github.com/joegiralt/ambika-shruthi-instruments). Die Urheber stehen in den Kategorienamen.

## Lizenz
GPL v3.0 – siehe `LICENSE`. Werksdaten aus der Shruthi-1-Firmware von Émilie Gillet / Mutable Instruments.

## Weitere Projekte in diesem Repo
- [`Volta4/`](Volta4/) – **VOLTA·4**, semimodularer Patch-Drumsynth (AU/VST3, eigener Build „Build VOLTA-4 macOS“, Release `volta4-latest`)
