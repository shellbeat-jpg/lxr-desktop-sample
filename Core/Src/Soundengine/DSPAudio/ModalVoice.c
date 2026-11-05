/*
 * ModalVoice.c
 *
 * MOD (OE02-Samples+): test engine "modal" (ModalVoice.h, ModalCore.h).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "Soundengine/DSPAudio/ModalVoice.h"
#include "Soundengine/DSPAudio/BufferTools.h"
#include "Soundengine/DSPAudio/modulationNode.h"
#include "Soundengine/config.h"
#include "IO/adcPots.h"
#include <string.h>

#define MODAL_OUT_GAIN	(0.3f * 32767.f)	/* a hard strike peaks at about full scale */

void Modal_init(ModalVoice* v, uint8_t slot)
{
	v->slot = slot;
	initOnePole(&v->FaderVolumeFilter);
	setOnePoleCoef(&v->FaderVolumeFilter, 0.01);
	for(uint8_t j=0;j<2;j++)
	{
		initOnePole(&v->MixerPageFilter[j]);
		setOnePoleCoef(&v->MixerPageFilter[j], 0.01);
	}
	initOnePole(&v->distortion.DriveParamFilter);
	setOnePoleCoef(&v->distortion.DriveParamFilter, 0.01);

	SnapEg_init(&v->snapEg);
	v->pan = 0;
	v->vol = 0.8f;
	v->volumeMod = 1;
	transient_init(&v->transGen);
	SVF_init(&v->filter);
	v->filterType = 0x01;
	lfo_init(&v->lfo);

	v->osc.freq = 440;
	v->osc.waveform = 0;
	v->osc.midiFreq = 70<<8 | 64;
	v->osc.pitchMod = 1.f;
	v->osc.modNodeValue = 1;

	slopeEg2_init(&v->oscVolEg);
	setDistortionShape(&v->distortion, 0.f);

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
		uint8_t m = isFloatModulator ? (uint8_t)(flt * MODAL_MODELS) : itg;
		v->par.model = m >= MODAL_MODELS ? MODAL_MODELS-1 : m;
		break;
	}
	case 1:	v->par.damp = itg;			break;
	case 2:	v->par.pos = itg;			break;
	case 3:	v->par.inh = itg;			break;
	case 4:	v->par.hard = itg;			break;
	case 5:	v->par.mat = itg;			break;
	default: v->par.noise = itg != 0;	break;
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
	v->triggeredPanL = sqrtf((127-v->pan)/127.f);
	v->triggeredPanR = sqrtf(v->pan/127.f);
	modalCore_strike(&v->core, &v->par);
}

void Modal_calcAsync(ModalVoice* v)
{
	//amp EG: no interpolation in the attack if it is 0 (like the drum)
	if((v->oscVolEg.attack == 1) && ((v->oscVolEg.state == EG_A) || (v->oscVolEg.state == EG_REPEAT)))
	{
		v->ampFilterInput = (float)slopeEg2_calc(&v->oscVolEg);
		v->lastGain = v->ampFilterInput;
	}
	else
	{
		v->lastGain = v->ampFilterInput;
		v->ampFilterInput = (float)slopeEg2_calc(&v->oscVolEg);
	}

	if(v->dirty)
	{
		v->dirty = 0;
		modalCore_setParams(&v->core, &v->par, REAL_FS);
	}
	float f0 = v->osc.freq * v->osc.pitchMod * v->osc.modNodeValue;
	if(v->osc.altPitch > 0.f) f0 *= v->osc.altPitch;	//track 7 coarse
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
	{
		const int32_t s = (int32_t)(out[i] * MODAL_OUT_GAIN);
		buf[i] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
	}
	SVF_calcBlockZDF(&v->filter, v->filterType, buf, size);
	bufferTool_addGainInterpolated(buf, v->ampFilterInput, v->lastGain, size);
	if(v->volumeMod)
		bufferTool_addGain(buf, v->velo, size);
	calcDistBlockFiltered(&v->distortion, buf, size);

	const float faderVolume = adc_getFaderVolume(v->slot);
	for(uint8_t i=0;i<size;i++)
	{
		const float mixVol = calcOnePole(&v->MixerPageFilter[0], v->vol);
		v->combinedVolume = mixVol * calcOnePole(&v->FaderVolumeFilter, faderVolume);
		buf[i] *= v->combinedVolume;
	}
}
