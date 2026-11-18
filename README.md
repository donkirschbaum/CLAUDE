# Anu – Anushri als Plugin (VST3 / CLAP / LV2 / AU)

**Anu** ist eine Emulation des **Mutable Instruments Anushri**: analoger Monosynth
mit eingebauter Drum Machine. Die Drum Machine erzeugt ihre Patterns generativ.
Das Plugin enthält den Synth-Teil, Arpeggiator, Step-Sequencer und die Drum Machine.
Alle Parameter lassen sich per MIDI steuern. Die Drums werden zusätzlich als MIDI-Noten ausgegeben.

> Der Name „Anu“ stammt aus der Original-Firmware (`namespace anu`). Das Projekt ist
> nicht mit Mutable Instruments verbunden. „Anushri“ ist ein Name von Mutable Instruments.

---

## Was 1:1 aus dem Original stammt

Die Anushri-Firmware ist Open Source (GPLv3, © 2012 Emilie Gillet). Ihr kompletter
digitaler Teil ist **bitgenau portiert**. Er rechnet mit denselben Festkomma-Operationen und
Lookup-Tabellen wie der ATmega644p und läuft mit den Original-Raten:
39 215,7 Hz Audio, 2 451 Hz Control-Rate.

| Teil | Status |
|---|---|
| ADSR-Hüllkurve (VCF), AD-Modulationshüllkurve, VCA-Hüllkurve mit „VCA Env“-Morph | Original-Code |
| LFO (8 Formen inkl. S&H, Bernoulli, Smooth Random, Noise; Clock-Sync bei Rate = 0) | Original-Code |
| Vibrato-LFO, Mod-Wheel → Vibrato/„Growl“ (Cutoff), Breath Controller | Original-Code |
| Glide, Legato-Modus, Velocity → VCF/VCA, Pitch Bend (±2 Halbtöne) | Original-Code |
| Arpeggiator (Up/Down/Up-Down/Random, 1–2 Oktaven, 6 Rhythmen, **Acidity** = Slides + Accents) | Original-Code |
| Step-Sequencer mit Aufnahme, Pausen, Haltebögen, Accent (Mod-Wheel), Slide (Pitch Bend), Transposition | Original-Code |
| Drum-Synth (BD/SD/HH, 5 Presets pro Instrument zum Morphen, Balance, Bandbreite/Lo-Fi) | Original-Code |
| Generative Drum-Map (3×3 Knoten, X/Y-Interpolation, Density, Zufallsvariation pro Takt) | Original-Code + Original-Daten |
| Swing-Clock (Shuffle-Groove-Template) | Original-Code |
| Original-MIDI-CC-Belegung, Drums per MIDI-Kanal 10, Pattern-Programmierung über Kanal 16 | Original-Verhalten |

Der **analoge** Teil (VCO, Filter, Fuzz, VCA) war im Original Hardware. Er ist hier als
DSP-Modell nachgebaut. Die Oszillatoren sind PolyBLEP-bandbegrenzt und laufen mit 2× Oversampling:

- **VCO**: Sägezahn und Puls, frei mischbar (MIX). PW-Regler (Mitte = Rechteck) mit PW-Modulation.
  Leichter analoger Drift.
- **DCO** (digitales Rechteck): Quelle für **lineare FM**, **Hard-Sync** und Sub.
- **Sub**: DCO, −1 Oktave oder −2 Oktaven (Flipflop am VCO).
- **VCF**: 12 dB/Okt. Multimode-Filter (LP/BP/HP), 18 Hz–22 kHz.
  Die Resonanz geht bis in die Selbstoszillation.
- **Fuzz**: schaltbar zwischen VCF und VCA, wie im Original.
- **VCA**.

## Bedienoberfläche

Das Layout folgt dem Anushri-Panel:

- **Oben – Analog-Sektion**: FM, VCO MOD, PW, PW MOD, SYNC · MIX, SUB, SUB SRC ·
  CUTOFF, RES, VCF ENV, VCF LFO, MODE · VOLUME, DRUMS, FUZZ.
  VCO MOD und PW MOD sind wie im Original Überblendregler mit Mittenraste:
  links = Hüllkurve, rechts = LFO.
- **Unten – 10 Soft-Potis mit Seitenwahl**, wie die drei Tasten am Original:
  - **SYNTH**: Glide, VCO Range (−12…+36 HT, mit Rastung), Detune, LFO Shape, LFO Rate,
    Attack, Decay, Sustain, Release, VCA Env
  - **KBD/CLK**: Tempo, Swing, Arp-Modus, Arp-Pattern, Acidity, DCO-Oktave, DCO Fine,
    Vibrato Rate, Mod-Wheel-Ziel, Velocity (VCF↔VCA)
  - **DRUMS**: Map X, BD/SD/HH Density, Balance, Map Y, BD/SD/HH Tone, Bandbreite.
    Dazu kommt ein 16-Step-Raster für feste Patterns (OVERRIDE).
  - **SETUP**: MIDI-Kanäle, Drum-MIDI-Out (Kanal, Noten, Notenlänge), Thru, Cutoff Bias,
    Keyboard Tracking, Legato
- **Sequencer**: REC, RUN/STOP, HOLD (bei Aufnahme: TIE), REST.
- **LEDs**: Gate, LFO, Beat. Daneben ein PANIC-Knopf (All Notes Off).

Maus: ziehen = drehen, **Shift** = fein, **Doppelklick** = Standardwert, Mausrad = Schritt.
**Rechtsklick** = MIDI Learn, **Shift+Rechtsklick** = Zuweisung löschen.

## MIDI

### Eingang
- **Synth** auf dem eingestellten Kanal. Standard ist Omni; Omni ignoriert den Drum-Kanal und Kanal 16.
- **Drums triggern**: Kanal 10, Noten 36 (BD), 38 (SD), 42 (HH). Noten und Kanal lassen sich im SETUP ändern.
- **Drum-Klang per CC** auf Kanal 10, CC 16–30 (Original-Belegung): für BD und SD je Pitch,
  Pitch-Decay, Pitch-Mod, Decay, Crunch und Level; für HH Pitch, Decay und Level.
- **Pattern programmieren** (Original-Funktion) auf Kanal 16:
  - F#/G#/A# (42/44/46) wählt BD/SD/HH.
  - Die weißen Tasten ab C2 (36) schalten die Steps 1–16.
  - C# (37) schaltet Override, D# (39) löscht das Pattern.
- CC 1 = Mod Wheel, CC 2 = Breath, CC 64 = Sustain/Hold, Pitch Bend, Aftertouch.
- In der Aufnahme setzt Mod Wheel > 64 einen Accent und Pitch Bend einen Slide auf den Step.
- Wenn der Sequencer läuft, transponieren gespielte Noten die Sequenz (Bezug C4).

### Parameter-CCs
Alle Parameter sind automatisierbar und per **MIDI Learn** belegbar. Ab Werk gelten die
Original-CCs von Anushri. Die Analog-Regler hatten im Original keine CCs; sie haben freie CCs bekommen:

| CC | Parameter | | CC | Parameter |
|---|---|---|---|---|
| 5 | Glide | | 50 | VCO Range |
| 7 | Volume | | 51 | VCO Detune |
| 16 / 17 | VCO Mod (Env / LFO) | | 52 | Vibrato Rate |
| 18 / 19 | PW Mod (Env / LFO) | | 53 | Mod-Wheel-Ziel |
| 20 | VCF Env Mod | | 54 | LFO Shape |
| 21 | VCF LFO Mod | | 55 / 76 | LFO Rate |
| 22 / 74 | Cutoff Bias | | 71 | Resonance |
| 23 | Cutoff Keyboard Tracking | | 80 | Cutoff |
| 24 / 73 | Attack | | 82 | VCF Mode |
| 25 / 75 | Decay | | 83 | Fuzz |
| 26 | Sustain | | 84 | Drum Level |
| 27 / 72 | Release | | 85 | FM |
| 28 | VCA Env | | 86 | Pulse Width |
| 29 / 30 | Velocity → VCF / VCA | | 87 | Saw/Square Mix |
| 31 | Legato | | 88 | Sub Level |
| 48 | DCO Range | | 89 | Sub Source |
| 49 | DCO Fine | | 90 | Sync |
| 102 / 103 | Drums Map X / Y | | 104–106 | BD / SD / HH Density |
| 107–109 | BD / SD / HH Tone | | 110 / 111 | Drums Balance / Bandwidth |
| 112 | Swing | | 113 / 114 / 115 | Arp Mode / Pattern / Acidity |

Tempo, Run/Stop, Rec, Hold usw. haben keinen festen CC. Sie lassen sich per MIDI Learn belegen.
Bei Run, Rec und Hold wirkt ein CC ≥ 64 als Tastendruck.

### Ausgang (MIDI Send)
- **Drums**: Jeder Schlag der Drum Machine geht als Note auf Kanal 10 raus: BD 36, SD 38, HH 42.
  Die Velocity folgt der Akzentstärke. Kanal, Notennummern und Notenlänge sind im SETUP einstellbar.
- **Arp/Sequencer**: Die erzeugten Noten gehen auf dem Out-Kanal raus. Slides werden
  überlappend gesendet, wie im Original.
- **MIDI Thru** ist optional.

Hinweis: MIDI-Ausgang von Instrument-Plugins geht z. B. in Reaper, Bitwig, Cubase und Ardour.
Ableton Live leitet MIDI aus Plugins nicht weiter.

## Sync / Tempo
- **Tempo 40–240 BPM**: interne Clock mit Swing, wie das Original.
- **Tempo ganz links („EXT“)**: Das Plugin folgt Tempo und Transport des Hosts.
  Play im Host startet Sequencer und Drums, Stop hält sie an.
  Die Steps liegen sample-genau auf der Host-Taktung.
  Steht der Host, läuft der Arpeggiator mit Host-Tempo frei weiter.
- **RUN/STOP** startet Sequencer und Drum Machine. Die Drums spielen, sobald eine Density > 0 ist
  oder OVERRIDE aktiv ist.

## Unterschiede zum Original
- Analog-Sektion als DSP-Modell, nicht als Schaltungssimulation.
- Kein externer Audio-Eingang, kein CV/Gate, kein Strummer-Easter-Egg, kein SysEx-Dump.
  Für Kalibrierung und Tuning gibt es im Plugin keinen Bedarf.
- HOLD hält Noten fest (Latch), auch Tasten, die erst danach gedrückt werden.
- Sequenz, Override-Patterns und MIDI-Learn-Zuweisungen werden im Projekt gespeichert.

## Installation
- **Windows**: `Anu.vst3` nach `C:\Program Files\Common Files\VST3\`, `Anu.clap` nach
  `C:\Program Files\Common Files\CLAP\`. Dazu gibt es `Anu.exe` als Standalone.
- **Linux**: `Anu.vst3` nach `~/.vst3/`, `Anu.clap` nach `~/.clap/`, `Anu.lv2` nach `~/.lv2/`.
- **macOS**: Mit `make macos` selbst bauen oder per GitHub Actions (siehe unten).
  Danach `~/Library/Audio/Plug-Ins/VST3|CLAP|Components/`.

## Selbst bauen
Abhängigkeit: [DPF](https://github.com/DISTRHO/DPF), liegt in `dpf/`.

```sh
make            # Linux (braucht libgl-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev)
make windows    # Windows-Cross-Build unter Linux (mingw-w64)
make macos      # auf einem Mac: Universal Binary (Intel + Apple Silicon)
make test       # Offline-Render- und Host-Sync-Tests
```

Bequemer geht es über `.github/workflows/build.yml`. Lade das Projekt in ein GitHub-Repo,
dann baut GitHub Actions Linux, Windows und macOS automatisch und stellt die Plugins als Download bereit.

## Lizenz
GPL-3.0-or-later. Grundlage ist die Anushri-Firmware von Emilie Gillet / Mutable Instruments (GPLv3).
DPF steht unter der ISC-Lizenz.
