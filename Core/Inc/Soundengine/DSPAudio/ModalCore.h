/*
 * ModalCore.h
 *
 * MOD (OE02-Samples+): the sound of the modal engine (ModalVoice.h), a physical
 * model: a struck body is a bank of 6 resonators (its modes), tuned to the mode
 * ratios of a model (membrane, bar, string, bell, plate, glass). An exciter
 * (a mallet pulse or a noise burst, plus the voice's transient sample) strikes
 * them; damping, material, strike position and inharmonicity shape the modes.
 * No hardware dependencies: also built by the host test (tests/host/modal.c).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef MODALCORE_H_
#define MODALCORE_H_

#include <stdint.h>
#include <math.h>

#define MODAL_MODES		6
#define MODAL_MODELS	6
#define MODAL_PI		3.14159265f

typedef struct
{
	uint8_t model;	/* 0..5 */
	uint8_t damp;	/* decay time of the modes 0..127 (30 ms .. 6 s) */
	uint8_t pos;	/* strike position 0..127 (edge .. centre) */
	uint8_t inh;	/* inharmonicity (stiffness) 0..127 */
	uint8_t hard;	/* hardness of the mallet 0..127 (soft = long pulse) */
	uint8_t mat;	/* material: high modes decay faster 0..127 */
	uint8_t noise;	/* exciter: 0 mallet, 1 noise burst */
} ModalParams;

typedef struct
{
	float y1[MODAL_MODES], y2[MODAL_MODES];	/* resonator states */
	float a1[MODAL_MODES], a2[MODAL_MODES], b[MODAL_MODES];
	float R[MODAL_MODES];		/* pole radius (damping) */
	float ratio[MODAL_MODES];	/* mode ratio incl. inharmonicity */
	float gain[MODAL_MODES];	/* strike position weight */
	float f0;					/* the coefficients were calculated for this pitch */
	float excAmp;
	uint32_t rnd;
	uint16_t excPos, excLen;
	uint8_t excNoise;
} ModalCore;

/* mode ratios of the models (first mode = the played pitch) */
static const float modalCore_ratios[MODAL_MODELS][MODAL_MODES] =
{
	{ 1.000f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f },		/* membrane (circular) */
	{ 1.000f, 2.756f, 5.404f, 8.933f, 13.345f, 18.638f },	/* free bar (marimba, xylophone) */
	{ 1.000f, 2.000f, 3.000f, 4.000f, 5.000f, 6.000f },		/* string / tube */
	{ 0.500f, 1.000f, 1.183f, 1.506f, 2.000f, 2.514f },		/* bell (hum, prime, tierce, quint, nominal) */
	{ 1.000f, 2.500f, 4.000f, 5.000f, 6.500f, 8.500f },		/* square plate */
	{ 1.000f, 2.710f, 5.150f, 8.170f, 11.700f, 15.700f },	/* glass / singing bowl */
};

/* damping, ratios and strike weights: on a parameter change */
static inline void modalCore_setParams(ModalCore* m, const ModalParams* p, const float fs)
{
	const uint8_t model = p->model < MODAL_MODELS ? p->model : MODAL_MODELS-1;
	const float t60 = 0.03f * powf(200.f, p->damp / 127.f);
	const float mat = p->mat * (2.f / 127.f);
	const float inh = p->inh / 127.f;
	const float B = inh * inh * 0.08f;
	const float pos = 0.02f + 0.48f * (p->pos / 127.f);
	for(uint8_t k=0;k<MODAL_MODES;k++)
	{
		const float r0 = modalCore_ratios[model][k];
		m->ratio[k] = r0 * sqrtf(1.f + B * (r0 * r0 - 1.f));
		const float t = t60 * powf(r0, -mat);
		m->R[k] = expf(-6.9078f / (t * fs));
		m->gain[k] = (0.05f + 0.95f * fabsf(sinf(MODAL_PI * (k + 1) * pos))) / (1.f + 0.3f * k);
	}
	m->f0 = -1.f;	/* recalculate the coefficients */
}

/* the coefficients for a pitch (on a pitch change) */
static inline void modalCore_setPitch(ModalCore* m, const float f0, const float fs)
{
	m->f0 = f0;
	for(uint8_t k=0;k<MODAL_MODES;k++)
	{
		const float w = 2.f * MODAL_PI * f0 * m->ratio[k] / fs;
		if(w >= 0.92f * MODAL_PI || w <= 0.f)
		{
			m->a1[k] = m->a2[k] = m->b[k] = 0.f;	/* above 20 kHz: silent */
			continue;
		}
		const float R = m->R[k];
		m->a1[k] = 2.f * R * cosf(w);
		m->a2[k] = -R * R;
		m->b[k] = m->gain[k] * sinf(w);		/* unit amplitude for a unit impulse */
	}
}

/* a strike: starts the exciter (the modes keep ringing) */
static inline void modalCore_strike(ModalCore* m, const ModalParams* p)
{
	const float h = 1.f - p->hard / 127.f;
	uint16_t len = (uint16_t)(2.f + h * h * 100.f);
	m->excNoise = p->noise != 0;
	if(m->excNoise)
	{
		len = (uint16_t)(len * 4u + 8u);
		m->excAmp = 2.5f / sqrtf((float)len);
	}
	else
		m->excAmp = 1.5f * (1.f + 1.5f * h) / (float)len;	/* pulse area 1, a soft one louder */
	m->excLen = len;
	m->excPos = 0;
	if(!m->rnd) m->rnd = 0x1234567u;
}

/* n samples; extra = the transient sample (int16) as an additional exciter */
static inline void modalCore_render(ModalCore* m, const int16_t* extra, float* out, const uint8_t n)
{
	for(uint8_t i=0;i<n;i++)
	{
		float x = extra[i] * (1.f / 65536.f);
		if(m->excPos < m->excLen)
		{
			const float t = (m->excPos + 0.5f) / m->excLen;
			float e = 4.f * t * (1.f - t) * m->excAmp;
			if(m->excNoise)
			{
				m->rnd = m->rnd * 1664525u + 1013904223u;
				e *= (int32_t)m->rnd * (1.f / 2147483648.f);
			}
			x += e;
			m->excPos++;
		}
		float s = 0.f;
		for(uint8_t k=0;k<MODAL_MODES;k++)
		{
			const float y = m->a1[k] * m->y1[k] + m->a2[k] * m->y2[k] + m->b[k] * x;
			m->y2[k] = m->y1[k];
			m->y1[k] = y;
			s += y;
		}
		out[i] = s;
	}
	/* no denormals while the modes fade out */
	for(uint8_t k=0;k<MODAL_MODES;k++)
	{
		if(fabsf(m->y1[k]) < 1e-12f && fabsf(m->y2[k]) < 1e-12f)
			m->y1[k] = m->y2[k] = 0.f;
	}
}

#endif /* MODALCORE_H_ */
