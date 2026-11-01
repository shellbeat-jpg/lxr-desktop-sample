# Samples and voice INIT

User guide for the additions of this fork (OpenElixir-02 + Samples). The base features of
OpenElixir-02 are described in [ENGINES.md](ENGINES.md) and [README_OPENELIXIR.md](../README_OPENELIXIR.md).

## Importing samples

1. Create the folder `SAMPLES` in the top folder of the SD card and copy WAV files into it. Loop
   samples go into the subfolder `SAMPLES/LOOPS`.
2. Switch the LXR-02 on normally (without holding the encoder). If the folder changed since the
   last import, the display shows "Import x/y", then "Import+Verify OK".
3. The start screen then shows "Samples:" with the number of stored samples and, below, the flash
   size of the chip ("Flash: 1024 KB" or "Flash: 2048 KB").

The samples stay in the internal flash: they also play without SD card.

| | |
|---|---|
| Format | WAV, 16-bit PCM, mono or stereo (stereo is mixed to mono), any sample rate |
| File names | 8.3 format, e.g. `01KICK.WAV` (long names are not recognized) |
| Number | up to 31 (both folders together); slots s1..s31 follow the order of the names, numbers by value (`2KICK` before `10KICK`), letters without case |
| Total length | 511 KB in flash = about 5.9 s mono at 44.1 kHz (11.9 s at 22.05 kHz); a stereo file counts half its size |
| Ignored | names starting with `.` or `_` (e.g. the `._` files of macOS) |

- **Changes are detected by file names and sizes.** A file replaced by another one with the same
  name and exactly the same size is not imported again: rename it.
- **Files that are not 16 bit** are skipped ("Not 16bit, skip:" with the name); the others are
  imported.
- **Files that don't fit** are skipped in name order ("Full(5.9s) skip:"); a later, smaller file may
  still fit. If not even one file fits: "Too big: x.ys / max 5.9s @44.1k", the flash stays unchanged.
- **A firmware update erases the samples.** Keep `/SAMPLES` on the card: the next normal start
  imports them again. Remove the `LXR*.img` file from the card after the update.

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

## Test status

Tested on the device: sample import and playback, voice INIT. Start/length, the skipping of files
that are invalid or don't fit, loops, the order of the names, 31 slots and the modulated sample
selection are host-tested (`sh tests/host/run.sh`, Linux, gcc), a full test on the device is
pending.

Under investigation, not yet attributed to this fork or the base: after the first save of a kit and
a project and switching the unit off and on again, the display stayed dark and there was no sound
for a few starts (buttons and LEDs worked); the `LXR*.img` was still on the card. As a precaution,
remove the image after an update and leave the unit off for at least 10 s before switching it on
again.
