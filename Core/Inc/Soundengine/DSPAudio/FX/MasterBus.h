/*
 * MasterBus.h
 *
 * MOD: FX3, the master bus of the stereo output St1 (DAC 1): saturation with an
 * optional split (only the band above the crossover is saturated, a 24 dB/oct
 * Linkwitz-Riley crossover) and a stereo-linked compressor with its own optional
 * split (only the band above its crossover is compressed), in either order.
 * Its settings are kit data (kitEngines[KIT_FX3_OFFSET ...]).
 *
 * Modified firmware - not an official Erica Synths release.
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef INC_SOUNDENGINE_DSPAUDIO_FX_MASTERBUS_H_
#define INC_SOUNDENGINE_DSPAUDIO_FX_MASTERBUS_H_

#include <stdint.h>

/* the settings, in this order in the kit data (0..127 each, ORD 0..1) */
enum
{
	MB_SAT,		/* saturation, 0 = off */
	MB_FRQ,		/* crossover: 0 = full band, else 40 Hz .. 10 kHz (logarithmic) */
	MB_THR,		/* compressor threshold -48 .. 0 dB */
	MB_RAT,		/* ratio 1:1 .. 20:1 (0 = compressor off) */
	MB_ATK,		/* attack 0.1 .. 100 ms */
	MB_REL,		/* release: 0 = auto (tempo: a 1/16 note), else 20 .. 1000 ms */
	MB_GAI,		/* make-up gain 0 .. +24 dB */
	MB_MIX,		/* compressor wet (parallel compression), 127 = 100 % */
	MB_ORD,		/* 0 = saturation then compressor, 1 = the other way round */
	MB_AMU,		/* 1 = auto make-up gain (half the reduction at 0 dBFS), plus GAI */
	MB_CFQ,		/* compressor split: 0 = full band, else as MB_FRQ */
	MB_CSM,		/* what CFQ does: 0 = band split (only the band above is compressed),
				   1 = sidechain (the band above is detected, everything compressed) */
	MB_NUM_PARAMS
};

/* process the St1 sum in place (interleaved L/R, 32 bit, 16 bit full scale) */
void masterBus_process(int32_t* mix, uint32_t frames, const uint8_t* settings);

/* status for the FX3 edit view: blocks processed (counts while audio runs), 1 = the
 * bus worked on the last block, gain reduction of the compressor in dB (>= 0) */
extern volatile uint32_t masterBus_blocks;
extern volatile uint8_t  masterBus_active;
extern volatile float    masterBus_grDb;
/* diagnostics (mixer): peaks of the two DAC sums before FX3 (16 bit = full scale,
 * the highest of about 0.25 s), 1 = FX3 runs on the second sum (St1 routed to St2) */
extern volatile int32_t  masterBus_peak[2];
extern volatile uint8_t  masterBus_onDac2;

/* display helpers: crossover in Hz (0 = off) */
uint32_t masterBus_crossoverHz(uint8_t v);

#endif /* INC_SOUNDENGINE_DSPAUDIO_FX_MASTERBUS_H_ */
