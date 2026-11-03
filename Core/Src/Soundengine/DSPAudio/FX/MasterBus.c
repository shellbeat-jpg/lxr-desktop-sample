/*
 * MasterBus.c
 *
 * MOD: FX3, the master bus of St1. See MasterBus.h.
 *
 * Modified firmware - not an official Erica Synths release.
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "Soundengine/DSPAudio/FX/MasterBus.h"
#include "Soundengine/config.h"
#include <math.h>
#include <string.h>
#include "Soundengine/Sequencer/sequencer.h"	/* seq_getBpm: auto release */

typedef struct { float b0, b1, b2, a1, a2; } mb_Coef;

static struct
{
	uint8_t  last[MB_NUM_PARAMS];	/* settings the coefficients were made for */
	uint8_t  valid;
	/* saturation */
	float    drive, satOut;
	uint8_t  split;
	mb_Coef  lp, hp;				/* 2nd order Butterworth, applied twice = LR4 */
	float    z[4][2][2];			/* [lp1, lp2, hp1, hp2][channel][state] */
	/* compressor */
	float    thr, slope, att, rel, makeup, wet;
	uint16_t relBpm;				/* tempo the auto release was made for (0 = fixed) */
	uint8_t  comp;
	uint8_t  cSplit;				/* 0 = off, 1 = band split, 2 = sidechain */
	mb_Coef  clp, chp;				/* compressor crossover */
	float    cz[4][2][2];
	float    env;					/* dB over the threshold */
	float    grMax;					/* highest reduction of this block (display) */
	float    grSum;					/* sum of the reduction of this block (adaptive make-up) */
	float    grAvg;					/* average reduction, smoothed (adaptive make-up) */
	float    gainDb;				/* GAI (+ static auto make-up) */
	/* EQ: low shelf, mid 1, mid 2, high shelf */
	uint8_t  eq;					/* bit n = band n active */
	mb_Coef  eqc[4];
	float    ez[4][2][2];
} mb;

volatile uint32_t masterBus_blocks = 0;
volatile uint8_t  masterBus_active = 0;
volatile float    masterBus_grDb = 0.f;
volatile int32_t  masterBus_peak[2] = { 0, 0 };
volatile uint8_t  masterBus_onDac2 = 0;

/*----------------------------------------------------------------------------*/
uint32_t masterBus_crossoverHz(uint8_t v)
{
	if(v == 0) return 0;
	return (uint32_t)(40.f * powf(250.f, (float)(v - 1u) / 126.f) + 0.5f);
}
float masterBus_eqGainDb(uint8_t v)
{
	if(v > 127) v = 127;
	return ((int)v - 64) * (15.f / 64.f);
}
float masterBus_eqFreqHz(uint8_t v)
{
	return 20.f * powf(800.f, (v > 127 ? 127 : v) / 127.f);
}
float masterBus_eqQ(uint8_t v)
{
	return 0.3f * powf(8.f / 0.3f, (v > 127 ? 127 : v) / 127.f);
}
const char* masterBus_orderName(uint8_t v, const uint8_t isLong)
{
	static const char* const shortNames[MB_ORD_COUNT] = { "SCE", "CSE", "ESC", "ECS", "SEC", "CES" };
	static const char* const longNames[MB_ORD_COUNT] = { "Sat>Comp>EQ", "Comp>Sat>EQ", "EQ>Sat>Comp",
			"EQ>Comp>Sat", "Sat>EQ>Comp", "Comp>EQ>Sat" };
	if(v >= MB_ORD_COUNT) v = 0;
	return isLong ? longNames[v] : shortNames[v];
}
uint8_t masterBus_maxValue(const uint8_t k)
{
	switch(k)
	{
	case MB_ORD:	return MB_ORD_COUNT - 1;
	case MB_AMU:	return 2;
	case MB_CSM:	return 1;
	default:		return 127;
	}
}
/* RBJ cookbook: peaking EQ and shelves (shelf slope 1) */
static void mb_peak(mb_Coef* c, const float fc, const float db, const float q)
{
	const float A = powf(10.f, db / 40.f);
	const float w = 2.f * 3.14159265f * fc / REAL_FS;
	const float cw = cosf(w), alpha = sinf(w) / (2.f * q);
	const float a0 = 1.f + alpha / A;
	c->b0 = (1.f + alpha * A) / a0;
	c->b1 = -2.f * cw / a0;
	c->b2 = (1.f - alpha * A) / a0;
	c->a1 = c->b1;
	c->a2 = (1.f - alpha / A) / a0;
}
static void mb_shelf(mb_Coef* c, const float fc, const float db, const uint8_t high)
{
	const float A = powf(10.f, db / 40.f);
	const float w = 2.f * 3.14159265f * fc / REAL_FS;
	const float cw = cosf(w), alpha = sinf(w) / 2.f * 1.41421356f;	/* S = 1 */
	const float sA = 2.f * sqrtf(A) * alpha;
	const float s = high ? -1.f : 1.f;	/* the high shelf mirrors the signs of cw */
	const float a0 = (A + 1.f) + s * (A - 1.f) * cw + sA;
	c->b0 = A * ((A + 1.f) - s * (A - 1.f) * cw + sA) / a0;
	c->b1 = 2.f * s * A * ((A - 1.f) - s * (A + 1.f) * cw) / a0;
	c->b2 = A * ((A + 1.f) - s * (A - 1.f) * cw - sA) / a0;
	c->a1 = -2.f * s * ((A - 1.f) + s * (A + 1.f) * cw) / a0;
	c->a2 = ((A + 1.f) + s * (A - 1.f) * cw - sA) / a0;
}
/* RBJ cookbook, Q = 1/sqrt(2) (Butterworth) */
static void mb_butter(mb_Coef* c, const float fc, const uint8_t high)
{
	const float w = 2.f * 3.14159265f * fc / REAL_FS;
	const float cw = cosf(w), alpha = sinf(w) / (2.f * 0.70710678f);
	const float a0 = 1.f + alpha;
	if(high)
	{
		c->b0 = (1.f + cw) / 2.f / a0;
		c->b1 = -(1.f + cw) / a0;
	}
	else
	{
		c->b0 = (1.f - cw) / 2.f / a0;
		c->b1 = (1.f - cw) / a0;
	}
	c->b2 = c->b0;
	c->a1 = -2.f * cw / a0;
	c->a2 = (1.f - alpha) / a0;
}
static inline float mb_biquad(const mb_Coef* c, float* z, const float x)
{
	/* transposed direct form II */
	const float y = c->b0 * x + z[0];
	z[0] = c->b1 * x - c->a1 * y + z[1];
	z[1] = c->b2 * x - c->a2 * y;
	return y;
}
static void mb_update(const uint8_t* s)
{
	const float sat = s[MB_SAT] / 127.f;
	mb.drive  = 1.f + 15.f * sat * sat;		/* up to +24 dB into the curve */
	mb.satOut = 1.f / sqrtf(mb.drive);
	const uint8_t split = s[MB_FRQ] != 0;
	if(split)
	{
		const float fc = (float)masterBus_crossoverHz(s[MB_FRQ]);
		mb_butter(&mb.lp, fc, 0);
		mb_butter(&mb.hp, fc, 1);
	}
	if(split != mb.split)
		memset(mb.z, 0, sizeof(mb.z));
	mb.split = split;

	mb.thr = -48.f + 48.f * s[MB_THR] / 127.f;
	const float r = s[MB_RAT] / 127.f;
	const float ratio = 1.f + 19.f * r * r;
	mb.slope = 1.f - 1.f / ratio;
	const float a = s[MB_ATK] / 127.f, l = s[MB_REL] / 127.f;
	mb.att = expf(-1000.f / ((0.1f + 100.f * a * a) * REAL_FS));
	mb.relBpm = 0;
	if(s[MB_REL])
	{
		const float lr = (s[MB_REL] - 1u) / 126.f;
		mb.rel = expf(-1000.f / ((20.f + 980.f * lr * lr) * REAL_FS));
	}
	(void)l;
	const uint8_t cSplit = s[MB_CFQ] == 0 ? 0 : s[MB_CSM] ? 2 : 1;
	if(cSplit)
	{
		const float fc = (float)masterBus_crossoverHz(s[MB_CFQ]);
		mb_butter(&mb.clp, fc, 0);
		mb_butter(&mb.chp, fc, 1);
	}
	if(cSplit != mb.cSplit)
		memset(mb.cz, 0, sizeof(mb.cz));
	mb.cSplit = cSplit;
	float gainDb = 24.f * s[MB_GAI] / 127.f;
	if(s[MB_AMU] == 1)
		gainDb += -mb.thr * mb.slope * 0.5f;	/* static: half the reduction of a 0 dBFS peak */
	mb.gainDb = gainDb;
	mb.makeup = powf(10.f, (gainDb + (s[MB_AMU] == 2 ? mb.grAvg : 0.f)) / 20.f);

	/* EQ: only the bands that are not at 0 dB */
	const uint8_t g[4] = { s[MB_EQL], s[MB_E1G], s[MB_E2G], s[MB_EQH] };
	uint8_t eq = 0;
	for(uint8_t b = 0; b < 4; b++)
		if(g[b] != 64) eq |= (uint8_t)(1u << b);
	if(eq & 1) mb_shelf(&mb.eqc[0], 100.f, masterBus_eqGainDb(g[0]), 0);
	if(eq & 2) mb_peak(&mb.eqc[1], masterBus_eqFreqHz(s[MB_E1F]), masterBus_eqGainDb(g[1]), masterBus_eqQ(s[MB_E1Q]));
	if(eq & 4) mb_peak(&mb.eqc[2], masterBus_eqFreqHz(s[MB_E2F]), masterBus_eqGainDb(g[2]), masterBus_eqQ(s[MB_E2Q]));
	if(eq & 8) mb_shelf(&mb.eqc[3], 8000.f, masterBus_eqGainDb(g[3]), 1);
	for(uint8_t b = 0; b < 4; b++)
		if((eq ^ mb.eq) & (1u << b))
			memset(mb.ez[b], 0, sizeof(mb.ez[b]));	/* band switched on or off */
	mb.eq = eq;
	mb.wet = s[MB_MIX] / 127.f;
	mb.comp = s[MB_RAT] != 0 || s[MB_GAI] != 0;
	memcpy(mb.last, s, MB_NUM_PARAMS);
	mb.valid = 1;
}
/* soft clip, rational tanh approximation (exact limits at +-3) */
static inline float mb_softclip(float x)
{
	if(x > 3.f) return 1.f;
	if(x < -3.f) return -1.f;
	const float x2 = x * x;
	return x * (27.f + x2) / (27.f + 9.f * x2);
}
static inline float mb_saturate(const float x, const uint8_t ch)
{
	if(!mb.split)
		return mb_softclip(x * mb.drive) * mb.satOut;
	const float lo = mb_biquad(&mb.lp, mb.z[1][ch], mb_biquad(&mb.lp, mb.z[0][ch], x));
	const float hi = mb_biquad(&mb.hp, mb.z[3][ch], mb_biquad(&mb.hp, mb.z[2][ch], x));
	return lo + mb_softclip(hi * mb.drive) * mb.satOut;
}
/* compress l/r, the level detected on dl/dr */
static inline void mb_compressBand(float* l, float* r, const float dl, const float dr)
{
	const float peak = fmaxf(fabsf(dl), fabsf(dr)) + 1e-9f;
	float over = 20.f * log10f(peak) - mb.thr;
	if(over < 0.f) over = 0.f;
	const float c = over > mb.env ? mb.att : mb.rel;
	mb.env = over + c * (mb.env - over);
	const float gr = mb.env * mb.slope;
	if(gr > mb.grMax) mb.grMax = gr;
	mb.grSum += gr;
	const float g = powf(10.f, -mb.env * mb.slope / 20.f) * mb.makeup;
	*l += (*l * g - *l) * mb.wet;
	*r += (*r * g - *r) * mb.wet;
}
/* with the split only the band above the crossover is detected and compressed */
static inline void mb_compress(float* l, float* r)
{
	if(!mb.cSplit)
	{
		mb_compressBand(l, r, *l, *r);
		return;
	}
	if(mb.cSplit == 2)
	{
		/* sidechain: only the band above the crossover drives the compressor */
		const float dL = mb_biquad(&mb.chp, mb.cz[3][0], mb_biquad(&mb.chp, mb.cz[2][0], *l));
		const float dR = mb_biquad(&mb.chp, mb.cz[3][1], mb_biquad(&mb.chp, mb.cz[2][1], *r));
		mb_compressBand(l, r, dL, dR);
		return;
	}
	const float loL = mb_biquad(&mb.clp, mb.cz[1][0], mb_biquad(&mb.clp, mb.cz[0][0], *l));
	float       hiL = mb_biquad(&mb.chp, mb.cz[3][0], mb_biquad(&mb.chp, mb.cz[2][0], *l));
	const float loR = mb_biquad(&mb.clp, mb.cz[1][1], mb_biquad(&mb.clp, mb.cz[0][1], *r));
	float       hiR = mb_biquad(&mb.chp, mb.cz[3][1], mb_biquad(&mb.chp, mb.cz[2][1], *r));
	mb_compressBand(&hiL, &hiR, hiL, hiR);
	*l = loL + hiL;
	*r = loR + hiR;
}
static inline void mb_eqProcess(float* l, float* r)
{
	for(uint8_t b = 0; b < 4; b++)
		if(mb.eq & (1u << b))
		{
			*l = mb_biquad(&mb.eqc[b], mb.ez[b][0], *l);
			*r = mb_biquad(&mb.eqc[b], mb.ez[b][1], *r);
		}
}
/*----------------------------------------------------------------------------*/
void masterBus_process(int32_t* mix, uint32_t frames, const uint8_t* s)
{
	if(!mb.valid || memcmp(mb.last, s, MB_NUM_PARAMS) != 0)
		mb_update(s);
	const uint8_t sat = s[MB_SAT] != 0;
	masterBus_blocks++;
	if(!sat && !mb.comp && !mb.eq)
	{
		mb.env = 0.f;
		mb.grAvg = 0.f;
		masterBus_active = 0;
		masterBus_grDb = 0.f;
		return;		/* off: the sum stays bit for bit as it was */
	}
	masterBus_active = 1;
	if(mb.comp && s[MB_REL] == 0)
	{
		/* auto release: the time constant follows the tempo, a 1/16 note (4/4) */
		uint16_t bpm = seq_getBpm();
		if(bpm < 20) bpm = 20;
		if(bpm != mb.relBpm)
		{
			mb.relBpm = bpm;
			const float ms = 60000.f / bpm / 4.f;
			mb.rel = expf(-1000.f / (ms * REAL_FS));
		}
	}
	if(mb.comp && s[MB_AMU] == 2)
		mb.makeup = powf(10.f, (mb.gainDb + mb.grAvg) / 20.f);	/* adaptive make-up */
	/* the three stages in the order of ORD: 'S', 'C', 'E' */
	const char* const ord = masterBus_orderName(s[MB_ORD], 0);
	mb.grSum = 0.f;
	for(uint32_t i = 0; i < frames; i++)
	{
		float l = mix[2u * i] * (1.f / 32768.f);
		float r = mix[2u * i + 1u] * (1.f / 32768.f);
		for(uint8_t k = 0; k < 3; k++)
		{
			switch(ord[k])
			{
			case 'S':
				if(sat)
				{
					l = mb_saturate(l, 0);
					r = mb_saturate(r, 1);
				}
				break;
			case 'C':
				if(mb.comp) mb_compress(&l, &r);
				break;
			default:
				if(mb.eq) mb_eqProcess(&l, &r);
				break;
			}
		}
		mix[2u * i]      = (int32_t)(l * 32768.f);
		mix[2u * i + 1u] = (int32_t)(r * 32768.f);
	}
	/* adaptive make-up: the average reduction, time constant about 1.5 s */
	if(mb.comp && frames)
	{
		const float k = 1.f - expf(-(float)frames / (1.5f * REAL_FS));
		mb.grAvg += (mb.grSum / frames - mb.grAvg) * k;
	}
	else
		mb.grAvg = 0.f;
	/* highest reduction since the display read it (it sets it back to 0) */
	const float gr = mb.comp ? mb.grMax : 0.f;
	if(gr > masterBus_grDb) masterBus_grDb = gr;
	mb.grMax = 0.f;
}
