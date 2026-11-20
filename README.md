# Grids 8

Topografischer 8-Spur Drum-Sequencer nach dem Vorbild von Mutable Instruments Grids – als **AU/VST3-Plugin für Ableton Live (macOS)**.

## Download

Fertige Builds: **[Releases](https://github.com/donkirschbaum/CLAUDE/releases)** → `Grids8-macOS.zip` (Universal: Apple Silicon + Intel).
Jeder Push auf den Branch `grids8` baut außerdem ein Artefakt unter *Actions*.

Installation: Zip entpacken, im Terminal `bash ~/Downloads/Grids8/Installieren.command`, Ableton neu starten. Details in [`scripts/LIESMICH.txt`](scripts/LIESMICH.txt).

## Funktionen

- **MIDI-Generator**: interne Sounds sind aus, nur zum Vorhören per Knopf „Sound“
- 8 Spuren (Namen per Doppelklick änderbar), Sektion „Sequencer“ einklappbar
- je Spur: X/Y-Feld, Dichte, Chaos, Länge (Polymetrie), Mute/Solo, Würfel
- je Spur eigene Modulation von X/Y/Dichte: LFO (Sinus, Dreieck, Säge, S&H, Drift) oder Hüllkurve (pro Zyklus oder ausgelöst durch Master-XY-Bewegung) mit Attack/Decay und Versatz (Phase/Verzögerung)
- je Spur Reaktion auf Master-XY: Anteil (auch negativ) und Drehung – so bewegt jede Spur sich beim Master-XY anders
- Master X/Y verschiebt alle Spuren, Master-Dichte, -Chaos, -Modulation
- Zufall: Alles würfeln, Mutieren, Neue Map, XY, Dichten, Chaos, Mutes, Sound, Modulation, XY-Glide, Längen, Tempo/Swing, Evolve, Auto-Fill, Humanize
- folgt Tempo, Transport und Songposition des Hosts; alle Regler automatisierbar
- **8 Slots nehmen ständig auf** (letzte 1/2/4/8/16 Takte, pro Slot einstellbar), „Halten“ friert ein
- Slot-Chip **„Drum Rack ziehen“** direkt auf eine Ableton-MIDI-Spur ziehen (Noten C1–G1 = Pads 1–8, Modulation als CC-Hüllkurven), „8 Spuren ziehen“ für getrennte Spuren
- MIDI-Ausgang (Kanal 10 oder 1–8 je Spur)

## Selbst bauen

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release -j
```

JUCE 8 wird automatisch geladen. Hinweis: JUCE steht unter AGPLv3/kommerzieller Lizenz.
