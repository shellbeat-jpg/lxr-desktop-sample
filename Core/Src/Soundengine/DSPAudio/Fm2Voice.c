/*
 * Fm2Voice.c
 *
 * MOD (OE02-Samples+): test engine "fm2" (Fm2Voice.h).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "Soundengine/DSPAudio/Fm2Voice.h"
#include "Soundengine/DSPAudio/EnginePost.h"
#include "Soundengine/DSPAudio/modulationNode.h"

#define FM2_OUT_GAIN	(0.85f * 32767.f)

/* modulator : carrier, as the menu lists them (fm2RatioNames) */
static const float fm2_ratios[FM2_NUM_RATIOS] =
{
	0.5f, 0.75f, 1.f, 1.414f, 1.5f, 2.f, 2.5f, 2.756f, 3.f, 3.5f, 4.f, 5.f, 6.f, 7.f, 8.f, 11.f
};

/* index decay 0..127: 5 ms .. 4 s to -60 dB, as a factor per sample */
static float fm2_decayFactor(const uint8_t v)
{
	const float t = 0.005f * powf(800.f, v / 127.f);
	return expf(-6.9078f / (t * REAL_FS));
}

void Fm2_init(Fm2Voice* v, uint8_t slot)
{
	enginePost_init((VoiceCommon*)v, slot, &v->MixerPageFilter[0]);
	DecayEg_init(&v->oscPitchEg);
	v->egPitchModAmount = 0.5f;
	v->ratio = 1.f;
	v->index = 0.5f;
	v->idxDecay = 40;
	v->envK = fm2_decayFactor(v->idxDecay);
}

void Fm2_setRole(Fm2Voice* v, const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	switch(role)
	{
	case 0:
	{
		/* modulated: around the stored list index (the LFO works on 0..127) */
		const uint8_t r = isFloatModulator ? (uint8_t)lrintf(flt * 127.f) : itg;
		v->ratio = fm2_ratios[r >= FM2_NUM_RATIOS ? FM2_NUM_RATIOS-1 : r];
		break;
	}
	case 4:	v->index = flt * flt * 2.f;		break;	/* up to 2 cycles (12.6 rad) */
	case 5:	v->idxDecay = itg; v->envK = fm2_decayFactor(itg);	break;
	case 6:	v->fb = flt * 0.25f;			break;
	default:								break;	/* 1-3: pitch EG (VoiceSlot.c) */
	}
}

void Fm2_trigger(Fm2Voice* v, const uint8_t vel, const uint8_t note)
{
	lfo_retrigger(v->slot);
	modNode_updateValue(&velocityModulators[v->slot], vel/127.f);
	osc_setBaseNote(&v->osc, note);
	DecayEg_trigger(&v->oscPitchEg);
	slopeEg2_trigger(&v->oscVolEg);
	v->velo = vel/127.f;
	transient_trigger(&v->transGen);
	SnapEg_trigger(&v->snapEg);
	enginePost_updatePan((VoiceCommon*)v);
	v->phC = v->phM = 0;	/* sine from 0: no click */
	v->lastM = 0.f;
	v->env = 1.f;
}

void Fm2_calcAsync(Fm2Voice* v)
{
	const float eg = DecayEg_calc(&v->oscPitchEg);
	v->osc.pitchMod = 1.f + eg * v->egPitchModAmount;
	if(v->transGen.waveform == 0)	/* snap, like the drum */
		v->osc.pitchMod += SnapEg_calc(&v->snapEg, v->transGen.pitch) * v->transGen.volume;
	enginePost_calcAmp((VoiceCommon*)v, &v->ampFilterInput, &v->lastGain);
}

void Fm2_calcSyncBlock(Fm2Voice* v, int16_t* buf, const uint8_t size)
{
	const float f0 = enginePost_pitch((VoiceCommon*)v);
	const uint32_t incC = enginePost_phaseInc(f0);
	const uint32_t incM = enginePost_phaseInc(f0 * v->ratio);
	uint32_t phC = v->phC, phM = v->phM;
	float m = v->lastM, env = v->env;
	const float k = v->envK, index = v->index, fb = v->fb;
	for(uint8_t i=0;i<size;i++)
	{
		m = enginePost_sin(phM + enginePost_phase(fb * m));
		const float x = enginePost_sin(phC + enginePost_phase(index * env * m));
		buf[i] = enginePost_s16(x * FM2_OUT_GAIN);
		phC += incC;
		phM += incM;
		env *= k;
	}
	v->phC = phC; v->phM = phM; v->lastM = m;
	v->env = env < 1e-6f ? 0.f : env;

	/* the transient sample on top, like the drum */
	int16_t tr[size];
	transient_calcBlock(&v->transGen, tr, size);
	bufferTool_addBuffersSaturating(buf, tr, size);
	enginePost_block((VoiceCommon*)v, &v->MixerPageFilter[0], buf, size, v->ampFilterInput, v->lastGain);
}
