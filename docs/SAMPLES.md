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
| Number | up to 31 (both folders together); slots s1..s31 follow the order of the names, numbers by value (`2KICK` before `10KICK`), letters without case |
| Total length | 511 KB in flash = about 5.9 s mono at 44.1 kHz (11.9 s at 22.05 kHz); with the 1.5 MB mode (`BIGFLASH`, see below) about 17.8 s (35.6 s at 22.05 kHz); a stereo file counts half its size |
| Ignored | names starting with `.` or `_` (e.g. the `._` files of macOS) |

- **Changes are detected by file names and sizes.** A file replaced by another one with the same
  name and exactly the same size is not imported again: rename it.
- **Files that are not 16 bit** are skipped ("Not 16bit, skip:" with the name); the others are
  imported.
- **Files that don't fit** are skipped in name order ("Full(5.9s) skip:", with `BIGFLASH`
  "Full(17.8s) skip:"); a later, smaller file may still fit. If not even one file fits:
  "Too big: x.ys / max 5.9s @44.1k" (or 17.8s), the flash stays unchanged.
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

On the OSC page of a voice, turn **Waveform** past Sin, Tri, Saw, Rec, Noi and PWM: s1..sN follow.

- **Coarse 60** plays the sample at its original pitch (the sample rate is taken into account).
- One-shot: the sample plays once per trigger; the AEG decay can end it earlier.
- FM from the mod oscillator works as for the other waveforms.
- Clicking into the waveform parameter (edit view) shows the file name, `L` for a loop and the
  slot, e.g. `KICK01        s3`.
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

- **Target:** `wav` = the waveform of the main oscillator (OSC page). The other waveform targets
  are shown as `fwv` (FM/mod oscillator) and `cwv` (transient/click) and don't select samples.
- **The range starts at the selected waveform** (OSC page `wav`) and covers **as many further
  samples as the modulation amount**: LFO page `mod`, velocity modulation `amt` (amount 4 with s5
  selected: s5..s9). Amount 0 = always the selected sample. The range starts at the waveform of the
  target voice (LFO `voi`).
- The LFO or velocity value is quantized evenly onto the range: with 5 samples each fifth of the
  velocity (or of the LFO swing) selects one sample. The range ends at the last stored sample.
- Example hi-hats only: name the hat files so they follow each other (`5HAT_A` ... `9HAT_E`), select
  s5 as waveform and set the amount to 4.
- Velocity layers: soft to loud versions in a row, the softest selected, amount = number of layers - 1.
- **The sample is chosen at the trigger** and kept until the next trigger: modulation in between
  doesn't switch samples in the middle of a hit.
- **Round robin in order:** LFO waveform `cyc` (cycle) steps through the range, one sample per
  trigger (s5, s6, ... s9, s5, ...). It counts the triggers of the voice set with `rtg`, with
  `rtg` off those of the LFO's own voice. Rate and sync don't matter for `cyc`.
- Random round robin: LFO waveform `rnd`; with `rtg` on the voice the value stays the same for
  every hit (the LFO restarts), so leave `rtg` off and use a rate faster than the hits.
- A retrigger cuts the sample that still plays with a short fade (about 1 ms) instead of a click.
- With a built-in waveform as base the modulation works as before.

## Choke groups

One choke group per kit: a track in the group cuts the other tracks of the group when it is
triggered (e.g. several hi-hat samples on different voices, an open hat cut by a closed one).

- AEG page of each voice: **`chk` on/off** puts this track into the group or takes it out. The edit
  view shows all tracks of the group on the left (e.g. `1--4-6-`).
- The cut is a fade of about 2 ms; a choked track stays silent until it is triggered again.
- Tracks 6 and 7 (CL HH / OP HH) share one voice and cut each other anyway.
- Stored in the kit (parameter slot of the former 7th MIDI note setting). Kits without it: no
  choke group.

## Voice morph

Morphs the sound of one voice towards another voice of the active kit, like the kit morph (`mrp`)
but between two voices. Performance page, after `res` of the DJ filter (scroll on with the encoder):

| Display | Function |
|---|---|
| `vmv` | the voice pair: `off`, `1>2`, `1>3` ... `6>5` (source > target); edit view `Voice 1 > 2` |
| `vmo` | amount 0..127: 0 = the source as set, 127 = the target's values |

- Every sound parameter of the source voice moves towards the same parameter of the target voice
  (pitch, envelopes, filter, FM, sample start, LFO ...). Switch-like values (waveform, filter type,
  on/off) change at half the amount.
- Engine specific parameters only morph if both voices run the same engine; the audio output
  routing never morphs.
- The settings of the voices stay as set: `vmo` = 0 or `vmv` = off gives the source its own sound
  back. Editing a parameter of either voice keeps the morph.
- Not saved; after loading a kit turn `vmo` once to apply the morph again. Not a modulation target.

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
(`sta`/`len`), loops, the modulated sample selection (`rnd` and `cyc`), the choke group (`chk`),
the file name in the waveform edit view, voice INIT and the experimental 1.5 MB mode (`BIGFLASH`,
samples above 1 MB play correctly). Host tests (Linux, gcc) for import and playback:
`sh tests/host/run.sh`.
