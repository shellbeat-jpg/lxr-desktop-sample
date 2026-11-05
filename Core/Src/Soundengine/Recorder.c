/*
 * Recorder.c
 *
 * MOD (OE02-Samples+): real-time WAV recording of St1 to the SD card (Recorder.h).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "Soundengine/Recorder.h"
#include "Soundengine/config.h"
#include "Soundengine/Soundengine.h"
#include "Soundengine/Sequencer/sequencer.h"
#include "menu/menu.h"
#include "fatfs.h"
#include <stdio.h>
#include <string.h>

#define REC_FRAMES		4096u		/* ring: 4096 stereo frames = 16 KB, about 93 ms */
#define REC_CHUNK		1024u		/* frames per write (4 KB) */
#define REC_RATE		44100u		/* nominal rate in the file (the DAC runs at 44.14 kHz) */
#define REC_ENDLESS_MB	128u		/* space reserved for an endless recording */

INDTCMZ static int16_t rec_ring[REC_FRAMES * 2];
static volatile uint32_t rec_head = 0;	/* frames written by the audio side */
static volatile uint32_t rec_tail = 0;	/* frames written to the file */
static volatile uint16_t rec_gaps = 0;	/* dropped blocks (the card was too slow) */
static volatile uint8_t rec_dropping = 0;
volatile uint8_t rec_state = REC_IDLE;

static FIL rec_file;
static uint8_t rec_mode = REC_X1;
static uint16_t rec_nr = 0;
static uint32_t rec_target = 0;	/* steps to record, 0 = until stop */
static uint32_t rec_steps = 0;
static uint32_t rec_bytes = 0;
static uint8_t rec_error = 0;

uint16_t rec_fileNr(void) { return rec_nr; }
/* the ring as working memory before the recorder is used (sample import at boot) */
void* rec_scratch(const uint32_t bytes)
{
	return (bytes <= sizeof(rec_ring) && rec_state == REC_IDLE) ? (void*)rec_ring : (void*)0;
}

static void rec_path(char* p, const uint16_t nr)
{
	sprintf(p, "/REC/REC%03u.WAV", (unsigned)nr);
}
static void rec_le32(uint8_t* p, const uint32_t v)
{
	p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void rec_header(uint8_t* h, const uint32_t dataBytes)
{
	memcpy(h, "RIFF", 4); rec_le32(h + 4, 36u + dataBytes);
	memcpy(h + 8, "WAVEfmt ", 8); rec_le32(h + 16, 16u);
	h[20] = 1; h[21] = 0; h[22] = 2; h[23] = 0;		/* PCM, stereo */
	rec_le32(h + 24, REC_RATE); rec_le32(h + 28, REC_RATE * 4u);
	h[32] = 4; h[33] = 0; h[34] = 16; h[35] = 0;	/* 4 bytes per frame, 16 bit */
	memcpy(h + 36, "data", 4); rec_le32(h + 40, dataBytes);
}
/* the loop length in 1/16 steps: the longest track of the playing pattern */
static uint32_t rec_loopSteps(void)
{
	uint8_t m = 1;
	for(uint8_t t=0;t<NUM_TRACKS;t++)
	{
		const uint8_t l = seq_patternSet.length[seq_activePattern][t];
		if(l > m) m = l;
	}
	return m;
}
static uint8_t rec_passes(const uint8_t mode)
{
	return mode == REC_X1 ? 1 : mode == REC_X2 ? 2 : mode == REC_X4 ? 4 : 0;
}

static void rec_note(const char* l1, const char* l2)
{
	menu_showNote(l1, l2);
}

/* create the file (folder, next free number, contiguous space) */
static uint8_t rec_open(void)
{
	char p[24];
	FILINFO fno;
	FRESULT res = f_stat("/REC", &fno);
	if(res == FR_NO_FILE || res == FR_NO_PATH)
		res = f_mkdir("/REC");
	if(res != FR_OK)
		return 0;
	uint16_t nr = rec_nr;
	for(uint16_t i=0;i<999;i++)
	{
		nr = (uint16_t)(nr % 999u + 1u);
		rec_path(p, nr);
		if(f_stat(p, &fno) == FR_NO_FILE)
			break;
	}
	rec_path(p, nr);
	if(f_open(&rec_file, p, FA_CREATE_NEW | FA_WRITE) != FR_OK)
		return 0;
	rec_nr = nr;
	/* contiguous space: short writes while recording */
	uint32_t mb = REC_ENDLESS_MB;
	const uint8_t passes = rec_passes(rec_mode);
	if(passes)
	{
		const float sec = passes * rec_loopSteps() * 15.f / (float)(seq_getBpm() ? seq_getBpm() : 120);
		mb = (uint32_t)(sec * REC_RATE * 4.f * 1.25f / 1048576.f) + 2u;
	}
	while(mb >= 1u && f_expand(&rec_file, (FSIZE_t)mb * 1048576u, 1) != FR_OK)
		mb /= 2u;
	uint8_t h[44];
	UINT bw;
	rec_header(h, 0);
	if(f_lseek(&rec_file, 0) != FR_OK || f_write(&rec_file, h, 44, &bw) != FR_OK || bw != 44)
	{
		f_close(&rec_file);
		f_unlink(p);
		return 0;
	}
	return 1;
}

static uint8_t rec_atPattern = 0;	/* start at the next pattern start (else with PLAY) */

static void rec_disarm(void)
{
	char p[24];
	const uint8_t bg = soundengine_backgroundAudioBegin();
	f_close(&rec_file);
	rec_path(p, rec_nr);
	f_unlink(p);
	soundengine_backgroundAudioEnd(bg);
	rec_state = REC_IDLE;
	rec_note("Recording", "off");
}

void rec_setLength(const uint8_t len)
{
	rec_mode = len >= REC_LEN_NUM ? REC_X1 : (uint8_t)(REC_X1 + len);
}

void rec_toggle(const uint8_t len, const uint8_t atPattern)
{
	if(rec_state == REC_RUNNING)
	{
		rec_state = REC_ENDING;		/* finished by rec_process */
		return;
	}
	if(rec_state == REC_ARMED)
	{
		rec_disarm();
		return;
	}
	if(rec_state != REC_IDLE)
		return;		/* still writing the last file */
	rec_setLength(len);
	rec_atPattern = atPattern != 0;
	const uint8_t bg = soundengine_backgroundAudioBegin();
	const uint8_t ok = rec_open();
	soundengine_backgroundAudioEnd(bg);
	if(!ok)
	{
		rec_note("Recording", "SD card error");
		return;
	}
	rec_head = rec_tail = 0;
	rec_gaps = 0;
	rec_dropping = 0;
	rec_bytes = 0;
	rec_error = 0;
	rec_state = REC_ARMED;
	char l2[17];
	snprintf(l2, sizeof(l2), "REC%03u %s", (unsigned)rec_nr,
			(rec_atPattern && seq_isRunning()) ? "on pattern" : "on Play");
	rec_note("Recording armed", l2);
}

static void rec_begin(const uint32_t stepsDone)
{
	const uint8_t passes = rec_passes(rec_mode);
	rec_target = passes ? passes * rec_loopSteps() : 0;
	rec_steps = stepsDone;
	rec_state = REC_RUNNING;
}
void rec_seqStart(void)
{
	if(rec_state == REC_ARMED)
		rec_begin(0);
}
void rec_seqPatternStart(void)
{
	/* the step that starts now was not counted (rec_seqStep came first) */
	if(rec_state == REC_ARMED && rec_atPattern)
		rec_begin(1);
}
void rec_seqStep(void)
{
	if(rec_state == REC_RUNNING && rec_target && ++rec_steps > rec_target)
		rec_state = REC_ENDING;
}
void rec_seqStop(void)
{
	if(rec_state == REC_RUNNING)
		rec_state = REC_ENDING;
}

void rec_push(const int16_t* st1)
{
	if(rec_state != REC_RUNNING)
		return;
	const uint32_t head = rec_head;
	if(head - rec_tail > REC_FRAMES - OUTPUT_DMA_SIZE)
	{
		if(!rec_dropping)
			rec_gaps++;
		rec_dropping = 1;
		return;
	}
	rec_dropping = 0;
	const uint32_t at = head % REC_FRAMES;	/* REC_FRAMES is a multiple of the block */
	memcpy(&rec_ring[at * 2u], st1, OUTPUT_DMA_SIZE * 2u * sizeof(int16_t));
	rec_head = head + OUTPUT_DMA_SIZE;
}

static void rec_finish(void)
{
	uint8_t h[44];
	UINT bw;
	rec_header(h, rec_bytes);
	FRESULT res = f_truncate(&rec_file);
	if(res == FR_OK) res = f_lseek(&rec_file, 0);
	if(res == FR_OK) res = f_write(&rec_file, h, 44, &bw);
	const FRESULT c = f_close(&rec_file);
	if(res == FR_OK) res = c;
	rec_state = REC_IDLE;
	char l1[17], l2[17];
	const uint32_t s = rec_bytes / (REC_RATE * 4u);
	snprintf(l1, sizeof(l1), "%s REC%03u", (res == FR_OK && !rec_error) ? "Saved" : "Error", (unsigned)rec_nr);
	if(rec_gaps)	snprintf(l2, sizeof(l2), "%lu:%02lu %u gaps", (unsigned long)(s / 60u), (unsigned long)(s % 60u), (unsigned)rec_gaps);
	else			snprintf(l2, sizeof(l2), "%lu:%02lu", (unsigned long)(s / 60u), (unsigned long)(s % 60u));
	rec_note(l1, l2);
}

void rec_process(void)
{
	const uint8_t st = rec_state;
	if(st != REC_RUNNING && st != REC_ENDING)
		return;
	uint32_t avail = rec_head - rec_tail;
	if(avail < REC_CHUNK && st == REC_RUNNING)
		return;
	if(avail)
	{
		/* one contiguous piece of the ring */
		const uint32_t at = rec_tail % REC_FRAMES;
		uint32_t n = avail > REC_CHUNK ? REC_CHUNK : avail;
		if(at + n > REC_FRAMES) n = REC_FRAMES - at;
		UINT bw = 0;
		const uint8_t bg = soundengine_backgroundAudioBegin();
		const FRESULT res = rec_error ? FR_OK : f_write(&rec_file, &rec_ring[at * 2u], n * 4u, &bw);
		soundengine_backgroundAudioEnd(bg);
		if(!rec_error && (res != FR_OK || bw != n * 4u))
		{
			rec_error = 1;		/* card full or failed: stop here, keep what is there */
			rec_state = REC_ENDING;
		}
		if(!rec_error)
			rec_bytes += n * 4u;
		rec_tail += n;
		return;
	}
	/* ending and empty: finish the file */
	const uint8_t bg = soundengine_backgroundAudioBegin();
	rec_finish();
	soundengine_backgroundAudioEnd(bg);
}
