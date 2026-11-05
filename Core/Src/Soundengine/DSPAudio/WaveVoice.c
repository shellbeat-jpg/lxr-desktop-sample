/*
 * WaveVoice.c
 *
 * MOD (OE02-Samples+): test engine "wavetable" (WaveVoice.h).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "Soundengine/DSPAudio/WaveVoice.h"
#include "Soundengine/DSPAudio/EnginePost.h"
#include "Soundengine/DSPAudio/modulationNode.h"
#include "SampleRom/sampleFlash.h"
#include "SampleRom/wtBuild.h"

#define WAVE_OUT_GAIN	(0.85f * 32767.f / 30000.f)	/* frames peak at 30000 */

/* the frames of the voice's set: part a (and part b for "all"); a missing set
 * of the user waves or of TABLES plays the basic set */
typedef struct { const int16_t* a; const int16_t* b; uint8_t na, nb; } WaveSet;
static void wave_set(const uint8_t set, WaveSet* s)
{
	s->a = &waveBasic[0][0];
	s->na = waveBasicFrames;
	s->b = NULL;
	s->nb = 0;
	uint8_t n = 0;
	const int16_t* t;
	switch(set)
	{
	case 0:	break;
	case 1:	if((t = sampleFlash_table(0, &n)) != NULL) { s->a = t; s->na = n; }	break;
	case 2:	if((t = sampleFlash_table(0, &n)) != NULL) { s->b = t; s->nb = n; }	break;
	default: if((t = sampleFlash_table((uint8_t)(set - 2u), &n)) != NULL) { s->a = t; s->na = n; }	break;
	}
}
static inline const int16_t* wave_frame(const WaveSet* s, const uint8_t i)
{
	return i < s->na ? s->a + (uint32_t)i * WT_FRAME_SIZE : s->b + (uint32_t)(i - s->na) * WT_FRAME_SIZE;
}
/* a frame at a phase: level la, blended towards level la + 1 by lw */
static inline float wave_read(const int16_t* f, const uint8_t la, const float lw, const uint32_t ph)
{
	float x = enginePost_read(f + WT_LEVEL_OFS(la), WT_LEVEL_BITS(la), ph);
	if(lw > 0.02f)
		x += (enginePost_read(f + WT_LEVEL_OFS(la + 1u), WT_LEVEL_BITS(la + 1u), ph) - x) * lw;
	return x;
}

void Wave_init(WaveVoice* v, uint8_t slot)
{
	enginePost_init((VoiceCommon*)v, slot, &v->MixerPageFilter[0]);
	DecayEg_init(&v->oscPitchEg);
	v->egPitchModAmount = 0.5f;
	v->sweep = 0.47f;
	v->lastFrame = -1.f;
}

void Wave_setRole(WaveVoice* v, const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	switch(role)
	{
	case 0:
	{
		const uint8_t s = isFloatModulator ? (uint8_t)(flt * WAVE_NUM_SETS) : itg;
		v->set = s >= WAVE_NUM_SETS ? WAVE_NUM_SETS-1 : s;
		v->lastFrame = -1.f;
		break;
	}
	case 4:	v->pos = flt;		break;
	case 5:	v->sweep = flt;		break;
	case 6:
	{
		const uint8_t s = isFloatModulator ? (uint8_t)(flt * WAVE_NUM_SCANS) : itg;
		v->scan = s >= WAVE_NUM_SCANS ? WAVE_NUM_SCANS-1 : s;
		break;
	}
	default:					break;	/* 1-3: pitch EG (VoiceSlot.c) */
	}
}

void Wave_trigger(WaveVoice* v, const uint8_t vel, const uint8_t note)
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
	v->ph = 0;				/* the frames start at 0 (aligned) */
	v->lastFrame = -1.f;	/* the sweep starts at its value, no glide from the last one */
}

void Wave_calcAsync(WaveVoice* v)
{
	const float eg = DecayEg_calc(&v->oscPitchEg);
	v->egValue = eg;
	v->osc.pitchMod = 1.f + eg * v->egPitchModAmount;
	if(v->transGen.waveform == 0)
		v->osc.pitchMod += SnapEg_calc(&v->snapEg, v->transGen.pitch) * v->transGen.volume;
	enginePost_calcAmp((VoiceCommon*)v, &v->ampFilterInput, &v->lastGain);
}

void Wave_calcSyncBlock(WaveVoice* v, int16_t* buf, const uint8_t size)
{
	const float f0 = enginePost_pitch((VoiceCommon*)v);
	WaveSet s;
	wave_set(v->set, &s);
	const uint8_t n = (uint8_t)(s.na + s.nb);

	/* level: the full frame has 127 harmonics; above ~150 Hz fewer fit below 20 kHz */
	float l = log2f(128.f * f0 / (0.45f * REAL_FS));
	if(!(l > 0.f)) l = 0.f;
	/* above the last level (about 6.6 kHz) towards a sine */
	float sw = l - (float)(WT_LEVELS - 1u);
	sw = sw < 0.f ? 0.f : sw > 1.f ? 1.f : sw;
	if(l > (float)(WT_LEVELS - 1u)) l = (float)(WT_LEVELS - 1u);
	uint8_t la = (uint8_t)l;
	float lw = l - (float)la;
	if(la >= WT_LEVELS - 1u) { la = WT_LEVELS - 1u; lw = 0.f; }

	/* position: the setting plus the sweep by the pitch EG */
	float p = v->pos + v->sweep * v->egValue;
	if(p > 1.f) p = 1.f;
	if(v->scan >= 2u) p = p <= 0.5f ? 2.f * p : 2.f - 2.f * p;	/* back and forth */
	float end = p * (float)(n - 1u);
	if(v->scan & 1u) end = rintf(end);		/* stepped: a short crossfade per step */
	const float start = (v->lastFrame < 0.f || v->lastFrame > (float)(n - 1u)) ? end : v->lastFrame;
	const float d = (end - start) / size;

	const uint32_t inc = enginePost_phaseInc(f0);
	uint32_t ph = v->ph;
	float fp = start;
	for(uint8_t i=0;i<size;i++)
	{
		fp += d;
		uint8_t a = (uint8_t)fp;
		if(n > 1u && a >= n - 1u) a = (uint8_t)(n - 2u);
		if(n <= 1u) a = 0;
		const float f = n > 1u ? fp - (float)a : 0.f;
		float x = wave_read(wave_frame(&s, a), la, lw, ph);
		if(f > 0.001f)
			x += (wave_read(wave_frame(&s, (uint8_t)(a + 1u)), la, lw, ph) - x) * f;
		if(sw > 0.f)
			x += (enginePost_sin(ph) * 24000.f - x) * sw;
		buf[i] = enginePost_s16(x * WAVE_OUT_GAIN);
		ph += inc;
	}
	v->ph = ph;
	v->lastFrame = end;

	int16_t tr[size];
	transient_calcBlock(&v->transGen, tr, size);
	bufferTool_addBuffersSaturating(buf, tr, size);
	enginePost_block((VoiceCommon*)v, &v->MixerPageFilter[0], buf, size, v->ampFilterInput, v->lastGain);
}
