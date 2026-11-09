/*
 * mixer.c
 *
 *  Created on: 11.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */

#include "Soundengine/DSPAudio/mixer.h"
#include "Soundengine/DSPAudio/DjFilter.h"
#include "Soundengine/Recorder.h"	//MOD
#include "Soundengine/config.h"
#include "Soundengine/AudioCodecManager.h"
#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/DSPAudio/HiHat.h"
#include "Soundengine/DSPAudio/VoiceSlot.h"
#include "Soundengine/DSPAudio/BufferTools.h"
#include "Soundengine/DSPAudio/squareRootLut.h"
#include "main.h"
#include "Soundengine/TriggerOut.h"
#include "menu/menu.h"
#include "Soundengine/FxBus.h"
#include "Preset/KitEngines.h"
#include "Soundengine/CpuMeter.h"
#include "Soundengine/DSPAudio/FX/MasterBus.h"	//MOD: FX3
#include "IO/adcPots.h"	//MOD: pre-fader sends

	uint8_t r2_Available;
	uint8_t l1_Available;
	uint8_t l2_Available;
	uint8_t r1_Available;


//-----------------------------------------------------------------------
INDTCMZ uint8_t mixer_audioRouting[6];
//-----------------------------------------------------------------------
#if USE_DECIMATOR
INDTCMZ float mixer_decimation_rate[7];		/**<sets the sample rate decimation. 0..1 = full rate*/
INDTCMZ float mixer_decimation_cnt[6];		/**<s'n'h counter for decimator*/
INDTCMZ int16_t mixer_voice_samples[6];		/**< stores the last outputted sample of the 6 voices*/
#endif
#if CPU_METER
//an extra cymbal voice, rendered to simulate kits with more FM voices
INDTCMZ static CymbalVoice mixer_stressVoice;
#endif
//-----------------------------------------------------------------------
void mixer_init()
{
#if USE_DECIMATOR
	int i;
	for(i=0;i<6;i++)
	{
		mixer_decimation_rate[i] 	= 1;
		mixer_decimation_cnt[i] 	= 0;
		mixer_voice_samples[i] 		= 0;
		mixer_audioRouting[i]		= 0;
	}
	mixer_decimation_rate[6] 		= 1;
#endif

	FxBus_init();
#if CPU_METER
	Cymbal_init(&mixer_stressVoice, 4);
#endif
}
//-----------------------------------------------------------------------
IN_ITCM_RAM void mixer_decimateBlock(const uint8_t voiceNr, int16_t* buffer)
{
	uint8_t i;
	//MOD: voices excluded from the global rate (OSX) only take their own
	float phaseInc = mixer_decimation_rate[voiceNr]*(((kitEngines[KIT_OSX_OFFSET] >> voiceNr) & 1u) ? 1.f : mixer_decimation_rate[6]);
	for(i=0;i<OUTPUT_DMA_SIZE;i++)
	{
		mixer_decimation_cnt[voiceNr] += phaseInc;
		if(mixer_decimation_cnt[voiceNr] >= 1.f)
		{
			mixer_decimation_cnt[voiceNr] -= 1.f;
			mixer_voice_samples[voiceNr] = buffer[i];

		}
		buffer[i] = mixer_voice_samples[voiceNr];
	}
}
//#pragma GCC push_options
//#pragma GCC optimize ("O0")
//-----------------------------------------------------------------------
/*IN_ITCM_RAM*/ uint8_t mixer_checkOutJackAvailable(uint8_t dest)
{
	//read input pins
	//TODO move pin reading in low priority idle loop
	/*
	 #define switch_l1_Pin GPIO_PIN_6
#define switch_l1_GPIO_Port GPIOD
#define switch_r1_Pin GPIO_PIN_7
#define switch_r1_GPIO_Port GPIOD
#define switch_l2_Pin GPIO_PIN_3
#define switch_l2_GPIO_Port GPIOB
#define switch_r2_Pin GPIO_PIN_4
#define switch_r2_GPIO_Port GPIOB
	 */


	//return MIXER_ROUTING_DAC2_STEREO;

	switch(dest)
	{
		case MIXER_ROUTING_FX_BUS:
			return MIXER_ROUTING_FX_BUS;
			break;

		case MIXER_ROUTING_DAC1_STEREO:
			if(r1_Available && l1_Available) {
				return MIXER_ROUTING_DAC1_STEREO;
			} else
			if (r2_Available && l2_Available) {
				return MIXER_ROUTING_DAC2_STEREO;
			}
			else if(r1_Available) {
				return MIXER_ROUTING_DAC1_R;
			}else if(l1_Available) {
				return MIXER_ROUTING_DAC1_L;
			} else
			if (r2_Available) {
				return MIXER_ROUTING_DAC2_R;
			}else
			if (l2_Available) {
				return MIXER_ROUTING_DAC2_L;
			}
			break;

		case MIXER_ROUTING_DAC2_STEREO:

			if (r2_Available && l2_Available) {
				return MIXER_ROUTING_DAC2_STEREO;
			} else
			if(r1_Available && l1_Available) {
				return MIXER_ROUTING_DAC1_STEREO;
			}
			else
			if (r2_Available) {
				return MIXER_ROUTING_DAC2_R;
			}else
			if (l2_Available) {
				return MIXER_ROUTING_DAC2_L;
			}else if(r1_Available) {
				return MIXER_ROUTING_DAC1_R;
			}else if(l1_Available) {
				return MIXER_ROUTING_DAC1_L;
			}
			break;

		case MIXER_ROUTING_DAC1_L:
			if(l1_Available) {
				return dest;
			} else if (r1_Available) {
				return MIXER_ROUTING_DAC1_R;
			} else if (l2_Available) {
				return MIXER_ROUTING_DAC2_L;
			} else if (r2_Available) {
				return MIXER_ROUTING_DAC2_R;
			}
			break;

		case MIXER_ROUTING_DAC1_R:
			if(r1_Available) {
				return dest;
			} else if (l1_Available) {
				return MIXER_ROUTING_DAC1_L;
			} else if (l2_Available) {
				return MIXER_ROUTING_DAC2_L;
			} else if (r2_Available) {
				return MIXER_ROUTING_DAC2_R;
			}
			break;

		case MIXER_ROUTING_DAC2_L:
			if(l2_Available) {
				return dest;
			} else if (r2_Available) {
				return MIXER_ROUTING_DAC2_R;
			} else if (l1_Available) {
				return MIXER_ROUTING_DAC1_L;
			} else if (r1_Available) {
				return MIXER_ROUTING_DAC1_R;
			}
			break;

		case MIXER_ROUTING_DAC2_R:
			if(r2_Available) {
				return dest;
			} else if (l2_Available) {
				return MIXER_ROUTING_DAC2_L;
			} else if (r1_Available) {
				return MIXER_ROUTING_DAC1_R;
			} else if (l1_Available) {
				return MIXER_ROUTING_DAC1_L;
			}
			break;
	}
	return dest;
}

//-----------------------------------------------------------------------
#if 0
static inline void mixer_moveDataToOutput(uint8_t dest, const float panL, const float panR, int16_t* data,int16_t* outL,int16_t* outR,int16_t* outL2, int16_t* outR2)
{
	//check if a cable is in the selected out
	dest = mixer_checkOutJackAvailable(dest);

	uint8_t i;
	switch(dest)
	{

	case MIXER_ROUTING_DAC1_STEREO:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL2 = data[i] * panL;
			outL2 += 2;

			*outR2 = data[i] * panR;
			outR2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_STEREO:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL = data[i] * panL;
			outL += 2;

			*outR = data[i] * panR;
			outR += 2;
		}
		break;
	case MIXER_ROUTING_DAC1_L:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL2 = data[i];
			outL2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC1_R:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outR2 = data[i];
			outR2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_L:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL = data[i];
			outL += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_R:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outR = data[i];
			outR += 2;
		}
		break;
	}
}
#endif
//-----------------------------------------------------------------------
#if 0
static float fastTanh(float var)
{
   if(var < -1.95f)     return -1.0f;
   else if(var > 1.95f) return  1.0f;
   else					return  4.15f*var/(4.29f+var*var);
}




double ecc83(float x, float v)
{
	/*
	double input = x -0.05*v*x +0.01*v;//(h100f(x -0.05*v*x +0.01*v))*(1.0+0.55*v) +0.55*v;
	input += 1.0; // [0:2]
	input *= 64.0;
	uint8_t itg = input;
	float frac = input-itg;
	double val;
	if(itg<127)
		val = (1.0-frac) * ecc83Table[itg] + frac * ecc83Table[itg+1];
	else val = ecc83Table[127];
	val = val *(1.0+0.55*v) +0.55*v;
	//if(val>0.999999)val =0.999999;
	return val;
	*/
}

void mixer_saturateOutputs(float gain,  int16_t* outL,int16_t* outR,int16_t* outL2, int16_t* outR2)
{
    gain = 1+gain*2;
    //float v = gain
	for(uint32_t i=0;i<OUTPUT_DMA_SIZE;i++)
	{
		/*
		*outR = __SSAT((int32_t)(ecc83(*outR/32768.0f, gain) * 32768.0f), 16); ;
		outR += 2;

		*outL =  __SSAT((int32_t)(ecc83(*outL/32768.0f, gain) * 32768.0f), 16); ;
		outL += 2;
		*/

		*outR2 =  __SSAT((int32_t)(ecc83(*outR2/32768.0f*gain, 1.) * 32768.0f), 16); ;
		outR2 += 2;

		*outL2 =  __SSAT((int32_t)(ecc83(*outL2/32768.0f*gain, 1.) * 32768.0f), 16); ;
		outL2 += 2;
	}
}
#endif
//-----------------------------------------------------------------------
IN_ITCM_RAM static inline void mixer_addDataToOutputFilteredPan(uint8_t dest, const float pan,  int16_t* data,int32_t* outL,int32_t* outR,int32_t* outL2, int32_t* outR2, int16_t *fxBuffer, OnePoleFilter* filter)
{


	//check if a cable is in the selected out
	dest = mixer_checkOutJackAvailable(dest);
	//assert_param(dest <= MIXER_ROUTING_DAC2_R);

	//TODO may be possible to optimize here using both halfwords of add for stereo mixing
	uint8_t i;
	switch(dest)
	{

	case MIXER_ROUTING_FX_BUS:
#if 0
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			fxBuffer[i] = __QADD16(fxBuffer[i],data[i]) & 0xFFFF;
		}
#else
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{

			//----
			float mixPan 	= calcOnePole(filter, pan);
			float panL = sqrtf((127-mixPan)/127.f);
			float panR = sqrtf(mixPan/127.f);
			//----
			int32_t y;
			int32_t x;
			y = *((int32_t*)fxBuffer);
			x = __PKHBT( (int16_t)(data[i] * panL), (int16_t)(data[i] * panR), 16);

			*((int32_t*)fxBuffer) = __QADD16(x, y);
			fxBuffer += 2;
		}
#endif
		break;

	case MIXER_ROUTING_DAC1_STEREO:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			//----
			float mixPan 	= calcOnePole(filter, pan);
			float panL = sqrtf((127-mixPan)/127.f);
			float panR = sqrtf(mixPan/127.f);
			//----
			outL2[0] += (int16_t)(data[i] * panL);
			outL2[1] += (int16_t)(data[i] * panR);
			outL2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_STEREO:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			//----
			float mixPan 	= calcOnePole(filter, pan);
			float panL = sqrtf((127-mixPan)/127.f);
			float panR = sqrtf(mixPan/127.f);
			//----
			outL[0] += (int16_t)(data[i] * panL);
			outL[1] += (int16_t)(data[i] * panR);
			outL += 2;
		}
		break;
	case MIXER_ROUTING_DAC1_L:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{

			*outL2 += data[i];
			outL2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC1_R:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outR2 += data[i];
			outR2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_L:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL += data[i];
			outL += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_R:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outR += data[i];
			outR += 2;
		}
		break;

	}
}
//-----------------------------------------------------------------------
IN_ITCM_RAM static inline void mixer_addDataToOutput(uint8_t dest, const float panL, const float panR,  int16_t* data,int32_t* outL,int32_t* outR,int32_t* outL2, int32_t* outR2, int16_t *fxBuffer)
{
	//check if a cable is in the selected out
	dest = mixer_checkOutJackAvailable(dest);
	//assert_param(dest <= MIXER_ROUTING_DAC2_R);

	//TODO may be possible to optimize here using both halfwords of add for stereo mixing
	uint8_t i;
	switch(dest)
	{

	case MIXER_ROUTING_FX_BUS:
#if 0
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			fxBuffer[i] = __QADD16(fxBuffer[i],data[i]) & 0xFFFF;
		}
#else
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			int32_t y;
			int32_t x;
			y = *((int32_t*)fxBuffer);
			x = __PKHBT( (int16_t)(data[i] * panL), (int16_t)(data[i] * panR), 16);

			*((int32_t*)fxBuffer) = __QADD16(x, y);
			fxBuffer += 2;
		}
#endif
		break;

	case MIXER_ROUTING_DAC1_STEREO:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			outL2[0] += (int16_t)(data[i] * panL);
			outL2[1] += (int16_t)(data[i] * panR);
			outL2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_STEREO:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			outL[0] += (int16_t)(data[i] * panL);
			outL[1] += (int16_t)(data[i] * panR);
			outL += 2;
		}
		break;
	case MIXER_ROUTING_DAC1_L:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL2 += data[i];
			outL2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC1_R:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outR2 += data[i];
			outR2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_L:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL += data[i];
			outL += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_R:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outR += data[i];
			outR += 2;
		}
		break;

	}
}
//#pragma GCC pop_options
//-----------------------------------------------------------------------
/* Output limiter, one per DAC (L and R linked, to keep the stereo image).
 * Up to full scale the mix passes unchanged, so anything that did not clip
 * before sounds exactly as before. When the sum of the voices and the FX
 * return gets louder, the gain drops at once so that no sample exceeds full
 * scale (no clipping), and then recovers smoothly. */
#define MIXER_LIMITER_THRESHOLD	32768.f
//gain recovery per sample: 1-exp(-1/(0.1s * REAL_FS)), i.e. ~100 ms time constant
#define MIXER_LIMITER_RELEASE	0.000227f

INDTCM float mixer_limiterGain[2] = {1.f, 1.f};

//1 = the limiter above, 0 = the stock mix (config menu "lim")
INDTCM uint8_t mixer_limiterOn = 1;
INDTCM uint8_t mixer_preFader = 0;		//MOD: global PAR_FX_PREFADER
static float mixer_faderGain[6] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };	//MOD: pre-fader: the fader, smoothed per block
//MOD: cue on St2 (mixer.h)
uint8_t mixer_cueTracks = 0;
uint8_t mixer_cueSlotMask = 0;
float mixer_cueMix = 0.f;
float mixer_cueVol = 1.f;
static int32_t mixer_cueBus[OUTPUT_DMA_SIZE*2];
//start of a block: is the cue on (a track cued, St1 has its own cable), its bus cleared
__attribute__((noinline)) static uint8_t mixer_cueBegin(void)
{
	uint8_t m = (uint8_t)(mixer_cueTracks & 0x3Fu);
	if(mixer_cueTracks & 0x40u) m |= 0x20u;		//track 7: slot 6
	if(!(l1_Available && r1_Available)) m = 0;	//St1 is on the jacks of St2: no cue
	mixer_cueSlotMask = m;
	if(m) memset(mixer_cueBus, 0, sizeof(mixer_cueBus));
	return m;
}
//a cued voice before its fader, with its pan
__attribute__((noinline)) static void mixer_cueVoice(const int16_t* data, const float panL, const float panR)
{
	for(uint32_t i = 0; i < OUTPUT_DMA_SIZE; i++)
	{
		mixer_cueBus[2*i]   += (int32_t)(data[i] * panL);
		mixer_cueBus[2*i+1] += (int32_t)(data[i] * panR);
	}
}
//what went to St2 (voices, FX returns) plays on St1 meanwhile
__attribute__((noinline)) static void mixer_cueFold(int32_t* st2, int32_t* st1)
{
	for(uint32_t i = 0; i < OUTPUT_DMA_SIZE*2; i++)
	{
		st1[i] += st2[i];
		st2[i] = 0;
	}
}
//St2 = level * (cue .. main)
__attribute__((noinline)) static void mixer_cueOut(int32_t* st2, const int32_t* st1)
{
	const float m = mixer_cueMix, g = mixer_cueVol;
	const float gc = g * (1.f - m), gm = g * m;
	for(uint32_t i = 0; i < OUTPUT_DMA_SIZE*2; i++)
	{
		const int32_t s = (int32_t)(gc * (float)mixer_cueBus[i] + gm * (float)st1[i]);
		st2[i] = mixer_limiterOn ? s : __SSAT(s, 16);
	}
}
//MOD: the send levels of the last block, per FX slot and voice (ramps)
static float mixer_sendGain[2][NUM_VOICE_SLOTS];
//MOD: a send whose level changed (knob, track 6/7 switch): a linear ramp over the
//block against clicks. Rare, so in flash.
__attribute__((noinline)) static void mixer_addSendRamp(const float g0, const float g1, const float panL, const float panR,
		const int16_t* data, int32_t* mix, int32_t* mix2, int16_t* fx)
{
	static int16_t ramp[OUTPUT_DMA_SIZE];
	const float step = (g1 - g0) * (1.f / OUTPUT_DMA_SIZE);
	float g = g0;
	for(uint32_t i = 0; i < OUTPUT_DMA_SIZE; i++)
	{
		g += step;
		ramp[i] = (int16_t)(data[i] * g);
	}
	mixer_addDataToOutput(MIXER_ROUTING_FX_BUS, panL, panR, ramp, &mix[0], &mix[1], &mix2[0], &mix2[1], fx);
}
IN_ITCM_RAM static inline void mixer_addSend(float* last, const float s, const float panL, const float panR, int16_t* data,
		int32_t* mix, int32_t* mix2, int16_t* fx)
{
	if(s != *last)
	{
		mixer_addSendRamp(*last, s, panL, panR, data, mix, mix2, fx);
		*last = s;
	}
	else if(s > 0.f)
		mixer_addDataToOutput(MIXER_ROUTING_FX_BUS, panL*s, panR*s, data, &mix[0], &mix[1], &mix2[0], &mix2[1], fx);
}
//MOD: the voice's sends into FX slot A and B (kit data), besides its output.
//Slot 6 takes those of track 7 while it plays a trigger of track 7 (if set).
IN_ITCM_RAM static void mixer_addSends(const uint8_t v, const float panL, const float panR, int16_t* data,
		int32_t* mix, int32_t* mix2, int16_t* fxA, int16_t* fxB)
{
	uint8_t a = kitEngines[KIT_SEND_OFFSET + v];
	uint8_t b = kitEngines[KIT_SENDB_OFFSET + v];
	if(v == NUM_VOICE_SLOTS-1 && slot_t7Playing)
	{
		const uint8_t* t = &kitEngines[KIT_T7_OFFSET];
		if(t[KIT_T7_SENDA]) a = (uint8_t)(t[KIT_T7_SENDA] - 1u);
		if(t[KIT_T7_SENDB]) b = (uint8_t)(t[KIT_T7_SENDB] - 1u);
	}
	mixer_addSend(&mixer_sendGain[0][v], a * (1.f/127.f), panL, panR, data, mix, mix2, fxA);
	mixer_addSend(&mixer_sendGain[1][v], b * (1.f/127.f), panL, panR, data, mix, mix2, fxB);
}
//MOD: pre-fader: the fader of voice v on its output (the engines leave it out), a
//linear ramp per block against zipper noise
IN_ITCM_RAM static void mixer_applyFader(const uint8_t v, int16_t* data)
{
	const float target = adc_getFaderVolumeRaw(v);
	float g = mixer_faderGain[v];
	const float step = (target - g) * (1.f / OUTPUT_DMA_SIZE);
	for(uint32_t i = 0; i < OUTPUT_DMA_SIZE; i++)
	{
		g += step;
		data[i] = (int16_t)(data[i] * g);
	}
	mixer_faderGain[v] = target;
}

//the stock mix added every voice and the FX return with 16-bit saturation;
//saturating the 32-bit sums after each of them gives exactly the same result
IN_ITCM_RAM static void mixer_saturateMix(int32_t* mix)
{
	for(uint32_t i=0;i<OUTPUT_DMA_SIZE*2;i++)
	{
		mix[i] = __SSAT(mix[i], 16);
	}
}

IN_ITCM_RAM static void mixer_limitBlock(float* gainState, const int32_t* in, int16_t* out)
{
	float gain = *gainState;
	for(uint32_t i=0;i<OUTPUT_DMA_SIZE*2;i+=2)
	{
		const int32_t l = in[i];
		const int32_t r = in[i+1];
		const int32_t absL = l < 0 ? -l : l;
		const int32_t absR = r < 0 ? -r : r;
		const int32_t peak = absL > absR ? absL : absR;

		//instant attack: scale this sample down to full scale
		if(peak * gain > MIXER_LIMITER_THRESHOLD)
		{
			gain = MIXER_LIMITER_THRESHOLD / peak;
		}

		if(gain < 1.f)
		{
			out[i]   = __SSAT((int32_t)(l * gain), 16);
			out[i+1] = __SSAT((int32_t)(r * gain), 16);

			gain += (1.f - gain) * MIXER_LIMITER_RELEASE;
			//snap to exactly 1 (a step of < 0.01 dB): in float the release stalls
			//at ~0.99987 and would never get back to a bit-exact pass-through
			if(gain > 0.999f) gain = 1.f;
		}
		else
		{
			out[i]   = __SSAT(l, 16);
			out[i+1] = __SSAT(r, 16);
		}
	}
	*gainState = gain;
}
//-----------------------------------------------------------------------
//engine functions of the voice in a slot
IN_ITCM_RAM static inline void mixer_calcVoiceAsync(VoiceSlot* s)
{
	switch(s->engine)
	{
	case ENGINE_SNARE:	Snare_calcAsync(&s->u.snare);		break;
	case ENGINE_CYMBAL:	Cymbal_calcAsync(&s->u.cymbal);	break;
	case ENGINE_HIHAT:	HiHat_calcAsync(&s->u.hat);		break;
	case ENGINE_MODAL:	Modal_calcAsync(&s->u.modal);		break;
	case ENGINE_FM2:	Fm2_calcAsync(&s->u.fm2);			break;
	case ENGINE_WAVE:	Wave_calcAsync(&s->u.wave);			break;
	case ENGINE_VOX:	Vox_calcAsync(&s->u.vox);			break;
	default:			calcDrumVoiceAsync(&s->u.drum);	break;
	}
}
IN_ITCM_RAM static inline void mixer_calcVoiceSyncBlock(VoiceSlot* s, int16_t* buf)
{
	switch(s->engine)
	{
	case ENGINE_SNARE:	Snare_calcSyncBlock(&s->u.snare, buf, OUTPUT_DMA_SIZE);		break;
	case ENGINE_CYMBAL:	Cymbal_calcSyncBlock(&s->u.cymbal, buf, OUTPUT_DMA_SIZE);	break;
	case ENGINE_HIHAT:	HiHat_calcSyncBlock(&s->u.hat, buf, OUTPUT_DMA_SIZE);		break;
	case ENGINE_MODAL:	Modal_calcSyncBlock(&s->u.modal, buf, OUTPUT_DMA_SIZE);		break;
	case ENGINE_FM2:	Fm2_calcSyncBlock(&s->u.fm2, buf, OUTPUT_DMA_SIZE);			break;
	case ENGINE_WAVE:	Wave_calcSyncBlock(&s->u.wave, buf, OUTPUT_DMA_SIZE);			break;
	case ENGINE_VOX:	Vox_calcSyncBlock(&s->u.vox, buf, OUTPUT_DMA_SIZE);			break;
	default:			calcDrumVoiceSyncBlock(&s->u.drum, buf, OUTPUT_DMA_SIZE);		break;
	}
}
//-----------------------------------------------------------------------
IN_ITCM_RAM void mixer_calcNextSampleBlock(int16_t* output,int16_t* output2)
{

	r2_Available = (switch_r2_GPIO_Port->IDR & switch_r2_Pin)>0;
	l1_Available = (switch_l1_GPIO_Port->IDR & switch_l1_Pin)>0;
	l2_Available = (switch_l2_GPIO_Port->IDR & switch_l2_Pin)>0;
	r1_Available = (switch_r1_GPIO_Port->IDR & switch_r1_Pin)>0;

	//MOD: calc the LFOs before the targets are reset: an LFO's modulation of another
	//LFO (rate, amount ...) of the last block then applies to every LFO. Before, the
	//reset undid it for LFOs with a lower number (LFO 3 -> rate of LFO 1 had no effect).
	float lfoVal[6];
	uint8_t v;
	for(v=0;v<6;v++)
	{
		lfoVal[v] = lfo_calc(&voiceSlots[v].u.c.lfo);
	}
	lfo_extraCalc();	//MOD: 2nd LFOs, track 7 LFO

	modNode_resetTargets();
	//re assign velocity modulation
	modNode_reassignVeloMod();

	//dispatch LFO
	const uint8_t t7Lfo = lfo_t7Active();	//MOD: instead of the LFO of voice 6
	for(v=0;v<6;v++)
	{
		if(v == 5 && t7Lfo)
			continue;
		modNode_updateValue(&voiceSlots[v].u.c.lfo.modTarget, lfoVal[v]);
	}
	lfo_extraDispatch();	//MOD

	//update filter frequencies
	for(v=0;v<6;v++)
	{
		SVF_recalcFreq(&voiceSlots[v].u.c.filter);
	}

	//--- Calc async -----
	for(v=0;v<6;v++)
	{
		mixer_calcVoiceAsync(&voiceSlots[v]);
	}

	//calculate trigger io phase
	//trigger_tickPhaseCounter();

	//an array to store intermediate voice samples
	//befor output distribution
	int16_t sampleData[OUTPUT_DMA_SIZE];
	int16_t fxBusData[OUTPUT_DMA_SIZE*2];
	int16_t fxBusDataB[OUTPUT_DMA_SIZE*2];	//MOD: input of the second FX slot

	const uint8_t pos = 0;
#if CPU_METER
	uint32_t meterStart;
#endif

	//32-bit mix buffers, interleaved L/R like the DMA buffers (see mixer.h)
	int32_t mix[OUTPUT_DMA_SIZE*2];
	int32_t mix2[OUTPUT_DMA_SIZE*2];

	//Clear is necessary, otherwise we keep trash data on non routed outputs
	memset(mix, 0, sizeof(mix));
	memset(mix2, 0, sizeof(mix2));

	bufferTool_clearBuffer(fxBusData,OUTPUT_DMA_SIZE*2);
	bufferTool_clearBuffer(fxBusDataB,OUTPUT_DMA_SIZE*2);

	const uint8_t cueOn = mixer_cueTracks ? mixer_cueBegin() : (mixer_cueSlotMask = 0);	//MOD: cue

	//calc the voices, decimate them and add them to the selected outputs
	for(v=0;v<6;v++)
	{
		VoiceCommon* const voice = &voiceSlots[v].u.c;
#if CPU_METER
		meterStart = DWT->CYCCNT;
#endif
		mixer_calcVoiceSyncBlock(&voiceSlots[v], sampleData);
		slot_applyChoke(v, sampleData, OUTPUT_DMA_SIZE);	//MOD: choke group
#if CPU_METER
		cpuMeter_section(CPU_SECTION_VOICE1+v, meterStart);
#endif
		//decimate voice
		mixer_decimateBlock(v,sampleData);
		//copy to selected dma buffer
#if USE_SQRT_LUT
		const float panL = squareRootLut[127-voice->pan];
		const float panR = squareRootLut[voice->pan];
#elif USE_TRIGGERED_PAN
		const float panL = voice->triggeredPanL;
		const float panR = voice->triggeredPanR;
#else
		const float panL = sqrtf((127-voice->pan)/127.f);
		const float panR = sqrtf(voice->pan/127.f);
#endif
		uint8_t route = mixer_audioRouting[v];
		if(route >= MIXER_ROUTING_SEND_A && route < MIXER_ROUTING_SEND_A + 12u)
			route = (uint8_t)((route - MIXER_ROUTING_SEND_A) % 6u);	//MOD: "1+A" ... of the 1.2.8 tests: the output
		//MOD: pre-fader: the sends get the voice before its fader, the output after it
		//(also OUT = FX / FXB: the fader keeps working there). Post-fader (default):
		//the engines applied the fader already.
		const uint8_t fadeSlot = voiceSlots[v].u.c.slot;
		//MOD: cue: a cued voice before its fader; a muted, cued track only there
		const uint8_t cued = (mixer_cueSlotMask >> v) & 1u;
		const uint8_t cueOnly = slot_cueOnly[v];
		if(mixer_preFader || cued)
		{
			if(mixer_preFader && !cueOnly)
				mixer_addSends(v, panL, panR, sampleData, &mix[pos], &mix2[pos], fxBusData, fxBusDataB);
			if(cued)
				mixer_cueVoice(sampleData, panL, panR);
			mixer_applyFader(fadeSlot, sampleData);
		}
		else
			mixer_faderGain[fadeSlot] = adc_getFaderVolumeRaw(fadeSlot);
		if(cueOnly)
			continue;
		{
			const uint8_t isFxB = route == MIXER_ROUTING_FX_BUS_B;	//MOD
			mixer_addDataToOutput(isFxB ? MIXER_ROUTING_FX_BUS : route, panL, panR, sampleData,&mix[pos],&mix[pos+1],&mix2[pos],&mix2[pos+1], isFxB ? fxBusDataB : fxBusData);
		}
		if(!mixer_preFader)
			mixer_addSends(v, panL, panR, sampleData, &mix[pos], &mix2[pos], fxBusData, fxBusDataB);
		if(!mixer_limiterOn)
		{
			mixer_saturateMix(mix);
			mixer_saturateMix(mix2);
		}
	}

#if CPU_METER
	//simulate kits with more FM voices: render an extra cymbal voice (into the
	//no longer needed sampleData)
	for(uint8_t i=0;i<cpuMeter.stressFm;i++)
	{
		Cymbal_calcSyncBlock(&mixer_stressVoice, sampleData,OUTPUT_DMA_SIZE);
	}
#endif
#if CPU_METER
	meterStart = DWT->CYCCNT;
#endif
	FxBus_process(fxBusData, fxBusDataB, &mix[pos],&mix[pos+1],&mix2[pos],&mix2[pos+1], OUTPUT_DMA_SIZE);
	if(cueOn)
		mixer_cueFold(mix, mix2);	//MOD: cue: St2 (mix) onto St1 (mix2)
	if(!mixer_limiterOn)
	{
		mixer_saturateMix(mix);
		mixer_saturateMix(mix2);
		//the sums are within 16 bit now: the limiter passes them unchanged
		mixer_limiterGain[0] = mixer_limiterGain[1] = 1.f;
	}
#if CPU_METER
	cpuMeter_section(CPU_SECTION_FX, meterStart);
#endif

	//MOD: FX3, the master bus of St1 (kit data); off: unchanged. Note the stock
	//naming: DAC1 (St1) is summed in mix2, DAC2 (St2) in mix (mixer_addDataToOutput).
	//Without a cable in St1 the mix routes St1 to the jacks of St2
	//(mixer_checkOutJackAvailable): FX3 then follows it there.
	{
		const uint8_t st1 = mixer_checkOutJackAvailable(MIXER_ROUTING_DAC1_STEREO);
		const uint8_t onDac2 = st1 == MIXER_ROUTING_DAC2_STEREO || st1 == MIXER_ROUTING_DAC2_L || st1 == MIXER_ROUTING_DAC2_R;
		int32_t* const bus = onDac2 ? mix : mix2;
		//diagnostics for the FX3 edit view: peak of both sums, decaying
		static uint16_t peakAge = 0;
		if(++peakAge > 100) { peakAge = 0; masterBus_peak[0] = masterBus_peak[1] = 0; }
		for(uint32_t i = 0; i < OUTPUT_DMA_SIZE*2; i++)
		{
			const int32_t a = mix2[i] < 0 ? -mix2[i] : mix2[i];	//St1
			const int32_t b = mix[i] < 0 ? -mix[i] : mix[i];		//St2
			if(a > masterBus_peak[0]) masterBus_peak[0] = a;
			if(b > masterBus_peak[1]) masterBus_peak[1] = b;
		}
		masterBus_onDac2 = onDac2;
#if CPU_METER
		meterStart = DWT->CYCCNT;
#endif
		masterBus_process(bus, OUTPUT_DMA_SIZE, &kitEngines[KIT_FX3_OFFSET]);
#if CPU_METER
		cpuMeter_section(CPU_SECTION_FX3, meterStart);
#endif
		if(!mixer_limiterOn)
			mixer_saturateMix(bus);
	}

	//global DJ filter on the final mix of all outputs (performance page); off: unchanged
	if(djFilter_process(mix, mix2) && !mixer_limiterOn)
	{
		//its resonance can exceed full scale: clip like the stock mix
		mixer_saturateMix(mix);
		mixer_saturateMix(mix2);
	}

	if(cueOn)
		mixer_cueOut(mix, mix2);	//MOD: St2 = the cue

	//bring the sums back into the 16-bit range of the DACs
	mixer_limitBlock(&mixer_limiterGain[0], mix, output);
	mixer_limitBlock(&mixer_limiterGain[1], mix2, output2);
	if(rec_state == REC_RUNNING)	//MOD: WAV recorder: St1 (on the jacks of St2 without a cable in St1)
		rec_push((l1_Available && r1_Available) ? output2 : output);
/*
	if(activeParameters[UNUSED_PAR_GLO_SAT])
	{
		mixer_saturateOutputs(activeParameters[UNUSED_PAR_GLO_SAT]/127.f, &mix[pos],&mix[pos+1],&mix2[pos],&mix2[pos+1]);
	}
	*/


}
