#!/bin/sh
# MOD: host tests for the sample extension (Linux, gcc). Simulates the
# STM32 flash at its real address (0x08080000) and the SD card in ./sd.
# Usage: sh tests/host/run.sh   (from the repository root)
set -e
R=$(cd "$(dirname "$0")/../.." && pwd)
T=$(mktemp -d)
cd "$T"
sed 's/^int main(){/int harness_main(){/; s/^static char root\[512\];/char root[512]=".\/sd";/' "$R/tests/host/harness.c" > harness_lib.c
gcc -O1 -w "-DSF_READ_FLASH_KB()=2048" -I"$R/tests/host/stub" -I"$R/Core/Inc" -no-pie -o import_test "$R/tests/host/harness.c" "$R/tests/host/dirwrap.c" "$R/Core/Src/SampleRom/sampleFlash.c"
gcc -O1 -w -DIN_ITCM_RAM= "-DSF_READ_FLASH_KB()=2048" -I"$R/tests/host/stub" -I"$R/tests/host/stub2" -I"$R/Core/Inc" -no-pie -o play_test \
    "$R/tests/host/play.c" harness_lib.c "$R/tests/host/dirwrap.c" \
    "$R/Core/Src/Soundengine/DSPAudio/Oscillator.c" "$R/Core/Src/SampleRom/sampleFlash.c" -lm
echo "== Import (Schritt 1)"; ./import_test
echo "== Wiedergabe (Schritt 2)"; ./play_test
