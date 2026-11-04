/*
 * Oscillator.c
 *
 *  Created on: 02.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */


#include "Soundengine/DSPAudio/Oscillator.h"
#include "Soundengine/DSPAudio/random.h"
#include "Soundengine/DSPAudio/Samples.h"
#include "Soundengine/MIDI/MidiParser.h"
#include "Soundengine/MIDI/MidiNoteNumbers.h"
#include "Soundengine/Sequencer/sequencer.h"
#include "SampleRom/sampleFlash.h"
#include <string.h>

/* MOD: user samples from flash. A sample plays at its original pitch when the
 * oscillator is tuned to MIDI note SMP_ROOT_NOTE (coarse 60, fine centre). */
#define SMP_ROOT_NOTE   60
#define SMP_FRAC_BITS   17
#define SMP_FRAC_MASK   ((1u << SMP_FRAC_BITS) - 1u)
#define SMP_PENDING     0xFFFFFFFFu   /* phase after a trigger: start not yet applied */
#define SMP_MIN_LEN     64u           /* frames, shortest length */
#define SMP_FADE_IN     32u           /* frames, only if start > 0 */
#define SMP_FADE_OUT    64u           /* frames, only if the length cuts the sample */
#define SMP_MIN_LOOP    16u           /* frames, shorter regions play once */
#define SMP_LOOP_XF     256u          /* frames, max. crossfade at the loop point */
#define SMP_LOOP_DIP    32u           /* frames, fade out/in at the loop point if start = 0 */
#define SMP_DECLICK     48u           /* output frames, a retrigger fades the cut-off sound out */

static __inline uint32_t freq2PhaseIncr(const float f) //4096
{
	return (((TABLESIZE*f)/REAL_FS))*1048576 ; // 1048576 <-> (<<20)
}
//TODO die phaseInc berechnung kann man doch sicher per LUT machen!
//-----------------------------------------------------------
static __inline uint8_t fast_log2 (const uint32_t val)
{
	return 31-__CLZ(val);
}
//-----------------------------------------------------------
static __inline int freqToMidiNote(const float f)
{
	return (12*fast_log2((uint32_t)(f/440.f)))+70;
}
//-----------------------------------------------------------
static __inline uint8_t freqToTableIndex(const float f)
{
	return freqToMidiNote(f)/12;
}
//-----------------------------------------------------------

//-----------------------------------------------------------
static __inline uint32_t freq2PhaseIncr1024(const float f)
{
	return (((1024*f)/REAL_FS))*4194304 ; //(<<22)
}
//-----------------------------------------------------------
static __inline uint32_t freq2PhaseIncr32767(const float f)
{
	return (((1024*f)/REAL_FS))*131072 ; //(<<17)
}
//-----------------------------------------------------------
IN_ITCM_RAM void calcSineBlock(OscInfo* osc, int16_t* buf, const uint8_t size ,const float gain)
{
	//tablesize 4096 => 0b111111111111 -> 12 bits
	uint8_t i;
	for(i=0;i<size;i++)
	{
		const uint32_t oscPhase = osc->phase;
		int16_t oscOut;

		const uint32_t index =  oscPhase;
		uint32_t  itg	= index>>20; // 12 msb as itg index

		#if INTERPOLATE_OSC
			oscOut = sine_table[itg++];

			//const float frac = (index&0x7ffff)*0.0000019073486328125f;  // 1/0x7ffff = 0.0000019073486328125f
			//oscOut += frac*(sine_table[itg] - oscOut);
			//a + t * (b - a);
			uint32_t frac = index&0xFFFFF;
			oscOut = oscOut + (int16_t)(((int64_t)(sine_table[itg] - oscOut) * frac)   >>20);
		#else
			oscOut = sine_table[itg];
		#endif

		osc->phase = oscPhase + osc->phaseInc;

		buf[i] = oscOut * gain;
	}
}
//-----------------------------------------------------------
int16_t calcSine(OscInfo* osc)
{
	const uint32_t oscPhase = osc->phase;
	int16_t oscOut;

	const uint32_t index =  oscPhase;
	uint32_t  itg	= index>>20;

#if INTERPOLATE_OSC
	oscOut = sine_table[itg++];
	//float frac	= (index&0x7ffff)*0.0000019073486328125f;
	//oscOut += frac*(sine_table[itg] - oscOut);
	uint32_t frac = index&0xFFFFF;
	oscOut = oscOut + (int16_t)(((int64_t)(sine_table[itg] - oscOut) * frac)   >>20);
#else
	oscOut = sine_table[itg];
#endif

	osc->phase = oscPhase + osc->phaseInc;
	osc->output = oscOut;
	return oscOut;
}

//-----------------------------------------------------------
IN_ITCM_RAM void calcFmSineBlock(OscInfo* osc, int16_t* modBuffer, int16_t* buf, uint8_t size,const float gain)
{
	uint8_t i;
	for(i=0;i<size;i++)
	{
		const uint32_t oscPhase = osc->phase;
		int16_t oscOut ;

		const uint32_t index =  oscPhase + (((uint32_t)(modBuffer[i]*osc->fmMod))<<17);
		uint32_t  itg	= index>>20;

	#if INTERPOLATE_FM_OSC
		oscOut = sine_table[itg++];
		//const float frac	= (index&0x7ffff)*0.0000019073486328125f;
		//oscOut += frac*(sine_table[itg] - oscOut);
		uint32_t frac = index&0xFFFFF;
		oscOut = oscOut + (int16_t)(((int64_t)(sine_table[itg] - oscOut) * frac)   >>20);
	#else
		oscOut = sine_table[itg];
	#endif

		osc->phase = oscPhase + osc->phaseInc;
		buf[i] =  oscOut * gain;
	}
}
//-----------------------------------------------------------
int16_t calcFmSine(OscInfo* osc, OscInfo* modOsc)
{
	const uint32_t oscPhase 	= osc->phase;
	int16_t oscOut ;

	const uint32_t index =  oscPhase + (((uint32_t)(modOsc->output*osc->fmMod))<<17);
	uint32_t  itg	= index>>20;

#if INTERPOLATE_FM_OSC
	oscOut = sine_table[itg++];
	//const float frac	= (index&0x7ffff)*0.0000019073486328125f;
	//oscOut += frac*(sine_table[itg] - oscOut);
	uint32_t frac = index&0xFFFFF;
	oscOut = oscOut + (int16_t)(((int64_t)(sine_table[itg] - oscOut) * frac)   >>20);
#else
	oscOut = sine_table[itg];
#endif

	osc->phase = oscPhase + osc->phaseInc;
	osc->output = oscOut;
	return oscOut;
};
//---------------------------------------------------------------
IN_ITCM_RAM void calcFmBlock(OscInfo* osc, const int16_t table[][1024], int16_t* modBuffer, int16_t* buf, uint8_t size ,const float gain)
{
	uint8_t i;
	for(i=0;i<size;i++)
	{
		const uint32_t oscPhase = osc->phase;
		int16_t oscOut ;
		const uint32_t index =  oscPhase + (((uint32_t)(modBuffer[i]*osc->fmMod))<<19);

		uint32_t  itg	= index>>22; //10 bit index, 22 bit fractional part

	#if INTERPOLATE_FM_OSC
		oscOut = table[osc->tableOffset][itg++];
		//const float frac	= (index&0x003FFFFF)*2.38418579101562e-07f;
		//oscOut += frac*(table[osc->tableOffset][itg] - oscOut);
		uint32_t frac = index & 0x3FFFFF;
		oscOut = oscOut + (int16_t)(((int64_t)(table[osc->tableOffset][itg] - oscOut) * frac)   >>22);

	#else
		oscOut = table[overtoneIndex][itg];
	#endif

		osc->phase = oscPhase + osc->phaseInc;
		osc->output = oscOut;
		buf[i] = oscOut * gain;
	}
}
//-----------------------------------------------------------
void calcPwmFmBlock(OscInfo* osc, int16_t* modBuffer, int16_t* buf, uint8_t size ,const float gain, const float pwm)
{
	uint8_t i;
	for(i=0;i<size;i++)
	{
		const uint32_t oscPhase = osc->phase;
		int16_t oscOut ;
		const uint32_t index =  oscPhase + (((uint32_t)(modBuffer[i]*osc->fmMod))<<19);
		const uint32_t index2 = index + pwm * (UINT32_MAX/2);

		 uint32_t  itg	 = index>>22;
		 uint32_t  itg2 = index2>>22;

	#if INTERPOLATE_FM_OSC
		oscOut = sawTable[osc->tableOffset][itg++];
		//const float frac	= (index&0x003FFFFF)*2.38418579101562e-07f;
		//oscOut += frac*(sawTable[osc->tableOffset][itg] - oscOut);
		uint32_t frac = index & 0x3FFFFF;
		oscOut = oscOut + (int16_t)(((int64_t)(sawTable[osc->tableOffset][itg] - oscOut) * frac)   >>22);

		int16_t osc2Out = sawTable[osc->tableOffset][itg2++];// * -1;
		//const float frac	= (index&0x003FFFFF)*2.38418579101562e-07f;
		frac = index2 & 0x3FFFFF;
		//osc2Out += frac*(sawTable[osc->tableOffset][itg2] * -1 - osc2Out);
		osc2Out = osc2Out + (int16_t)(((int64_t)(sawTable[osc->tableOffset][itg2] - osc2Out) * frac)   >>22);

		oscOut =  __QSUB16(oscOut,osc2Out);


	#else
		oscOut = sawTable[overtoneIndex][itg];
	#endif

		osc->phase = oscPhase + osc->phaseInc;
		osc->output = oscOut;
		buf[i] = oscOut * gain;
	}
}
//-----------------------------------------------------------
int16_t calcFm(OscInfo* osc, OscInfo* modOsc, const int16_t table[][1024])
{

	const uint32_t oscPhase = osc->phase;
	int16_t oscOut ;

	const uint32_t index =  oscPhase + (((uint32_t)(modOsc->output*osc->fmMod))<<19);

	uint32_t  itg	= index>>22;

#if INTERPOLATE_FM_OSC
	oscOut = table[osc->tableOffset][itg++];
	//const float frac	= (index&0x003FFFFF)*2.38418579101562e-07f;
	//oscOut += frac*(table[osc->tableOffset][itg] - oscOut);
	uint32_t frac = index & 0x3FFFFF;
	oscOut = oscOut + (int16_t)(((int64_t)(table[osc->tableOffset][itg] - oscOut) * frac)   >>22);
#else
	oscOut = table[overtoneIndex][itg];
#endif

	osc->phase = oscPhase + osc->phaseInc;
	osc->output = oscOut;
	return oscOut;
}
//-----------------------------------------------------------
IN_ITCM_RAM __attribute__((noinline)) static void calcNextOscSampleFmBlock_osc(OscInfo* osc, int16_t* modBuffer, int16_t* buf, uint8_t size ,const float gain, const float pwm)
{
	switch(osc->waveform)
		{
		case SINE:
			calcFmSineBlock(osc,modBuffer,buf,size,gain);
			break;

		case SAW:
			calcFmBlock(osc,sawTable,modBuffer,buf,size,gain);
			break;

		case TRI:
			calcFmBlock(osc, triTable, modBuffer,buf,size,gain);
			break;

		case REC:
			calcFmBlock(osc, recTable, modBuffer,buf,size,gain);
			break;

		case NOISE:
			//TODO FM noise - needed since its tuned digi noise!
			calcNoiseBlock(osc,buf,size,gain);
			break;

		case PWM:
			calcPwmFmBlock(osc,modBuffer,buf,size,gain, pwm);//calcSampleOscFmBlock(osc,modBuffer,buf,size,gain);
			break;

		default:
			if(osc_isUserWave(osc->waveform))
				calcUserWaveBlock(osc,modBuffer,buf,size,gain);	//MOD
			else
				calcUserSampleOscFmBlock(osc,modBuffer,buf,size,gain,pwm);
			break;

		}
}
//-----------------------------------------------------------
/*
int16_t calcNextOscSampleFm(OscInfo* osc,OscInfo* modOsc)
{
	switch(osc->waveform)
	{
	case SINE:
		return calcFmSine(osc,modOsc);
		break;

	case SAW:
		return calcFm(osc,modOsc,sawTable);
		break;

	case TRI:
		return calcFm(osc,modOsc, triTable);
		break;

	case REC:
		return calcFm(osc,modOsc,recTable);
		break;

	case NOISE:
		return calcNoise(osc);
		break;

	case PWM:
		return calcSampleOscFm(osc,modOsc);
		break;

	default:

		return 0;
		break;

	}
	return 0;
}
*/
//-----------------------------------------------------------
IN_ITCM_RAM void calcNoiseBlock(OscInfo* osc, int16_t* buf, const uint8_t size ,const float gain)
{
	int i;
	for(i=0;i<size;i++)
	{
		const uint32_t lastPhase = osc->phase;
		osc->phase += osc->phaseInc;

		if(lastPhase > osc->phase)
		{
			//overflow happened -> phaseWrapped
			osc->output = GetRngValue(); //normal pitched white noise
		}

		buf[i] = osc->output * gain;
	}
}
//-----------------------------------------------------------
int16_t calcNoise(OscInfo* osc)
{
	osc->phase += osc->phaseInc;

	//check overflow flag, ( if osc->phase == osc->phaseInc a reset occured ==> retrigger, too
	APSR_Type apsr;
	apsr.w = __get_APSR();
	if(apsr.b.V ||  (osc->phase == osc->phaseInc))
	{
		//overflow happened -> phaseWrapped

		uint16_t rnd = GetRngValue();
		if( rnd > 0x00ff)
		{
			if( rnd > 0x000f)
			{
				osc->output = 32767;
			}
			else
			{
				osc->output = -32768;
			}

		}
	}
	else
	{
			osc->output = 0;
	}
	return osc->output;
}
//-----------------------------------------------------------
IN_ITCM_RAM __attribute__((noinline)) static void calcNextOscSampleBlock_osc(OscInfo* osc, int16_t* buf, const uint8_t size, const float gain, const float pwm)
{
	switch(osc->waveform)
		{
		case SINE:
			calcSineBlock(osc,buf,size, gain);
			break;

		case SAW:
			calcWavetableOscBlock(osc,sawTable,buf,size, gain);
			break;

		case TRI:
			calcWavetableOscBlock(osc,triTable,buf,size, gain);
			break;

		case REC:
			calcWavetableOscBlock(osc,recTable,buf,size, gain);
			break;

		case NOISE:
			calcNoiseBlock(osc,buf,size, gain);
			break;

		case PWM:
			calcPwmBlock(osc,buf,size, gain, pwm);
			break;

		default://sample playback (MOD)
			if(osc_isUserWave(osc->waveform))
				calcUserWaveBlock(osc,NULL,buf,size,gain);	//MOD: user wave
			else
				calcUserSampleOscBlock(osc,buf,size, gain, pwm);
			break;
		}
}
//-----------------------------------------------------------
/*
int16_t calcNextOscSample(OscInfo* osc)
{
	switch(osc->waveform)
	{
	case SINE:
		return calcSine(osc);
		break;

	case SAW:
		return calcWavetableOsc(osc,sawTable);
		break;

	case TRI:
		return calcWavetableOsc(osc,triTable);
		break;

	case REC:
		return calcWavetableOsc(osc,recTable);
		break;

	case NOISE:
		return calcNoise(osc);
		break;

	case CRASH:
		return calcSampleOsc(osc);
		break;

	default:

		return 0;
		break;

	}
	return 0;
};
*/
//---------------------------------------------------------------
IN_ITCM_RAM void calcWavetableOscBlock(OscInfo* osc, const int16_t table[][1024], int16_t* buf, const uint8_t size ,const float gain)
{
	uint8_t i;
	for(i=0;i<size;i++)
	{
		const uint32_t oscPhase = osc->phase;
			int16_t oscOut ;
			uint32_t  itg	= oscPhase>>22;
		#if INTERPOLATE_OSC
			//todo use ldm instead of ldr to load multiple values from memory
			oscOut = table[osc->tableOffset][itg++];
			//const float frac = (itg&0x003FFFFF)*2.38418579101562e-07f; //2.38... => 1.f/0x3fffff
			//oscOut += frac*(table[osc->tableOffset][itg] - oscOut);
			uint32_t frac = oscPhase & 0x3FFFFF;
			oscOut = oscOut + (int16_t)(((int64_t)(table[osc->tableOffset][itg] - oscOut) * frac)   >>22);
		#else
			oscOut = table[osc->tableOffset][itg];
		#endif

			osc->phase = oscPhase + osc->phaseInc;
			buf[i] = oscOut * gain;
	}
}

//---------------------------------------------------------------
int16_t calcWavetableOsc(OscInfo* osc,  const int16_t table[][1024])
{
	const uint32_t oscPhase = osc->phase;
	int16_t oscOut ;

	uint32_t  itg	= oscPhase>>22;

#if INTERPOLATE_OSC
	//todo use ldm instead of ldr to laod multiple values from memory
	oscOut = table[osc->tableOffset][itg++];
	//const float frac = (itg&0x003FFFFF)*2.38418579101562e-07f; //2.38... => 1.f/0x3fffff
	//oscOut += frac*(table[osc->tableOffset][itg] - oscOut);
	uint32_t frac = oscPhase & 0x3FFFFF;
	oscOut = oscOut + (int16_t)(((int64_t)(table[osc->tableOffset][itg] - oscOut) * frac)   >>22);
#else
	oscOut = table[osc->tableOffset][itg];
#endif

	osc->phase = oscPhase + osc->phaseInc;
	osc->output = oscOut;
	return oscOut;

};

//------------------------------------------------------------------
/*
void calcSampleOscFmBlock(OscInfo* osc,int16_t* modBuffer, int16_t* buf, uint8_t size ,const float gain)
{
	uint8_t i;
	for(i=0;i<size;i++)
	{
		const uint32_t oscPhase 	= osc->phase;
		int16_t oscOut ;

		const uint32_t index =  oscPhase + (((uint32_t)(modBuffer[i]*osc->fmMod))<<14);
		uint32_t  itg	= index>>17;

	#if INTERPOLATE_FM_OSC
		oscOut = crashSample[itg++];
		const float frac	= (index&20000)*0.00000762939453125f;//=> * 1/0x20000
		oscOut += frac*(crashSample[itg] - oscOut);
	#else
		oscOut = crashSample[itg];
	#endif

		oscOut = (oscOut - 127)*256;

		osc->phase = oscPhase + osc->phaseInc;
		osc->output = oscOut;
		buf[i] = oscOut * gain;
	}
}
*/
//------------------------------------------------------------------
/*
int16_t calcSampleOscFm(OscInfo* osc, OscInfo* modOsc)
{

	const uint32_t oscPhase 	= osc->phase;
	int16_t oscOut ;

	const uint32_t index =  oscPhase + (((uint32_t)(modOsc->output*osc->fmMod))<<14);
	uint32_t  itg	= index>>17;

#if INTERPOLATE_FM_OSC
	oscOut = crashSample[itg++];
	const float frac	= (index&20000)*0.00000762939453125f;//=> * 1/0x20000
	oscOut += frac*(crashSample[itg] - oscOut);
#else
	oscOut = crashSample[itg];
#endif

	oscOut = (oscOut - 127)*256;

	osc->phase = oscPhase + osc->phaseInc;
	osc->output = oscOut;
	return oscOut;
};
*/
//------------------------------------------------------------------
/* MOD: start/end of a sample voice, evaluated on the first block after a
 * trigger (osc_setBaseNote marks it with SMP_PENDING). Start comes from the
 * PWM value (incl. its modulation), the length from osc->smpLen; both 0..127
 * on a quadratic scale (fine steps at the beginning). smpLen 0 = to the end. */
static void smp_begin(OscInfo* osc, const SampleFlashEntry* e, float pwm)
{
	const uint32_t n = e->frames;
	if(!(pwm > 0.f)) pwm = 0.f;          /* also NaN */
	if(pwm > 1.f) pwm = 1.f;
	uint32_t start = (uint32_t)(pwm * pwm * (float)n);
	if(start >= n) start = n - 1u;
	uint32_t end = n;
	if(osc->smpLen)
	{
		const float l = (osc->smpLen > 127u ? 127u : osc->smpLen) * (1.f / 127.f);
		uint32_t len = (uint32_t)(l * l * (float)n);
		if(len < SMP_MIN_LEN) len = SMP_MIN_LEN;
		if(start + len < end) end = start + len;
	}
	osc->smpStart = start;
	osc->smpEnd   = end;
	osc->phase    = start;
	osc->smpFrac  = 0;
	osc->smpWrapped = 0;
	/* a retrigger cuts the sound that still plays: fade its last value out
	 * instead of jumping (choke without click) */
	osc->smpTail    = osc->output;
	osc->smpTailPos = osc->output ? SMP_DECLICK : 0u;
}
//------------------------------------------------------------------
/* one-shot playback core: linear interpolation, phase = integer frame position,
 * smpFrac = 17 bit fraction. Short fades where the sample is cut (start > 0,
 * end before the last frame) avoid clicks. modBuffer != NULL: phase modulation
 * from the mod oscillator shifts the read position by up to +/-4096 frames
 * (same scaling as the original Sonic Potions sample FM). */
static inline int32_t smp_read(const int16_t* data, const uint32_t n, int32_t rp, const uint32_t frac)
{
	if(rp < 0) rp = 0;
	if(rp >= (int32_t)n) rp = (int32_t)n - 1;
	const int32_t s0 = data[rp];
	const int32_t s1 = ((uint32_t)rp + 1u < n) ? data[rp + 1] : 0;
	return s0 + (((s1 - s0) * (int32_t)(frac >> 2)) >> 15);
}
static void smp_render(OscInfo* osc, const int16_t* modBuffer, int16_t* buf, const uint8_t size, const float gain, const float pwm)
{
	/* the sample is chosen at the trigger (incl. LFO/velocity modulation of the
	 * waveform) and kept until the next one: a modulation that changes the
	 * waveform meanwhile doesn't switch samples in the middle */
	if(osc->phase == SMP_PENDING) osc->smpWave = osc->waveform;
	const uint8_t wave = osc->smpWave >= NUM_OSC_WAVEFORMS ? osc->smpWave : osc->waveform;
	const SampleFlashEntry* e = sampleFlash_entry((uint8_t)(wave - NUM_OSC_WAVEFORMS));
	if(e == NULL || e->frames == 0)
	{
		memset(buf, 0, size * sizeof(int16_t));
		osc->output = 0;
		osc->smpTailPos = 0;
		return;
	}
	if(osc->phase == SMP_PENDING) smp_begin(osc, e, pwm);

	const int16_t* data = (const int16_t*)e->offset;
	const uint32_t n     = e->frames;
	const uint32_t start = osc->smpStart < n ? osc->smpStart : 0u;
	uint32_t end         = osc->smpEnd   < n ? osc->smpEnd   : n;   /* also a stale end of another sample */
	if(end <= start) end = start + 1u <= n ? start + 1u : n;
	/* loop samples (from /SAMPLES/LOOPS) repeat start..end until the amp
	 * envelope ends them. The loop point is crossfaded with the audio before
	 * the start (up to SMP_LOOP_XF frames); with start 0 there is none, then
	 * a short dip (fade out/in) avoids the click. */
	const uint8_t  loop = (e->flags & SF_FLAG_LOOP) && end - start >= SMP_MIN_LOOP;
	const uint32_t len  = end - start;
	uint32_t xf = 0;
	if(loop)
	{
		xf = len / 4u;
		if(xf > SMP_LOOP_XF) xf = SMP_LOOP_XF;
		if(xf > start) xf = start;
	}
	const uint8_t fadeOut = !loop && end < n;
	uint8_t wrapped = osc->smpWrapped;
	uint32_t pos  = osc->phase;
	uint32_t frac = osc->smpFrac;
	int16_t out = 0;
	for(uint8_t i=0;i<size;i++)
	{
		if(pos >= end)
		{
			if(!loop)
			{
				out = 0;
				buf[i] = 0;
				continue;
			}
			pos = start + (pos - end) % len;
			wrapped = 1;
		}
		int32_t rp = (int32_t)pos;
		if(modBuffer)
			rp += ((int32_t)(modBuffer[i]*osc->fmMod)) >> 3;
		int32_t s = smp_read(data, n, rp, frac);
		const uint32_t toEnd = end - pos;		/* 1..len */
		if(loop)
		{
			if(xf)
			{
				if(toEnd <= xf)		/* crossfade into the audio just before the start */
				{
					const int32_t t = smp_read(data, n, rp - (int32_t)len, frac);
					s = (s * (int32_t)toEnd + t * (int32_t)(xf - toEnd)) / (int32_t)xf;
				}
			}
			else
			{
				if(toEnd < SMP_LOOP_DIP) s = (s * (int32_t)toEnd) / (int32_t)SMP_LOOP_DIP;
				if(wrapped && pos - start < SMP_LOOP_DIP) s = (s * (int32_t)(pos - start + 1u)) / (int32_t)SMP_LOOP_DIP;
			}
		}
		if(fadeOut && toEnd < SMP_FADE_OUT) s = (s * (int32_t)toEnd) / (int32_t)SMP_FADE_OUT;
		if(start > 0u && !wrapped && pos - start < SMP_FADE_IN) s = (s * (int32_t)(pos - start + 1u)) / (int32_t)SMP_FADE_IN;
		if(osc->smpTailPos)
		{
			s += ((int32_t)osc->smpTail * (int32_t)osc->smpTailPos) / (int32_t)SMP_DECLICK;
			osc->smpTailPos--;
			if(s > 32767) s = 32767;
			if(s < -32768) s = -32768;
		}
		out = (int16_t)s;
		frac += osc->phaseInc;
		pos  += frac >> SMP_FRAC_BITS;
		frac &= SMP_FRAC_MASK;
		buf[i] = out * gain;
	}
	osc->phase = (!loop && pos > end) ? end : pos;
	osc->smpFrac = frac;
	osc->smpWrapped = wrapped;
	osc->output = out;
}
//------------------------------------------------------------------
/* MOD: a user wave (one cycle of SF_WAVE_POINTS points from /SAMPLES/WAVES) as
 * a wavetable: 10 bit index, 22 bit fraction as the built-in tables, wrapping at
 * the end; modBuffer != NULL: FM like calcFmBlock */
__attribute__((noinline)) void calcUserWaveBlock(OscInfo* osc, const int16_t* modBuffer, int16_t* buf, const uint8_t size, const float gain)
{
	const int16_t* t = sampleFlash_wave((uint8_t)(osc->waveform - OSC_WAVE_USER));
	if(t == NULL)
	{
		memset(buf, 0, size * sizeof(int16_t));
		osc->output = 0;
		return;
	}
	int16_t out = 0;
	for(uint8_t i=0;i<size;i++)
	{
		uint32_t index = osc->phase;
		if(modBuffer)
			index += ((uint32_t)(modBuffer[i]*osc->fmMod))<<19;
		const uint32_t itg = index >> 22;
		const uint32_t frac = index & 0x3FFFFF;
		const int32_t a = t[itg];
		const int32_t b = t[(itg + 1u) & (SF_WAVE_POINTS - 1u)];
		out = (int16_t)(a + (int32_t)(((int64_t)(b - a) * frac) >> 22));
		osc->phase += osc->phaseInc;
		buf[i] = out * gain;
	}
	osc->output = out;
}
//------------------------------------------------------------------
void calcUserSampleOscFmBlock(OscInfo* osc,int16_t* modBuffer, int16_t* buf, uint8_t size ,const float gain, const float pwm)
{
	smp_render(osc, modBuffer, buf, size, gain, pwm);
}
//---------------------------------------------------------------
void calcUserSampleOscBlock(OscInfo* osc, int16_t* buf, const uint8_t size ,const float gain, const float pwm)
{
	smp_render(osc, NULL, buf, size, gain, pwm);
}
//---------------------------------------------------------------
/*
void calcSampleOscBlock(OscInfo* osc, int16_t* buf, const uint8_t size ,const float gain)
{
	uint8_t i;
	for(i=0;i<size;i++)
	{
		const uint32_t oscPhase = osc->phase;
		int16_t oscOut ;
		uint32_t  itg	= oscPhase>>17;


	#if INTERPOLATE_OSC
		//todo use ldm instead of ldr to laod multiple values from memory
		oscOut = crashSample[itg++];
		const float frac = (oscPhase&20000)*0.00000762939453125f;//=> * 1/0x20000
		oscOut += frac*(crashSample[itg] - oscOut);
	#else
		oscOut = crashSample[itg];
	#endif
		oscOut = (oscOut - 127)*256;

		osc->phase = oscPhase + osc->phaseInc;
		buf[i] = oscOut * gain;
	}
}
*/
//-----------------------------------------------------------
IN_ITCM_RAM void calcPwmBlock(OscInfo* osc, int16_t* buf, uint8_t size ,const float gain, const float pwm)
{



	uint8_t i;
	for(i=0;i<size;i++)
	{
		const uint32_t oscPhase = osc->phase;
		const uint32_t oscPhase2 = osc->phase + pwm * (UINT32_MAX/2);
		int16_t oscOut ;
		uint32_t  itg	= oscPhase>>22;
		uint32_t  itg2	= oscPhase2>>22;


		#if INTERPOLATE_FM_OSC
			oscOut = sawTable[osc->tableOffset][itg++];
			//const float frac	= (oscPhase&0x003FFFFF)*2.38418579101562e-07f;
			//const float frac = (itg&0x003FFFFF)*2.38418579101562e-07f;
			//oscOut += frac*(sawTable[osc->tableOffset][itg] - oscOut);
			uint32_t frac = oscPhase & 0x3FFFFF;
			oscOut = oscOut + (int16_t)(((int64_t)(sawTable[osc->tableOffset][itg] - oscOut) * frac)   >>22);

			int16_t osc2Out = sawTable[osc->tableOffset][itg2++] ;//* -1;
			//const float frac	= (index&0x003FFFFF)*2.38418579101562e-07f;
			//osc2Out += frac*(sawTable[osc->tableOffset][itg2] * -1 - osc2Out);
			frac = oscPhase2 & 0x3FFFFF;
			osc2Out = osc2Out + (int16_t)(((int64_t)(sawTable[osc->tableOffset][itg2++] - osc2Out) * frac)   >>22);

			oscOut =  __QSUB16(oscOut,osc2Out);

		#else
			oscOut = sawTable[overtoneIndex][itg];
		#endif

			osc->phase = oscPhase + osc->phaseInc;
			osc->output = oscOut;
			buf[i] = oscOut * gain;
	}
}
//---------------------------------------------------------------
/*
int16_t calcSampleOsc(OscInfo* osc)
{
	const uint32_t oscPhase = osc->phase;
	int16_t oscOut ;

	uint32_t  itg	= oscPhase>>17;

#if INTERPOLATE_OSC
	//todo use ldm instead of ldr to laod multiple values from memory
	oscOut = crashSample[itg++];
	const float frac = (oscPhase&20000)*0.00000762939453125f;//=> * 1/0x20000
	oscOut += frac*(crashSample[itg] - oscOut);
#else
	oscOut = crashSample[itg];
#endif
	oscOut = (oscOut - 127)*256;

	osc->phase = oscPhase + osc->phaseInc;
	osc->output = oscOut;
	return oscOut;

};
*/
 //-----------------------------------------------------------
void osc_calcSineFreq(OscInfo* osc)
{
	osc->phaseInc = freq2PhaseIncr(osc->freq* osc->pitchMod*osc->modNodeValue);
}
//-----------------------------------------------------------
void osc_calcNoiseFreq(OscInfo* osc)
{
	osc_calcSineFreq(osc);
}
//-----------------------------------------------------------
void osc_calcWavetableFreq(OscInfo* osc)
{
	const float currentFreq = osc->freq*osc->pitchMod*osc->modNodeValue;
	osc->phaseInc = freq2PhaseIncr1024(currentFreq);

	const uint8_t overtoneIndex = freqToTableIndex(currentFreq);
	osc->tableOffset = overtoneIndex>10?10:overtoneIndex;
}
//-----------------------------------------------------------
void osc_calcSampleFreq(OscInfo* osc)
{
	const float currentFreq = osc->freq*osc->pitchMod*osc->modNodeValue;
	osc->phaseInc = freq2PhaseIncr32767(currentFreq);
}
//-----------------------------------------------------------
void osc_calcUserSampleFreq(OscInfo* osc)
{
	/* MOD: frames per output sample (17 bit fixed point). Original pitch at
	 * SMP_ROOT_NOTE, corrected for the sample's own rate. */
	const SampleFlashEntry* e = sampleFlash_entry((uint8_t)(osc->waveform - NUM_OSC_WAVEFORMS));
	const float rate = (e != NULL && e->rate > 0u) ? (float)e->rate : REAL_FS;
	const float currentFreq = osc->freq*osc->pitchMod*osc->modNodeValue;
	float inc = (currentFreq / MidiNoteFrequencies[SMP_ROOT_NOTE]) * (rate / REAL_FS) * (float)(1u << SMP_FRAC_BITS);
	if(!(inc > 0.f)) inc = 0.f;                                      /* also catches NaN */
	if(inc > 64.f * (float)(1u << SMP_FRAC_BITS)) inc = 64.f * (float)(1u << SMP_FRAC_BITS);
	osc->phaseInc = (uint32_t)inc;
}
//-----------------------------------------------------------
 __attribute__((noinline)) static void osc_setFreq_osc(OscInfo* osc)
 {
		switch(osc->waveform)
		{
		case SINE:
			osc_calcSineFreq(osc);
			break;

		case SAW:
			osc_calcWavetableFreq(osc);
			break;

		case TRI:
			osc_calcWavetableFreq(osc);
			break;

		case REC:
			osc_calcWavetableFreq(osc);
			break;

		case NOISE:
			osc_calcNoiseFreq(osc);
			break;


		case PWM:
			osc_calcWavetableFreq(osc);
			break;

		//samples
		default:
			if(osc_isUserWave(osc->waveform))
				osc->phaseInc = freq2PhaseIncr1024(osc->freq*osc->pitchMod*osc->modNodeValue);	//MOD: user wave
			else
				osc_calcUserSampleFreq(osc);
			break;

		}
 }
 //-----------------------------------------------------------
 void osc_setBaseNote(OscInfo* osc, uint8_t baseNote)
 {

	 //get fine tune
	 const float cent = midiParser_calcDetune(osc->midiFreq&0xff);
	 //calc coarse tune
	 int16_t note =  (osc->midiFreq>>8) + (baseNote-SEQ_DEFAULT_NOTE);
	 if(note>127)note=127;
	 if(note<0)note=0;

	 osc->freq = MidiNoteFrequencies[note]*cent;
	 osc->baseNote = baseNote;

	 /* MOD: called on every voice trigger - start position and length are
	  * applied on the next block (smp_begin), with the current PWM/start value */
	 const uint8_t wave = osc->altWave ? osc->altWave - 1u : osc->waveform;	/* MOD: track 7 */
	 if(wave >= NUM_OSC_WAVEFORMS && !osc_isUserWave(wave))
	 {
		 osc->phase = SMP_PENDING;
		 osc->smpFrac = 0;
	 }
 };

 //-----------------------------------------------------------
 IN_ITCM_RAM void osc_recalcFreq(OscInfo* osc)
 {
	 //get fine tune
	 const float cent = midiParser_calcDetune(osc->midiFreq&0xff);
	 //calc coarse tune
	 int16_t note =  (osc->midiFreq>>8) + (osc->baseNote-SEQ_DEFAULT_NOTE);

	 if(note>127)note=127;
 	 if(note<0)note=0;

	 osc->freq = MidiNoteFrequencies[note]*cent;
 }
 //-----------------------------------------------------------

//-----------------------------------------------------------
/* MOD: track 7 (the second track of slot 6) can play its own waveform and pitch
 * on the shared voice: for the time of the call the oscillator uses them. */
IN_ITCM_RAM void calcNextOscSampleBlock(OscInfo* osc, int16_t* buf, const uint8_t size, const float gain, const float pwm)
{
	if(!osc->altWave) { calcNextOscSampleBlock_osc(osc, buf, size, gain, pwm); return; }
	const uint8_t w = osc->waveform;
	osc->waveform = osc->altWave - 1u;
	calcNextOscSampleBlock_osc(osc, buf, size, gain, pwm);
	osc->waveform = w;
}
IN_ITCM_RAM void calcNextOscSampleFmBlock(OscInfo* osc, int16_t* modBuffer, int16_t* buf, uint8_t size ,const float gain, const float pwm)
{
	if(!osc->altWave) { calcNextOscSampleFmBlock_osc(osc, modBuffer, buf, size, gain, pwm); return; }
	const uint8_t w = osc->waveform;
	osc->waveform = osc->altWave - 1u;
	calcNextOscSampleFmBlock_osc(osc, modBuffer, buf, size, gain, pwm);
	osc->waveform = w;
}
void osc_setFreq(OscInfo* osc)
{
	const uint8_t w = osc->waveform;
	const float f = osc->freq;
	if(osc->altWave) osc->waveform = osc->altWave - 1u;
	if(osc->altPitch > 0.f) osc->freq *= osc->altPitch;
	osc_setFreq_osc(osc);
	osc->waveform = w;
	osc->freq = f;
}
