/*
 * WaveVoice.h
 *
 * MOD (OE02-Samples+): test engine "wavetable": scans through a set of
 * single-cycle waves (sine > triangle > saw > square, the user waves W1..W8,
 * or both), the position swept by the pitch EG. Drum pages; engine settings
 * (roles of VoiceSlot.h): 0 table set, 1-3 pitch EG (decay, amount, slope, as
 * the drum), 4 position, 5 position sweep by the pitch EG, 6 stepped scan.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef WAVEVOICE_H_
#define WAVEVOICE_H_

#include "Soundengine/DSPAudio/VoiceCommon.h"
#include "Soundengine/DSPAudio/Decay.h"

#define WAVE_NUM_SETS	3	/* basic, user waves, both */

typedef struct
{
	VOICE_COMMON_FIELDS

	DecayEg		oscPitchEg;
	float		egPitchModAmount;
	float		egValue;		/* pitch EG of this block (position sweep) */
	uint32_t	ph;
	float		pos, sweep;		/* 0..1 */
	float		lastFrame;		/* frame position of the last block's end */
	uint8_t		set, stepped;
	float		ampFilterInput, lastGain;
	OnePoleFilter MixerPageFilter[2];
} WaveVoice;

void Wave_init(WaveVoice* v, uint8_t slot);
void Wave_trigger(WaveVoice* v, const uint8_t vel, const uint8_t note);
void Wave_calcAsync(WaveVoice* v);
void Wave_calcSyncBlock(WaveVoice* v, int16_t* buf, const uint8_t size);
void Wave_setRole(WaveVoice* v, const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator);

#endif /* WAVEVOICE_H_ */
