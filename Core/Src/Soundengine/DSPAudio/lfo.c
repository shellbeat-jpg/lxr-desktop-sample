/*
 * lfo.c
 *
 *  Created on: 14.01.2013
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */

//-------------------------------------------------------------
#include "Soundengine/DSPAudio/lfo.h"
#include "Soundengine/DSPAudio/VoiceSlot.h"
#include "Soundengine/DSPAudio/random.h"
#include <math.h>

#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/DSPAudio/HiHat.h"
#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/MIDI/valueShaper.h"
#include "SampleRom/sampleFlash.h"	//MOD: user waves
#include "Preset/KitEngines.h"		//MOD: extra LFOs
#include "menu/menu.h"				//MOD: modTargets
#include "menu/CcNr2Text.h"
#include <string.h>
//-------------------------------------------------------------
void lfo_init(Lfo *lfo)
{
	lfo->phase 			= 0;
	lfo->phaseInc 		= 1;
	lfo->phaseOffset	= 0;
	lfo->retrigger		= 0;
	lfo->waveform		= SINE;
	lfo->sync			= 0;
	lfo->freq			= 1;
	lfo->modNodeValue	= 1;
	lfo->cycleCount		= 0;
	lfo->osDelay		= 0;
	lfo->osDone			= 1;	//one-shots wait for the first trigger

	modNode_init(&lfo->modTarget);
}
//-------------------------------------------------------------
//MOD: the shapes, also used by the one-shot versions (x = phase 0..1)
static inline float lfo_expTri(const float x)
{
	const float y = x < 0.5f ? 2.f * x : 2.f * (1.f - x);
	return y * y * y;
}
//-------------------------------------------------------------
//MOD: a user wave (W1..W8) at phase x (0..2^32): 0..1, 0.5 if it isn't stored
static float lfo_userWave(const uint8_t k, const uint32_t phase)
{
	const int16_t* t = sampleFlash_wave(k);
	if(t == NULL)
		return 0.5f;
	const uint32_t i = phase >> 22;
	const int32_t a = t[i], b = t[(i + 1u) & (SF_WAVE_POINTS - 1u)];
	const int32_t s = a + (int32_t)(((int64_t)(b - a) * (phase & 0x3FFFFF)) >> 22);
	return (s + 32768) * (1.f / 65535.f);
}
//MOD: a running user wave (new cycles counted as starts, like the built-in ones)
__attribute__((noinline)) static float lfo_calcUser(Lfo *lfo, const uint32_t incInt)
{
	const uint32_t old = lfo->phase;
	lfo->phase += incInt;
	if(lfo->phase < old) { lfo->starts++; lfo->startCause = 0xff; }
	return lfo_userWave((uint8_t)(lfo->waveform - LFO_USER), lfo->phase);
}
//MOD: one-shot waveforms: delay, one cycle, hold
__attribute__((noinline)) static float lfo_calcOneShot(Lfo *lfo, const uint32_t incInt)
{
	const uint8_t w = lfo->waveform;
	if(w == LFO_NOISE_1)
		return lfo->rnd;		//drawn at the trigger
	if(lfo->osDelay)
	{
		if(lfo->osDelay > incInt) lfo->osDelay -= incInt;
		else { lfo->osDelay = 0; lfo->starts++; }	//MOD: the cycle starts after the delay
		lfo->phase = 0;
	}
	else if(!lfo->osDone)
	{
		const uint32_t old = lfo->phase;
		lfo->phase += incInt;
		if(lfo->phase < old)	//the cycle ended: hold its end
		{
			lfo->phase = 0xffffffff;
			lfo->osDone = 1;
		}
	}
	if(w >= LFO_USER_1)
		return lfo_userWave((uint8_t)(w - LFO_USER_1), lfo->phase);	//MOD: holds its end value after the cycle
	const float x = lfo->phase / (float)0xffffffff;
	switch(w)
	{
	case LFO_SINE_1:		return ((sine_table[(lfo->phase>>20)]/32767.f) + 1)/2.f;
	case LFO_TRI_1:			return 1.f - fabsf(x*2 - 1);
	case LFO_SAW_UP_1:		return x;
	case LFO_SAW_DOWN_1:	return 1.f - x;
	case LFO_REC_1:			return (lfo->osDelay || lfo->osDone || lfo->phase > 0x7fffffff) ? 0.f : 1.f;
	case LFO_EXP_UP_1:		return x*x*x;
	case LFO_EXP_DOWN_1:	{ const float y = 1.f - x; return y*y*y; }
	default:				return lfo_expTri(x);	//LFO_EXP_TRI_1
	}
}
//-------------------------------------------------------------
IN_ITCM_RAM float lfo_calc(Lfo *lfo)
{
	float inc = lfo->phaseInc*lfo->modNodeValue;
	uint32_t incInt = inc;

	//MOD: one-shot and user waveforms (in flash, not in the nearly full ITCM)
	if(lfo->waveform >= LFO_SINE_1)
		return (lfo->waveform >= LFO_USER && lfo->waveform < LFO_USER_1) ? lfo_calcUser(lfo, incInt) : lfo_calcOneShot(lfo, incInt);

	uint32_t oldPhase 		= lfo->phase;
	lfo->phase 			   += incInt;//lfo->phaseInc*lfo->modNodeValue;
	const uint8_t overflow 	= oldPhase>lfo->phase;
	if(overflow) { lfo->starts++; lfo->startCause = 0xff; }	//MOD: a new cycle

	switch(lfo->waveform)
	{
	//---
		case LFO_SINE:
			return ((sine_table[(lfo->phase>>20)]/32767.f) + 1)/2.f;
		break;
	//---
		case LFO_TRI:
			return (1.f-fabsf( (lfo->phase/(float)0xffffffff)*2-1 ) );
			break;
	//---
		case LFO_SAW_UP:
		return lfo->phase/(float)0xffffffff ;
		break;
	//---
		case LFO_REC:
		if(lfo->phase > 0x7fffffff)
		{
			return 1.f;
		}
		else
		{
			return 0.f;
		}
		break;
	//---
		case LFO_NOISE:
		if(overflow)
		{
			lfo->rnd = GetRngValue();
			lfo->rnd = lfo->rnd  / (float)0xffffffff ;
		}

			return lfo->rnd;
		break;
	//---
		case LFO_SAW_DOWN:
		return (0xffffffff-(lfo->phase)) / (float)0xffffffff;
		break;
	//---
		case LFO_EXP_UP:
			{
				float x = lfo->phase/(float)0xffffffff;
				return x*x*x;
			}
			break;
	//---
		case LFO_CYCLE:
			{
				//MOD: mod+1 steps (the modulation amount counts the further
				//values), one step per trigger; the step's centre so that a
				//sample range (sampleFlash_rangeValue) gets exactly one sample
				const uint32_t n = (uint32_t)(lfo->modTarget.amount * 127.f + 0.5f) + 1u;
				const uint32_t k = (lfo->cycleCount + n - 1u) % n;
				return ((float)k + 0.5f) / (float)n;
			}
			break;
	//---
		case LFO_EXP_TRI:	//MOD
			return lfo_expTri(lfo->phase/(float)0xffffffff);
	//---
		case LFO_EXP_DOWN:
			{
				float x = (0xffffffff-(lfo->phase)) / (float)0xffffffff;
				return x*x*x;//valueShaperF2F(x,-0.9f);
			}
			break;
	//---
		default:
		return 0;
		break;
	}
	return 0;
}
//-------------------------------------------------------------
void lfo_dispatchNextValue(Lfo* lfo)
{
	float val = lfo_calc(lfo);
	modNode_updateValue(&lfo->modTarget,val);
}
//-------------------------------------------------------------
//MOD: in flash (only on parameter and tempo changes; the ITCM is full)
uint32_t lfo_calcPhaseInc(float freq, uint8_t sync)
{
	if(sync==0)//no sync
	{
		return freq/(LFO_SR) * 0xffffffff;
	}

	//sync is on

	float bpm =  seq_getBpm();

	float tempoAsFrequency = bpm > 0.0 ? bpm / 60.f /4: 0.0f; //bpm/60 = beat duration /4 = bar duration

	float scaler;

	switch(sync)
	{
		default: //no sync

		return freq/(LFO_SR) * 0xffffffff;
		break;

		case 1: // 4/1
		scaler = 0.25f;//NoteTypeScalers[Quadruple] * NoteDivisionScalers[Whole];
		break;

		case 2: // 2/1
		scaler = 0.5f;//NoteTypeScalers[Double] * NoteDivisionScalers[Whole];
		break;

		case 3: // 1/1
		scaler = 1;//NoteTypeScalers[Regular] * NoteDivisionScalers[Whole];
		break;

		case 4: // 1/2
		scaler = 2;// NoteTypeScalers[Regular] * NoteDivisionScalers[Half];
		break;

		case 5: //1/3
		scaler = 3;// NoteTypeScalers[Regular] * NoteDivisionScalers[Third];
		break;

		case 6: // 1/4
		scaler = 4;// NoteTypeScalers[Regular] * NoteDivisionScalers[Quarter];
		break;

		case 7: // 1/6
		scaler = 6;// NoteTypeScalers[Regular] * NoteDivisionScalers[Sixth];
		break;

		case 8: // 1/8
		scaler = 8;// NoteTypeScalers[Regular] * NoteDivisionScalers[Eighth];
		break;

		case 9: // 1/12
		scaler = 12;//NoteTypeScalers[Regular] * NoteDivisionScalers[Twelfth];
		break;

		case 10: // 1/16
		scaler = 16;//NoteTypeScalers[Regular] * NoteDivisionScalers[Sixteenth];
		break;

		case 11: // 1/32
		scaler = 32;//NoteTypeScalers[Regular] * NoteDivisionScalers[Sixteenth];
		break;
	}

	float lfoSyncfreq = tempoAsFrequency * scaler;
	return (lfoSyncfreq / (LFO_SR)) * 0xffffffff;
}
//-------------------------------------------------------------
void lfo_setFreq(Lfo *lfo, float f)	//MOD: in flash
{
	//f += 1;
	//f = f/128.f;
	f = 0.0078125f + f* (1.f-0.0078125f); //to prevent f from being zero
	f = f*f*f;
	lfo->freq = f*LFO_MAX_F;
	lfo->phaseInc = lfo_calcPhaseInc(lfo->freq,lfo->sync);
}
//-------------------------------------------------------------
void lfo_setSync(Lfo* lfo, uint8_t sync)	//MOD: in flash
{
	lfo->sync = sync;
	lfo->phaseInc = lfo_calcPhaseInc(lfo->freq,lfo->sync);
}
//-------------------------------------------------------------
void lfo_recalcSync()
{
	for(uint8_t i=0;i<6;i++)
	{
		Lfo* lfo = &voiceSlots[i].u.c.lfo;
		lfo->phaseInc = lfo_calcPhaseInc(lfo->freq,lfo->sync);
	}
	for(uint8_t i=0;i<LFO_NUM_EXTRA;i++)	//MOD
	{
		Lfo* lfo = &lfo_extra[i];
		lfo->phaseInc = lfo_calcPhaseInc(lfo->freq,lfo->sync);
	}
}
//-------------------------------------------------------------
uint8_t lfo_getStarts(uint8_t slot, uint8_t* cause)
{
	const Lfo* lfo = &voiceSlots[slot < 6 ? slot : 5].u.c.lfo;
	*cause = lfo->startCause;
	return lfo->starts;
}
//-------------------------------------------------------------
//a trigger of voice slot 'voice' for one LFO (own = it is the LFO's own voice)
static void lfo_retriggerOne(Lfo* lfo, const uint8_t voice, const uint8_t own)
{
		if(lfo->retrigger == voice+1)
		{
			lfo->phase = lfo->phaseOffset;
			if(!lfo_isOneShot(lfo->waveform))
			{
				lfo->starts++;	//MOD: restarted
				lfo->startCause = voice;
			}
		}
		//MOD: the cycle LFO steps on each trigger of its retrigger voice, without
		//retrigger on each trigger of its own voice
		if(lfo->waveform == LFO_CYCLE && (lfo->retrigger == voice+1 || (lfo->retrigger == 0 && own)))
		{
			lfo->cycleCount++;
		}
		//MOD: one-shot waveforms start (after the delay = offset) on each trigger
		//of the retrigger voice, without retrigger of the LFO's own voice
		if(lfo_isOneShot(lfo->waveform) && (lfo->retrigger == voice+1 || (lfo->retrigger == 0 && own)))
		{
			lfo->phase   = 0;
			lfo->osDone  = 0;
			lfo->osDelay = lfo->phaseOffset;
			lfo->startCause = voice;	//MOD
			if(!lfo->osDelay)
				lfo->starts++;		//MOD: starts now (otherwise after the delay)
			if(lfo->waveform == LFO_NOISE_1)
				lfo->rnd = GetRngValue() / (float)0xffffffff;
		}
}
//-------------------------------------------------------------
void lfo_retrigger(uint8_t voice)
{
	//restart the LFOs that are retriggered by this voice
	for(uint8_t i=0;i<6;i++)
		lfo_retriggerOne(&voiceSlots[i].u.c.lfo, voice, i == voice);
}
//-------------------------------------------------------------
//MOD: extra LFOs
Lfo lfo_extra[LFO_NUM_EXTRA];
static uint8_t lfo_extraSet[LFO_NUM_EXTRA][KIT_LFOX_STRIDE];	//the settings taken over
static uint8_t lfo_extraReady = 0;
static float lfo_extraVal[LFO_NUM_EXTRA];

static const uint8_t* lfo_extraKit(const uint8_t x)
{
	return &kitEngines[KIT_LFOX_OFFSET + x * KIT_LFOX_STRIDE];
}
//the settings of the kit (the values of the stock LFO parameters, see MidiParser.c)
static void lfo_extraApply(const uint8_t x, const uint8_t* s)
{
	Lfo* lfo = &lfo_extra[x];
	uint8_t* o = lfo_extraSet[x];
	if(!lfo_extraReady || o[LFOX_FRQ] != s[LFOX_FRQ] || o[LFOX_SYN] != s[LFOX_SYN])
	{
		lfo->sync = s[LFOX_SYN];
		lfo_setFreq(lfo, s[LFOX_FRQ] / 127.f);
	}
	lfo->modTarget.amount = s[LFOX_AMT] / 127.f;
	lfo->waveform = s[LFOX_WAV] < NUM_LFO_WAVEFORMS ? s[LFOX_WAV] : 0;
	lfo->retrigger = s[LFOX_RTG] <= NUM_VOICE_SLOTS ? s[LFOX_RTG] : 0;
	lfo->phaseOffset = (uint32_t)(s[LFOX_OFS] / 127.f * 0xffffffff);
	if(!lfo_extraReady || o[LFOX_DST] != s[LFOX_DST])
	{
		const uint8_t t = s[LFOX_DST] < getNumModTargets() ? s[LFOX_DST] : 0;
		modNode_setDestination(&lfo->modTarget, t ? (uint8_t)modTargets[t].param : 0);	//as the stock LFO (CC_LFO_TARGET)
	}
	memcpy(o, s, KIT_LFOX_STRIDE);
}
void lfo_extraCalc(void)
{
	if(!lfo_extraReady)
	{
		for(uint8_t x=0;x<LFO_NUM_EXTRA;x++)
			lfo_init(&lfo_extra[x]);
	}
	for(uint8_t x=0;x<LFO_NUM_EXTRA;x++)
	{
		const uint8_t* s = lfo_extraKit(x);
		if(!lfo_extraReady || memcmp(s, lfo_extraSet[x], KIT_LFOX_STRIDE) != 0)
			lfo_extraApply(x, s);
	}
	lfo_extraReady = 1;
	for(uint8_t x=0;x<LFO_NUM_EXTRA;x++)
	{
		if(lfo_extra[x].modTarget.destination)	//without target: off
			lfo_extraVal[x] = lfo_calc(&lfo_extra[x]);
	}
}
uint8_t lfo_t7Active(void)
{
	return slot_t7Playing && lfo_extra[LFO_T7].modTarget.destination;
}
void lfo_extraDispatch(void)
{
	for(uint8_t x=0;x<LFO_T7;x++)
	{
		if(lfo_extra[x].modTarget.destination)
			modNode_updateValue(&lfo_extra[x].modTarget, lfo_extraVal[x]);
	}
	if(lfo_t7Active())
		modNode_updateValue(&lfo_extra[LFO_T7].modTarget, lfo_extraVal[LFO_T7]);
}
void lfo_extraResetTargets(void)
{
	for(uint8_t x=0;x<LFO_NUM_EXTRA;x++)
	{
		const ModulationNode* t = &lfo_extra[x].modTarget;
		if(t->destination)
			paramArray_setParameter(t->destination, t->originalValue);
	}
}
void lfo_extraRetrigger(const uint8_t slot, const uint8_t second)
{
	for(uint8_t x=0;x<NUM_VOICE_SLOTS;x++)
		lfo_retriggerOne(&lfo_extra[x], slot, x == slot);
	//the track 7 LFO: its own trigger (and rtg v6) is a trigger of track 7
	if(slot != NUM_VOICE_SLOTS-1 || second)
		lfo_retriggerOne(&lfo_extra[LFO_T7], slot, second);
}
//-------------------------------------------------------------
