/*
 * ModalVoice.h
 *
 * MOD (XT): test engine "modal", a physical model (ModalCore.h).
 * Uses the drum voice's pages; its 7 engine specific settings (roles of
 * VoiceSlot.h): 0 model, 1 damping, 2 strike position, 3 inharmonicity,
 * 4 hardness, 5 material, 6 exciter. Pitch, amp EG, filter, transient
 * (as an additional exciter), distortion, LFO and mix as for every engine.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef MODALVOICE_H_
#define MODALVOICE_H_

#include "Soundengine/DSPAudio/VoiceCommon.h"
#include "Soundengine/DSPAudio/ModalCore.h"

typedef struct
{
	VOICE_COMMON_FIELDS

	ModalCore	core;
	ModalParams	par;
	uint8_t		dirty;			/* par changed: new damping, ratios, weights */
	float		ampFilterInput;	/* amp EG, this block and the last */
	float		lastGain;
	OnePoleFilter MixerPageFilter[2];
} ModalVoice;

void Modal_init(ModalVoice* v, uint8_t slot);
void Modal_trigger(ModalVoice* v, const uint8_t vel, const uint8_t note);
void Modal_calcAsync(ModalVoice* v);
void Modal_calcSyncBlock(ModalVoice* v, int16_t* buf, const uint8_t size);
/* role 0..6 (see above) */
void Modal_setRole(ModalVoice* v, const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator);

#endif /* MODALVOICE_H_ */
