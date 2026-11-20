# MIKROKORN

Granularer Lo-Fi-Sampler als **VST3- und AU-Plugin für macOS** (Ableton Live, Logic, Bitwig …) – inspiriert von kleinen Hardware-Granular-Samplern.

## ⬇️ Download

**[Releases öffnen](https://github.com/donkirschbaum/CLAUDE/releases) → neuestes „MIKROKORN v…“** – dort den `…-macOS-Installer.pkg` laden, Rechtsklick → „Öffnen“, durchklicken. Danach in Ableton unter *Einstellungen → Plug-Ins* neu scannen.

Ausführliche Anleitung: [docs/INSTALL.txt](docs/INSTALL.txt)

## Features

- **Granular-Engine:** Grain-Größe, Shift (Tempo/Richtung des Grain-Fensters, 0 = Freeze), Smooth (hart/klickig bis weich)
- **Lo-Fi:** Bit-Crush (16 → 1 Bit), Samplerate-Reduktion
- **Sample:** Pitch, Start/Ende (auch per Maus in der Wellenform), Loop, Reverse (Ende vor Start = rückwärts)
- **Hüllkurve & Spielweise:** Attack, Release, Legato, Hold, monophon mit Notenspeicher, Pitchbend ±2
- **Ausgang:** Lowpass-Filter mit Resonanz, Drive, Volume
- **250 Werks-Presets** in 10 Kategorien, durchschaltbar mit ◀ ▶ (auch als Host-Presets)
- **Sample-Library:** 16 Werks-Samples (Clean Break, Aah, Warm Pad, Tape Keys, FM Bell, Acid Line, …) + eigene Samples per Drag & Drop
- **Eigene Presets:** Sample + Einstellungen speichern, erscheinen unter „Eigene Presets“
- **6 Slots** für schnellen Wechsel zwischen Sounds

Eigene Daten liegen in `~/Music/MIKROKORN/Samples` und `~/Music/MIKROKORN/Presets`.

## Browser-Version

`web/index.html` ist die spielbare Browser-Version (gleiche Engine, gleiche Presets). Die Werks-Samples und Presets des Plugins werden beim Build aus dieser Datei erzeugt (`tools/gen_assets.js`), damit beide gleich klingen.

## Selbst bauen

```bash
node tools/gen_assets.js
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
```

Jeder Push auf `main` baut automatisch per GitHub Actions auf macOS und veröffentlicht ein Release.

## Lizenz

Gebaut mit [JUCE](https://juce.com) (AGPLv3). Dieser Quellcode steht ebenfalls unter der **GNU AGPLv3**.
