# OpenElixir-02 + Samples

An unofficial fork of [OpenElixir-02](https://github.com/tunefish777/openelixir-02) v1.2 that adds
user samples to the Erica Synths LXR-02: WAV import from the SD card, sample playback as oscillator
waveforms, and start/length parameters.

> **Not official firmware.** This fork is not affiliated with or approved by Erica Synths or by the
> author of OpenElixir-02. You install it at your own risk.

## Who made what

| Part | Author |
|---|---|
| LXR V2 firmware, bootloader, build system | [Erica Synths](https://github.com/erica-synths/lxr-desktop-app) (open-sourced 2026), originally based on the Sonic Potions LXR |
| OpenElixir-02: voice engines, kit management, uninterrupted audio, limiter, DJ filter, bug fixes | Moritz Waser ([tunefish777/openelixir-02](https://github.com/tunefish777/openelixir-02)) |
| Sample extension (this fork, `MOD` commits after OpenElixir-02 v1.2) | shellbeat-jpg, written with Claude (Anthropic) |

Everything that isn't listed under "What this fork adds" below is OpenElixir-02 v1.2 as released by its
author; its features and documentation are in [README_OPENELIXIR.md](README_OPENELIXIR.md) and
[docs/](docs/). The changes of this fork can be listed with `git log v1.2..HEAD`.

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
