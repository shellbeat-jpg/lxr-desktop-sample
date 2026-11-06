# Overview: XT firmware for LXR-02

[Deutsch](OVERVIEW.de.md)

Everything new compared with the Erica Synths firmware. **[OE]** = from OpenElixir-02 v1.2 (Moritz
Waser), everything else from XT.
"Global" = saved with SAVE → Global and in the project file. NRPN n = parameter 128 + n
(computed, not measured on the device). Details: [SAMPLES.md](SAMPLES.md).

## Voices

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Engines [OE] | Any engine freely assignable to each voice | VOICE (SHIFT + KIT) | Kit | NRPN 98 |
| Samples | Up to 31 WAVs, 5.9 s in total (with `BIGFLASH` ≈ 17.8 s). Start and length parameters in ms with fades, loops with crossfade, declick on retrigger. | VOICE (OSC `WAV`, `STA`, `LEN`) | Kit, samples in flash | CC 2–5, 7, 8 (`WAV` / Voice 1–6), NRPN 100–105 (`STA` / Voice 1–6), NRPN 93, 94, 97 (`LEN` / Voice 1–3) |
| User waves | Up to 8 WAVs from `SAMPLES/WAVES` as single cycles (1024 points): oscillator waveforms `W1`…`W8` (fixed values after `S1`…`S31`) and LFO shapes `W1`…`W8` (running) / `W1.`…`W8.` (one-shot); numbered on their own. | VOICE (OSC `WAV`, LFO `WAV`) | Kit (number), waves in flash | CC 2–5, 7, 8 (`WAV` / Voice 1–6), NRPN 27–32 (LFO `WAV` / Voice 1–6) |
| Choke | Voices of the choke group cut each other (≈ 2 ms). | VOICE (AEG `CHK`) | Kit | NRPN 99 |
| Track 7 | Own waveform/sample, pitch, sends and pan for track 7 on the shared voice of slot 6, e.g. closed/open hi-hat samples, only one of them into the reverb. | VOICE 7 (OSC `WAV`, `COA`; AEG `SNA`, `SNB`; MIX `PAN`) | Kit | – |
| LFO 2 | A second LFO per voice and an own LFO for track 7 (acts while track 7 plays, instead of the LFO of voice 6). Test. | VOICE 1-7, LFO button pressed again on its 2nd screen | Kit | – |
| OSX | Exclude single voices from the global sample rate (`SR` / `0sr`). | PERF (`OSX`, on/off for the active voice) | Kit | – |
| FX presets | List for FX1/FX2: 17 factory presets (delay, distortion, compressor, ring mod) and 8 user presets per project; load/save with an encoder click. | FX setup page (`PRS`) | user: project (`FXPRESET.BIN`) | – |
| Cue | Pre-listening on St2 (pre-fader, also muted tracks); `CMX` cue/main mix, `CVL` level. | SHIFT + step 1–7, PERF (`CMX`, `CVL`) | Global (`CMX`, `CVL`) | – |
| Fast rolls | Roll rates `24`, `32`, `48` (1/16 triplets, 1/32, 1/32 triplets). | PERF (`ROL`) | – | – |
| Step accent | Accent per step, `ACA` = level (+0.1 … +6 dB). | Step page (`ACC`), CONFIG 2 (`ACA`) | Pattern (`ACC`), global (`ACA`) | – |
| Random voice | SHIFT + encoder click on a voice page varies the active voice by `RND`; volume, pan, output, sends, LFO and engine stay. | Voice pages, PERF (`RND`) | Global (`RND`) | – |
| Random octave | Each trigger plays a random octave in a range (`+-1` … `-3`); per step `OCT` replaces it (`vce` = as the voice). Test, not released yet. | VOICE (OSC `OCT`), step page (`OCT`) | Kit, pattern (step) | – |
| WAV recorder | Records St1 to the SD card (`REC/RECnnn.WAV`, 16 bit stereo): 1, 2 or 4 passes of the pattern or until stop; starts with PLAY or at the next pattern start. | PERF, last screen (`REC`, `RST`, encoder click) | Global | – |
| Modal engine | New engine: physical model of a struck membrane, bar, string, bell, plate or glass (`MDL`, `DMP`, `POS`, `INH`, `HRD`, `MAT`, `EXC`). Test. | VOICE (SHIFT + KIT, `modal`) | Kit | NRPN 98 |
| FM2 engine | 2-operator FM percussion: ratio `RAT`, index `IDX` with its own decay `IDC`, feedback `FBK`. Test. | VOICE (SHIFT + KIT, `fm2`) | Kit | NRPN 98 |
| Wavetable engine | Scans sine › triangle › square › saw, a set made from the user waves or 4 own sets from `SAMPLES/TABLES` (Serum, Vital, WaveEdit); lined-up frames, real in-between waves, band limited; `SWP` sweep by the pitch EG, `SCN` smooth, stepped or back and forth. Test. | VOICE (SHIFT + KIT, `wavetable`) | Kit; sets: sample memory | NRPN 98 |

## Sequencer / tracks

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Lock trigger | Lock trigger for automation with velocity 0 | STEP (`VEL`) | Pattern | – |
| Pattern mode | Patterns switch at once on the next step | CONFIG (`IPS`) | Global | – |
| Grid per track | `1/4` … `1/32T` incl. `1/16.`, `5/64`, `1/32.`: 1/16 tracks keep their timing. Roll `ROL` counts in the track's own steps. | LAST STEP (encoder click), PERF (`ROL`) | Pattern | – |
| Ratchets and roll locks | Flam `X12`, `X24`, `X4+`/`X4-` (velocity slope); `1/8` … `1/1` repeats the step every 2–16 steps until the next active step. | STEP (`FLM`) | Pattern | – |
| Trigger tracks | Each active step (any velocity) acts on a target track: `RST` = back to step 1, `MUT` = muted while the step lasts, `CLK` = the target only steps on these impulses. Each target has one source. | LAST STEP (encoder click, `Trg`) | Pattern | – |
| Uninterrupted playback [OE] | Knobs, menus, loading and saving don't interrupt the audio or the sequencer. | – | – | – |

## MIDI

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Mod wheel → kit morph | CC 1 on the global channel sets `MRP`. | – | – | CC 1 |
| Velocity layers | The velocity selects samples from a defined range. | VOICE (velocity `DST` = `WAV`, `AMT`) | Kit | Note velocity |
| Bug fixes [OE] | Many stock firmware bugs (MIDI, sequencer, SD card, memory). | – | – | – |

## LFO

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Sample selection | Target `WAV`: `MOD` = number of further samples; `CYC` = round robin (next sample per trigger). | VOICE (LFO) | Kit | CC 122–127 (`MOD` / Voice 1–6), NRPN 27–32 (`WAV` / Voice 1–6) |
| New waveforms | `XTR`; one-shots `SI1` … `XT1`, `RN1` (random per trigger), `SQ1` (gate); `OFS` = one-shot delay; `CYC` (`MOD` = number of steps) | VOICE (LFO `WAV`, `OFS`) | Kit | NRPN 27–32 (`WAV` / Voice 1–6), NRPN 57–62 (`OFS` / Voice 1–6) |
| LFO start | Each start of the waveform of a voice's LFO starts a target track at step 1 (`NXT` = on its next step, `NOW` = at once); it plays to its end and waits. | LAST STEP (encoder click, `LFO>`) | Pattern | – |
| LFO → LFO | An LFO modulates rate, amount … of any other LFO (before only of LFOs with a higher number). | VOICE (LFO `VOI`, `DST`) | Kit | – |
| Display | `MOD` shows the target range (`21..127`); distinct target names `WAV` / `FWV` / `CWV`. | VOICE (LFO) | – | – |

## FX / mixer

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Limiter [OE] | 32-bit mix with a limiter on each output. | CONFIG (`LIM`) | Global | – |
| DJ filter [OE] | LP/HP filter | PERF (`DJ`, `RES`) | not saved | – |
| Second FX slot | FX2 = project FX. FX routing `ORD`: `A>B`, `B>A`, `A+B` (parallel), `A>+` (A to its output and into B). | FX (SHIFT + FX, twice for FX2) | FX1 and `ORD`: kit, FX2: global | NRPN 106–117 (FX1) |
| Routing and sends | `OUT` = `FX` / `FXB`: 100 % into slot A / B. `SNA` / `SNB` = send into A / B besides the output, both at once if wanted. Meant for `OUT` = `St1` … `R2`; with `FX` / `FXB` they raise the level in the same slot or also feed the other one. | VOICE (MIX `OUT`, AEG `SNA` / `SNB`) | Kit | NRPN 87–92 (`OUT` / Voice 1–6) |
| Delay tempo sync | `1/4` … `1/32T`, dotted, triplets and the track grids | FX (setup `SYA` / `SYB`) | Global | – |
| Master bus FX3 | Last on `St1`: saturation `SAT` with split `FRQ` (24 dB/oct, only above 40 Hz … 10 kHz) and bias `BIA` (even harmonics, level dependent), compressor with split `CFQ` as band split or sidechain (`CSM`; release `AUT` = 1/16 note, auto make-up `AMU` static / adaptive) and EQ (shelves, 2 mids); `ORD` = order of the three. | FX (SHIFT + FX, 3× for FX3), PERF (`SAT`, `FRQ`) | Kit | – |
| FX knobs | SHIFT + knob 1 / 2 = input gain FX1 / FX2, knob 3 / 4 = mix FX1 / FX2. | SHIFT + knobs (all pages) | Input gain: Global | – |
| Pre-fader sends | `SNA` / `SNB` before the volume fader: the fader only sets the dry signal (`OFF` = after it, as before). | CONFIG (`PRE`) | Global | – |

## Load / save

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Sample import | At boot from `SAMPLES`, `SAMPLES/LOOPS` and `SAMPLES/WAVES`, only when changed; bad files are reported. Shows `Samples: N`, `Used x.x/5.9s`. `BIGFLASH` (experimental): ≈ 17.8 s instead of 5.9 s. | – (boot) | Flash | – |
| Kit management [OE] | Load kits and single voices from any project; "Change Project". | LOAD (+ step) | Project | – |
| Kit extension block | Sends, `ORD` and FX3 in the kit; older kits load with defaults. | – | Kit | – |
| Safe project save [OE] | An interrupted save can't damage the project. | SAVE | Project | – |

## Preset system

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Voice INIT templates | Per project, global or factory; load, save, delete. | LOAD (+ voice's step, row `INIT`) | Project / global (`INIT.SND`) | – |

## Morph

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Kit morph | `MRP` 0 → 127, start value in the active kit → target value in the morph kit (with SHIFT); SHIFT + click takes the parameter out of the morph; `MRV` = morph on/off per voice. | VOICE (edit view), CONFIG (`MRV`) | Morph kit (project), `MRV`: global | CC 1 (`MRP`) |
| Voice morph | Morph between two voices: `VMV` = pair, `VMO` = amount, `VSW` = switch point (sample, filter type …) per voice. | PERF | `VSW`: global, otherwise not saved | – |
