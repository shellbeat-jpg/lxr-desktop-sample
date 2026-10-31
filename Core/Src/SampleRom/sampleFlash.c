/*
 * sampleFlash.c
 *
 * Step 1 of the sample extension: import WAV files from /SAMPLES on the SD
 * card into the internal flash (sectors 6 + 7) and verify them.
 *
 * Safety rules (the bootloader lives in sector 0):
 *  - Only FLASH_SECTOR_6 and FLASH_SECTOR_7 can ever be erased here
 *    (hard check in sf_eraseSector()).
 *  - Every program call checks that the address lies inside
 *    [SF_REGION_START, SF_REGION_END).
 *  - The header magic is written LAST, so an interrupted import (power
 *    loss) leaves an invalid header and is simply redone on the next boot.
 *  - Nothing is erased if the stored signature already matches /SAMPLES.
 *
 * Modified firmware - not an official Erica Synths release.
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "SampleRom/sampleFlash.h"
#include "main.h"
#include "ff.h"
#include "lcd.h"
#include <string.h>
#include <stdio.h>

#define SF_MAGIC        "LXRSMP01"
#define SF_FOLDER       "/SAMPLES"
#define SF_READ_FRAMES  256u

/* the header must fit into the reserved header area */
typedef char sf_headerFits[(sizeof(SampleFlashHeader) <= SF_HEADER_SIZE) ? 1 : -1];

typedef struct
{
	char     name[13];
	uint32_t fsize;
	uint16_t channels;
	uint32_t rate;
	uint32_t dataOffset;   /* byte offset of PCM data in the file */
	uint32_t dataBytes;
	uint32_t frames;       /* mono frames after down-mix */
} sf_FileInfo;

static sf_FileInfo sf_files[SF_MAX_SAMPLES];
static SampleFlashHeader sf_newHeader;                 /* built in RAM, written last */
static int16_t sf_readBuf[SF_READ_FRAMES * 2u];        /* stereo frames */
static FIL sf_file;

static const SampleFlashHeader* const sf_flashHeader =
		(const SampleFlashHeader*)SF_REGION_START;

/*----------------------------------------------------------------------------*/
static void sf_lcd(const char* line1, const char* line2)
{
	lcd_clear();
	lcd_home();
	lcd_string(line1);
	lcd_setcursor(0, 2);
	lcd_string(line2);
}
/*----------------------------------------------------------------------------*/
static uint32_t sf_fnv(uint32_t h, const uint8_t* p, uint32_t n)
{
	while(n--)
	{
		h ^= *p++;
		h *= 16777619u;
	}
	return h;
}
/*----------------------------------------------------------------------------*/
static uint8_t sf_headerValid(const SampleFlashHeader* h)
{
	if(memcmp(h->magic, SF_MAGIC, 8) != 0) return 0;
	if(h->count == 0 || h->count > SF_MAX_SAMPLES) return 0;
	return 1;
}
/*----------------------------------------------------------------------------*/
const SampleFlashHeader* sampleFlash_getHeader(void)
{
	return sf_headerValid(sf_flashHeader) ? sf_flashHeader : NULL;
}
uint8_t sampleFlash_getCount(void)
{
	return sf_headerValid(sf_flashHeader) ? (uint8_t)sf_flashHeader->count : 0;
}
/*----------------------------------------------------------------------------*/
/* Flash access - the ONLY functions that touch the flash controller.          */
/*----------------------------------------------------------------------------*/
static uint8_t sf_eraseSector(uint32_t sector)
{
	/* hard guard: never anything but the two sample sectors */
	if(sector != FLASH_SECTOR_6 && sector != FLASH_SECTOR_7) return 0;

	FLASH_EraseInitTypeDef er;
	uint32_t sectorError = 0;
	er.TypeErase    = FLASH_TYPEERASE_SECTORS;
	er.Sector       = sector;
	er.NbSectors    = 1;
	er.VoltageRange = FLASH_VOLTAGE_RANGE_3;
	if(HAL_FLASHEx_Erase(&er, &sectorError) != HAL_OK) return 0;
	return (sectorError == 0xFFFFFFFFu);
}
/*----------------------------------------------------------------------------*/
static uint8_t sf_programWord(uint32_t addr, uint32_t word)
{
	if(addr < SF_REGION_START || addr > (SF_REGION_END - 4u) || (addr & 3u)) return 0;
	if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, (uint64_t)word) != HAL_OK) return 0;
	return 1;
}
/*----------------------------------------------------------------------------*/
static uint8_t sf_programBytes(uint32_t addr, const void* src, uint32_t bytes)
{
	/* bytes must be a multiple of 4 */
	const uint8_t* p = (const uint8_t*)src;
	for(uint32_t i = 0; i < bytes; i += 4u)
	{
		uint32_t w;
		memcpy(&w, p + i, 4);
		if(!sf_programWord(addr + i, w)) return 0;
	}
	return 1;
}
/*----------------------------------------------------------------------------*/
static void sf_invalidateCache(void)
{
	/* flash is read through the D-cache on the M7 - make sure we see new data */
	SCB_CleanInvalidateDCache();
}
/*----------------------------------------------------------------------------*/
/* WAV parsing                                                                */
/*----------------------------------------------------------------------------*/
static uint32_t sf_le32(const uint8_t* b) { return b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24); }
static uint16_t sf_le16(const uint8_t* b) { return (uint16_t)(b[0] | (b[1] << 8)); }

/* returns SF_OK_IMPORTED on success (used as "ok" here) */
static SampleFlashResult sf_parseWav(sf_FileInfo* fi)
{
	char path[24];
	UINT br;
	uint8_t hdr[12];
	snprintf(path, sizeof(path), SF_FOLDER "/%s", fi->name);
	if(f_open(&sf_file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK) return SF_ERR_READ;

	SampleFlashResult res = SF_ERR_FORMAT;
	if(f_read(&sf_file, hdr, 12, &br) != FR_OK || br != 12) { res = SF_ERR_READ; goto done; }
	if(memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) goto done;

	uint8_t haveFmt = 0;
	uint16_t bits = 0, format = 0;
	for(;;)
	{
		uint8_t ch[8];
		if(f_read(&sf_file, ch, 8, &br) != FR_OK || br != 8) goto done;  /* no data chunk */
		uint32_t len = sf_le32(ch + 4);
		if(memcmp(ch, "fmt ", 4) == 0)
		{
			uint8_t fmt[16];
			if(len < 16) goto done;
			if(f_read(&sf_file, fmt, 16, &br) != FR_OK || br != 16) { res = SF_ERR_READ; goto done; }
			format       = sf_le16(fmt);
			fi->channels = sf_le16(fmt + 2);
			fi->rate     = sf_le32(fmt + 4);
			bits         = sf_le16(fmt + 14);
			haveFmt = 1;
			if(f_lseek(&sf_file, f_tell(&sf_file) + (len - 16) + (len & 1u)) != FR_OK) { res = SF_ERR_READ; goto done; }
		}
		else if(memcmp(ch, "data", 4) == 0)
		{
			if(!haveFmt) goto done;
			/* PCM (1) or WAVE_FORMAT_EXTENSIBLE (0xFFFE), 16 bit, mono/stereo */
			if((format != 1 && format != 0xFFFE) || bits != 16) goto done;
			if(fi->channels != 1 && fi->channels != 2) goto done;
			fi->dataOffset = f_tell(&sf_file);
			fi->dataBytes  = len;
			fi->frames     = len / (2u * fi->channels);
			if(fi->frames == 0) goto done;
			res = SF_OK_IMPORTED;
			goto done;
		}
		else
		{
			if(f_lseek(&sf_file, f_tell(&sf_file) + len + (len & 1u)) != FR_OK) { res = SF_ERR_READ; goto done; }
		}
	}
done:
	f_close(&sf_file);
	return res;
}
/*----------------------------------------------------------------------------*/
static uint8_t sf_isWav(const FILINFO* fno)
{
	if(fno->fattrib & (AM_DIR | AM_HID | AM_SYS)) return 0;
	if(fno->fname[0] == '.' || fno->fname[0] == '_') return 0;   /* macOS "._" files */
	const char* dot = strrchr(fno->fname, '.');
	return dot && (strcmp(dot, ".WAV") == 0 || strcmp(dot, ".wav") == 0);
}
/*----------------------------------------------------------------------------*/
/* scan /SAMPLES, sorted by name; returns count, 0xFF if folder missing */
static uint8_t sf_scanFolder(uint32_t* signature)
{
	DIR dir;
	FILINFO fno;
	uint8_t n = 0;
	if(f_opendir(&dir, SF_FOLDER) != FR_OK) return 0xFF;
	while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
	{
		if(!sf_isWav(&fno) || n >= SF_MAX_SAMPLES) continue;
		memset(&sf_files[n], 0, sizeof(sf_FileInfo));
		strncpy(sf_files[n].name, fno.fname, 12);
		sf_files[n].fsize = fno.fsize;
		n++;
	}
	f_closedir(&dir);

	/* simple insertion sort by name -> deterministic slot order */
	for(uint8_t i = 1; i < n; i++)
	{
		sf_FileInfo t = sf_files[i];
		int8_t j = (int8_t)i - 1;
		while(j >= 0 && strcmp(sf_files[j].name, t.name) > 0) { sf_files[j + 1] = sf_files[j]; j--; }
		sf_files[j + 1] = t;
	}

	uint32_t h = 2166136261u;
	h = sf_fnv(h, (const uint8_t*)SF_MAGIC, 8);
	for(uint8_t i = 0; i < n; i++)
	{
		h = sf_fnv(h, (const uint8_t*)sf_files[i].name, 13);
		h = sf_fnv(h, (const uint8_t*)&sf_files[i].fsize, 4);
	}
	*signature = h;
	return n;
}
/*----------------------------------------------------------------------------*/
/* read one file, down-mix to mono, program it at addr; returns checksum */
static SampleFlashResult sf_writeFile(const sf_FileInfo* fi, uint32_t addr, uint32_t* checksum,
                                      uint8_t index, uint8_t count)
{
	char path[24];
	UINT br;
	uint32_t h = 2166136261u;
	uint32_t framesLeft = fi->frames;
	uint32_t written = 0;
	uint8_t lastPct = 0xFF;
	snprintf(path, sizeof(path), SF_FOLDER "/%s", fi->name);
	if(f_open(&sf_file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK) return SF_ERR_READ;
	if(f_lseek(&sf_file, fi->dataOffset) != FR_OK) { f_close(&sf_file); return SF_ERR_READ; }

	while(framesLeft)
	{
		uint32_t n = framesLeft > SF_READ_FRAMES ? SF_READ_FRAMES : framesLeft;
		uint32_t bytes = n * 2u * fi->channels;
		if(f_read(&sf_file, sf_readBuf, bytes, &br) != FR_OK || br != bytes)
		{
			f_close(&sf_file);
			return SF_ERR_READ;
		}
		if(fi->channels == 2)
		{
			for(uint32_t i = 0; i < n; i++)
				sf_readBuf[i] = (int16_t)(((int32_t)sf_readBuf[2 * i] + sf_readBuf[2 * i + 1]) / 2);
		}
		/* pad an odd last frame with silence so we always write full words */
		uint32_t outBytes = n * 2u;
		if(outBytes & 3u) { sf_readBuf[n] = 0; outBytes += 2u; }

		h = sf_fnv(h, (const uint8_t*)sf_readBuf, outBytes);
		if(!sf_programBytes(addr + written, sf_readBuf, outBytes))
		{
			f_close(&sf_file);
			return SF_ERR_FLASH;
		}
		written += outBytes;
		framesLeft -= n;

		uint8_t pct = (uint8_t)(((fi->frames - framesLeft) * 100u) / fi->frames);
		if(pct / 10 != lastPct / 10)
		{
			char l1[17], l2[17];
			lastPct = pct;
			snprintf(l1, sizeof(l1), "Import %u/%u", index + 1, count);
			snprintf(l2, sizeof(l2), "%-12.12s%3u%%", fi->name, pct);
			sf_lcd(l1, l2);
		}
	}
	f_close(&sf_file);
	*checksum = h;
	return SF_OK_IMPORTED;
}
/*----------------------------------------------------------------------------*/
static void sf_showResult(SampleFlashResult r, const char* name, uint8_t count, uint32_t totalFrames)
{
	char l2[17];
	switch(r)
	{
	case SF_OK_UNCHANGED:
		snprintf(l2, sizeof(l2), "%u stored", count);
		sf_lcd("Samples OK", l2);
		HAL_Delay(800);
		return;
	case SF_OK_IMPORTED:
	{
		/* duration at 44.1 kHz in 1/100 s, the real rate may differ */
		uint32_t cs = (uint32_t)((uint64_t)totalFrames * 100u / 44100u);
		snprintf(l2, sizeof(l2), "%u smp %lu.%02lus", count, (unsigned long)(cs / 100), (unsigned long)(cs % 100));
		sf_lcd("Import+Verify OK", l2);
		break;
	}
	case SF_NO_FOLDER:
		return;
	case SF_ERR_FORMAT:  sf_lcd("Not 16bit WAV:", name); break;
	case SF_ERR_TOO_BIG: sf_lcd("Samples too big", "max 511KB total"); break;
	case SF_ERR_READ:    sf_lcd("SD read error:", name); break;
	case SF_ERR_FLASH:   sf_lcd("Flash error!", "Samples disabled"); break;
	case SF_ERR_VERIFY:  sf_lcd("Verify FAILED:", name); break;
	}
	HAL_Delay(3000);
}
/*----------------------------------------------------------------------------*/
SampleFlashResult sampleFlash_importAtBoot(void)
{
	uint32_t signature = 0;
	uint8_t count = sf_scanFolder(&signature);
	if(count == 0xFF) return SF_NO_FOLDER;          /* feature not used */
	if(count == 0)    { sf_showResult(SF_NO_FOLDER, "", 0, 0); return SF_NO_FOLDER; }

	if(sf_headerValid(sf_flashHeader) && sf_flashHeader->signature == signature)
	{
		sf_showResult(SF_OK_UNCHANGED, "", (uint8_t)sf_flashHeader->count, 0);
		return SF_OK_UNCHANGED;
	}

	/* 1. parse all files and check the total size before touching the flash */
	uint32_t needed = 0;
	for(uint8_t i = 0; i < count; i++)
	{
		SampleFlashResult r = sf_parseWav(&sf_files[i]);
		if(r != SF_OK_IMPORTED) { sf_showResult(r, sf_files[i].name, 0, 0); return r; }
		needed += (sf_files[i].frames * 2u + 3u) & ~3u;
	}
	if(needed > (SF_REGION_END - SF_DATA_START))
	{
		sf_showResult(SF_ERR_TOO_BIG, "", 0, 0);
		return SF_ERR_TOO_BIG;
	}

	/* 2. erase - code runs from flash, so the CPU stalls ~1-2 s per sector */
	sf_lcd("Samples changed", "Erasing flash...");
	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
	SampleFlashResult res = SF_OK_IMPORTED;
	if(!sf_eraseSector(FLASH_SECTOR_6)) res = SF_ERR_FLASH;
	if(res == SF_OK_IMPORTED && needed + SF_HEADER_SIZE > 0x40000u)   /* data reaches sector 7 */
	{
		if(!sf_eraseSector(FLASH_SECTOR_7)) res = SF_ERR_FLASH;
	}

	/* 3. program the PCM data */
	memset(&sf_newHeader, 0, sizeof(sf_newHeader));
	uint32_t addr = SF_DATA_START;
	uint32_t totalFrames = 0;
	const char* failName = "";
	for(uint8_t i = 0; i < count && res == SF_OK_IMPORTED; i++)
	{
		uint32_t cs = 0;
		res = sf_writeFile(&sf_files[i], addr, &cs, i, count);
		failName = sf_files[i].name;
		SampleFlashEntry* e = &sf_newHeader.entry[i];
		strncpy(e->name, sf_files[i].name, 15);
		e->offset   = addr;
		e->frames   = sf_files[i].frames;
		e->rate     = sf_files[i].rate;
		e->checksum = cs;
		addr += (sf_files[i].frames * 2u + 3u) & ~3u;
		totalFrames += sf_files[i].frames;
	}

	/* 4. verify every sample by reading the flash back */
	if(res == SF_OK_IMPORTED)
	{
		sf_invalidateCache();
		sf_lcd("Verifying...", "");
		for(uint8_t i = 0; i < count; i++)
		{
			const SampleFlashEntry* e = &sf_newHeader.entry[i];
			uint32_t bytes = (e->frames * 2u + 3u) & ~3u;
			uint32_t h = sf_fnv(2166136261u, (const uint8_t*)e->offset, bytes);
			if(h != e->checksum) { res = SF_ERR_VERIFY; failName = e->name; break; }
		}
	}

	/* 5. header: everything but the magic first, the magic last */
	if(res == SF_OK_IMPORTED)
	{
		sf_newHeader.count       = count;
		sf_newHeader.signature   = signature;
		sf_newHeader.totalFrames = totalFrames;
		const uint32_t rest = sizeof(SampleFlashHeader) - 8u;
		if(!sf_programBytes(SF_REGION_START + 8u, ((const uint8_t*)&sf_newHeader) + 8u, rest) ||
		   !sf_programBytes(SF_REGION_START, SF_MAGIC, 8u))
		{
			res = SF_ERR_FLASH;
		}
		sf_invalidateCache();
		if(res == SF_OK_IMPORTED && !sf_headerValid(sf_flashHeader)) res = SF_ERR_VERIFY;
	}
	HAL_FLASH_Lock();

	sf_showResult(res, failName, count, totalFrames);
	return res;
}
