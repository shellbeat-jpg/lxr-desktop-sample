/*
 * sampleFlash.h
 *
 * Sample storage in the internal flash (sectors 6 and 7, 512 KB).
 * Step 1 of the sample extension: import WAV files from /SAMPLES on the
 * SD card into flash and verify them. Playback is not part of this step.
 *
 * Modified firmware - not an official Erica Synths release.
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef SAMPLEFLASH_H_
#define SAMPLEFLASH_H_

#include <stdint.h>

/* Flash region reserved for samples: sector 6 + sector 7 (single-bank layout). */
#define SF_REGION_START      ((uint32_t)0x08080000u)
#define SF_REGION_END        ((uint32_t)0x08100000u)   /* exclusive */
#define SF_HEADER_SIZE       ((uint32_t)0x00000800u)   /* 2 KB: sample table + user wave table */
#define SF_DATA_START        (SF_REGION_START + SF_HEADER_SIZE)
#define SF_MAX_SAMPLES       31u
/* MOD: user waveforms from /SAMPLES/WAVES: single cycles of SF_WAVE_POINTS int16
 * points (resampled at the import), for the oscillators and the LFOs */
#define SF_MAX_WAVES         8u
#define SF_WAVE_POINTS       1024u

#define SF_FLAG_LOOP         0x01u   /* from /SAMPLES/LOOPS: loops between start and end */

typedef struct
{
	char     name[13];   /* 8.3 file name, zero terminated */
	uint8_t  flags;      /* SF_FLAG_... */
	uint8_t  pad[2];
	uint32_t offset;     /* absolute flash address of int16 mono PCM */
	uint32_t frames;     /* number of int16 frames */
	uint32_t rate;       /* original sample rate in Hz (no conversion yet) */
	uint32_t checksum;   /* FNV-1a over the PCM bytes as written */
} SampleFlashEntry;      /* 32 bytes */

typedef struct
{
	char     magic[8];   /* "LXRSMP02" - written last, marks a complete import */
	uint32_t count;
	uint32_t signature;  /* hash of the /SAMPLES file list (names + sizes) */
	uint32_t totalFrames;
	uint32_t reserved[3];  /* skipped files: [0] not 16 bit, [1] did not fit */
	SampleFlashEntry entry[SF_MAX_SAMPLES];
} SampleFlashHeader;     /* 32 + 31*32 = 1024 bytes */

/* MOD: user waveforms, at SF_REGION_START + 1 KB (header "LXRSMP03" only) */
typedef struct
{
	char     magic[8];   /* "LXRWAV01" */
	uint32_t count;
	uint32_t offset;     /* absolute flash address of count * SF_WAVE_POINTS int16 */
	uint32_t checksum;   /* FNV-1a over the points */
	uint32_t reserved;
	char     name[SF_MAX_WAVES][16];   /* 8.3 file names */
} SampleFlashWaves;      /* 32 + 8*16 = 160 bytes */

/* MOD: wave sets of the wavetable engine (wtBuild.h): set 0 is built from the
 * user waves, sets 1..4 from the files in /SAMPLES/TABLES. Directory at
 * SF_REGION_START + 0x600; the frames lie in the sample memory. */
#define SF_MAX_TABLES        5u
typedef struct
{
	char     name[16];   /* 8.3 file name ("" for set 0) */
	uint32_t offset;     /* absolute flash address of frames * WT_FRAME_SIZE int16 */
	uint32_t frames;     /* 0 = no set */
	uint32_t checksum;   /* FNV-1a over the frames */
} SampleFlashTable;      /* 28 bytes */
typedef struct
{
	char     magic[8];   /* "LXRTBL01" */
	uint32_t count;      /* sets with frames */
	uint32_t reserved;
	SampleFlashTable table[SF_MAX_TABLES];
} SampleFlashTables;     /* 16 + 5*28 = 156 bytes */

/* Result codes, also shown on the LCD. */
typedef enum
{
	SF_OK_UNCHANGED = 0,  /* flash already matches /SAMPLES, nothing written */
	SF_OK_IMPORTED,       /* new import written and verified */
	SF_NO_FOLDER,         /* no /SAMPLES folder - feature unused, flash untouched */
	SF_ERR_FORMAT,        /* a WAV file is not 16-bit PCM mono/stereo */
	SF_ERR_TOO_BIG,       /* not even one file fits (511 KB = 5.9 s mono @44.1k) */
	SF_ERR_READ,          /* SD read error */
	SF_ERR_FLASH,         /* erase/program error or guard tripped */
	SF_ERR_VERIFY         /* read-back checksum mismatch */
} SampleFlashResult;

/* Call once at boot after the SD card is mounted and BEFORE the sound engine
 * starts (erasing a sector stalls code running from flash for ~2 s).
 * Shows progress and the result on the LCD. */
SampleFlashResult sampleFlash_importAtBoot(void);

/* Call once at boot, with or without SD card, after the import: makes the
 * stored samples available to the audio engine and shows "Samples in flash"
 * with the count (and skipped files) on the LCD. */
void sampleFlash_showStatus(void);

/* Read access for later steps (playback). NULL / 0 if nothing valid stored. */
const SampleFlashHeader* sampleFlash_getHeader(void);
uint8_t sampleFlash_getCount(void);

/* Fast access for the audio engine (no validity check per call): the header is
 * validated once by sampleFlash_importAtBoot(). Returns NULL for an empty or
 * out-of-range slot (slot 0 = first file). */
const SampleFlashEntry* sampleFlash_entry(uint8_t slot);

/* Modulated waveform of a main oscillator. base = the waveform set in the kit,
 * v = the modulated value [0:1] (value/127). A sample as base: v*127 is the
 * waveform, clamped to s1..sN. A built-in waveform as base: as before
 * (0..numOscWaves-1). */
uint8_t sampleFlash_modulatedWave(uint8_t base, float v, uint8_t numOscWaves);

/* Sample range for LFO/velocity modulation of the waveform when base is a
 * sample: the range is base .. base+span, span = amount*127 (the modulation
 * amount counts the further samples). val [0:1] (LFO, velocity) is quantized
 * evenly onto the range. Returns the modulated value for the handler (w/127). */
float sampleFlash_rangeValue(uint8_t base, float amount, float val);

/* MOD: user waveforms: number stored, the SF_WAVE_POINTS points of wave k
 * (NULL if not stored), its file name ("" if not stored) */
uint8_t sampleFlash_getWaveCount(void);
const int16_t* sampleFlash_wave(uint8_t k);
const char* sampleFlash_waveName(uint8_t k);
/* MOD: wave set k (0 = from the user waves, 1..4 = TABLES): its frames and their
 * number, NULL / 0 if not stored; the file name */
const int16_t* sampleFlash_table(uint8_t k, uint8_t* frames);
const char* sampleFlash_tableName(uint8_t k);

/* Flash size of the chip in KB (from its flash size register). */
uint16_t sampleFlash_chipKB(void);

#endif /* SAMPLEFLASH_H_ */
