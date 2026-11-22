# PolyRhythm32 – AU/VST3-Plugin

Polyrhythmischer 6-Spur MIDI-Sequencer für macOS (Apple Silicon und Intel).

- 6 Spuren, Standard-Noten C1, C#1, F#1, G#1, B1, C2 (C3 = 60)
- Länge pro Spur 1/32 – 32/32, Rate pro Spur (1/32, 1/16, 1/8, 1/4, 1/16T, 1/8T)
- Zufallsgenerator für alle Spuren + RND-Button pro Spur (alles außer Noten; optional Noten im Bereich C0–C8)
- Rückgängig / Wiederherstellen
- MIDI-Loop-Export als Datei oder per Drag & Drop in die DAW
- Synchron zum Host-Transport, Standalone mit eigener Clock
- Virtueller MIDI-Port „PolyRhythm32 Out“ (unabhängig vom DAW-Routing)

## Download
Unter **Releases** → `polyrhythm32-v1.2.0` → `PolyRhythm32-macOS.zip`.
Installation: siehe `scripts/LIESMICH.txt` (liegt auch im ZIP).

Jeder Push auf den Branch `polyrhythm32` baut das Plugin über GitHub Actions neu und aktualisiert das Release.

## Aufbau
- `Source/PluginProcessor.*` – Sequencer-Engine (sample-genaues Timing aus der Host-PPQ-Position), Parameter, Zufall, MIDI-Export, Zustand
- `Source/PluginEditor.*` – Oberfläche (Step-Raster, Spur-Regler, Export/Drag)

## Selbst bauen
```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release -j 8
```
Benötigt Xcode-Kommandozeilenwerkzeuge und CMake ≥ 3.22. JUCE 8 wird automatisch geladen.
