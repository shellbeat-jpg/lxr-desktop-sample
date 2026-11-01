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
#define SF_HEADER_SIZE       ((uint32_t)0x00000400u)   /* 1 KB header/table */
#define SF_DATA_START        (SF_REGION_START + SF_HEADER_SIZE)
#define SF_MAX_SAMPLES       16u

typedef struct
{
	char     name[16];   /* 8.3 file name, zero terminated */
	uint32_t offset;     /* absolute flash address of int16 mono PCM */
	uint32_t frames;     /* number of int16 frames */
	uint32_t rate;       /* original sample rate in Hz (no conversion yet) */
	uint32_t checksum;   /* FNV-1a over the PCM bytes as written */
} SampleFlashEntry;      /* 32 bytes */

typedef struct
{
	char     magic[8];   /* "LXRSMP01" - written last, marks a complete import */
	uint32_t count;
	uint32_t signature;  /* hash of the /SAMPLES file list (names + sizes) */
	uint32_t totalFrames;
	uint32_t reserved[3];
	SampleFlashEntry entry[SF_MAX_SAMPLES];
} SampleFlashHeader;     /* 32 + 16*32 = 544 bytes */

/* Result codes, also shown on the LCD. */
typedef enum
{
	SF_OK_UNCHANGED = 0,  /* flash already matches /SAMPLES, nothing written */
	SF_OK_IMPORTED,       /* new import written and verified */
	SF_NO_FOLDER,         /* no /SAMPLES folder - feature unused, flash untouched */
	SF_ERR_FORMAT,        /* a WAV file is not 16-bit PCM mono/stereo */
	SF_ERR_TOO_BIG,       /* total data does not fit into 511 KB */
	SF_ERR_READ,          /* SD read error */
	SF_ERR_FLASH,         /* erase/program error or guard tripped */
	SF_ERR_VERIFY         /* read-back checksum mismatch */
} SampleFlashResult;

/* Call once at boot after the SD card is mounted and BEFORE the sound engine
 * starts (erasing a sector stalls code running from flash for ~2 s).
 * Shows progress and the result on the LCD. */
SampleFlashResult sampleFlash_importAtBoot(void);

/* Read access for later steps (playback). NULL / 0 if nothing valid stored. */
const SampleFlashHeader* sampleFlash_getHeader(void);
uint8_t sampleFlash_getCount(void);

#endif /* SAMPLEFLASH_H_ */
