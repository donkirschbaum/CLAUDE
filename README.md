# Drone Acht

Ein 8-stimmiger Drone-Synthesizer als Plugin für den Mac (VST3, AU und Standalone-App), inspiriert vom Aufbau der Lyra-8.

## Installieren

1. Lade `DroneAcht-macOS.zip` unter **Releases** herunter und entpacke die Datei.
2. Klicke im Ordner „Drone Acht“ mit der rechten Maustaste auf **Installieren.command** und wähle **Öffnen**. Bestätige die Rückfrage.
3. Starte deine DAW neu und lass sie nach Plug-ins suchen.

Das Skript kopiert die Plugins nach `~/Library/Audio/Plug-Ins/VST3` und `~/Library/Audio/Plug-Ins/Components` und hebt die Download-Sperre von macOS auf. Die Plugins sind nicht von Apple notarisiert, deshalb ist dieser Schritt nötig.

Von Hand geht es so:

```bash
cp -R "Drone Acht.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "Drone Acht.component" ~/Library/Audio/Plug-Ins/Components/
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Drone Acht.vst3" ~/Library/Audio/Plug-Ins/Components/"Drone Acht.component"
```

Drone Acht läuft auf Apple Silicon und Intel ab macOS 11.

- **Logic Pro / GarageBand**: Das AU erscheint als Instrument unter *Drone Acht*. Wenn nicht: Einstellungen → Plug-in-Manager → neu scannen.
- **Ableton Live**: Einstellungen → Plug-Ins → „VST3 Plug-In System-Ordner“ einschalten und neu scannen.
- **Bitwig, Reaper, Cubase**: Plugin-Ordner neu scannen.

## Klang

- **8 Stimmen** in 4 Paaren und 2 Quads. Jede Stimme ist ein Dreieck-Oszillator, den **Sharp** bis zum Rechteck verformt.
- **Kreuz-FM**: **Mod** pro Paar regelt die Stärke. Die Quelle wählst du pro Quad: das eigene Paar, das Nachbarpaar oder den LFO.
- **Hold** und **Pitch** pro Quad. Hold lässt die vier Stimmen dauerhaft klingen.
- **Hyper-LFO**: zwei LFOs, addiert (A+B) oder multipliziert (A×B), als Dreieck oder Rechteck.
- **Mod-Delay**: zwei kreuzgekoppelte Ping-Pong-Leitungen, deren Zeit der LFO moduliert.
- **Master**: Drive (Verzerrung), Vibrato, Volume und ein Oszilloskop.

## Zufall

| Knopf | Was passiert |
|---|---|
| **Zufall** | würfelt Stimmung, Paare, Quads, LFO und Delay neu |
| **Mutieren** | verändert alles nur ein kleines Stück, gut zum langsamen Weiterwandern |
| **Chaos** | wirft jeden Regler quer durch seinen Bereich (Lautstärke und Feedback bleiben gezähmt) |
| **Stimmung** | würfelt nur die Tonhöhen aller Stimmen und Quads |
| Würfel **Master** | Drive und Vibrato |
| Würfel **Hyper-LFO** | Raten, Verknüpfung und Form |
| Würfel **Mod-Delay** | Zeiten, Feedback, Modulation und Mix |
| Würfel **Quad A / B** | Hold, Pitch, Mod-Quelle, Hüllkurven und beide Paare des Quads |
| Würfel **Paar** | Stimmung, Mod und Sharp eines Paars |
| **Zurücksetzen** | stellt den Grundklang wieder her |

## Spielen

| Stimme | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| MIDI-Note | C3 | D3 | E3 | F3 | G3 | A3 | H3 (B3) | C4 |
| Computertaste | A | S | D | F | G | H | J | K |

Du kannst die Pads auch mit der Maus halten. Die MIDI-Note löst die Stimme nur aus. Die Tonhöhe kommt aus **Tune** und **Pitch**, wie beim Original.

Die Knöpfe ziehst du nach oben oder unten. Ein Doppelklick setzt einen Knopf auf seinen Grundwert.

## MIDI-Steuerung

Jeder Regler, jeder Schalter und jeder Zufallsknopf lässt sich per MIDI-CC steuern. Alle Parameter sind außerdem in der DAW automatisierbar.

- **Ab Werk** sind alle 57 Ziele auf die freien Controller CC 14–31, 70–90 und 102–119 gelegt (Tabelle unten). Der MIDI-Kanal ist egal.
- **MIDI Learn**: Klicke mit der rechten Maustaste auf einen Regler, Schalter oder Würfel und wähle **MIDI Learn**. Bewege dann einen Regler an deinem Controller. Das Element leuchtet, bis die Zuordnung steht. Esc bricht ab.
- **Zuordnung sehen**: Fahr mit der Maus über einen Knopf, dann steht dort die CC-Nummer. Bei Schaltern steht sie oben rechts, bei Zufallsknöpfen im Tooltip.
- **MIDI-Menü** oben rechts: Lernmodus abbrechen, Standard-Zuordnung laden oder alle Zuordnungen löschen.
- **Zufallsknöpfe** lösen aus, wenn der CC-Wert von unter 64 auf 64 oder mehr steigt. Taster am Controller funktionieren deshalb direkt.
- Die Zuordnung wird mit deinem DAW-Projekt gespeichert.

Tipp für Logic: Schicke die Controller-Daten auf die Spur mit Drone Acht. Logic reicht CCs an Software-Instrumente weiter.

### Standard-Zuordnung

| CC | Ziel |
|---|---|
| 14 | Volume |
| 15 | Drive |
| 16 | Vibrato |
| 17 | Vib-Rate |
| 18 | LFO Rate A |
| 19 | LFO Rate B |
| 20 | LFO Verknuepfung |
| 21 | LFO Form |
| 22 | Delay Zeit L |
| 23 | Delay Zeit R |
| 24 | Delay Feedback |
| 25 | Delay Mod |
| 26 | Delay Mix |
| 27 | Quad A Hold |
| 28 | Quad A Pitch |
| 29 | Quad A Mod-Quelle |
| 30 | Quad B Hold |
| 31 | Quad B Pitch |
| 70 | Quad B Mod-Quelle |
| 71 | Paar 1-2 Mod |
| 72 | Paar 1-2 Sharp |
| 73 | Paar 3-4 Mod |
| 74 | Paar 3-4 Sharp |
| 75 | Paar 5-6 Mod |
| 76 | Paar 5-6 Sharp |
| 77 | Paar 7-8 Mod |
| 78 | Paar 7-8 Sharp |
| 79 | Stimme 1 Tune |
| 80 | Stimme 1 Schnell |
| 81 | Stimme 2 Tune |
| 82 | Stimme 2 Schnell |
| 83 | Stimme 3 Tune |
| 84 | Stimme 3 Schnell |
| 85 | Stimme 4 Tune |
| 86 | Stimme 4 Schnell |
| 87 | Stimme 5 Tune |
| 88 | Stimme 5 Schnell |
| 89 | Stimme 6 Tune |
| 90 | Stimme 6 Schnell |
| 102 | Stimme 7 Tune |
| 103 | Stimme 7 Schnell |
| 104 | Stimme 8 Tune |
| 105 | Stimme 8 Schnell |
| 106 | Zufall: alles |
| 107 | Mutieren |
| 108 | Chaos |
| 109 | Zufall: Stimmung |
| 110 | Zufall: Master |
| 111 | Zufall: Hyper-LFO |
| 112 | Zufall: Delay |
| 113 | Zufall: Quad A |
| 114 | Zufall: Quad B |
| 115 | Zufall: Paar 1/2 |
| 116 | Zufall: Paar 3/4 |
| 117 | Zufall: Paar 5/6 |
| 118 | Zufall: Paar 7/8 |
| 119 | Zurücksetzen |

## Selbst bauen

```bash
xcode-select --install
brew install cmake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 8
```

Der erste Build lädt JUCE von GitHub. Danach liegen die Plugins automatisch in `~/Library/Audio/Plug-Ins`. Jeder Push auf den Branch `drone-acht` baut über GitHub Actions eine neue Mac-Version und legt sie unter **Releases** ab.

```
Source/DroneEngine.h       DSP-Kern: Oszillatoren, FM, LFO, Delay, Verzerrung
Source/PluginProcessor.*   Parameter, MIDI-Noten, CC-Zuordnung, Zufall, Zustand
Source/PluginEditor.*      Oberfläche: Knöpfe, Pads, Würfel, MIDI Learn, Oszilloskop
```

## Lizenz

JUCE steht unter AGPLv3 oder einer kommerziellen JUCE-Lizenz. Für eigene Nutzung und Open-Source-Weitergabe reicht AGPLv3.

Lyra-8 ist ein Produkt von SOMA Laboratory. Drone Acht ist eine eigenständige Umsetzung der Signalstruktur und steht in keiner Verbindung zu SOMA.
