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

	//MOD: one-shot waveforms (in flash, not in the nearly full ITCM)
	if(lfo->waveform >= LFO_SINE_1)
		return lfo_calcOneShot(lfo, incInt);

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
IN_ITCM_RAM uint32_t lfo_calcPhaseInc(float freq, uint8_t sync)
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
IN_ITCM_RAM void lfo_setFreq(Lfo *lfo, float f)
{
	//f += 1;
	//f = f/128.f;
	f = 0.0078125f + f* (1.f-0.0078125f); //to prevent f from being zero
	f = f*f*f;
	lfo->freq = f*LFO_MAX_F;
	lfo->phaseInc = lfo_calcPhaseInc(lfo->freq,lfo->sync);
}
//-------------------------------------------------------------
IN_ITCM_RAM void lfo_setSync(Lfo* lfo, uint8_t sync)
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
}
//-------------------------------------------------------------
uint8_t lfo_getStarts(uint8_t slot, uint8_t* cause)
{
	const Lfo* lfo = &voiceSlots[slot < 6 ? slot : 5].u.c.lfo;
	*cause = lfo->startCause;
	return lfo->starts;
}
//-------------------------------------------------------------
void lfo_retrigger(uint8_t voice)
{
	//restart the LFOs that are retriggered by this voice
	for(uint8_t i=0;i<6;i++)
	{
		Lfo* lfo = &voiceSlots[i].u.c.lfo;
		if(lfo->retrigger == voice+1)
		{
			lfo->phase = lfo->phaseOffset;
			if(lfo->waveform < LFO_SINE_1)
			{
				lfo->starts++;	//MOD: restarted
				lfo->startCause = voice;
			}
		}
		//MOD: the cycle LFO steps on each trigger of its retrigger voice, without
		//retrigger on each trigger of its own voice
		if(lfo->waveform == LFO_CYCLE && (lfo->retrigger == voice+1 || (lfo->retrigger == 0 && i == voice)))
		{
			lfo->cycleCount++;
		}
		//MOD: one-shot waveforms start (after the delay = offset) on each trigger
		//of the retrigger voice, without retrigger of the LFO's own voice
		if(lfo->waveform >= LFO_SINE_1 && (lfo->retrigger == voice+1 || (lfo->retrigger == 0 && i == voice)))
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
}
//-------------------------------------------------------------
