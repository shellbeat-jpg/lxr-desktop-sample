/*
 * FxBus.c
 *
 *  Created on: 26.08.2020
 *      Author: julian
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 *
 * MOD (XT): two FX slots. Slot A (fxBus) has the kit's FX settings as
 * before, slot B (fxBusB) has the same effects with its settings in the global
 * settings. Voices are routed to slot A ("FX") or slot B ("FXB"). The order
 * (fxBus_mode): A>B and B>A chain the slots (the output of the first is added
 * to the input of the second; the chain goes to slot A's output), A+B runs them
 * side by side, each to its own output. With slot B off (and A>B) everything
 * is processed as before.
 */


#include "Soundengine/FxBus.h"
#include "Soundengine/DSPAudio/BufferTools.h"
#include "Soundengine/DSPAudio/mixer.h"
#include "Soundengine/Sequencer/sequencer.h"
#include "Preset/KitEngines.h"
#include <string.h>

INDTCMZ FxBus fxBus;
FxBus fxBusB;				//MOD: second slot
uint8_t fxBus_mode = FXBUS_MODE_A_B;
static uint16_t fxBus_syncBpm = 0;

//tempo sync of the delay: length in quarter notes (index 0 = off)
//(the grids of the tracks and 1/8.): off, 1/4, 1/8., 1/4T, 1/8, 1/16., 1/8T, 5/64,
//1/16, 1/32., 1/16T, 1/32, 1/32T
static const float fxBus_syncBeats[FXBUS_NUM_SYNC] =
{
	0.f, 1.f, 0.75f, 2.f/3.f, 0.5f, 0.375f, 1.f/3.f, 0.3125f, 0.25f, 0.1875f, 1.f/6.f, 0.125f, 1.f/12.f
};

static void FxBus_initSlot(FxBus* b)
{
	b->dryWet = 0;
	b->audioRouting = 0;
	b->fxType = FX_OFF;
	b->sync = 0;
	b->delay.delayType = DELAY_NORMAL;
	Distortion_init(&b->distortion);
	Ringmod_init(&b->ringmod);
	Compressor_init(&b->compressor);
	Delay_init(&b->delay);
}

void FxBus_init()
{
	FxBus_initSlot(&fxBus);
	FxBus_initSlot(&fxBusB);
	fxBusB.delay.buf = FX_BUFFER_B;
	fxBus_mode = FXBUS_MODE_A_B;
#if USE_REVERB
	Reverb_init(&fxBus.reverb);
#endif
}
//------------------------------------------------------------------------------
//the output buffers of a routing
static uint8_t FxBus_outputs(const uint8_t routing, int32_t* outL,int32_t* outR,int32_t* outL2, int32_t* outR2, int32_t** out, int32_t** out2)
{
	*out2 = NULL;
	switch(routing)
	{
	default:
	case MIXER_ROUTING_DAC1_STEREO:	*out = outL2; *out2 = outR2;	break;
	case MIXER_ROUTING_DAC2_STEREO:	*out = outL; *out2 = outR;		break;
	case MIXER_ROUTING_DAC1_L:		*out = outL2;					break;
	case MIXER_ROUTING_DAC1_R:		*out = outR2;					break;
	case MIXER_ROUTING_DAC2_L:		*out = outL;					break;
	case MIXER_ROUTING_DAC2_R:		*out = outR;					break;
	}
	// < MIXER_ROUTING_DAC1_L -> stereo 1 or 2
	return routing < MIXER_ROUTING_DAC1_L ? 2 : 1;
}
//one slot: its input (interleaved stereo 16 bit) processed and added to out
static void FxBus_processSlot(FxBus* b, int16_t *fxBuffer, int32_t* out, int32_t* out2, uint32_t size, uint8_t channels)
{
	switch(b->fxType)
	{
	default:
	case FX_OFF:
	{
		if(channels == 2)
		{
			for(uint8_t i=0;i<size*2;i++)
			{
				out[i] += fxBuffer[i];
			}
		}
		else
		{
			//mono out: left channel of the FX bus
			for(uint8_t i=0;i<size;i++)
			{
				*out += fxBuffer[i*2];
				out +=2;
			}
		}
	}
	break;
	case FX_DISTORTION:
		Distortion_process(&b->distortion, fxBuffer, out, out2, size, channels, b->dryWet);
		break;
	case FX_RING:
		Ringmod_process(&b->ringmod, fxBuffer, out, out2, size, channels, b->dryWet);
		break;
	case FX_COMPRESSOR:
		Compressor_process(&b->compressor, fxBuffer, out, out2, size, channels, b->dryWet);
		break;
	case FX_DELAY:
		if(b->delay.delayType == DELAY_NORMAL)
		{
			Delay_process(&b->delay, fxBuffer, out, out2, size, channels, b->dryWet);
		}
		else
		{
			Delay_processPingPong(&b->delay, fxBuffer, out, out2, size, channels, b->dryWet);
		}
		break;
#if USE_REVERB
	case FX_REVERB:
		Reverb_process(&b->reverb, fxBuffer, out, out2, size, channels, b->dryWet);
		break;
#endif
	}
}
//first slot of a chain: processed in stereo and added to the input of the second
static void FxBus_processInto(FxBus* b, int16_t *in, int16_t *next, uint32_t size)
{
	int32_t tmp[OUTPUT_DMA_SIZE*2];
	memset(tmp, 0, sizeof(tmp));
	FxBus_processSlot(b, in, tmp, tmp+1, size, 2);
	for(uint32_t i=0;i<size*2;i++)
	{
		next[i] = (int16_t)__SSAT(next[i] + tmp[i], 16);
	}
}
//MOD: A>B+: slot A to its own output and into the input of slot B
static void FxBus_processIntoAndOut(FxBus* b, int16_t *in, int16_t *next, int32_t* out, uint32_t size, uint8_t channels)
{
	int32_t tmp[OUTPUT_DMA_SIZE*2];
	memset(tmp, 0, sizeof(tmp));
	FxBus_processSlot(b, in, tmp, tmp+1, size, 2);
	for(uint32_t i=0;i<size*2;i++)
	{
		next[i] = (int16_t)__SSAT(next[i] + tmp[i], 16);
	}
	if(channels == 2)
	{
		for(uint32_t i=0;i<size*2;i++)
			out[i] += tmp[i];
	}
	else
	{
		for(uint32_t i=0;i<size;i++)
			out[i*2] += tmp[i*2];	//mono out: left channel
	}
}
//------------------------------------------------------------------------------
void FxBus_setSync(FxBus* b, const uint8_t sync)
{
	b->sync = sync < FXBUS_NUM_SYNC ? sync : 0;
	const uint16_t bpm = seq_getBpm();
	b->delay.syncSamples = (b->sync && bpm) ? fxBus_syncBeats[b->sync] * 60.f / (float)bpm * REAL_FS : 0.f;
	Delay_setDelayTime(&b->delay, b->delay.delayTime);
}
static void FxBus_checkTempo(void)
{
	const uint16_t bpm = seq_getBpm();
	if(bpm == fxBus_syncBpm)
		return;
	fxBus_syncBpm = bpm;
	if(fxBus.sync)	FxBus_setSync(&fxBus, fxBus.sync);
	if(fxBusB.sync)	FxBus_setSync(&fxBusB, fxBusB.sync);
}
//------------------------------------------------------------------------------
//MOD: input gain of the slots (global PAR_FXA_GAIN / PAR_FXB_GAIN, SHIFT + knob 1 / 2)
float fxBus_inputGain[2] = { 1.f, 1.f };
static void FxBus_applyInputGain(int16_t* buf, const float g, const uint32_t size)
{
	if(g == 1.f) return;
	for(uint32_t i=0;i<size*2;i++)
		buf[i] = (int16_t)__SSAT((int32_t)(buf[i] * g), 16);
}
void FxBus_process(int16_t *fxBuffer, int16_t *fxBufferB, int32_t* outL,int32_t* outR,int32_t* outL2, int32_t* outR2, uint32_t size)
{
	FxBus_checkTempo();
	FxBus_applyInputGain(fxBuffer, fxBus_inputGain[0], size);	//MOD
	FxBus_applyInputGain(fxBufferB, fxBus_inputGain[1], size);
	int32_t *out, *out2;
	const uint8_t channels = FxBus_outputs(fxBus.audioRouting, outL, outR, outL2, outR2, &out, &out2);

	fxBus_mode = kitEngines[KIT_FXMODE_OFFSET];	//MOD: the order is kit data
	switch(fxBus_mode)
	{
	default:
	case FXBUS_MODE_A_B:
		if(fxBusB.fxType == FX_OFF)
		{
			//as before: slot A to its output, slot B's input passes through
			FxBus_processSlot(&fxBus, fxBuffer, out, out2, size, channels);
			FxBus_processSlot(&fxBusB, fxBufferB, out, out2, size, channels);
		}
		else
		{
			FxBus_processInto(&fxBus, fxBuffer, fxBufferB, size);
			FxBus_processSlot(&fxBusB, fxBufferB, out, out2, size, channels);
		}
		break;
	case FXBUS_MODE_B_A:
		FxBus_processInto(&fxBusB, fxBufferB, fxBuffer, size);
		FxBus_processSlot(&fxBus, fxBuffer, out, out2, size, channels);
		break;
	case FXBUS_MODE_A_BPLUS:
	{
		FxBus_processIntoAndOut(&fxBus, fxBuffer, fxBufferB, out, size, channels);
		int32_t *outB, *outB2;
		const uint8_t channelsB = FxBus_outputs(fxBusB.audioRouting, outL, outR, outL2, outR2, &outB, &outB2);
		FxBus_processSlot(&fxBusB, fxBufferB, outB, outB2, size, channelsB);
	}
		break;
	case FXBUS_MODE_PARALLEL:
	{
		FxBus_processSlot(&fxBus, fxBuffer, out, out2, size, channels);
		int32_t *outB, *outB2;
		const uint8_t channelsB = FxBus_outputs(fxBusB.audioRouting, outL, outR, outL2, outR2, &outB, &outB2);
		FxBus_processSlot(&fxBusB, fxBufferB, outB, outB2, size, channelsB);
	}
		break;
	}
}

void FxBus_prepareFxChange()
{
	//memset(FX_BUFFER, 0, FX_BUFFER_SIZE*sizeof(__fp16) );
//	Ringmod_resetPhase(&fxBus.ringmod);
}

void FxBus_updateSlot(FxBus* b)
{
	switch(b->fxType)
	{
	case FX_OFF:
		break;

	case FX_DISTORTION:
		Distortion_setDrive(&b->distortion, b->parameter[0] );
		Distortion_setParam1(&b->distortion, b->parameter[1] );
		Distortion_setParam2(&b->distortion, b->parameter[2] );
		Distortion_setParam3(&b->distortion, b->parameter[3] );
		break;

	case FX_RING:
		Ringmod_setFreq(&b->ringmod, b->parameter[0] );
		//Ringmod_setWave(&b->ringmod, b->parameter[1] );
		break;

	case FX_COMPRESSOR:

		Compressor_setAttack(&b->compressor , b->parameter[0]);
		Compressor_setRelease(&b->compressor , b->parameter[1]);
		Compressor_setTreshold(&b->compressor , b->parameter[2]);
		Compressor_setGain(&b->compressor , b->parameter[3]);
		break;

	case FX_DELAY:
		Delay_setRange(&b->delay, b->parameter[1]);
		Delay_setDelayTime(&b->delay, b->parameter[0]);
		Delay_setFilter(&b->delay, b->parameter[2]);
		Delay_setFeedback(&b->delay, b->parameter[3]);

		break;
#if USE_REVERB
	case FX_REVERB:
		Reverb_setDelayTime(&b->reverb, b->parameter[0]);
		Reverb_setFeedback(&b->reverb, b->parameter[1]);
		Reverb_setComposition(&b->reverb, b->parameter[2]);

		Reverb_setVariation(&b->reverb, b->parameter[3]);
		break;
#endif
	default:
		break;
	}
}

void FxBus_updateParameter()
{
	FxBus_updateSlot(&fxBus);
}

void FxBus_setParameter(uint8_t idx, float value)
{
	fxBus.parameter[idx] = value;
	FxBus_updateParameter();
}
void FxBus_setParameterB(uint8_t idx, float value)
{
	fxBusB.parameter[idx] = value;
	FxBus_updateSlot(&fxBusB);
}
