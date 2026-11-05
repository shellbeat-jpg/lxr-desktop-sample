/*
 * Recorder.h
 *
 * MOD (OE02-Samples+): records St1 as a WAV file (16 bit stereo) to the SD card,
 * in real time. Armed from the PERF page ("rec", encoder click), it starts with the
 * sequencer or at the next pattern start ("rst") and stops after 1, 2 or 4 passes of
 * the longest track (or at the sequencer stop, or with another click).
 * Files: /REC/RECnnn.WAV. The audio side fills a ring buffer, the main loop writes.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef SOUNDENGINE_RECORDER_H_
#define SOUNDENGINE_RECORDER_H_

#include <stdint.h>

enum { REC_OFF, REC_X1, REC_X2, REC_X4, REC_ENDLESS, REC_NUM_MODES };
#define REC_LEN_NUM	(REC_NUM_MODES - 1)	/* PAR_REC_LEN: 0 = x1 ... 3 = until stop */
enum { REC_IDLE, REC_ARMED, REC_RUNNING, REC_ENDING };

extern volatile uint8_t rec_state;

/* menu: arm (creates the file; len = PAR_REC_LEN, atPattern = PAR_REC_START),
 * disarm (deletes the empty file) or stop a recording */
void rec_toggle(uint8_t len, uint8_t atPattern);
/* menu: the length changed (an armed recording takes it at its start) */
void rec_setLength(uint8_t len);
/* sequencer: start, each 1/16 step, pattern start (master track at step 1), stop */
void rec_seqStart(void);
void rec_seqPatternStart(void);
void rec_seqStep(void);
void rec_seqStop(void);
/* audio: one block of St1 (OUTPUT_DMA_SIZE stereo frames) */
void rec_push(const int16_t* st1);
/* main loop: writes the buffer, finishes the file */
void rec_process(void);
/* the ring buffer as working memory while the recorder is idle (sample import at
 * boot), NULL if too small or in use */
void* rec_scratch(uint32_t bytes);
/* the file number shown */
uint16_t rec_fileNr(void);

#endif
