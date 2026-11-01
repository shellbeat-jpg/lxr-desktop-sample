# OpenElixir-02 + Samples

A fork of [OpenElixir-02](https://github.com/tunefish777/openelixir-02) v1.2 that adds user samples
to the Erica Synths LXR-02: WAV import from the SD card, sample playback as oscillator waveforms, and
start/length parameters.

> Not official Erica Synths firmware, not affiliated with or approved by Erica Synths. You install it
> at your own risk.

## Who made what

| Part | Author |
|---|---|
| LXR V2 firmware, bootloader, build system | [Erica Synths](https://github.com/erica-synths/lxr-desktop-app) (open-sourced 2026), originally based on the Sonic Potions LXR |
| OpenElixir-02: voice engines, kit management, uninterrupted audio, limiter, DJ filter, bug fixes | Moritz Waser ([tunefish777/openelixir-02](https://github.com/tunefish777/openelixir-02)) |
| Sample extension (this fork, `MOD` commits after OpenElixir-02 v1.2) | shellbeat-jpg, written with Claude (Anthropic) |

Everything that isn't listed under "What this fork adds" below is OpenElixir-02 v1.2 as released by its
author: its features are listed in the next section, its full README is
[README_OPENELIXIR.md](README_OPENELIXIR.md), more details are in [docs/](docs/). The changes of this
fork can be listed with `git log v1.2..HEAD`.

## Base: features of OpenElixir-02 v1.2

The following is taken unchanged from the
[OpenElixir-02 README](https://github.com/tunefish777/openelixir-02#features) by Moritz Waser. These
features are his work, not part of this fork.

**Kit management.** Load full kits and single voices from any project:
- Hold LOAD and press step button 1-7 (Drum 1 ... OP HH) to load just that voice from any kit of
  any project into the current kit.
- "Change Project" below kit 0 in the load and save kit menus loads kits from other projects and
  saves kits into them.

**Freely assignable voice engines.** Each of the 6 voice slots can run any engine: drum, snare,
cymbal or hi-hat. Select a voice and press SHIFT + KIT (or send NRPN 98). The engines are saved in
the kit; kits from the stock firmware load with the stock engines.

Details for both: [docs/ENGINES.md](docs/ENGINES.md).

**Limiter.** Voices and FX are summed in 32 bit, followed by a limiter on each output, so loud kits
no longer hard-clip. Levels that didn't clip before are unchanged. Config menu "lim" (2nd page)
switches it off for exactly the stock mix.

**DJ filter.** A filter on the final mix of all outputs, on the performance page: `dj` turned left of
the center is a low-pass (down to 40 Hz), right of it a high-pass (up to 8 kHz), around the center it
is off; `res` sets the resonance. It isn't saved and starts off; while off, the sound is unchanged.

**Uninterrupted playback.** Turning knobs, working in the menus, loading and saving kits (also
from and to other projects on the SD card) and saving projects don't interrupt the audio or the
sequencer while it plays.

**Under the hood.** About 10 % less CPU load, many stock firmware bugs fixed (MIDI, sequencer, SD
card, memory corruption; most found in the firmware review by jhw in
[erica-synths/lxr-desktop-app#3](https://github.com/erica-synths/lxr-desktop-app/issues/3)), and a
project save that can't lose the saved project. Stock issues that are kept on purpose:
[docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md). The changes of each version are in the
[release notes](https://github.com/tunefish777/openelixir-02/releases).

## What this fork adds

- **Sample import at boot.** Up to 16 WAV files from the folder `/SAMPLES` on the SD card are copied
  into the internal flash (sectors 6 and 7, 512 KB) and verified. The import runs only when the folder
  content changed (file names and sizes).
  - 16-bit PCM, mono or stereo (mixed to mono), any sample rate (pitch is corrected).
  - 8.3 file names (e.g. `01KICK.WAV`); slots s1..s16 follow the alphabetical order.
  - Total length about 5.9 s mono at 44.1 kHz. Files that are not 16 bit or no longer fit are
    skipped and reported; the others are imported.
  - The start screen shows "Samples in flash" with the count.
  - A firmware update erases the samples: keep `/SAMPLES` on the card, the next boot imports again.
- **Playback.** After the 6 standard waveforms the waveform menu lists s1..sN. Coarse 60 plays the
  original pitch, one-shot, linear interpolation, FM from the mod oscillator works as for the other
  waveforms. Works in every slot that runs an engine with a main oscillator.
- **Start and length.**
  - Start: while a sample is selected, the PWM knob becomes "Sta" (all 6 slots, modulatable).
  - Length: new parameter "Len" on the oscillator page of drum 1-3 (0 = "all").
  - Both are shown in ms, use a quadratic scale and are stored in kits. Short fades avoid clicks
    when the start or the length cuts the sample.
- **Voice INIT templates.** In the voice load menu (hold LOAD + step button 1-7) an "INIT" row sits
  between "Change Project" and kit 0. Clicking it opens: Load INIT, Save to Project, Save to Global,
  Del Project INIT, Del Global INIT.
  - Load INIT takes the voice from the project template, else from the global template, else the
    factory values of its engine (the engine and the step automation stay).
  - Saving stores only this voice (incl. engine, sample, start/length) in the template; the other
    voices of the template stay. Deleting removes only this voice.
  - Project template: `INIT.SND` in the project folder (needs a saved project; a project save keeps
    it). Global template: `/INIT.SND`.

Kits from the stock firmware and from OpenElixir-02 load unchanged (start 0, length "all").

## Building and installing

As for OpenElixir-02 (see [README_OPENELIXIR.md](README_OPENELIXIR.md)):

```sh
cmake --preset release
cmake --build --preset release
```

Copy `build/release/LXR_OpenElixir-02_V<version>.img` to the SD card and install it with the
LXR-02's updater (hold the encoder while switching on). Host tests of the sample code:
`sh tests/host/run.sh` (Linux, gcc).

To go back, install the official OpenElixir-02 or Erica Synths image the same way.

## Licence

GPL-3.0-only, with the additional terms in [NOTICE.md](NOTICE.md); see [LICENSE](LICENSE). "Erica
Synths", "LXR" and "LXR-02" are trademarks of Erica Synths, and the LXR hardware is their property.
The copyrights of Erica Synths and of the OpenElixir-02 author in their files are unchanged.
