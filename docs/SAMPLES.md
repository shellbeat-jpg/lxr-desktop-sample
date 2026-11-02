# Samples and voice INIT

User guide for the additions of this fork (OpenElixir-02 + Samples). The base features of
OpenElixir-02 are described in [ENGINES.md](ENGINES.md) and [README_OPENELIXIR.md](../README_OPENELIXIR.md).

## Importing samples

1. Create the folder `SAMPLES` in the top folder of the SD card and copy WAV files into it. Loop
   samples go into the subfolder `SAMPLES/LOOPS`.
2. Switch the LXR-02 on normally (without holding the encoder). If the folder changed since the
   last import, the display shows "Import x/y", then "Import+Verify OK".
3. The start screen then shows "Samples:" with the number of stored samples and, below, the memory
   used and available in seconds at 44.1 kHz mono ("Used 4.2/5.9s", with `BIGFLASH` "/17.8s").

The samples stay in the internal flash: they also play without SD card.

| | |
|---|---|
| Format | WAV, 16-bit PCM, mono or stereo (stereo is mixed to mono), any sample rate |
| File names | 8.3 format, e.g. `01KICK.WAV` (long names are not recognized) |
| Number | up to 31 (both folders together); slots S1..S31 follow the order of the names, numbers by value (`2KICK` before `10KICK`), letters without case |
| Total length | 511 KB in flash = about 5.9 s mono at 44.1 kHz (11.9 s at 22.05 kHz); with the 1.5 MB mode (`BIGFLASH`, see below) about 17.8 s (35.6 s at 22.05 kHz); a stereo file counts half its size |
| Ignored | names starting with `.` or `_` (e.g. the `._` files of macOS) |

- **Changes are detected by file names and sizes.** A file replaced by another one with the same
  name and exactly the same size is not imported again: rename it.
- **Files that are not 16 bit** are skipped ("Not 16bit, skip:" with the name); the others are
  imported.
- **Files that don't fit** are skipped in name order ("Full(5.9s) skip:", with `BIGFLASH`
  "Full(17.8s) skip:"); a later, smaller file may still fit. If not even one file fits:
  "Too big: x.ys / max 5.9s @44.1k" (or 17.8s), the flash stays unchanged.
- **Switch it off for about 10 seconds before switching it on again**: right after switching off, the OLED display can stay dark at the next start (the unit still works; switch off, wait, switch on).
- **A firmware update erases the samples.** Keep `/SAMPLES` on the card: the next normal start
  imports them again. Remove the `LXR*.img` file from the card after the update.

## Experimental: 1.5 MB sample memory

The chip of the LXR-02 (STM32F765VG) reports 1 MB flash in its flash size register. Its die usually has
2 MB: on the tested unit the area above 1 MB could be erased, written and read (first checked with
the LXR02Open port, which stores its samples there). ST doesn't guarantee that area (not tested by
the manufacturer, no specified data retention). Use it at your own risk:

- Create a file or a folder named `BIGFLASH` in `SAMPLES` (or in the top folder of the card). The
  name may have an extension (`BIGFLASH.txt`). The next start imports again with up to about
  **17.8 s** (44.1 kHz mono) instead of 5.9 s.
- The start shows `Big flash: on / 1.5 MB, testing`, the status after the import `Used x.x/17.8s`.
- `Big flash: no / dual bank`: the flash has another sector layout, 512 KB are used.
- Every sample is verified after writing. If something in the upper area fails: `Big flash
  failed / using 512K`, then the import is repeated into the normal 512 KB.
- Removing `BIGFLASH` imports again into the normal 512 KB.
- If the unit ever hangs at the start screen with `BIGFLASH` on the card: remove `BIGFLASH` (or
  start without card) - the firmware and the bootloader are not affected.

## Playing samples

On the OSC page of a voice, turn **Waveform** past Sin, Tri, Saw, Rec, Noi and PWM: S1..SN follow.

- **Coarse 60** plays the sample at its original pitch (the sample rate is taken into account).
- One-shot: the sample plays once per trigger; the AEG decay can end it earlier.
- FM from the mod oscillator works as for the other waveforms.
- Clicking into the waveform parameter (edit view) shows the file name, `L` for a loop and the
  slot, e.g. `KICK01        S3`.
- Works in every voice whose engine has a main oscillator. For snare, cymbal or hi-hat tracks give
  the voice the drum engine first (SHIFT + KIT, see [ENGINES.md](ENGINES.md)) if needed.

## Start and length

| Display | Parameter | Voices | Range |
|---|---|---|---|
| **Sta** (Start) | the PWM knob, while a sample is selected | all 6 | 0 = beginning of the sample |
| **Len** (Length) | 5th parameter of the OSC page | drum 1-3 | 0 = "all" (to the end) |

- Both values are shown in ms (in the 4-parameter view: `230` ms, `1.2` s, `12s`), on a quadratic
  scale: fine steps at the beginning, coarser towards the end.
- The length display shows the time that is actually played (limited by the end of the sample
  after the start).
- The start is taken at the trigger, including LFO or velocity modulation of PWM.
- Short fades avoid clicks: 32 frames fade-in only if the start is after the first frame, 64 frames
  fade-out only if the length cuts the sample. A sample started at 0 and played to the end is
  unchanged.
- Both are stored in kits. Kits without them play from the start to the end.

## Loops

Files from `/SAMPLES/LOOPS` are loop samples: they play from the start (Sta) to the end (Len, "all"
= end of the file) and then jump back to the start, until the amp envelope (AEG decay) ends the
sound. Drum voices have no gate, so a long decay keeps the loop going.

- Sta and Len set the loop points, shown in ms.
- The loop point is crossfaded with the audio just before the start (up to 256 frames, at most a
  quarter of the loop). With start 0 there is no audio before it: then a short fade out/in (32
  frames) avoids the click.
- The fade-in at a later start happens only once, not on every repeat.

## Modulated sample selection

When the waveform of a voice is a sample, modulating the waveform (LFO or velocity modulation with
the waveform as target) selects within a range of samples instead of switching to the built-in
waveforms:

- **Target:** `WAV` = the waveform of the main oscillator (OSC page). The other waveform targets
  are shown as `FWV` (FM/mod oscillator) and `CWV` (transient/click) and don't select samples.
- **The range starts at the selected waveform** (OSC page `WAV`) and covers **as many further
  samples as the modulation amount**: LFO page `MOD`, velocity modulation `AMT` (amount 4 with S5
  selected: S5..S9). Amount 0 = always the selected sample. The range starts at the waveform of the
  target voice (LFO `VOI`).
- The LFO or velocity value is quantized evenly onto the range: with 5 samples each fifth of the
  velocity (or of the LFO swing) selects one sample. The range ends at the last stored sample.
- Example hi-hats only: name the hat files so they follow each other (`5HAT_A` ... `9HAT_E`), select
  S5 as waveform and set the amount to 4.
- Velocity layers: soft to loud versions in a row, the softest selected, amount = number of layers - 1.
- **The sample is chosen at the trigger** and kept until the next trigger: modulation in between
  doesn't switch samples in the middle of a hit.
- **Round robin in order:** LFO waveform `CYC` (cycle) steps through the range, one sample per
  trigger (S5, S6, ... S9, S5, ...). It counts the triggers of the voice set with `RTG`, with
  `RTG` off those of the LFO's own voice. Rate and sync don't matter for `CYC`.
- Random round robin: LFO waveform `RND`; with `RTG` on the voice the value stays the same for
  every hit (the LFO restarts), so leave `RTG` off and use a rate faster than the hits.
- A retrigger cuts the sample that still plays with a short fade (about 1 ms) instead of a click.
- With a built-in waveform as base the modulation works as before.

## Choke groups

One choke group per kit: a track in the group cuts the other tracks of the group when it is
triggered (e.g. several hi-hat samples on different voices, an open hat cut by a closed one).

- AEG page of each voice: **`CHK` on/off** puts this track into the group or takes it out. The edit
  view shows all tracks of the group on the left (e.g. `1--4-6-`).
- The cut is a fade of about 2 ms; a choked track stays silent until it is triggered again.
- Tracks 6 and 7 (CL HH / OP HH) share one voice and cut each other anyway.
- Stored in the kit (parameter slot of the former 7th MIDI note setting). Kits without it: no
  choke group.

## Voice morph

Morphs the sound of one voice towards another voice of the active kit, like the kit morph (`MRP`)
but between two voices. Performance page, after `RES` of the DJ filter (scroll on with the encoder):

| Display | Function |
|---|---|
| `VMV` | the voice pair: `OFF`, `1>2`, `1>3` ... `6>5` (source > target); edit view `Voice 1 > 2` |
| `VMO` | amount 0..127: 0 = the source as set, 127 = the target's values |
| `VSW` | switch point of the source voice, 1..127 (default 64): from this `VMO` value on, switch-like values are the target's; one value per voice (edit view `Voice 1   64`) |

- Every sound parameter of the source voice moves towards the same parameter of the target voice
  (pitch, envelopes, filter, FM, sample start, LFO ...). Switch-like values (waveform/sample, filter
  type, on/off) change at `VSW` (default 64); level jumps there come from the sample or filter type
  changing at once.
- Engine specific parameters only morph if both voices run the same engine; the audio output
  routing never morphs.
- The settings of the voices stay as set: `VMO` = 0 or `VMV` = off gives the source its own sound
  back. Editing a parameter of either voice keeps the morph.
- `VMV` and `VMO` are not saved; after loading a kit turn `VMO` once to apply the morph again. Not a
  modulation target. The six `VSW` values are kept with the global settings (SAVE → Global or a
  project save).

## Performance and sequencer additions (1.2.4, in test)

### Mod wheel → kit morph

The mod wheel (MIDI CC 1) on the **global MIDI channel** sets the kit morph (`MRP` on the
performance page): 0 = the active kit, 127 = the morph kit. CC 1 had no function before.

### Morph target quick access

In the edit view of a sound parameter (click the encoder), **hold SHIFT**: the top row shows
`Morph>` and the value is the one of the **morph kit** (the target of `MRP`); turning the encoder
changes it there. Release SHIFT to see and edit the active kit's value again.

- With `MRP` above 0 the sound follows the change right away.
- The change is in the morph kit's copy in memory: a project save (Save → Project / All) stores it in
  that kit. Saving the active kit into the same slot overwrites it.
- Not for LFO/velocity/automation targets, the audio outputs, the choke group and engine specific
  parameters of slots whose engine differs in the morph kit.
- **SHIFT + encoder click** gives the morph kit the active value: the parameter no longer changes
  with the kit morph (e.g. keep `Coarse` and `Fine` of a voice while everything else morphs).

### Voices of the kit morph

Global settings (SHIFT + step button 12, CONFIG), second page: **`MRV`** = kit morph on/off for the
active voice; select the voice with its voice button while the page is shown. The edit view shows
the morphed voices on the left (e.g. `----56`: only snare/cymbal and hi-hats morph). Voices that
are off keep their own sound at every `MRP` value (also with the mod wheel). Kept after a restart
when the global settings are saved (SAVE → Global); default: all voices morph.

### Velocity 0 = automation only

A step with velocity 0 doesn't trigger the voice and sends no MIDI note; its step automation
(`P1`/`P2`) is still applied, like a "trigless lock". Patterns that used velocity-0 steps to cut a
sound no longer do.

### LFO range display

The edit view of an LFO's `MOD` shows on the left the range its target moves in, e.g. `21..127`:
the target parameter's value ± `MOD`, limited to 0..127 (pitch-like values in their own units,
e.g. `-12..12`). To modulate between *a* and *b*, set the target parameter to (a+b)/2 and `MOD` to
(b−a)/2. No range for targets without a number scale (waveform, filter type ...).

### Instant pattern switching

Global settings (SHIFT + step button 12, CONFIG), second page: **`IPS`** (Pattern Instant)
on/off, kept after a restart when the global settings are saved (SAVE → Global). On: a pattern selected by hand starts on the **next step at
the same position** instead of at the end of the playing pattern. Automatic changes (pattern chains,
songs) still happen at the end of the pattern. Tracks shorter in the new pattern wrap at their length.

### LFO waveforms

New after `CYC`:

| Display | Waveform |
|---|---|
| `XTR` | exponential triangle (free-running) |
| `SI1` `TR1` `SU1` `SD1` `SQ1` `XU1` `XD1` `XT1` | one-shot sine, triangle, saw up, saw down, square, exp. up, exp. down, exp. triangle |
| `RN1` | one-shot random: a new random value at every trigger, held until the next one |

- One-shots run **one cycle per trigger** and then hold their end value. They start with a trigger of
  the voice set with `RTG`; with `RTG` off with a trigger of the LFO's own voice.
- **`OFS` is the delay** before the cycle starts (a fraction of one cycle: 64 ≈ half a cycle). The
  value stays at the start during the delay.
- `FRQ`/`SNC` set the length of the cycle.
- `SQ1` is 1 (high) in the first half of the cycle and 0 in the second half, before the start and
  after the end: a gate after each trigger.

## Step grid per track (1.2.7, in test)

Each track of a pattern can run in its own step grid, e.g. hi-hats in 1/16 triplets against straight
drums. **SHIFT + LAST STEP** (step button 14): the bottom row shows `[16]  1/16    1` = length,
grid and the step length compared with 1/16. **Click the encoder** to switch the encoder between
length and grid (brackets). In the `ALL` mode (press LAST STEP again) it sets all tracks.

| Grid | Step length (1/16 = 1) | Steps per bar |
|---|---|---|
| `1/4` | x4 | 4 |
| `1/4T` | 8/3 | 6 |
| `1/8` | x2 | 8 |
| `1/16.` | 6/4 | 10⅔ |
| `1/8T` | 4/3 | 12 |
| `5/64` | 5/4 | 12⅘ |
| `1/16` | 1 (default) | 16 |
| `1/32.` | 3/4 | 21⅓ |
| `1/16T` | 2/3 | 24 |
| `1/32` | 1/2 | 32 |
| `1/32T` | 1/3 | 48 |

- The 1/16 tracks keep their timing unchanged; the other tracks step on the internal 96 ppq clock
  (24 ticks per 1/16). With MIDI clock all grids except `1/32.` and `5/64` fall on clock pulses.
- Each track loops at its own length, as before (polymeter). The pattern ends after its longest track
  in time.
- Stored in the pattern (a small block after the pattern data). Older patterns load as `1/16`; other
  firmwares ignore the block and play all tracks in 1/16.
- Roll on a grid track repeats on the track's own steps. Shuffle moves grid tracks slightly; live
  recording is quantized to the 1/16 grid.

## Step flam: ratchets and roll locks (1.2.8, in test)

The step parameter flam (step mode) has more values after `X2` … `X8`:

| Value | Effect |
|---|---|
| `X12`, `X24` | 12 or 24 repeats within the step |
| `X4+`, `X4-` | 4 repeats with rising / falling velocity (25 … 100 % of the step velocity) |
| `1/8`, `1/4`, `1/2`, `1/1` | **roll lock**: the step repeats every 2, 4, 8, 16 steps of its track (like the slower PERF roll rates) until the next active step of the track |

On grid tracks the repeats follow the track's own steps. Other firmwares play these steps once.

## Two FX slots (1.2.8, in test)

The FX bus has two slots, A and B, each with the same effects: `OFF`, `DRV` (distortion), `RNG`
(ring modulator), `CMP` (compressor), `DEL` (delay).

- **Slot A** is the FX of the kit as before (**SHIFT + FX**, step button 15). **Press SHIFT + FX
  again** for **slot B**: its type is shown as `FX2`, the edit view shows `FX B`. Its parameters
  are the same as slot A's (type, `OUT`, mix, effect parameters).
- **Routing:** a voice's output (`OUT` on its mix page) `FX` goes to slot A, the new **`FXB`** to
  slot B (100 %, nothing dry).
- **Send (dry and FX):** `1+A`, `2+A`, `L1A`, `R1A`, `L2A`, `R2A` = dry to St1, St2, L1, R1, L2, R2
  **and** a send to slot A; `1+B` … `R2B` the same with slot B (edit view e.g. `St1 + FX A`). The
  send level is **`SND`** on the AEG page of the voice (0..127, default 64), stored in the kit. Set
  the mix of the slot to fully wet for a classic send/return, otherwise the dry bus signal is heard
  twice.
- **Kit format:** the send levels are a small block after the engine block of the kit file. Older
  kits load with 64; OpenElixir-02 ignores the block. Voices routed to a send play silent on other
  firmwares.
- **Order** (global settings, third page, `ORD`):
  - `A>B` (default): slot A, then slot B; voices on `FXB` only go through B. The chain goes to the
    `OUT` of slot A. With slot B `OFF` everything sounds as before.
  - `B>A`: slot B, then slot A (to the `OUT` of slot A).
  - `A+B`: side by side, each slot to its own `OUT`.
- **Delay tempo sync** (global settings, third page): `SYA` for slot A, `SYB` for slot B: `OFF`
  (free time and range as before), `1/4`, `1/4T`, `1/8.`, `1/8`, `1/8T`, `1/16.`, `1/16`,
  `1/16T`, `1/32`. Follows the tempo (also external clock). The delay is at most about 0.74 s
  (ping-pong 0.36 s): longer values are limited.
- Slot B, the order and the sync are kept with the global settings (SAVE → Global or a project save),
  not in the kit; slot A stays in the kit.
- CPU: each slot costs about as much as one effect did before (measure with the CPU meter build).

## Voice INIT

Hold **LOAD** and press the step button of a voice (1-7). From kit 0 turn the encoder one step back:
the row **"INIT: Project"**, "INIT: Global" or "INIT: Factory" shows where this voice's INIT comes
from. One step further is "Change Project" as before. Click the INIT row for its actions:

| Action | |
|---|---|
| Load INIT | loads the voice from the project template, else from the global template, else the factory values of its engine |
| Save to Project | stores this voice of the active kit in the template of the listed project |
| Save to Global | stores this voice in the global template |
| Del Project INIT / Del Global INIT | removes this voice from the template (second click to confirm: "Delete? Click") |
| Back | back to the kit list |

- A template holds each voice separately: saving or deleting changes only this voice.
- A template voice contains everything of the voice: engine, sound parameters, LFO and velocity
  settings, sample, start and length.
- The factory INIT keeps the voice's engine and the step automation of the patterns.
- A project template needs a saved project ("Save proj first" otherwise). Saving the project keeps
  its template.
- With "Change Project" the INIT row uses the template of the listed project.
- Files: `INIT.SND` in the project folder (`/projNN/INIT.SND`) and in the top folder of the card
  (global). They are kit files; the kit lists ignore them.

## History of the 1.5 MB mode

A first version checked the upper area with a fault-catching read before using it; on the tested
unit that check itself hung the start. The LXR02Open port then showed that plain reads and writes
above 1 MB work on the same unit, so the current version uses plain accesses like that port and
relies on the verify after writing.

## Test status

Tested on the device: sample import and playback, the skipping of invalid files, start and length
(`STA`/`LEN`), loops, the modulated sample selection (`RND` and `CYC`), the choke group (`CHK`),
the file name in the waveform edit view, voice INIT and the experimental 1.5 MB mode (`BIGFLASH`,
samples above 1 MB play correctly). Of the additions of 1.2.4, the morph target access (SHIFT) and `MRV` are tried on the device; the
mod wheel, SHIFT + click, velocity 0, instant pattern switching, the LFO range display and the new
LFO waveforms are not tested on the device yet. Host tests (Linux, gcc) for import and playback:
`sh tests/host/run.sh`.
