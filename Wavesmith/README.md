# Wavesmith – Wavetable-Synthesizer (AU / VST3)

Eigenständiger Wavetable-Synth für Ableton Live und andere DAWs auf macOS (Apple Silicon und Intel), mit **1000 Werks-Presets** in den Kategorien Bass (300), Lead (250), Pluck (225) und Sequence (225).

## Download
Jeder Push in `Wavesmith/` baut das Plugin automatisch über GitHub Actions und veröffentlicht es als Release **`wavesmith-latest`**:

- `Wavesmith-Installer.pkg` – installiert AU + VST3
- `Wavesmith-macOS.zip` – AU, VST3 und Standalone-App zum manuellen Kopieren

Wie du es installierst, steht in [`INSTALLIEREN.txt`](INSTALLIEREN.txt).

## Klangerzeugung
- **2 Wavetable-Oszillatoren** mit 16 Tabellen à 32 Frames, stufenlosem Morphing (Position) und Mip-Mapping gegen Aliasing
- **Unison** bis 8 Stimmen pro Oszillator mit Detune und Stereo-Spread
- **FM** von Osc B auf Osc A, **Sub-Oszillator** (Sinus/Dreieck/Rechteck), **Rauschen** mit Farbe
- **Filter**: Low 12/24, Band, High, Notch, mit Drive, Hüllkurve, Keytracking und Velocity
- **2 Hüllkurven** (Amp und Mod), **2 LFOs** (7 Formen, Tempo-Sync, Retrigger)
- **Modulationsmatrix** mit 6 Slots (10 Quellen, 19 Ziele)
- **16-Step-Sequencer** als Modulationsquelle (Rate, Länge, Glättung)
- **Arpeggiator** (Up, Down, Up/Down, Random, As Played; 1–4 Oktaven, Gate)
- Poly (bis 16 Stimmen), Mono und Legato mit Glide
- **Effekte**: Distortion, Chorus, Ping-Pong-Delay (tempo-synchron), Reverb
- Skalierbare Oberfläche, Bildschirmtastatur und eigene User-Presets

## Aufbau
- `Source/Wavetables.*` – erzeugt die Wavetables (additiv/FFT, band-limitiert)
- `Source/Voice.*` – Stimme: Oszillatoren, Filter, Hüllkurven, LFOs, Modulation
- `Source/SynthEngine.*` – Stimmenverwaltung, MIDI, Arpeggiator, Step-Sequencer, Clock
- `Source/Effects.*` – Effektkette
- `Source/PresetManager.*` – Werks- und User-Presets
- `Source/PluginProcessor.*`, `Source/PluginEditor.*` – Plugin und Oberfläche
- `tools/generate_presets.py` – erzeugt die 1000 Presets (`Resources/factory_presets.json`)
- `tests/PresetTest.cpp` – Headless-Test: spielt jedes Preset an und prüft Pegel, NaN und CPU

## Selbst bauen (optional)
```
cd Wavesmith
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target Wavesmith_AU Wavesmith_VST3 -j 8
```
Dafür brauchst du die Xcode-Kommandozeilenwerkzeuge und CMake ≥ 3.22. JUCE wird automatisch geladen.

Wenn du die Presets änderst: `python3 tools/generate_presets.py Resources/factory_presets.json`

## Lizenz
Eigener Code. Er nutzt das [JUCE-Framework](https://juce.com) (AGPLv3 bzw. JUCE-Lizenz).
