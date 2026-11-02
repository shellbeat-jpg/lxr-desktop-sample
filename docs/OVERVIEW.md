# Overview: OE02-Samples and OpenElixir-02

[Deutsch](OVERVIEW.de.md)

Everything new compared with the Erica Synths firmware. **[OE]** = from OpenElixir-02 v1.2 (Moritz
Waser), everything else from OE02-Samples. "Global" =
saved with SAVE → Global and in the project file. NRPN n = parameter 128 + n (computed, not measured
on the device). Details: [SAMPLES.md](SAMPLES.md).

## Voices

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Assignable engines [OE] | Each voice slot runs drum, snare, cymbal or hi-hat. | VOICE (SHIFT + KIT) | Kit | NRPN 98 |
| Samples | Up to 31 WAVs as `S1`…`S31` after the waveforms; coarse 60 = original pitch. Start and length in ms with fades, loops with crossfade, declick on retrigger. | VOICE (OSC `WAV`, `STA`, `LEN`) | Kit, samples in flash | CC 2–5, 7, 8 (`WAV`), NRPN 100–105 (`STA`), NRPN 93, 94, 97 (`LEN`) |
| Choke group | Tracks of the group cut each other when triggered (≈ 2 ms). | VOICE (AEG `CHK`) | Kit | NRPN 99 |

## Sequencer / tracks

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Velocity 0 = lock | The step applies its automation only and doesn't trigger the voice. | STEP (`VEL`) | Pattern | – |
| Instant pattern switching | A pattern selected by hand starts on the next step at the same position. | CONFIG (`IPS`) | Global | – |
| Step grid per track | `1/4` … `1/32T` incl. `1/16.`, `5/64`, `1/32.`: polyrhythms; 1/16 tracks keep their timing. The roll `ROL` counts in the track's own steps. | LAST STEP (encoder click), PERF (`ROL`) | Pattern | – |
| Ratchets and roll locks | Flam `X12`, `X24`, `X4+`/`X4-` (velocity ramp); `1/8` … `1/1` repeats the step every 2–16 steps until the next active step. | STEP (`FLM`) | Pattern | – |
| Uninterrupted playback [OE] | Knobs, menus, loading and saving don't interrupt the audio or the sequencer. | – | – | – |

## MIDI

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Mod wheel → kit morph | CC 1 on the global channel sets `MRP`. | – | – | CC 1 |
| Velocity layers | The velocity selects samples from a range. | VOICE (velocity `DST` = `WAV`, `AMT`) | Kit | Note velocity |
| Bug fixes [OE] | Many stock firmware bugs (MIDI, sequencer, SD card, memory). | – | – | – |

## LFO

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Sample selection | Target `WAV`: `MOD` = number of further samples; `CYC` = round robin (next sample per trigger). | VOICE (LFO) | Kit | CC 122–127 (`MOD`), NRPN 27–32 (`WAV`) |
| New waveforms | `XTR`; one-shots `SI1` … `XT1`, `RN1` (random per trigger), `SQ1` (gate); `OFS` = one-shot delay. | VOICE (LFO `WAV`, `OFS`) | Kit | NRPN 27–32, 57–62 |
| Display | `MOD` shows the target range (`21..127`); distinct target names `WAV` / `FWV` / `CWV`. | VOICE (LFO) | – | – |

## FX / mixer

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Limiter [OE] | 32-bit mix with a limiter on each output. | CONFIG (`LIM`) | Global | – |
| DJ filter [OE] | LP/HP filter on the final mix. | PERF (`DJ`, `RES`) | not saved | – |
| Two FX slots | FX1 = the kit's FX, FX2 with the same effects (`DRV`, `RNG`, `CMP`, `DEL`). Order `ORD`: `A>B`, `B>A`, `A+B` (parallel). | FX (SHIFT + FX, twice for FX2) | FX1 and `ORD`: kit, FX2: global | NRPN 106–117 (FX1) |
| Routing and sends | `OUT` = `FX` / `FXB`: 100 % into slot A / B. `SNA` / `SNB` = send into A / B besides the output, both at once if wanted. Meant for `OUT` = `St1` … `R2`; with `FX` / `FXB` they raise the level in the same slot or also feed the other one. | VOICE (MIX `OUT`, AEG `SNA` / `SNB`) | Kit | NRPN 87–92 (`OUT`) |
| Delay tempo sync | `1/4` … `1/32T`, dotted, triplets and the track grids. | FX (setup `SYA` / `SYB`) | Global | – |

## Load / save

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Sample import | At boot from `SAMPLES` and `SAMPLES/LOOPS`, only when changed; bad files are reported. Shows `Samples: N`, `Used x.x/5.9s`. `BIGFLASH` (experimental): ≈ 17.8 s instead of 5.9 s. | – (boot) | Flash | – |
| Kit management [OE] | Load kits and single voices from any project; "Change Project". | LOAD (+ step) | Project | – |
| Kit extension block | Sends and `ORD` in the kit; older kits load with defaults. | – | Kit | – |
| Safe project save [OE] | An interrupted save can't damage the project. | SAVE | Project | – |

## Preset system

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Voice INIT templates | Per project, global or factory; load, save, delete. | LOAD (+ voice's step, row `INIT`) | Project / global (`INIT.SND`) | – |

## Morph

| Feature | Description | UI | Stored in | MIDI |
|---|---|---|---|---|
| Kit morph | SHIFT shows and edits the morph kit's value, SHIFT + click takes the parameter out of the morph; `MRV` = morph on/off per voice. | VOICE (edit view), CONFIG (`MRV`) | Morph kit (project), `MRV`: global | CC 1 (`MRP`) |
| Voice morph | Morph one voice towards another: `VMV` = pair, `VMO` = amount, `VSW` = switch point (sample, filter type …) per voice. | PERF | `VSW`: global, otherwise not saved | – |
