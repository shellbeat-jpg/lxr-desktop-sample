# Session Handoff: OE02-Samples (LXR-02 custom firmware)

Nov 3, 2026

## Context

v1.2.9 is released and marked Latest; both branches are at commit `123b352`.

| Item | Value |
| --- | --- |
| Project | Custom firmware "OE02-Samples" for the Erica Synths LXR-02: a fork of OpenElixir-02 v1.2 by Moritz Waser (GPL-3) with user samples and sequencer, FX and modulation features |
| Repo | `github.com/shellbeat-jpg/lxr-desktop-sample`; branches `oe-samples` and `main` stay identical (push `oe-samples oe-samples:main`) |
| Commit identity | `shellbeat-jpg` / `307613755+shellbeat-jpg@users.noreply.github.com` |
| Build | `cmake --build build/release` → `build/release/LXR_OE02-Samples_V<version>.img`; the version comes from `project(VERSION)` in `CMakeLists.txt` |
| Line endings | Many files are CRLF: edit with a helper that normalizes to LF, asserts a unique match, restores CRLF |
| Language | Replies in German; docs and release notes in English; display terms uppercase in backticks (`RST`, `LFO>`) |

## Main goal

Extend the LXR-02 firmware step by step with practical features. This session covered the 1.2.4–1.2.9 releases, the documentation and feature overviews, and new trigger logic (trigger tracks, LFO → track start).

## Approaches tried

In chronological order; failed attempts are in bold.

| # | Topic | What happened | Resolution |
| --- | --- | --- | --- |
| 1 | Black display after the 1.2.2 update | **Stopping the display interrupt before re-init didn't help.** | 1.2.4: extra `lcd_init()` calls removed (one init, as in 1.2.1); wait about 10 s between power off and on |
| 2 | Pitch jump of voice 1 while turning `VMO` | No cause found by code reading. **Never confirmed fixed.** | 1.2.5: the voice morph sends in small steps |
| 3 | Kit morph extras (SHIFT edit of the target, `MRV`, LFO range display) | **The global settings file fallback was wrong at first.** | Fixed in 1.2.4 |
| 4 | Step grid per track (1.2.7) | **Compile error: helpers placed before `preset_fwrite`.** | Own tick counter per track (96 ppq, `seq_gridTicks`) |
| 5 | Two FX slots (1.2.8) | **Encoder navigation skipped `fbk`/`sya`: checks used the value before the increment.** | FX1 = kit FX, FX2 = project FX; `ORD`, `SYA`/`SYB`, FXB routing |
| 6 | FX sends, first design `OUT` = `1+A` … `R2B` plus `SND` | **`SND` only worked with a combined output; with `OUT` = `St1` it did nothing.** | `SNA`/`SNB` per voice, always active besides `OUT`; kit parts `SNA`/`ORD`/`SNB`, default 0 |
| 7 | Knobs on special parameters | **Knobs changed `CHK` (a bit mask) and others as raw numbers: tracks 1+2 = 3, +1 = 4 = track 3.** | Knobs routed through the encoder logic |
| 8 | Release tags | **Tag `v1.2.6` pointed at `main` (version 1.2.8), so the version check failed. The proxy blocks tag pushes, tag deletes via the API and release asset uploads.** | Branches `release-1.2.6`/`-1.2.7` at the version commits; the user tags in the GitHub UI. Assets and text via the manual workflow `release-overview.yml` (REST dispatch works) |
| 9 | Trigger tracks `RST`/`MUT`/`CLK` (1.2.9) | **`CLK` skipped step 1 at the start.** | Fixed; all three work |
| 10 | LFO → track start | **Built the wrong way round: the page's track was started by an LFO, so a trigger track with an LFO setting went silent, with no target choice. A source-side exclusivity was then added by mistake.** | The LFO of the page's voice starts a chosen target `T1`…`T7`; exclusivity is per target |
| 11 | Settings reset when clicking through LAST STEP views | **The handler runs with `inc = 0` after a click; trigger/LFO code treated 0 as a left turn (e.g. `NOW` → `NXT`).** | A click only switches the view |
| 12 | LFO 3 → LFO 1 `FRQ` had no effect (original firmware) | **Targets were reset every block before LFOs 1–6 were calculated, undoing modulation of lower-numbered LFOs.** | All LFOs calculated before the reset, dispatched after it |

## Key decisions

- **Releases:** the user creates tags and releases. The workflow builds and attaches the `.img` and `OE02-Samples_Overview_EN/DE.md`, with notes from `.github/releases/v<version>.md`. Remove test markers ("(test version)", "in test", "(T)") before tagging.
- **Sends:** `SNA`/`SNB` are meant for `OUT` = `St1`…`R2`. With `OUT` = `FX`/`FXB` they raise the level in the same slot or also feed the other one.
- **FX2:** project FX, stored globally and in the project file. `ORD` ("FX routing") lives in the kit and is set only on the FX1 setup page.
- **Trigger tracks:** stored per pattern (pattern file block `TRG1`, value = mode<<4 | target).
  - Any velocity counts as a trigger; at velocity 0 the step plays no sound of its own.
  - `MUT` mutes step by step while trigger steps are active.
  - `CLK`: the target only advances on impulses.
- **LFO start:** stored per pattern (block `LFT1`, value = target 1..7 | `0x10` for NOW).
  - Counted starts: a new cycle, a restart by `RTG`, a one-shot after its `OFS` delay.
  - A restart caused by the target's own trigger is ignored (no feedback loop).
  - `NXT` = next step of the target's grid, `NOW` = at once; the target plays to its end, then waits.
- **Exclusivity:** each target has one source; targets in use are skipped when choosing. A source track may have one `Trg` target and one `LFO>` target. No multiple `Trg` targets.
- **UI:** LAST STEP click cycle = length → grid → `Trg` mode → (target) → `LFO>` target → (`NXT`/`NOW`). The top row always shows "Trk: n" in the Trg/LFO views.
- **No NRPN** for the new features (declined by the user).
- **Docs:** README with the EN overview, `docs/OVERVIEW.md` / `docs/OVERVIEW.de.md` (per-voice ranges as "(`MOD` / Voice 1–6)"), `docs/SAMPLES.md` (user guide incl. a generative 3-LFO example), source notes in `ENGINES.md` / `README_OPENELIXIR.md` linking OpenElixir-02 v1.2.

## Next steps

- [ ] Build interactive diagrams: LFO settings interplay (`WAV`/`FRQ`/`SNC`/`RTG`/`OFS`/`CYC`/`MOD` range, LFO start `NXT` vs. `NOW` on a timeline) and trigger-track modes `RST`/`MUT`/`CLK` against a reference track
- [ ] Test 1.2.9 on the device: LFO → LFO modulation in all directions (how strongly `MOD` acts on `FRQ`), trigger tracks with different grids and across pattern changes, `CLK` targets with flams/ratchets, the generative 3-LFO example
- [ ] Check known limitations: negative-shift hits can be missing at jump points of `RST`/`CLK` targets; after a pattern change controlled targets return to the pattern clock and LFO-started targets wait for the next start; unsaved pattern edits are lost on a pattern change (original behaviour)
- [ ] Confirm whether the voice-1 pitch jump on `VMO` is gone
- [ ] Keep resources in mind: about 41 KB RAM free (≈ 90 % used), ITCM about 3 KB free
- [ ] Each new release: bump `VERSION` in `CMakeLists.txt`, add `.github/releases/v<x>.md`; the user tags `main`; verify with `gh run watch` and `gh api repos/shellbeat-jpg/lxr-desktop-sample/releases/tags/v<x>`
