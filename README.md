# Shaker Lab

4-Spur-Shaker-Sequencer als Plugin für macOS (VST3 + Audio Unit, Universal: Apple Silicon + Intel) – mit Polyrhythmen, Polymetern, Tempo pro Spur, ADSR, Sample-Editor und einem Zufallsgenerator, der passende Samples und Spuren zusammenstellt.

## Download & Installation (Mac)

1. Unter **Releases** die neueste „Shaker Lab“-Version öffnen, `ShakerLab-macOS.zip` herunterladen und entpacken.
2. **Rechtsklick auf `Installieren.command` → Öffnen** (beim ersten Mal fragt macOS nach, weil das Skript nicht von einem registrierten Apple-Entwickler stammt → „Öffnen“ bestätigen).
   Das Skript kopiert die Plugins nach `~/Library/Audio/Plug-Ins/VST3` und `~/Library/Audio/Plug-Ins/Components` und entfernt die Download-Quarantäne.
3. Ableton neu starten. Das Plugin heißt **Shaker Lab** und liegt unter *Plug-Ins → Don Kirschbaum*.
   Falls es fehlt: *Einstellungen → Plug-Ins →* „VST3-Plug-In-Ordner verwenden“ bzw. „Audio Units verwenden“ einschalten und **Neu scannen**.

Manuell geht es auch: die beiden Bundles in die genannten Ordner kopieren und im Terminal
`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Shaker Lab.vst3" ~/Library/Audio/Plug-Ins/Components/"Shaker Lab.component"` ausführen.

## Bedienung

- **Ableton-Transport** startet und stoppt den Sequencer, das Tempo kommt vom Projekt. Ohne laufenden Host spielt der **Play**-Knopf mit dem internen Tempo (zum Ausprobieren).
- **Zufall**: wählt vier zueinander passende Samples (Rollen *Main, Akzent, Textur, Farbe*) und baut passende Spuren. Setzt Chaos, Evolution, Pitch-Drift, Dichte, Ghosts, Raum und Raumgröße auf 0.
  **Pattern** würfelt nur die Spuren, **Samples** nur die Samples, **Mutieren** verändert einzelne Schritte.
- **Schritte**: antippen = an/aus, hoch/runter ziehen = Lautstärke des Schritts, seitlich weiterziehen = über mehrere Schritte malen.
- **PM / PR** pro Spur: Polymeter (jeder Schritt = 1/16) oder Polyrhythmus (alle Schritte teilen sich einen Takt). Daneben das **Tempo pro Spur** (/4 … x4).
- Klick auf eine Spur zeigt unten ihre Regler: **Sound** (Gain, Tune, Finetune, ADSR, Klangformung), **Mix**, **Zufall**.
- **Edit** öffnet den Sample-Editor: Sample wählen, eigene Dateien laden (WAV, AIFF, FLAC, MP3, M4A), Start- und Endpunkt ziehen, zoomen, vorhören.
- Alles – Pattern, Samples, Zuschnitte, eigene Sample-Pfade, Reglerstände – wird im Ableton-Projekt gespeichert. Alle Regler lassen sich automatisieren.

## Selbst bauen

```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target ShakerLab_VST3 ShakerLab_AU ShakerLab_Standalone
```

JUCE wird automatisch geladen (oder `-DJUCE_DIR=/pfad/zu/JUCE`). Jeder Push auf den Branch `shaker-lab` baut über GitHub Actions ein neues Release.
