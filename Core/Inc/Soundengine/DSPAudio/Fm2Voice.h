/*
 * Fm2Voice.h
 *
 * MOD (OE02-Samples+): test engine "fm2", 2-operator FM percussion: a sine
 * carrier, phase modulated by a sine modulator with its own decaying index
 * envelope and feedback (bells, metallic hits, FM toms and kicks, snaps).
 * Drum pages; engine settings (roles of VoiceSlot.h): 0 ratio, 1-3 pitch EG
 * (decay, amount, slope, as the drum), 4 index, 5 index decay, 6 feedback.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef FM2VOICE_H_
#define FM2VOICE_H_

#include "Soundengine/DSPAudio/VoiceCommon.h"
#include "Soundengine/DSPAudio/Decay.h"

#define FM2_NUM_RATIOS	16

typedef struct
{
	VOICE_COMMON_FIELDS

	DecayEg		oscPitchEg;
	float		egPitchModAmount;
	uint32_t	phC, phM;		/* carrier, modulator phase */
	float		lastM;			/* last modulator output (feedback) */
	float		env;			/* index envelope 1 .. 0 */
	float		envK;			/* its factor per sample */
	float		index;			/* peak index in cycles */
	float		fb;				/* feedback in cycles */
	float		ratio;
	uint8_t		idxDecay;
	float		ampFilterInput, lastGain;
	OnePoleFilter MixerPageFilter[2];
} Fm2Voice;

void Fm2_init(Fm2Voice* v, uint8_t slot);
void Fm2_trigger(Fm2Voice* v, const uint8_t vel, const uint8_t note);
void Fm2_calcAsync(Fm2Voice* v);
void Fm2_calcSyncBlock(Fm2Voice* v, int16_t* buf, const uint8_t size);
void Fm2_setRole(Fm2Voice* v, const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator);

#endif /* FM2VOICE_H_ */
