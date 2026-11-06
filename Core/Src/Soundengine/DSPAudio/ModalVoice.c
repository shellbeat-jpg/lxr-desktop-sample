/*
 * ModalVoice.c
 *
 * MOD (XT): test engine "modal" (ModalVoice.h, ModalCore.h).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "Soundengine/DSPAudio/ModalVoice.h"
#include "Soundengine/DSPAudio/EnginePost.h"
#include "Soundengine/DSPAudio/modulationNode.h"
#include <string.h>
#include <math.h>

#define MODAL_OUT_GAIN	(0.3f * 32767.f)	/* a hard strike peaks at about full scale */

void Modal_init(ModalVoice* v, uint8_t slot)
{
	enginePost_init((VoiceCommon*)v, slot, &v->MixerPageFilter[0]);
	const ModalParams p = { 1, 70, 30, 0, 80, 60, 0 };
	v->par = p;
	v->dirty = 1;
}

void Modal_setRole(ModalVoice* v, const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	switch(role)
	{
	case 0:
	{
		/* modulated: around the stored list index (the LFO works on 0..127) */
		const uint8_t m = isFloatModulator ? (uint8_t)lrintf(flt * 127.f) : itg;
		v->par.model = m >= MODAL_MODELS ? MODAL_MODELS-1 : m;
		break;
	}
	case 1:	v->par.damp = itg;			break;
	case 2:	v->par.pos = itg;			break;
	case 3:	v->par.inh = itg;			break;
	case 4:	v->par.hard = itg;			break;
	case 5:	v->par.mat = itg;			break;
	default: v->par.noise = (isFloatModulator ? lrintf(flt * 127.f) : itg) != 0;	break;
	}
	v->dirty = 1;
}

void Modal_trigger(ModalVoice* v, const uint8_t vel, const uint8_t note)
{
	lfo_retrigger(v->slot);
	modNode_updateValue(&velocityModulators[v->slot], vel/127.f);
	osc_setBaseNote(&v->osc, note);
	slopeEg2_trigger(&v->oscVolEg);
	v->velo = vel/127.f;
	transient_trigger(&v->transGen);
	SnapEg_trigger(&v->snapEg);
	enginePost_updatePan((VoiceCommon*)v);
	modalCore_strike(&v->core, &v->par);
}

void Modal_calcAsync(ModalVoice* v)
{
	enginePost_calcAmp((VoiceCommon*)v, &v->ampFilterInput, &v->lastGain);
	//MOD: a gate keeps the modes ringing until it closes
	if(v->par.hold != v->oscVolEg.hold)
	{
		v->par.hold = v->oscVolEg.hold;
		v->dirty = 1;
	}
	if(v->dirty)
	{
		v->dirty = 0;
		modalCore_setParams(&v->core, &v->par, REAL_FS);
	}
	const float f0 = enginePost_pitch((VoiceCommon*)v);
	//new coefficients only when the pitch changes (by more than 0.05 %)
	if(fabsf(f0 - v->core.f0) > f0 * 0.0005f)
		modalCore_setPitch(&v->core, f0, REAL_FS);
}

void Modal_calcSyncBlock(ModalVoice* v, int16_t* buf, const uint8_t size)
{
	int16_t tr[size];
	float out[size];
	transient_calcBlock(&v->transGen, tr, size);
	modalCore_render(&v->core, tr, out, size);
	for(uint8_t i=0;i<size;i++)
		buf[i] = enginePost_s16(out[i] * MODAL_OUT_GAIN);
	enginePost_block((VoiceCommon*)v, &v->MixerPageFilter[0], buf, size, v->ampFilterInput, v->lastGain);
}
