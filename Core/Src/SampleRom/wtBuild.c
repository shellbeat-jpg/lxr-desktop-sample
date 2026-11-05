/*
 * wtBuild.c
 *
 * MOD (OE02-Samples+): wavetable frames from source cycles (wtBuild.h).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "SampleRom/wtBuild.h"
#include <math.h>
#include <string.h>

#define WT_PI	3.14159265358979f

/* the running analysis: per harmonic the rotation per point and its state */
static WtWork* wt_w;
#define wt_cs	(wt_w->cs)
#define wt_sn	(wt_w->sn)
#define wt_c	(wt_w->c)
#define wt_s	(wt_w->s)
#define wt_re	(wt_w->re)
#define wt_im	(wt_w->im)
#define wt_tmp	(wt_w->tmp)
static uint32_t wt_n, wt_i;
static uint8_t wt_h;	/* harmonics that fit into n points */

void wtBuild_setWork(WtWork* w)
{
	wt_w = w;
}

void wtBuild_begin(uint32_t n)
{
	wt_n = n ? n : 1u;
	wt_i = 0;
	const uint32_t hmax = wt_n / 2u > 0u ? (wt_n - 1u) / 2u : 0u;
	wt_h = (uint8_t)(hmax < WT_HARM ? hmax : WT_HARM);
	for(uint8_t k=0;k<WT_HARM;k++)
	{
		const float w = 2.f * WT_PI * (float)(k + 1u) / (float)wt_n;
		wt_cs[k] = cosf(w);
		wt_sn[k] = sinf(w);
		wt_c[k] = 1.f;
		wt_s[k] = 0.f;
		wt_re[k] = wt_im[k] = 0.f;
	}
}

void wtBuild_add(const float x)
{
	for(uint8_t k=0;k<wt_h;k++)
	{
		wt_re[k] += x * wt_c[k];	/* cos part */
		wt_im[k] += x * wt_s[k];	/* sin part */
		const float c = wt_c[k] * wt_cs[k] - wt_s[k] * wt_sn[k];
		wt_s[k] = wt_s[k] * wt_cs[k] + wt_c[k] * wt_sn[k];
		wt_c[k] = c;
	}
	/* against the drift of the rotation */
	if((++wt_i & 255u) == 0u)
	{
		for(uint8_t k=0;k<wt_h;k++)
		{
			const float g = 1.f / sqrtf(wt_c[k] * wt_c[k] + wt_s[k] * wt_s[k]);
			wt_c[k] *= g;
			wt_s[k] *= g;
		}
	}
}

static float wt_wrap(float p)
{
	while(p > WT_PI) p -= 2.f * WT_PI;
	while(p <= -WT_PI) p += 2.f * WT_PI;
	return p;
}

void wtBuild_end(WtSpectrum* out)
{
	const float g = 2.f / (float)wt_n;
	for(uint8_t k=0;k<WT_HARM;k++)
	{
		/* x = re cos + im sin = m sin(h x + ph): re = m sin ph, im = m cos ph */
		const float re = k < wt_h ? wt_re[k] * g : 0.f;
		const float im = k < wt_h ? wt_im[k] * g : 0.f;
		out->mag[k] = sqrtf(re * re + im * im);
		out->ph[k] = (re == 0.f && im == 0.f) ? 0.f : atan2f(re, im);
	}
	/* line up: the fundamental at phase 0 (shift by -ph1/h per harmonic h). A
	 * cycle without fundamental (e.g. one octave up) stays as it is. */
	if(out->mag[0] > 1e-3f)
	{
		const float p1 = out->ph[0];
		for(uint8_t k=0;k<WT_HARM;k++)
			out->ph[k] = wt_wrap(out->ph[k] - (float)(k + 1u) * p1);
	}
}

void wtBuild_mix(const WtSpectrum* a, const WtSpectrum* b, const float t, WtSpectrum* out)
{
	for(uint8_t k=0;k<WT_HARM;k++)
	{
		out->mag[k] = a->mag[k] + (b->mag[k] - a->mag[k]) * t;
		/* the phase the short way; a harmonic missing on one side keeps the other's */
		float pa = a->ph[k], pb = b->ph[k];
		if(a->mag[k] < 1e-6f) pa = pb;
		if(b->mag[k] < 1e-6f) pb = pa;
		out->ph[k] = wt_wrap(pa + wt_wrap(pb - pa) * t);
	}
}

/* one level: n points with the harmonics 1..hmax */
static void wt_level(const WtSpectrum* s, const uint16_t n, const uint16_t hmax, float* y)
{
	memset(y, 0, n * sizeof(float));
	for(uint16_t k=0;k<hmax;k++)
	{
		const float m = s->mag[k];
		if(m < 1e-6f)
			continue;
		const float w = 2.f * WT_PI * (float)(k + 1u) / (float)n;
		const float cs = cosf(w), sn = sinf(w);
		float c = cosf(s->ph[k]), si = sinf(s->ph[k]);	/* sin(w i + ph) = si */
		for(uint16_t i=0;i<n;i++)
		{
			y[i] += m * si;
			const float c2 = c * cs - si * sn;
			si = si * cs + c * sn;
			c = c2;
		}
	}
}

void wtBuild_render(const WtSpectrum* s, int16_t* frame)
{
	/* the gain from the full frame's peak */
	wt_level(s, WT_POINTS, WT_HARM, wt_tmp);
	float peak = 0.f;
	for(uint16_t i=0;i<WT_POINTS;i++)
		if(fabsf(wt_tmp[i]) > peak) peak = fabsf(wt_tmp[i]);
	const float g = peak > 1e-6f ? 30000.f / peak : 0.f;
	for(uint8_t l=0;l<WT_LEVELS;l++)
	{
		const uint16_t n = WT_LEVEL_LEN(l);
		if(l) wt_level(s, n, WT_LEVEL_HARM(l), wt_tmp);
		int16_t* o = frame + WT_LEVEL_OFS(l);
		for(uint16_t i=0;i<n;i++)
		{
			const float v = wt_tmp[i] * g;
			o[i] = (int16_t)(v > 32767.f ? 32767 : v < -32767.f ? -32767 : (int32_t)lrintf(v));
		}
	}
}

void wtBuild_plan(const uint32_t n, const uint8_t maxInter, uint32_t* used, uint8_t* inter, uint32_t* total)
{
	if(n >= WT_MAX_FRAMES)
	{
		*used = WT_MAX_FRAMES;
		*inter = 0;
	}
	else
	{
		*used = n;
		uint32_t k = n > 1u ? (WT_MAX_FRAMES - 1u) / (n - 1u) - 1u : 0u;
		if(k > maxInter) k = maxInter;
		*inter = (uint8_t)k;
	}
	*total = *used ? *used + (*used - 1u) * *inter : 0u;
}

uint32_t wtBuild_source(const uint32_t n, const uint32_t used, const uint32_t i)
{
	if(used <= 1u || used >= n)
		return i;
	return (uint32_t)(((uint64_t)i * (n - 1u) * 2u + (used - 1u)) / (2u * (used - 1u)));	/* rounded */
}
