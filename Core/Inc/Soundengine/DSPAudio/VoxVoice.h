/*
 * VoxVoice.h
 *
 * MOD (XT): test engine "vox", a physical model of a voice (VoxCore.h).
 * Uses the drum voice's pages; its 7 engine specific settings (roles of
 * VoiceSlot.h): 0 source (vocal folds / syrinx), 1 tract length, 2 nasal,
 * 3 vowel EG, 4 vowel, 5 tension, 6 chord. Pitch, amp EG,
 * filter, transient (as an additional source), distortion, LFO and mix as for
 * every engine.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef VOXVOICE_H_
#define VOXVOICE_H_

#include "Soundengine/DSPAudio/VoiceCommon.h"
#include "Soundengine/DSPAudio/VoxCore.h"

typedef struct
{
	VOICE_COMMON_FIELDS

	VoxCore		core;
	VoxParams	par;
	uint8_t		dirty;			/* par changed: new shape, length, source */
	float		ampFilterInput;	/* amp EG, this block and the last */
	float		lastGain;
	OnePoleFilter MixerPageFilter[2];
} VoxVoice;

void Vox_init(VoxVoice* v, uint8_t slot);
void Vox_trigger(VoxVoice* v, const uint8_t vel, const uint8_t note);
void Vox_calcAsync(VoxVoice* v);
void Vox_calcSyncBlock(VoxVoice* v, int16_t* buf, const uint8_t size);
/* role 0..6 (see above) */
void Vox_setRole(VoxVoice* v, const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator);

#endif /* VOXVOICE_H_ */
