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

#define WAVE_OUT_GAIN	(0.85f)
#define WAVE_MAX_FRAMES	(4 + 8)

/* a frame: its table, 2^bits points, the phase offset that starts it at 0 */
typedef struct { const int16_t* t; uint32_t off; uint8_t bits; } WaveFrame;

/* the frames of the voice's set; the built-in ones band limited for the pitch */
static uint8_t wave_frames(const WaveVoice* v, const float f0, WaveFrame* fr)
{
	uint8_t n = 0;
	const uint8_t users = sampleFlash_getWaveCount();
	const uint8_t set = (v->set == 1 && !users) ? 0 : v->set;	/* no user waves: the basic set */
	if(set != 1)
	{
		/* tables per octave from 8.18 Hz (wavetable.h) */
		int lvl = f0 > 8.1758f ? (int)log2f(f0 / 8.1758f) : 0;
		if(lvl > 10) lvl = 10;
		/* sine and triangle start at their minimum */
		fr[n].t = sine_table;		fr[n].off = ENGINEPOST_QUARTER;	fr[n++].bits = 12;
		fr[n].t = triTable[lvl];	fr[n].off = ENGINEPOST_QUARTER;	fr[n++].bits = 10;
		fr[n].t = sawTable[lvl];	fr[n].off = 0;					fr[n++].bits = 10;
		fr[n].t = recTable[lvl];	fr[n].off = 0;					fr[n++].bits = 10;
	}
	if(set != 0)
	{
		for(uint8_t k=0;k<users && k<8;k++)
		{
			fr[n].t = sampleFlash_wave(k);
			fr[n].off = 0;
			fr[n++].bits = 10;	/* SF_WAVE_POINTS = 1024 */
		}
	}
	return n;
}

void Wave_init(WaveVoice* v, uint8_t slot)
{
	enginePost_init((VoiceCommon*)v, slot, &v->MixerPageFilter[0]);
	DecayEg_init(&v->oscPitchEg);
	v->egPitchModAmount = 0.5f;
	v->sweep = 0.4f;
	v->lastFrame = -1.f;
}

void Wave_setRole(WaveVoice* v, const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	switch(role)
	{
	case 0:
	{
		uint8_t s = isFloatModulator ? (uint8_t)(flt * WAVE_NUM_SETS) : itg;
		v->set = s >= WAVE_NUM_SETS ? WAVE_NUM_SETS-1 : s;
		break;
	}
	case 4:	v->pos = flt;			break;
	case 5:	v->sweep = flt;			break;
	case 6:	v->stepped = itg != 0;	break;
	default:						break;	/* 1-3: pitch EG (VoiceSlot.c) */
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
	v->ph = 0;
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
	WaveFrame fr[WAVE_MAX_FRAMES];
	const uint8_t n = wave_frames(v, f0, fr);
	/* position: the setting plus the sweep by the pitch EG, over the frames */
	float p = v->pos + v->sweep * v->egValue;
	if(p > 1.f) p = 1.f;
	float end = p * (float)(n - 1u);
	if(v->stepped) end = rintf(end);
	float start = v->lastFrame < 0.f || v->lastFrame > (float)(n - 1u) ? end : v->lastFrame;
	const float d = (end - start) / size;
	const uint32_t inc = enginePost_phaseInc(f0);
	uint32_t ph = v->ph;
	float fp = start;
	for(uint8_t i=0;i<size;i++)
	{
		fp += d;
		uint8_t a = (uint8_t)fp;
		if(a >= n - 1u) a = (uint8_t)(n > 1u ? n - 2u : 0u);
		float x = enginePost_read(fr[a].t, fr[a].bits, ph + fr[a].off);
		if(n > 1u)
		{
			const float f = fp - a;
			const float y = enginePost_read(fr[a+1u].t, fr[a+1u].bits, ph + fr[a+1u].off);
			x += (y - x) * f;
		}
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
