# Modulation targets

The targets of the step automation (`P1`/`P2`, step page `dst`), the LFOs (`dst`) and velocity
(`vel`/`dst`), as the display shows them. Generated from `modTargets` in `Core/Src/menu/Cc2Text.c`.

- **Step `dst`** (step page, 3 characters): the voice number and the first two letters of the short name,
  e.g. `1wa` = waveform of voice 1. `0sr` = sample rate of all voices.
- **LFO / velocity `dst`**: the short name of the target voice (`voi` of the LFO).
- **Edit view** (encoder pressed): category and full name.
- Waveform targets: `wav` = main oscillator, `fwv` = FM / modulator oscillator, `cwv` = transient (click).
- With another engine on a slot, the targets that engine doesn't have show `off` and are skipped.
- Value of a `wav` lock (`val`): 0 … 5 = standard waveforms, 6 … 36 = samples S1 … S31, 37 … 44 = user waves W1 … W8.

## All voices

| Page | Step `dst` | LFO/velocity `dst` | Edit view |
|---|---|---|---|
| Mix | `0sr` | – | Voice SampleRt |

## Voice 1 (Drum 1)

| Page | Step `dst` | LFO/velocity `dst` | Edit view |
|---|---|---|---|
| Oscillator | `1co` | `coa` | Oscilltr Coarse |
| Oscillator | `1fi` | `fin` | Oscilltr Fine |
| Oscillator | `1wa` | `wav` | Oscilltr Waveform |
| Oscillator | `1pw` | `pwm` | Oscilltr PulsWdth |
| Amp EG | `1at` | `atk` | Veloc EG Attack |
| Amp EG | `1de` | `dec` | Veloc EG Decay |
| Amp EG | `1sl` | `slp` | Veloc EG Slope |
| Modulation | `1de` | `dec` | Mod EG Decay |
| Modulation | `1sl` | `slp` | Veloc EG Slope |
| Modulation | `1mo` | `mod` | PitchMod Amount |
| Modulation | `1ds` | `dst` | Velocity DstParam |
| Modulation | `1am` | `amt` | Velocity Amount |
| Modulation | `1vo` | `vol` | Velocity Vol mod |
| FM | `1am` | `amt` | FM Amount |
| FM | `1fr` | `frq` | FM Frequncy |
| FM | `1fw` | `fwv` | Oscilltr Waveform |
| FM | `1mo` | `mod` | Oscilltr Mode |
| Transient | `1cw` | `cwv` | Transnt Waveform |
| Transient | `1vo` | `vol` | Transnt Volume |
| Transient | `1fr` | `frq` | Transnt Frequncy |
| Filter | `1fr` | `frq` | Filter Frequncy |
| Filter | `1re` | `res` | Filter Resnance |
| Filter | `1ty` | `typ` | Filter Type |
| Filter | `1dr` | `drv` | Filter Overdriv |
| LFO | `1fr` | `frq` | LFO Frequncy |
| LFO | `1sn` | `snc` | LFO ClockSnc |
| LFO | `1mo` | `mod` | LFO Amount |
| LFO | `1wa` | `wav` | LFO Waveform |
| LFO | `1rt` | `rtg` | LFO Retriggr |
| LFO | `1of` | `ofs` | LFO Offset |
| Mix | `1vo` | `vol` | Voice Volume |
| Mix | `1pa` | `pan` | Voice Panning |
| Mix | `1sr` | `srt` | Voice SampleRt |
| Mix | `1dr` | `drv` | Voice Overdriv |
| FX | `1p1` | `p1` | Fx Param 1 |
| FX | `1p2` | `p2` | Fx Param 2 |
| FX | `1p3` | `p3` | Fx Param 3 |
| FX | `1p4` | `p4` | Fx Param 4 |

## Voice 2 (drum 2)

| Page | Step `dst` | LFO/velocity `dst` | Edit view |
|---|---|---|---|
| Oscillator | `2co` | `coa` | Oscilltr Coarse |
| Oscillator | `2fi` | `fin` | Oscilltr Fine |
| Oscillator | `2wa` | `wav` | Oscilltr Waveform |
| Oscillator | `2pw` | `pwm` | Oscilltr PulsWdth |
| Amp EG | `2at` | `atk` | Veloc EG Attack |
| Amp EG | `2de` | `dec` | Veloc EG Decay |
| Amp EG | `2sl` | `slp` | Veloc EG Slope |
| Modulation | `2de` | `dec` | Mod EG Decay |
| Modulation | `2sl` | `slp` | Veloc EG Slope |
| Modulation | `2mo` | `mod` | PitchMod Amount |
| Modulation | `2ds` | `dst` | Velocity DstParam |
| Modulation | `2am` | `amt` | Velocity Amount |
| Modulation | `2vo` | `vol` | Velocity Vol mod |
| FM | `2am` | `amt` | FM Amount |
| FM | `2fr` | `frq` | FM Frequncy |
| FM | `2fw` | `fwv` | Oscilltr Waveform |
| FM | `2mo` | `mod` | Oscilltr Mode |
| Transient | `2cw` | `cwv` | Transnt Waveform |
| Transient | `2vo` | `vol` | Transnt Volume |
| Transient | `2fr` | `frq` | Transnt Frequncy |
| Filter | `2fr` | `frq` | Filter Frequncy |
| Filter | `2re` | `res` | Filter Resnance |
| Filter | `2ty` | `typ` | Filter Type |
| Filter | `2dr` | `drv` | Filter Overdriv |
| LFO | `2fr` | `frq` | LFO Frequncy |
| LFO | `2sn` | `snc` | LFO ClockSnc |
| LFO | `2mo` | `mod` | LFO Amount |
| LFO | `2wa` | `wav` | LFO Waveform |
| LFO | `2rt` | `rtg` | LFO Retriggr |
| LFO | `2of` | `ofs` | LFO Offset |
| Mix | `2vo` | `vol` | Voice Volume |
| Mix | `2pa` | `pan` | Voice Panning |
| Mix | `2sr` | `srt` | Voice SampleRt |
| Mix | `2dr` | `drv` | Voice Overdriv |
| FX | `2p1` | `p1` | Fx Param 1 |
| FX | `2p2` | `p2` | Fx Param 2 |
| FX | `2p3` | `p3` | Fx Param 3 |
| FX | `2p4` | `p4` | Fx Param 4 |

## Voice 3 (drum 3)

| Page | Step `dst` | LFO/velocity `dst` | Edit view |
|---|---|---|---|
| Oscillator | `3co` | `coa` | Oscilltr Coarse |
| Oscillator | `3fi` | `fin` | Oscilltr Fine |
| Oscillator | `3wa` | `wav` | Oscilltr Waveform |
| Oscillator | `3pw` | `pwm` | Oscilltr PulsWdth |
| Amp EG | `3at` | `atk` | Veloc EG Attack |
| Amp EG | `3de` | `dec` | Veloc EG Decay |
| Amp EG | `3sl` | `slp` | Veloc EG Slope |
| Modulation | `3de` | `dec` | Mod EG Decay |
| Modulation | `3sl` | `slp` | Veloc EG Slope |
| Modulation | `3mo` | `mod` | PitchMod Amount |
| Modulation | `3ds` | `dst` | Velocity DstParam |
| Modulation | `3am` | `amt` | Velocity Amount |
| Modulation | `3vo` | `vol` | Velocity Vol mod |
| FM | `3am` | `amt` | FM Amount |
| FM | `3fr` | `frq` | FM Frequncy |
| FM | `3fw` | `fwv` | Oscilltr Waveform |
| FM | `3mo` | `mod` | Oscilltr Mode |
| Transient | `3cw` | `cwv` | Transnt Waveform |
| Transient | `3vo` | `vol` | Transnt Volume |
| Transient | `3fr` | `frq` | Transnt Frequncy |
| Filter | `3fr` | `frq` | Filter Frequncy |
| Filter | `3re` | `res` | Filter Resnance |
| Filter | `3ty` | `typ` | Filter Type |
| Filter | `3dr` | `drv` | Filter Overdriv |
| LFO | `3fr` | `frq` | LFO Frequncy |
| LFO | `3sn` | `snc` | LFO ClockSnc |
| LFO | `3mo` | `mod` | LFO Amount |
| LFO | `3wa` | `wav` | LFO Waveform |
| LFO | `3rt` | `rtg` | LFO Retriggr |
| LFO | `3of` | `ofs` | LFO Offset |
| Mix | `3vo` | `vol` | Voice Volume |
| Mix | `3pa` | `pan` | Voice Panning |
| Mix | `3sr` | `srt` | Voice SampleRt |
| Mix | `3dr` | `drv` | Voice Overdriv |
| FX | `3p1` | `p1` | Fx Param 1 |
| FX | `3p2` | `p2` | Fx Param 2 |
| FX | `3p3` | `p3` | Fx Param 3 |
| FX | `3p4` | `p4` | Fx Param 4 |

## Voice 4 (snare/clap)

| Page | Step `dst` | LFO/velocity `dst` | Edit view |
|---|---|---|---|
| Oscillator | `4co` | `coa` | Oscilltr Coarse |
| Oscillator | `4fi` | `fin` | Oscilltr Fine |
| Oscillator | `4no` | `noi` | Noise Frequncy |
| Oscillator | `4mi` | `mix` | Nois/Osc Mix |
| Oscillator | `4wa` | `wav` | Oscilltr Waveform |
| Oscillator | `4pw` | `pwm` | Oscilltr PulsWdth |
| Amp EG | `4at` | `atk` | Veloc EG Attack |
| Amp EG | `4de` | `dec` | Veloc EG Decay |
| Amp EG | `4rp` | `rpt` | Veloc EG RepeatCt |
| Amp EG | `4sl` | `slp` | Veloc EG Slope |
| Modulation | `4de` | `dec` | Mod EG Decay |
| Modulation | `4sl` | `slp` | Veloc EG Slope |
| Modulation | `4mo` | `mod` | PitchMod Amount |
| Modulation | `4ds` | `dst` | Velocity DstParam |
| Modulation | `4am` | `amt` | Velocity Amount |
| Modulation | `4vo` | `vol` | Velocity Vol mod |
| Transient | `4cw` | `cwv` | Transnt Waveform |
| Transient | `4vo` | `vol` | Transnt Volume |
| Transient | `4fr` | `frq` | Transnt Frequncy |
| Filter | `4fr` | `frq` | Filter Frequncy |
| Filter | `4re` | `res` | Filter Resnance |
| Filter | `4ty` | `typ` | Filter Type |
| Filter | `4dr` | `drv` | Filter Overdriv |
| LFO | `4fr` | `frq` | LFO Frequncy |
| LFO | `4sn` | `snc` | LFO ClockSnc |
| LFO | `4mo` | `mod` | LFO Amount |
| LFO | `4wa` | `wav` | LFO Waveform |
| LFO | `4rt` | `rtg` | LFO Retriggr |
| LFO | `4of` | `ofs` | LFO Offset |
| Mix | `4vo` | `vol` | Voice Volume |
| Mix | `4pa` | `pan` | Voice Panning |
| Mix | `4sr` | `srt` | Voice SampleRt |
| Mix | `4dr` | `drv` | Voice Overdriv |
| FX | `4p1` | `p1` | Fx Param 1 |
| FX | `4p2` | `p2` | Fx Param 2 |
| FX | `4p3` | `p3` | Fx Param 3 |
| FX | `4p4` | `p4` | Fx Param 4 |

## Voice 5 (cymbal)

| Page | Step `dst` | LFO/velocity `dst` | Edit view |
|---|---|---|---|
| Oscillator | `5co` | `coa` | Oscilltr Coarse |
| Oscillator | `5fi` | `fin` | Oscilltr Fine |
| Oscillator | `5wa` | `wav` | Oscilltr Waveform |
| Oscillator | `5pw` | `pwm` | Oscilltr PulsWdth |
| Amp EG | `5at` | `atk` | Veloc EG Attack |
| Amp EG | `5de` | `dec` | Veloc EG Decay |
| Amp EG | `5rp` | `rpt` | Veloc EG RepeatCt |
| Amp EG | `5sl` | `slp` | Veloc EG Slope |
| Modulation | `5ds` | `dst` | Velocity DstParam |
| Modulation | `5am` | `amt` | Velocity Amount |
| Modulation | `5vo` | `vol` | Velocity Vol mod |
| FM | `5f1` | `f1` | Mod Osc Freqcy 1 |
| FM | `5f2` | `f2` | Mod Osc Freqcy 2 |
| FM | `5g1` | `g1` | Mod Osc Gain 1 |
| FM | `5g2` | `g2` | Mod Osc Gain 2 |
| FM | `5fw` | `fwv` | Oscilltr Waveform |
| FM | `5fw` | `fwv` | Oscilltr Waveform |
| Transient | `5cw` | `cwv` | Transnt Waveform |
| Transient | `5vo` | `vol` | Transnt Volume |
| Transient | `5fr` | `frq` | Transnt Frequncy |
| Filter | `5fr` | `frq` | Filter Frequncy |
| Filter | `5re` | `res` | Filter Resnance |
| Filter | `5ty` | `typ` | Filter Type |
| Filter | `5dr` | `drv` | Filter Overdriv |
| LFO | `5fr` | `frq` | LFO Frequncy |
| LFO | `5sn` | `snc` | LFO ClockSnc |
| LFO | `5mo` | `mod` | LFO Amount |
| LFO | `5wa` | `wav` | LFO Waveform |
| LFO | `5rt` | `rtg` | LFO Retriggr |
| LFO | `5of` | `ofs` | LFO Offset |
| Mix | `5vo` | `vol` | Voice Volume |
| Mix | `5pa` | `pan` | Voice Panning |
| Mix | `5sr` | `srt` | Voice SampleRt |
| Mix | `5dr` | `drv` | Voice Overdriv |
| FX | `5p1` | `p1` | Fx Param 1 |
| FX | `5p2` | `p2` | Fx Param 2 |
| FX | `5p3` | `p3` | Fx Param 3 |
| FX | `5p4` | `p4` | Fx Param 4 |

## Voice 6 (HH open and closed)

| Page | Step `dst` | LFO/velocity `dst` | Edit view |
|---|---|---|---|
| Oscillator | `6co` | `coa` | Oscilltr Coarse |
| Oscillator | `6fi` | `fin` | Oscilltr Fine |
| Oscillator | `6wa` | `wav` | Oscilltr Waveform |
| Oscillator | `6pw` | `pwm` | Oscilltr PulsWdth |
| Amp EG | `6at` | `atk` | Veloc EG Attack |
| Amp EG | `6d1` | `d1` | Veloc EG Dcy Clsd |
| Amp EG | `6d2` | `d2` | Veloc EG Dcy Open |
| Amp EG | `6sl` | `slp` | Veloc EG Slope |
| Modulation | `6ds` | `dst` | Velocity DstParam |
| Modulation | `6am` | `amt` | Velocity Amount |
| Modulation | `6vo` | `vol` | Velocity Vol mod |
| FM | `6f1` | `f1` | Mod Osc Freqcy 1 |
| FM | `6f2` | `f2` | Mod Osc Freqcy 2 |
| FM | `6g1` | `g1` | Mod Osc Gain 1 |
| FM | `6g2` | `g2` | Mod Osc Gain 2 |
| FM | `6fw` | `fwv` | Oscilltr Waveform |
| FM | `6fw` | `fwv` | Oscilltr Waveform |
| Transient | `6cw` | `cwv` | Transnt Waveform |
| Transient | `6vo` | `vol` | Transnt Volume |
| Transient | `6fr` | `frq` | Transnt Frequncy |
| Filter | `6fr` | `frq` | Filter Frequncy |
| Filter | `6re` | `res` | Filter Resnance |
| Filter | `6ty` | `typ` | Filter Type |
| Filter | `6dr` | `drv` | Filter Overdriv |
| LFO | `6fr` | `frq` | LFO Frequncy |
| LFO | `6sn` | `snc` | LFO ClockSnc |
| LFO | `6mo` | `mod` | LFO Amount |
| LFO | `6wa` | `wav` | LFO Waveform |
| LFO | `6rt` | `rtg` | LFO Retriggr |
| LFO | `6of` | `ofs` | LFO Offset |
| Mix | `6vo` | `vol` | Voice Volume |
| Mix | `6pa` | `pan` | Voice Panning |
| Mix | `6sr` | `srt` | Voice SampleRt |
| Mix | `6dr` | `drv` | Voice Overdriv |
| FX | `6p1` | `p1` | Fx Param 1 |
| FX | `6p2` | `p2` | Fx Param 2 |
| FX | `6p3` | `p3` | Fx Param 3 |
| FX | `6p4` | `p4` | Fx Param 4 |
