/*
 * VoxVoice.c
 *
 * MOD (XT): test engine "vox" (VoxVoice.h, VoxCore.h).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "Soundengine/DSPAudio/VoxVoice.h"
#include "Soundengine/DSPAudio/EnginePost.h"
#include "Soundengine/DSPAudio/modulationNode.h"
#include <string.h>
#include <math.h>

#define VOX_OUT_GAIN	(0.3f * 32767.f)	/* a loud vowel peaks at about full scale */

void Vox_init(VoxVoice* v, uint8_t slot)
{
	enginePost_init((VoiceCommon*)v, slot, &v->MixerPageFilter[0]);
	memset(&v->core, 0, sizeof v->core);	/* the slot's memory held another engine */
	const VoxParams p = { 54, 0, 64, 20, 64, 0, 0 };	/* "a", adult, no nasal, no consonant */
	v->par = p;
	v->dirty = 1;
}

void Vox_setRole(VoxVoice* v, const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	switch(role)
	{
	case 0:	v->par.src = (isFloatModulator ? lrintf(flt * 127.f) : itg) != 0;	break;
	case 1:	v->par.len = itg;	break;
	case 2:	v->par.nas = itg;	break;
	case 3:	v->par.cns = itg;	break;
	case 4:	v->par.vow = itg;	break;
	case 5:	v->par.tns = itg;	break;
	default: v->par.bre = itg;	break;
	}
	v->dirty = 1;
}

void Vox_trigger(VoxVoice* v, const uint8_t vel, const uint8_t note)
{
	lfo_retrigger(v->slot);
	modNode_updateValue(&velocityModulators[v->slot], vel/127.f);
	osc_setBaseNote(&v->osc, note);
	slopeEg2_trigger(&v->oscVolEg);
	v->velo = vel/127.f;
	transient_trigger(&v->transGen);
	SnapEg_trigger(&v->snapEg);
	enginePost_updatePan((VoiceCommon*)v);
	if(v->dirty)
	{
		v->dirty = 0;
		voxCore_setParams(&v->core, &v->par);
	}
	voxCore_trigger(&v->core);
}

void Vox_calcAsync(VoxVoice* v)
{
	enginePost_calcAmp((VoiceCommon*)v, &v->ampFilterInput, &v->lastGain);
	if(v->dirty)
	{
		v->dirty = 0;
		voxCore_setParams(&v->core, &v->par);
	}
	const float f0 = enginePost_pitch((VoiceCommon*)v);
	if(fabsf(f0 - v->core.f0) > f0 * 0.0005f)
		voxCore_setPitch(&v->core, f0, REAL_FS);
}

void Vox_calcSyncBlock(VoxVoice* v, int16_t* buf, const uint8_t size)
{
	int16_t tr[size];
	float out[size];
	transient_calcBlock(&v->transGen, tr, size);
	voxCore_render(&v->core, tr, out, size, REAL_FS);
	for(uint8_t i=0;i<size;i++)
		buf[i] = enginePost_s16(out[i] * VOX_OUT_GAIN);
	enginePost_block((VoiceCommon*)v, &v->MixerPageFilter[0], buf, size, v->ampFilterInput, v->lastGain);
}
