/*
 * EnginePost.h
 *
 * MOD (OE02-Samples+): the parts the added engines (modal, FM2, wavetable) share
 * with the drum engine: amp EG, filter, velocity, distortion, volume and fader,
 * the common init, and a sine lookup.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef ENGINEPOST_H_
#define ENGINEPOST_H_

#include "Soundengine/DSPAudio/VoiceCommon.h"
#include "Soundengine/DSPAudio/BufferTools.h"
#include "Soundengine/DSPAudio/wavetable.h"
#include "Soundengine/config.h"
#include "IO/adcPots.h"
#include <math.h>

/* the common fields as the drum engine sets them (filter low-pass, volume 0.8 ...) */
static inline void enginePost_init(VoiceCommon* v, const uint8_t slot, OnePoleFilter* mixFilter)
{
	v->slot = slot;
	initOnePole(&v->FaderVolumeFilter);
	setOnePoleCoef(&v->FaderVolumeFilter, 0.01);
	initOnePole(mixFilter);
	setOnePoleCoef(mixFilter, 0.01);
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
}

static inline void enginePost_updatePan(VoiceCommon* v)
{
	v->triggeredPanL = sqrtf((127-v->pan)/127.f);
	v->triggeredPanR = sqrtf(v->pan/127.f);
}

/* amp EG of this block (no interpolation in an attack of 0, like the drum) */
static inline void enginePost_calcAmp(VoiceCommon* v, float* ampIn, float* lastGain)
{
	if((v->oscVolEg.attack == 1) && ((v->oscVolEg.state == EG_A) || (v->oscVolEg.state == EG_REPEAT)))
	{
		*ampIn = (float)slopeEg2_calc(&v->oscVolEg);
		*lastGain = *ampIn;
	}
	else
	{
		*lastGain = *ampIn;
		*ampIn = (float)slopeEg2_calc(&v->oscVolEg);
	}
}

/* the pitch of the voice's note incl. pitch modulation and track 7 coarse */
static inline float enginePost_pitch(const VoiceCommon* v)
{
	float f = v->osc.freq * v->osc.pitchMod * v->osc.modNodeValue;
	if(v->osc.altPitch > 0.f) f *= v->osc.altPitch;
	return f;
}

/* filter, amp EG, velocity, distortion, volume and fader */
static inline void enginePost_block(VoiceCommon* v, OnePoleFilter* mixFilter, int16_t* buf, const uint8_t size,
		const float ampIn, const float lastGain)
{
	SVF_calcBlockZDF(&v->filter, v->filterType, buf, size);
	bufferTool_addGainInterpolated(buf, ampIn, lastGain, size);
	if(v->volumeMod)
		bufferTool_addGain(buf, v->velo, size);
	calcDistBlockFiltered(&v->distortion, buf, size);
	const float faderVolume = adc_getFaderVolume(v->slot);
	for(uint8_t i=0;i<size;i++)
	{
		const float mixVol = calcOnePole(mixFilter, v->vol);
		v->combinedVolume = mixVol * calcOnePole(&v->FaderVolumeFilter, faderVolume);
		buf[i] *= v->combinedVolume;
	}
}

/* float to int16 with clipping */
static inline int16_t enginePost_s16(const float x)
{
	const int32_t s = (int32_t)x;
	return (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
}

/* phase (32 bit = one cycle) of a fraction of a cycle, any size */
static inline uint32_t enginePost_phase(float cycles)
{
	cycles -= rintf(cycles);	/* -0.5 .. 0.5 */
	if(cycles > 0.4999999f) cycles = 0.4999999f;
	return (uint32_t)(int32_t)(cycles * 4294967296.f);
}
static inline uint32_t enginePost_phaseInc(const float f)
{
	const float c = f * (1.f / REAL_FS);
	return c >= 0.5f ? 0x7fffffffu : (uint32_t)(c * 4294967296.f);
}

/* a table of 2^bits points (one cycle) at a phase, linear interpolation */
static inline float enginePost_read(const int16_t* t, const uint8_t bits, const uint32_t ph)
{
	const uint32_t i = ph >> (32u - bits);
	const float f = (float)((ph << bits) >> 8) * (1.f / 16777216.f);
	const float a = t[i];
	return a + (t[(i + 1u) & ((1u << bits) - 1u)] - a) * f;
}
/* sine -1 .. 1, 0 at phase 0 (the table starts at its minimum) */
#define ENGINEPOST_QUARTER	0x40000000u
static inline float enginePost_sin(const uint32_t ph)
{
	return enginePost_read(sine_table, 12, ph + ENGINEPOST_QUARTER) * (1.f / 32767.f);
}

#endif /* ENGINEPOST_H_ */
