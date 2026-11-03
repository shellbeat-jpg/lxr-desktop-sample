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
	uint8_t  comp;
	float    env;					/* dB over the threshold */
} mb;

/*----------------------------------------------------------------------------*/
uint32_t masterBus_crossoverHz(uint8_t v)
{
	if(v == 0) return 0;
	return (uint32_t)(40.f * powf(50.f, (float)(v - 1u) / 126.f) + 0.5f);
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
	mb.rel = expf(-1000.f / ((20.f + 980.f * l * l) * REAL_FS));
	mb.makeup = powf(10.f, (24.f * s[MB_GAI] / 127.f) / 20.f);
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
static inline void mb_compress(float* l, float* r)
{
	const float peak = fmaxf(fabsf(*l), fabsf(*r)) + 1e-9f;
	float over = 20.f * log10f(peak) - mb.thr;
	if(over < 0.f) over = 0.f;
	const float c = over > mb.env ? mb.att : mb.rel;
	mb.env = over + c * (mb.env - over);
	const float g = powf(10.f, -mb.env * mb.slope / 20.f) * mb.makeup;
	*l += (*l * g - *l) * mb.wet;
	*r += (*r * g - *r) * mb.wet;
}
/*----------------------------------------------------------------------------*/
void masterBus_process(int32_t* mix, uint32_t frames, const uint8_t* s)
{
	if(!mb.valid || memcmp(mb.last, s, MB_NUM_PARAMS) != 0)
		mb_update(s);
	const uint8_t sat = s[MB_SAT] != 0;
	if(!sat && !mb.comp)
	{
		mb.env = 0.f;
		return;		/* off: the sum stays bit for bit as it was */
	}
	const uint8_t compFirst = s[MB_ORD] != 0;
	for(uint32_t i = 0; i < frames; i++)
	{
		float l = mix[2u * i] * (1.f / 32768.f);
		float r = mix[2u * i + 1u] * (1.f / 32768.f);
		if(compFirst && mb.comp) mb_compress(&l, &r);
		if(sat)
		{
			l = mb_saturate(l, 0);
			r = mb_saturate(r, 1);
		}
		if(!compFirst && mb.comp) mb_compress(&l, &r);
		mix[2u * i]      = (int32_t)(l * 32768.f);
		mix[2u * i + 1u] = (int32_t)(r * 32768.f);
	}
}
