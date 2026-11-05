/*
 * Recorder.h
 *
 * MOD (OE02-Samples+): records St1 as a WAV file (16 bit stereo) to the SD card,
 * in real time. Armed from the PERF page ("rec"), it starts with the sequencer and
 * stops after 1, 2 or 4 passes of the longest track (or at the sequencer stop).
 * Files: /REC/RECnnn.WAV. The audio side fills a ring buffer, the main loop writes.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef SOUNDENGINE_RECORDER_H_
#define SOUNDENGINE_RECORDER_H_

#include <stdint.h>

enum { REC_OFF, REC_X1, REC_X2, REC_X4, REC_ENDLESS, REC_NUM_MODES };
enum { REC_IDLE, REC_ARMED, REC_RUNNING, REC_ENDING };

extern volatile uint8_t rec_state;

/* menu: arm (creates the file) with a mode, or disarm / stop with REC_OFF */
void rec_arm(uint8_t mode);
/* sequencer: start, each 1/16 step, stop */
void rec_seqStart(void);
void rec_seqStep(void);
void rec_seqStop(void);
/* audio: one block of St1 (OUTPUT_DMA_SIZE stereo frames) */
void rec_push(const int16_t* st1);
/* main loop: writes the buffer, finishes the file */
void rec_process(void);
/* the file number shown */
uint16_t rec_fileNr(void);

#endif
