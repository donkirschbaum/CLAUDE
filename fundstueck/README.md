# Fundstück

**Alltagsgeräusche rein, Beats raus.** Ein Instrument-Plugin (VST3 / Audio Unit / eigenständige App) für macOS:
Du lädst eine Aufnahme (Klopfen, Schlüssel, Gläser, Stimme …). Fundstück zerschneidet sie automatisch in einzelne Treffer,
sortiert sie als tief / mittel / hell / tonal und baut daraus Drums und eine Melodie.

## Installieren (Mac)

1. Unter **[Releases](https://github.com/donkirschbaum/CLAUDE/releases)** (Einträge „Fundstück …“) die Datei `Fundstueck-…-macOS.pkg` herunterladen.
2. **Rechtsklick → Öffnen** (das Plugin ist nicht bei Apple notarisiert, deshalb fragt macOS nach).
   Falls macOS blockiert: *Systemeinstellungen → Datenschutz & Sicherheit → „Dennoch öffnen“*.
3. Durchklicken. Installiert wird:
   - `/Library/Audio/Plug-Ins/VST3/Fundstueck.vst3` (Ableton, Reaper, Bitwig, Cubase …)
   - `/Library/Audio/Plug-Ins/Components/Fundstueck.component` (Logic, GarageBand)
   - `/Programme/Fundstueck.app` (ohne DAW spielbar)
4. DAW neu starten, als **Instrument** auf eine Spur laden.

<details><summary>Manuell per ZIP statt Installer</summary>

ZIP entpacken, Dateien in die Ordner oben kopieren (oder `~/Library/Audio/Plug-Ins/...`), dann im Terminal:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Fundstueck.vst3 ~/Library/Audio/Plug-Ins/Components/Fundstueck.component
```
</details>

## Bedienung

| Bereich | Was passiert |
|---|---|
| **1 Geräusche sammeln** | *Demo-Geräusche* oder *Datei laden…* (WAV, AIFF, MP3, M4A/Sprachmemo, FLAC, CAF), auch per Drag & Drop. *Empfindlichkeit* bestimmt, wie viele Schnipsel erkannt werden. Klick auf Wellenform oder Schnipsel = vorhören. |
| **2 Groove** | Stile Boom Bap, House, Trap, Afro, Lo-Fi, Breakbeat · *Dichte* · Tonart/Tonleiter · Würfeln (alles, nur Drums, nur Melodie, nur Sounds). |
| **3 Sequencer** | 16 Schritte × 5 Spuren. Spurnamen anklicken → Schnipsel anklicken = Sound tauschen. ▲▼ blättert durch die Schnipsel. Hats: mehrfach klicken = Wirbel (2×/3×). Melodie: Mausrad, Shift-Klick (hoch), Rechtsklick (runter). |
| **Kopfzeile** | Play (intern) · Tempo · Swing · Feel (menschliche Ungenauigkeit) · Raum · Tape · Sub-Kick · Master |

- **Host-Sync:** Standardmäßig aus – Fundstück läuft dann mit eigenem Tempo über den Play-Knopf.
  Mit **HOST-SYNC** an folgt es Tempo und Transport der DAW, die Tempo-Anzeige zeigt das Host-BPM.
- **MIDI:** C1 (36) Kick · D1/E1 (38/40) Snare · F#1/G#1/A#1 (42/44/46) Hat · ungerade Töne 37–47 Perc · ab C2 (48) chromatisch die Melodie-Stimme.
- Das Projekt speichert Aufnahme, Pattern und alle Einstellungen mit (bis 2 Minuten Audio).

## Direkt aufnehmen (● Aufnehmen)

Der Knopf nimmt den Audio-Eingang des Plugins auf (max. 2 Minuten), Stille am Rand wird abgeschnitten,
danach wird sofort analysiert. Die **IN**-Anzeige daneben zeigt, ob Signal ankommt.

**Ableton Live** (Instrumente bekommen Audio nur über den Sidechain):
1. Eine **Audiospur** anlegen, Eingang = dein Mikrofon, Monitor auf **In** stellen (oder Spur scharf schalten bei *Auto*).
2. Im Fundstück-Gerät oben links das kleine Dreieck/**Sidechain**-Symbol aufklappen und bei *Audio From*
   die Mikrofon-Spur wählen.
3. In Fundstück auf **● Aufnehmen** – klopfen, klappern, summen – **■ Stopp**.

**Logic:** Im Plugin-Fenster oben rechts *Side Chain* → Mikrofon-Spur oder Eingang wählen.
**Eigenständige App:** nimmt direkt das Mikrofon auf (Eingang unter *Options → Audio/MIDI Settings*).
Beim ersten Start fragt macOS nach der Mikrofon-Erlaubnis.

## Selbst bauen

```bash
cd fundstueck
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release -j
```

Jeder Push mit Änderungen in `fundstueck/` baut auf GitHub Actions automatisch ein neues Release (siehe `.github/workflows/build-fundstueck.yml`).

## Lizenz

Basiert auf [JUCE](https://juce.com) (AGPLv3). Fundstück steht daher ebenfalls unter der
[GNU AGPLv3](https://www.gnu.org/licenses/agpl-3.0.html).
