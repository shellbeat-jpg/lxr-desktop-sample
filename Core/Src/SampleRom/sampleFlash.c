/*
 * sampleFlash.c
 *
 * Step 1 of the sample extension: import WAV files from /SAMPLES on the SD
 * card into the internal flash (sectors 6 + 7) and verify them.
 *
 * Safety rules (the bootloader lives in sector 0, the firmware in 1-5):
 *  - Only sectors 6 and 7 can be erased here, 8-11 only in the experimental
 *    big flash mode (hard check in sf_eraseSector()).
 *  - Every program call checks that the address lies inside
 *    [SF_REGION_START, sf_end).
 *
 * Experimental big flash mode (file /SAMPLES/BIGFLASH): the chip reports 1 MB,
 * but its die usually has 2 MB; sectors 8-11 (0x08100000-0x081FFFFF) are used
 * only if the flash is in single-bank mode and they can be read without a bus
 * fault. ST doesn't guarantee that area: every sample is verified, a failure
 * falls back to the normal 512 KB.
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

#define SF_MAGIC        "LXRSMP02"
#define SF_FOLDER       "/SAMPLES"
#define SF_LOOP_FOLDER  "/SAMPLES/LOOPS"
#define SF_READ_FRAMES  256u
#define SF_BIG_FILE     "/SAMPLES/BIGFLASH"
#define SF_BIG_END      ((uint32_t)0x08200000u)           /* sectors 6-11 */
#define SF_SECTOR_SIZE  ((uint32_t)0x00040000u)

/* end of the sample region of the current import / of the stored samples */
static uint32_t sf_end = SF_REGION_END;
static void sf_capSeconds(char* out);
#define SF_CAPACITY     (sf_end - SF_DATA_START)          /* bytes for PCM data */

/* the header must fit into the reserved header area */
typedef char sf_headerFits[(sizeof(SampleFlashHeader) <= SF_HEADER_SIZE) ? 1 : -1];

typedef struct
{
	char     name[13];
	uint8_t  loop;         /* from SF_LOOP_FOLDER */
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

/* set at the end of sampleFlash_importAtBoot(): valid header or NULL */
static const SampleFlashHeader* volatile sf_cache = NULL;

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
uint8_t sampleFlash_modulatedWave(uint8_t base, float v, uint8_t numOscWaves)
{
	if(!(v > 0.f)) v = 0.f;   /* also NaN */
	if(v > 1.f) v = 1.f;
	const SampleFlashHeader* h = sf_cache;
	if(base >= numOscWaves && h == NULL)
		return base;		/* no samples stored: stays silent */
	if(base < numOscWaves)
	{
		const uint8_t w = (uint8_t)(numOscWaves * v);
		return w >= numOscWaves ? (uint8_t)(numOscWaves - 1u) : w;
	}
	int32_t w = (int32_t)(v * 127.f + 0.5f);
	const int32_t last = (int32_t)numOscWaves + (int32_t)h->count - 1;
	if(w < (int32_t)numOscWaves) w = numOscWaves;
	if(w > last) w = last;
	return (uint8_t)w;
}
float sampleFlash_rangeValue(uint8_t base, float amount, float val)
{
	if(!(amount > 0.f)) amount = 0.f;
	if(amount > 1.f) amount = 1.f;
	if(!(val > 0.f)) val = 0.f;
	if(val > 1.f) val = 1.f;
	const uint32_t span = (uint32_t)(amount * 127.f + 0.5f);
	uint32_t k = (uint32_t)(val * (float)(span + 1u));
	if(k > span) k = span;
	uint32_t w = base + k;
	if(w > 127u) w = 127u;
	return (float)w / 127.f;
}
#ifndef SF_READ_FLASH_KB
#define SF_READ_FLASH_KB() (*(const volatile uint16_t*)FLASHSIZE_BASE)
#endif
uint16_t sampleFlash_chipKB(void)
{
	return SF_READ_FLASH_KB();
}
const SampleFlashEntry* sampleFlash_entry(uint8_t slot)
{
	const SampleFlashHeader* h = sf_cache;
	if(h == NULL || slot >= h->count) return NULL;
	return &h->entry[slot];
}
static SampleFlashResult sf_finish(SampleFlashResult r)
{
	sf_cache = sf_headerValid(sf_flashHeader) ? sf_flashHeader : NULL;
	return r;
}
/*----------------------------------------------------------------------------*/
/* Flash access - the ONLY functions that touch the flash controller.          */
/*----------------------------------------------------------------------------*/
static uint8_t sf_eraseSector(uint32_t sector)
{
	/* hard guard: never anything but the sample sectors (8-11 only in big mode) */
	if(sector < FLASH_SECTOR_6) return 0;
	if(sector > FLASH_SECTOR_7 && !(sf_end == SF_BIG_END && sector <= FLASH_SECTOR_11)) return 0;

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
	if(addr < SF_REGION_START || addr > (sf_end - 4u) || (addr & 3u)) return 0;
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

static void sf_path(char* path, uint32_t size, const sf_FileInfo* fi)
{
	snprintf(path, size, "%s/%s", fi->loop ? SF_LOOP_FOLDER : SF_FOLDER, fi->name);
}
/* returns SF_OK_IMPORTED on success (used as "ok" here) */
static SampleFlashResult sf_parseWav(sf_FileInfo* fi)
{
	char path[32];
	UINT br;
	uint8_t hdr[12];
	sf_path(path, sizeof(path), fi);
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
/* names in "natural" order: digit runs compare by their value (2KICK before
 * 10KICK), letters without case; equal names: one-shot before loop */
static int sf_nameCmp(const sf_FileInfo* a, const sf_FileInfo* b)
{
	const char* p = a->name;
	const char* q = b->name;
	while(*p && *q)
	{
		if(*p >= '0' && *p <= '9' && *q >= '0' && *q <= '9')
		{
			uint32_t x = 0, y = 0;
			while(*p >= '0' && *p <= '9') x = x * 10u + (uint32_t)(*p++ - '0');
			while(*q >= '0' && *q <= '9') y = y * 10u + (uint32_t)(*q++ - '0');
			if(x != y) return x < y ? -1 : 1;
			continue;
		}
		char c = *p, d = *q;
		if(c >= 'a' && c <= 'z') c -= 32;
		if(d >= 'a' && d <= 'z') d -= 32;
		if(c != d) return c < d ? -1 : 1;
		p++; q++;
	}
	if(*p || *q) return *p ? 1 : -1;
	const int s = strcmp(a->name, b->name);
	return s ? s : (int)a->loop - (int)b->loop;
}
/* add the WAVs of one folder; returns 0 if the folder can't be opened */
static uint8_t sf_scanDir(const char* folder, uint8_t loop, uint8_t* n)
{
	DIR dir;
	FILINFO fno;
	if(f_opendir(&dir, folder) != FR_OK) return 0;
	while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
	{
		if(!sf_isWav(&fno) || *n >= SF_MAX_SAMPLES) continue;
		memset(&sf_files[*n], 0, sizeof(sf_FileInfo));
		strncpy(sf_files[*n].name, fno.fname, 12);
		sf_files[*n].fsize = fno.fsize;
		sf_files[*n].loop  = loop;
		(*n)++;
	}
	f_closedir(&dir);
	return 1;
}
/* scan /SAMPLES and /SAMPLES/LOOPS, sorted by name; returns count, 0xFF if
 * /SAMPLES is missing */
static uint8_t sf_scanFolder(uint32_t* signature)
{
	uint8_t n = 0;
	if(!sf_scanDir(SF_FOLDER, 0, &n)) return 0xFF;
	sf_scanDir(SF_LOOP_FOLDER, 1, &n);   /* optional */

	/* insertion sort -> deterministic slot order */
	for(uint8_t i = 1; i < n; i++)
	{
		sf_FileInfo t = sf_files[i];
		int8_t j = (int8_t)i - 1;
		while(j >= 0 && sf_nameCmp(&sf_files[j], &t) > 0) { sf_files[j + 1] = sf_files[j]; j--; }
		sf_files[j + 1] = t;
	}

	uint32_t h = 2166136261u;
	h = sf_fnv(h, (const uint8_t*)SF_MAGIC, 8);
	for(uint8_t i = 0; i < n; i++)
	{
		h = sf_fnv(h, (const uint8_t*)sf_files[i].name, 13);
		h = sf_fnv(h, (const uint8_t*)&sf_files[i].loop, 1);
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
	char path[32];
	UINT br;
	uint32_t h = 2166136261u;
	uint32_t framesLeft = fi->frames;
	uint32_t written = 0;
	uint8_t lastPct = 0xFF;
	sf_path(path, sizeof(path), fi);
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
		return;   /* sampleFlash_showStatus() shows the count */
	case SF_OK_IMPORTED:
	{
		/* duration at 44.1 kHz in 1/100 s, the real rate may differ */
		uint32_t cs = (uint32_t)((uint64_t)totalFrames * 100u / 44100u);
		snprintf(l2, sizeof(l2), "%u smp %lu.%02lus", count, (unsigned long)(cs / 100), (unsigned long)(cs % 100));
		sf_lcd("Import+Verify OK", l2);
		HAL_Delay(1500);
		return;
	}
	case SF_NO_FOLDER:
		return;
	case SF_ERR_FORMAT:  sf_lcd("Not 16bit WAV:", name); break;
	case SF_ERR_TOO_BIG: /* name carries "Too big: x.ys" */
	{
		char l2[17], s[8];
		sf_capSeconds(s);
		snprintf(l2, sizeof(l2), "max %ss @44.1k", s);
		sf_lcd(name, l2);
		break;
	}
	case SF_ERR_READ:    sf_lcd("SD read error:", name); break;
	case SF_ERR_FLASH:   sf_lcd("Flash error!", "Samples disabled"); break;
	case SF_ERR_VERIFY:  sf_lcd("Verify FAILED:", name); break;
	}
	HAL_Delay(3000);
}
/*----------------------------------------------------------------------------*/
static void sf_showSkipped(const char* line1, uint8_t skipped, const char* firstName)
{
	char l2[17];
	if(skipped > 1) snprintf(l2, sizeof(l2), "%-12.12s +%u", firstName, (unsigned)(skipped - 1u));
	else            snprintf(l2, sizeof(l2), "%s", firstName);
	sf_lcd(line1, l2);
	HAL_Delay(2500);
}
/* "Too big: x.ys" - mono length at 44.1 kHz */
static void sf_fmtSeconds(char* out, uint32_t bytes)
{
	uint32_t ds = (uint32_t)(((uint64_t)bytes / 2u) * 10u / 44100u);
	snprintf(out, 17, "Too big: %lu.%lus", (unsigned long)(ds / 10u), (unsigned long)(ds % 10u));
}
/*----------------------------------------------------------------------------*/
void sampleFlash_showStatus(void)
{
	char l1[17], l2[17];
	sf_cache = sf_headerValid(sf_flashHeader) ? sf_flashHeader : NULL;
	const SampleFlashHeader* h = sf_cache;
	const uint32_t skipped = h ? h->reserved[0] + h->reserved[1] : 0u;
	if(h == NULL)                   snprintf(l1, sizeof(l1), "Samples: none");
	else if(skipped && skipped <= 99u)
	                                snprintf(l1, sizeof(l1), "Samples:%u skp:%u", (unsigned)h->count, (unsigned)skipped);
	else                            snprintf(l1, sizeof(l1), "Samples: %u", (unsigned)h->count);
	if(h && h->reserved[2] == 1u)
		snprintf(l2, sizeof(l2), "Flash %uK big", (unsigned)sampleFlash_chipKB());
	else
		snprintf(l2, sizeof(l2), "Flash: %u KB", (unsigned)sampleFlash_chipKB());
	sf_lcd(l1, l2);
	HAL_Delay(1500);
}
/*----------------------------------------------------------------------------*/
/* capacity in seconds at 44.1 kHz mono, one decimal ("5.9", "17.8") */
static void sf_capSeconds(char* out)
{
	const uint32_t ds = (SF_CAPACITY / 2u) * 10u / 44100u;
	snprintf(out, 8, "%lu.%lu", (unsigned long)(ds / 10u), (unsigned long)(ds % 10u));
}
/*----------------------------------------------------------------------------*/
/* big flash mode: requested by the file /SAMPLES/BIGFLASH */
/* any file in /SAMPLES whose name starts with BIGFLASH (BIGFLASH, BIGFLASH.TXT, ...) */
static uint8_t sf_bigRequested(void)
{
	DIR dir;
	FILINFO fno;
	uint8_t found = 0;
	if(f_opendir(&dir, SF_FOLDER) != FR_OK) return 0;
	while(!found && f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
	{
		const char* k = "BIGFLASH";
		uint8_t i = 0;
		while(k[i] && (fno.fname[i] == k[i] || fno.fname[i] == k[i] + 32)) i++;
		found = (k[i] == 0) && !(fno.fattrib & AM_DIR);
	}
	f_closedir(&dir);
	return found;
}
/* why the upper megabyte can't be used (shown on the LCD), "" if it can */
static char sf_bigReason[17];
#ifndef SF_BIG_AVAILABLE
/* read a word without crashing if the memory doesn't exist: bus faults are
 * ignored while FAULTMASK is set and BFHFNMIGN is on; the BusFault status
 * tells whether it happened. The D-cache is off meanwhile, so the read goes
 * to the flash on the AXI address that programming and playback use. */
static uint8_t sf_probeRead(const uint32_t addr)
{
	SCB->CFSR = SCB_CFSR_BUSFAULTSR_Msk;		/* write 1 to clear */
	__disable_fault_irq();
	SCB->CCR |= SCB_CCR_BFHFNMIGN_Msk;
	__DSB(); __ISB();
	volatile uint32_t v = *(volatile const uint32_t*)addr;
	(void)v;
	__DSB();
	SCB->CCR &= ~SCB_CCR_BFHFNMIGN_Msk;
	__DSB(); __ISB();
	__enable_fault_irq();
	const uint8_t ok = (SCB->CFSR & SCB_CFSR_BUSFAULTSR_Msk) == 0u;
	SCB->CFSR = SCB_CFSR_BUSFAULTSR_Msk;
	return ok;
}
/* single-bank layout (sectors 8-11 at 0x08100000) and readable */
static uint8_t sf_bigAvailable(void)
{
	snprintf(sf_bigReason, sizeof(sf_bigReason), "dual bank");
	if((FLASH->OPTCR & FLASH_OPTCR_nDBANK) == 0u) return 0;
	SCB_DisableDCache();
	uint8_t ok = 1;
	for(uint32_t a = SF_REGION_END; ok && a < SF_BIG_END; a += SF_SECTOR_SIZE)
	{
		if(!sf_probeRead(a))
		{
			snprintf(sf_bigReason, sizeof(sf_bigReason), "read err %08lX", (unsigned long)a);
			ok = 0;
		}
		else if(!sf_probeRead(a + SF_SECTOR_SIZE - 4u))
		{
			snprintf(sf_bigReason, sizeof(sf_bigReason), "read err %08lX", (unsigned long)(a + SF_SECTOR_SIZE - 4u));
			ok = 0;
		}
	}
	SCB_EnableDCache();
	if(ok) sf_bigReason[0] = 0;
	return ok;
}
#define SF_BIG_AVAILABLE() sf_bigAvailable()
#endif
/*----------------------------------------------------------------------------*/
static SampleFlashResult sf_import(uint8_t count, const uint32_t signature)
{
	/* 1. parse all files and check the total size before touching the flash.
	 *    Files that are no 16-bit PCM WAV are skipped (not fatal), and so are
	 *    files that no longer fit (in name order: earlier names have priority,
	 *    a later, smaller file may still fit). The others move up, so s1..sN
	 *    stay gap-free and keep the name order. */
	uint32_t needed = 0, wanted = 0;
	uint8_t kept = 0, skipped = 0, full = 0;
	char skipName[13], fullName[13];
	memset(skipName, 0, sizeof(skipName));
	memset(fullName, 0, sizeof(fullName));
	for(uint8_t i = 0; i < count; i++)
	{
		SampleFlashResult r = sf_parseWav(&sf_files[i]);
		if(r == SF_ERR_FORMAT)
		{
			if(skipped == 0) strncpy(skipName, sf_files[i].name, 12);
			skipped++;
			continue;
		}
		if(r != SF_OK_IMPORTED) { sf_showResult(r, sf_files[i].name, 0, 0); return sf_finish(r); }
		const uint32_t bytes = (sf_files[i].frames * 2u + 3u) & ~3u;
		wanted += bytes;
		if(needed + bytes > SF_CAPACITY)
		{
			if(full == 0) strncpy(fullName, sf_files[i].name, 12);
			full++;
			continue;
		}
		if(kept != i) sf_files[kept] = sf_files[i];
		needed += bytes;
		kept++;
	}
	if(skipped) sf_showSkipped("Not 16bit, skip:", skipped, skipName);
	if(kept == 0 && full)
	{
		char l1[17];
		sf_fmtSeconds(l1, wanted);
		sf_showResult(SF_ERR_TOO_BIG, l1, 0, 0);
		return sf_finish(SF_ERR_TOO_BIG);   /* nothing fits, flash untouched */
	}
	if(full)
	{
		char l1[17], s[8];
		sf_capSeconds(s);
		snprintf(l1, sizeof(l1), "Full(%ss) skip:", s);
		sf_showSkipped(l1, full, fullName);
	}
	if(kept == 0) return sf_finish(SF_ERR_FORMAT);   /* nothing usable, flash untouched */
	count = kept;

	/* 2. erase - code runs from flash, so the CPU stalls ~1-2 s per sector */
	sf_lcd("Samples changed", "Erasing flash...");
	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
	SampleFlashResult res = SF_OK_IMPORTED;
	for(uint32_t s = 0; res == SF_OK_IMPORTED && SF_REGION_START + s * SF_SECTOR_SIZE < SF_DATA_START + needed; s++)
	{
		if(!sf_eraseSector(FLASH_SECTOR_6 + s)) res = SF_ERR_FLASH;
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
		strncpy(e->name, sf_files[i].name, 12);
		e->flags    = sf_files[i].loop ? SF_FLAG_LOOP : 0u;
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
		sf_newHeader.reserved[0] = skipped;   /* shown by sampleFlash_showStatus() */
		sf_newHeader.reserved[1] = full;
		sf_newHeader.reserved[2] = (sf_end == SF_BIG_END && SF_DATA_START + needed > SF_REGION_END) ? 1u : 0u;   /* big flash used */
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
	return sf_finish(res);
}
/*----------------------------------------------------------------------------*/
SampleFlashResult sampleFlash_importAtBoot(void)
{
	uint32_t signature = 0;
	uint8_t count = sf_scanFolder(&signature);
	if(count == 0xFF) return sf_finish(SF_NO_FOLDER);   /* feature not used, old samples stay usable */
	if(count == 0)    { sf_showResult(SF_NO_FOLDER, "", 0, 0); return sf_finish(SF_NO_FOLDER); }

	/* the big flash request is part of the signature: adding or removing
	 * BIGFLASH imports again */
	const uint8_t wantBig = sf_bigRequested();
	if(wantBig) signature = sf_fnv(signature, (const uint8_t*)"BIGFLSH2", 8);

	if(sf_headerValid(sf_flashHeader) && sf_flashHeader->signature == signature)
	{
		sf_showResult(SF_OK_UNCHANGED, "", (uint8_t)sf_flashHeader->count, 0);
		return sf_finish(SF_OK_UNCHANGED);
	}

	sf_end = SF_REGION_END;
	if(wantBig)
	{
		if(SF_BIG_AVAILABLE())
		{
			sf_end = SF_BIG_END;
			sf_lcd("Big flash: on", "1.5 MB, testing");
			HAL_Delay(1200);
		}
		else
		{
			sf_lcd("Big flash: no", sf_bigReason[0] ? sf_bigReason : "not usable");
			HAL_Delay(3500);
		}
	}
	SampleFlashResult res = sf_import(count, signature);
	if(sf_end == SF_BIG_END && (res == SF_ERR_FLASH || res == SF_ERR_VERIFY))
	{
		/* the upper megabyte failed: import again into the normal region */
		sf_lcd("Big flash failed", "using 512K");
		HAL_Delay(2500);
		sf_end = SF_REGION_END;
		count = sf_scanFolder(&signature);
		if(wantBig) signature = sf_fnv(signature, (const uint8_t*)"BIGFLSH2", 8);
		res = sf_import(count, signature);
	}
	sf_end = SF_REGION_END;
	return res;
}
