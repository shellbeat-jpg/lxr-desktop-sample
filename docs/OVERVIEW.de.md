# Übersicht: XT-Firmware für LXR-02

[English](OVERVIEW.md)

Alle Neuerungen gegenüber der Erica-Synths-Firmware. **[OE]** = aus OpenElixir-02 v1.2 (Moritz
Waser), alles andere aus XT.
„Global“ = gespeichert mit SAVE → Global und in der Projektdatei. NRPN n = Parameter 128 + n
(errechnet, nicht am Gerät nachgemessen). Details: [SAMPLES.md](SAMPLES.md).

## Voices

| Funktion | Beschreibung | UI | Speicherort | MIDI |
|---|---|---|---|---|
| Engines [OE] | Jede Voice zur Engine frei zuweisbar | VOICE (SHIFT + KIT) | Kit | NRPN 98 |
| Samples | Max. 31 WAVs, 5,9 s Gesamtlänge (mit `BIGFLASH` ≈ 17,8 s). Parameter Start und Länge in ms mit Fade, Loops mit Crossfade, Declick beim Retrigger. | VOICE (OSC `WAV`, `STA`, `LEN`) | Kit, Samples im Flash | CC 2–5, 7, 8 (`WAV` / Voice 1–6), NRPN 100–105 (`STA` / Voice 1–6), NRPN 93, 94, 97 (`LEN` / Voice 1–3) |
| User-Waves | Bis zu 8 WAVs aus `SAMPLES/WAVES` als Single-Cycle (1024 Punkte): Oszillator-Wellenformen `W1`…`W8` (feste Werte nach `S1`…`S31`) und LFO-Formen `W1`…`W8` (durchlaufend) / `W1.`…`W8.` (One-Shot); eigene Nummerierung. | VOICE (OSC `WAV`, LFO `WAV`) | Kit (Nummer), Waves im Flash | CC 2–5, 7, 8 (`WAV` / Voice 1–6), NRPN 27–32 (LFO `WAV` / Voice 1–6) |
| Choke | Voices einer Choke-Gruppe (A, B, C) blenden sich gegenseitig aus (≈ 2 ms). | VOICE (AEG `CHK`) | Kit | NRPN 99 |
| Track 7 | Eigene Wellenform bzw. eigenes Sample, Tonhöhe, Sends und Panorama für Track 7 auf der gemeinsamen Voice von Slot 6, z. B. Closed-/Open-Hat-Samples, nur eines davon in den Hall. | VOICE 7 (OSC `WAV`, `COA`; AEG `SNA`, `SNB`; MIX `PAN`) | Kit | – |
| LFO 2 | Ein zweiter LFO pro Voice und ein eigener LFO für Track 7 (wirkt, solange Track 7 klingt, statt des LFO von Voice 6). Test. | VOICE 1-7, LFO-Taste auf dem 2. Bildschirm erneut drücken | Kit | – |
| OSX | Einzelne Voices von der globalen Samplerate (`SR` / `0sr`) ausnehmen. | PERF (`OSX`, an/aus für die aktive Voice) | Kit | – |
| FX presets | Liste für FX1/FX2: 17 Werks-Presets (Delay, Distortion, Kompressor, Ring-Mod) und 8 eigene pro Projekt; Laden/Speichern per Encoder-Klick. | FX-Setup-Seite (`PRS`) | eigene: Projekt (`FXPRESET.BIN`) | – |
| Cue | Vorhören auf St2 (vor dem Fader, auch gemutete Tracks); `CMX` Cue/Main-Mix, `CVL` Pegel. | SHIFT + Step 1–7, PERF (`CMX`, `CVL`) | Global (`CMX`, `CVL`) | – |
| Schnelle Rolls | Roll-Raten `24`, `32`, `48` (1/16-Triolen, 1/32, 1/32-Triolen). | PERF (`ROL`) | – | – |
| Step-Akzent | Akzent pro Step, `ACA` = Stärke (+0,1 … +6 dB). | Step-Seite (`ACC`), CONFIG 2 (`ACA`) | Pattern (`ACC`), global (`ACA`) | – |
| Zufalls-Voice | SHIFT + Encoder-Klick auf einer Voice-Seite variiert die aktive Voice um `RND`; Lautstärke, Pan, Ausgang, Sends, LFO und Engine bleiben. | Voice-Seiten, PERF (`RND`) | Global (`RND`) | – |
| Zufallsoktave | Jeder Trigger spielt eine zufällige Oktave in einem Bereich (`+-1` … `-3`); pro Step ersetzt `OCT` sie (`vce` = wie Voice). Test. | VOICE (OSC `OCT`), Step-Seite (`OCT`) | Kit, Pattern (Step) | – |
| Start, Loop, Steps kopieren | `Start` pro Track (LAST-STEP-Seite); Loop pro Track (`SHIFT + PROJECT`, auch rückwärts); gewählte Steps kopieren/einfügen (`COPY`); Bereich duplizieren (`SHIFT + MORPH`); Undo/Redo (`COPY + ‹ / ›`). Test. | Sequencer | Pattern (Start) | – |
| Gates | Step halten, späteren drücken: der erste hält bis zum Ende des zweiten (AEG voll, MIDI-Note-Off am Ende). Test. | Sequencer | Pattern | – |
| Vorbereitete Steps | Voice-Taste + Step markiert einen Step; solange er aus ist, blinkt er im Takt. Test. | Sequencer | Pattern | – |
| WAV-Recorder | Nimmt St1 auf die SD-Karte auf (`REC/RECnnn.WAV`, 16 Bit Stereo): 1, 2 oder 4 Durchläufe des Patterns oder bis Stop; Start mit PLAY oder beim nächsten Pattern-Start. | PERF, letzter Screen (`REC`, `RST`, Encoder-Klick) | Global | – |
| Modal-Engine | Neue Engine: Physical Modelling einer angeschlagenen Membran, eines Stabs, einer Saite, Glocke, Platte oder eines Glases (`MDL`, `DMP`, `POS`, `INH`, `HRD`, `MAT`, `EXC`). Test. | VOICE (SHIFT + KIT, `modal`) | Kit | NRPN 98 |
| FM2-Engine | 2-Operator-FM-Percussion: Ratio `RAT`, Index `IDX` mit eigenem Abklingen `IDC`, Feedback `FBK`. Test. | VOICE (SHIFT + KIT, `fm2`) | Kit | NRPN 98 |
| Wavetable-Engine | Durchfährt Sinus › Dreieck › Rechteck › Sägezahn, ein Set aus den User-Waves oder 4 eigene Sets aus `SAMPLES/TABLES` (Serum, Vital, WaveEdit); ausgerichtete Frames, echte Zwischenwellen, höhenbegrenzt; `SWP` Sweep durch den Pitch-EG, `SCN` weich, stufig oder vor und zurück. Test. | VOICE (SHIFT + KIT, `wavetable`) | Kit; Sets: Sample-Speicher | NRPN 98 |
| Vox-Engine | Physikalisches Modell einer Stimme: Stimmlippen oder Vogel-Syrinx in einen Vokaltrakt mit Nasenabzweig; Vokal-Morph `VOW` (u … y), `LEN`, `NAS`, Konsonant-Burst `CNS`, `TNS`, `BRE`, `SRC`. Test. | VOICE (SHIFT + KIT, `vox`) | Kit | NRPN 98 |

## Sequencer / Tracks

| Funktion | Beschreibung | UI | Speicherort | MIDI |
|---|---|---|---|---|
| Lock Trigger | Lock Trigger für Automationen über Velocity 0 | STEP (`VEL`) | Pattern | – |
| Pattern-Mode | Pattern wechseln sofort zum nächsten Step | CONFIG (`IPS`) | Global | – |
| Raster pro Track | `1/4` … `1/32T` inkl. `1/16.`, `5/64`, `1/32.`: 1/16-Tracks mit unverändertem Timing.  Roll `ROL` zählt in den Steps des Tracks. | LAST STEP (Encoder-Klick), PERF (`ROL`) | Pattern | – |
| Ratchets und Roll-Lock | Flam `X12`, `X24`, `X4+`/`X4-` (Velocity-Slope); `1/8` … `1/1` wiederholt den Step alle 2–16 Steps bis zum nächsten aktiven Step. | STEP (`FLM`) | Pattern | – |
| Trigger-Tracks | Jeder aktive Step (jede Velocity) wirkt auf einen Ziel-Track: `RST` = zurück auf Step 1, `MUT` = stumm, solange der Step dauert, `CLK` = das Ziel läuft nur mit diesen Impulsen. Jedes Ziel hat nur eine Quelle. | LAST STEP (Encoder-Klick, `Trg`) | Pattern | – |
| Wiedergabe ohne Unterbrechung [OE] | Regler, Menüs, Laden und Speichern unterbrechen weder Audio noch Sequencer. | – | – | – |

## MIDI

| Funktion | Beschreibung | UI | Speicherort | MIDI |
|---|---|---|---|---|
| Mod Wheel → Kit-Morph | CC 1 auf globalem Kanal steuert `MRP`. | – | – | CC 1 |
| Velocity-Layer | Die Anschlagstärke wählt Samples aus einem definierten Bereich. | VOICE (Velocity `DST` = `WAV`, `AMT`) | Kit | Note-Velocity |
| Fehlerbehebungen [OE] | Zahlreiche Fehler der Original-Firmware (MIDI, Sequencer, SD-Karte, Speicher). | – | – | – |

## LFO

| Funktion | Beschreibung | UI | Speicherort | MIDI |
|---|---|---|---|---|
| Sample-Auswahl | Ziel `WAV`: `MOD` = Anzahl weiterer Samples; `CYC` = Round Robin (ein Sample pro Trigger weiter). | VOICE (LFO) | Kit | CC 122–127 (`MOD` / Voice 1–6), NRPN 27–32 (`WAV` / Voice 1–6) |
| Neue Wellenformen | `XTR`; One-Shots `SI1` … `XT1`, `RN1` (Zufall pro Trigger), `SQ1` (Gate); `OFS` = One-Shot-Verzögerung; `CYC` (MOD = Anzahl der Stufen) | VOICE (LFO `WAV`, `OFS`) | Kit | NRPN 27–32 (`WAV` / Voice 1–6), NRPN 57–62 (`OFS` / Voice 1–6) |
| LFO-Start | Jeder Wellenstart des LFOs einer Voice startet einen Ziel-Track bei Step 1 (`NXT` = beim nächsten Step, `NOW` = sofort); er spielt bis zum Ende und wartet. | LAST STEP (Encoder-Klick, `LFO>`) | Pattern | – |
| LFO → LFO | Ein LFO moduliert Rate, Amount … jedes anderen LFOs (bisher nur LFOs mit höherer Nummer). | VOICE (LFO `VOI`, `DST`) | Kit | – |
| Anzeige | `MOD` zeigt den Zielbereich (`21..127`); eindeutige Zielnamen `WAV` / `FWV` / `CWV`. | VOICE (LFO) | – | – |

## FX / Mixer

| Funktion | Beschreibung | UI | Speicherort | MIDI |
|---|---|---|---|---|
| Limiter [OE] | 32-Bit-Summe mit Limiter je Ausgang. | CONFIG (`LIM`) | Global | – |
| DJ-Filter [OE] | LP/HP-Filter | PERF (`DJ`, `RES`) | nicht gespeichert | – |
| Zweiter FX-Slot | FX2 = Projekt-FX. FX-Routing `ORD`: `A>B`, `B>A`, `A+B` (parallel), `A>+` (A zum Ausgang und in B). | FX (SHIFT + FX, 2× für FX2) | FX1 und `ORD`: Kit, FX2: Global | NRPN 106–117 (FX1) |
| Routing und Sends | `OUT` = `FX` / `FXB`: 100 % in Slot A / B. `SNA` / `SNB` = Send in A / B zusätzlich zum Ausgang, beide gleichzeitig möglich. Sinnvoll bei `OUT` = `St1` … `R2`; bei `FX` / `FXB` erhöhen sie den Anteil im selben Slot bzw. füttern zusätzlich den anderen. | VOICE (MIX `OUT`, AEG `SNA` / `SNB`) | Kit | NRPN 87–92 (`OUT` / Voice 1–6) |
| Delay-Tempo-Sync | `1/4` … `1/32T`, punktiert, Triolen und die Track-Raster | FX (Setup `SYA` / `SYB`) | Global | – |
| Master-Bus FX3 | Zuletzt auf `St1`: Sättigung `SAT` mit Split `FRQ` (24 dB/Okt., nur oberhalb 40 Hz … 10 kHz) und Bias `BIA` (gerade Obertöne, pegelabhängig), Kompressor mit Split `CFQ` als Band-Split oder Sidechain (`CSM`; Release `AUT` = 1/16-Note, Auto-Make-up `AMU` statisch / adaptiv) und EQ (Shelves, 2 Mitten); `ORD` = Reihenfolge der drei. | FX (SHIFT + FX, 3× für FX3), PERF (`SAT`, `FRQ`) | Kit | – |
| FX-Regler | SHIFT + Regler 1 / 2 = Input-Gain FX1 / FX2, Regler 3 / 4 = Mix FX1 / FX2. | SHIFT + Regler (alle Seiten) | Input-Gain: Global | – |
| Pre-Fader-Sends | `SNA` / `SNB` vor dem Volume-Fader: Der Fader regelt nur das trockene Signal (`OFF` = danach, wie bisher). | CONFIG (`PRE`) | Global | – |

## Load / Save

| Funktion | Beschreibung | UI | Speicherort | MIDI |
|---|---|---|---|---|
| Sample-Import | Beim Start aus `SAMPLES`, `SAMPLES/LOOPS` und `SAMPLES/WAVES`, nur bei Änderungen; fehlerhafte Dateien werden gemeldet. Anzeige `Samples: N`, `Used x.x/5.9s`. `BIGFLASH` (experimentell): ≈ 17,8 s statt 5,9 s. | – (Start) | Flash | – |
| Kit-Management [OE] | Kits und einzelne Voices aus beliebigen Projekten laden; „Change Project“. | LOAD (+ Step) | Projekt | – |
| Kit-Zusatzblock | Sends, `ORD` und FX3 im Kit; ältere Kits laden mit Standardwerten. | – | Kit | – |
| Sicheres Projekt-Speichern [OE] | Ein abgebrochenes Speichern kann das Projekt nicht beschädigen. | SAVE | Projekt | – |

## Preset-System

| Funktion | Beschreibung | UI | Speicherort | MIDI |
|---|---|---|---|---|
| Voice-INIT-Vorlagen | Pro Projekt, global oder Werkszustand; laden, speichern, löschen. | LOAD (+ Step der Voice, Zeile `INIT`) | Projekt / Global (`INIT.SND`) | – |

## Morph

| Funktion | Beschreibung | UI | Speicherort | MIDI |
|---|---|---|---|---|
| Kit-Morph | `MRP` 0 → 127, Startwert im aktiven Kit → Zielwert im Morph-Kit (mit SHIFT); SHIFT + Klick nimmt den Parameter vom Morph aus; `MRV` = Morph pro Voice ein/aus. | VOICE (Edit-Ansicht), CONFIG (`MRV`) | Morph-Kit (Projekt), `MRV`: Global | CC 1 (`MRP`) |
| Voice-Morph | Morph zwischen zwei Voices: `VMV` = Paar, `VMO` = Stärke, `VSW` = Umschaltpunkt (Sample, Filtertyp …) pro Voice. | PERF | `VSW`: Global, sonst nicht gespeichert | – |
