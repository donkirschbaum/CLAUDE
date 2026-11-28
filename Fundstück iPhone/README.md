# Fundstück – Found-Sound Drums fürs iPhone (App + AUv3)

Nimm beliebige Geräusche auf (Klopfen, Beatbox, Schlüssel, Tassen …) – Fundstück findet die
einzelnen Schläge, erkennt, welcher sich als Kick, Snare, Hat usw. eignet, formt daraus
druckvolle Drum-Sounds und baut einen passenden Groove dazu. Das AUv3-Plugin spielt die
Kits in AUM, GarageBand, Cubasis, Logic for iPad, Loopy Pro & Co.

## Funktionen

**App**
- Aufnahme übers Mikro (bis 30 s) oder Audiodatei importieren (bis 60 s)
- Automatische Transienten-Erkennung (Spectral Flux + Energieanstieg), Marker in der Wellenform
- Klassifizierung nach Frequenzschwerpunkt, Bassanteil, Höhenanteil, Rauschigkeit, Ausklingzeit
- Sounddesign pro Rolle: Pitch, Filter, Clap-Smear, Transienten-Boost, Sättigung und eine
  synthetische Verstärkungsschicht (Sub-Sweep für Kick, Rauschen für Snare/Hats, Ton für Toms)
- Regler **Empfindlichkeit** (wie viele Schläge erkannt werden) und **Charakter** (roh → fett)
- **Neu würfeln**: andere Zuordnung/Variation aus derselben Aufnahme; „Groove halten" behält das Pattern
- 8 Pads (Level, Tune ±24, Decay, Pan), 16-Step-Sequencer (Tippen: an → Ghost-Note → aus)
- Groove-Generator: Boom Bap, House, Trap, Broken – mit Dichte, Swing, Drive, Tempo
- Kits speichern (landen in der App Group → sofort im Plugin) und als 24-bit-WAVs exportieren

**AUv3-Plugin (Instrument, `aumu` / `fdsk` / `Fdst`)**
- MIDI-Noten nach GM: 36 Kick, 38 Snare, 39 Clap, 42 Hat, 46 Open Hat, 45 Tom Lo, 48 Tom Hi, 37 Perc
  (alternativ chromatisch ab Note 60)
- Interner Sequencer läuft synchron zu Host-Tempo und -Transport (abschaltbar)
- 30 automatisierbare Parameter, Kit + Pattern werden im Host-Projekt gespeichert
- Kits aus der App per Menü laden, gleiche Oberfläche wie in der App

## So baust du es (≈ 5 Minuten)

Voraussetzungen: Mac mit Xcode 15 oder neuer, iPhone mit iOS 16+, (kostenlose) Apple-ID.

1. XcodeGen installieren: `brew install xcodegen`
2. Im Ordner `Fundstueck/`: `xcodegen generate` → öffnet sich als `Fundstueck.xcodeproj`
3. **Bundle-IDs & App Group anpassen** (müssen dir gehören):
   - In `project.yml` `com.example.fundstueck` / `com.example.fundstueck.au` z. B. durch
     `de.deinname.fundstueck` / `de.deinname.fundstueck.au` ersetzen
   - App Group `group.com.example.fundstueck` an **drei** Stellen gleich ändern:
     `App/Fundstueck.entitlements`, `AUExtension/FundstueckAU.entitlements`, `Shared/Swift/KitStore.swift`
   - `DEVELOPMENT_TEAM` eintragen oder in Xcode bei beiden Targets unter *Signing & Capabilities*
     dein Team wählen. Dann `xcodegen generate` nochmal ausführen.
4. iPhone anschließen, Schema **Fundstueck** wählen, ▶︎ Run.
5. App einmal öffnen (registriert das Plugin), dann im Host z. B. AUM: *Instrument → Fundstück: Drums*.

> Ohne bezahlten Developer-Account funktionieren App Groups mit „Personal Team“ meist auch;
> falls Xcode meckert, läuft die App trotzdem – nur sieht das Plugin dann die Kits nicht
> (es startet mit dem Demo-Kit, Projekte speichern ihr Kit trotzdem).

## Ohne Mac: Cloud-Build + Sideloadly (Windows)

1. Kostenloses GitHub-Konto anlegen, neues Repository erstellen (öffentlich = unbegrenzte
   Build-Minuten) und den Inhalt dieses Ordners hochladen (inkl. `.github/`).
2. Der Workflow *Build Fundstück iPhone (IPA)* startet automatisch. Nach ~10 Min. unter **Actions → letzter Lauf →
   Artifacts** die Datei `Fundstueck-ipa` herunterladen und entpacken.
3. Auf dem PC **iTunes von apple.com** (nicht aus dem Microsoft Store) und **Sideloadly** installieren,
   iPhone per USB anschließen, `Fundstueck.ipa` reinziehen, Apple-ID eingeben, *Start*.
4. iPhone: *Einstellungen → Allgemein → VPN & Geräteverwaltung →* deiner Apple-ID vertrauen,
   bei iOS 16+ zusätzlich *Datenschutz & Sicherheit → Entwicklermodus* einschalten.

Mit kostenloser Apple-ID läuft die App 7 Tage, dann in Sideloadly erneut signieren (Daten bleiben).
Mit bezahltem Developer-Account (99 $/Jahr) hält sie 1 Jahr oder geht per TestFlight aufs iPhone.

## Aufbau

```
Shared/DSP/       C-Kern: Analyse, Sounddesign, Echtzeit-Engine, Kit-Format (.fskit)
Shared/Swift/     Engine-Wrapper, Kit-Speicher (App Group), gemeinsame SwiftUI-Oberfläche
App/              App: Aufnahme, Import, Kit bauen, speichern, exportieren
AUExtension/      AUv3: AUAudioUnit (MIDI, Host-Sync, Parameter, State) + View-Controller
Tests/            DSP-Test, läuft auf jedem Mac/Linux: make -C Tests
```

Die Render-Pfade allozieren nicht und nehmen keine Locks (Kit-Wechsel per Try-Lock, Pad-Taps
über einen Lock-freien Ringpuffer), damit es im Host nicht knackst.

## Tipps für gute Kits
- Einzelne, klar getrennte Geräusche mit kurzer Pause dazwischen funktionieren am besten.
- Etwas Tiefes (Faust auf Tisch, „Bum“), etwas Zischendes („ts“, Papier), etwas Knackiges
  (Klatschen, „ka“) – dann bekommt jede Rolle einen passenden Kandidaten.
- Zu wenig erkannt? Empfindlichkeit hoch. Zu viel Müll? Runter.
