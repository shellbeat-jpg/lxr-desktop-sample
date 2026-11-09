# Vox engine tables (prototype)

Offline scripts for `Core/Inc/Soundengine/DSPAudio/VoxCore.h` (Python 3, numpy, scipy):

- `fit.py` fits the area functions of 8 vowels (u o ɔ a ɛ e i y) to their formants F1–F3 with
  the same tube as the engine (`tract.py`, 22 sections at 22.07 kHz) → `vowels.json`.
- `syr.py` measures period and range of the syrinx oscillator (Gardner/Mindlin normal form) on an
  8 × 8 grid of drive α and tension β → `syr.json`.
- `gen_tables.py` writes both as C tables (`tables.inc`), pasted into `VoxCore.h`.

Host test with demo WAVs: `gcc -O2 -ICore/Inc tests/host/vox.c -lm && ./a.out <dir>`.
