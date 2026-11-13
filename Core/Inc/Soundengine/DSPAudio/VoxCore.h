/*
 * VoxCore.h
 *
 * MOD (XT): the sound of the vox engine, a physical model of a voice after the
 * idea of CHIMERA (github.com/jamesondh/chimera-vocal-synth, MIT), reduced for
 * the LXR: a source (vocal folds or a bird's syrinx) drives a tube of
 * sections (the vocal tract, Kelly-Lochbaum lattice at half the sample rate)
 * with a nasal branch. Vowel shapes (area functions) were fitted offline to
 * the formants of 8 vowels; VOW morphs between them, LEN scales the tract
 * (continuously: the lips sit between the last two sections).
 * Syrinx: the normal form of Gardner/Mindlin (labial oscillator).
 * No hardware dependencies: also built by the host test (tests/host/vox.c).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef VOXCORE_H_
#define VOXCORE_H_

#include <stdint.h>
#include <math.h>

#define VOX_VOWELS	8		/* u o ɔ a ɛ e i y */
#define VOX_REFN	22		/* sections of the fitted shapes (17.4 cm at 22 kHz) */
#define VOX_MINN	10
#define VOX_MAXN	36
#define VOX_NASN	12		/* nasal tube (at LEN = 22 sections) */
#define VOX_SYR		8

typedef struct
{
	uint8_t vow;	/* vowel 0..127: u o ɔ a ɛ e i y */
	uint8_t nas;	/* velum opening 0..127 */
	uint8_t len;	/* tract length 0..127 (10 .. 36 sections, 64 = 22 = adult) */
	uint8_t bre;	/* voice: breath noise; syrinx: drive (air sac pressure) */
	uint8_t tns;	/* voice: tension (soft/breathy .. pressed); syrinx: labial tension */
	uint8_t src;	/* 0 vocal folds, 1 syrinx */
	uint8_t cns;	/* consonant: noise burst at the front of the mouth on a trigger, 0 = off */
} VoxParams;

typedef struct
{
	float R[VOX_MAXN];		/* right-going waves (towards the lips), delayed */
	float k[VOX_MAXN];		/* reflection coefficients of the junctions */
	float A[VOX_MAXN];		/* areas */
	float nR[VOX_NASN];
	float nk[VOX_NASN];
	float nA0;				/* velum port area */
	float phase, f0;		/* source phase 0..1, pitch [Hz] */
	float oq, tilt, tiltZ, breath, gain;
	float sx, sy, sa, sb, sT, sLo, sHi, sPrev;	/* syrinx */
	float nzZ;				/* breath noise highpass */
	float out0, out1, dcX, dcY;
	float pn;				/* level by pitch: sqrt(f0 / tract rate) */
	float Nf, NfT;			/* tract length in sections (smoothed, target): the last section is partial */
	float lipF;				/* part of the last section that is used, 0..1 */
	float vowPos;			/* vowel position 0..VOX_VOWELS-1 */
	float sLoT, sHiT, sTT;	/* syrinx: targets (smoothed per tick: no clicks) */
	uint8_t shapeDirty;
	uint16_t bPos, bLen;	/* consonant burst */
	uint8_t cns;
	uint32_t rnd;
	uint8_t N, nv, src, odd, nasOn;
} VoxCore;

static const float voxCore_vowels[VOX_VOWELS][VOX_REFN] =
{
	{ 2.869f, 2.863f, 2.698f, 2.314f, 1.813f, 1.371f, 1.078f, 0.932f, 0.903f, 0.966f, 1.107f, 1.316f, 1.579f, 1.872f, 2.162f, 2.420f, 2.626f, 2.775f, 2.874f, 2.933f, 0.232f, 0.232f },	/* u */
	{ 2.667f, 2.364f, 1.947f, 1.484f, 1.075f, 0.785f, 0.624f, 0.578f, 0.638f, 0.814f, 1.120f, 1.541f, 2.003f, 2.409f, 2.696f, 2.863f, 2.946f, 2.981f, 2.994f, 2.998f, 0.355f, 0.355f },	/* o */
	{ 1.602f, 1.220f, 0.897f, 0.660f, 0.508f, 0.427f, 0.406f, 0.440f, 0.535f, 0.706f, 0.963f, 1.302f, 1.691f, 2.076f, 2.404f, 2.649f, 2.810f, 2.905f, 2.956f, 2.981f, 0.614f, 0.614f },	/* O */
	{ 0.824f, 0.677f, 0.554f, 0.444f, 0.353f, 0.294f, 0.272f, 0.289f, 0.350f, 0.462f, 0.638f, 0.883f, 1.191f, 1.538f, 1.889f, 2.207f, 2.469f, 2.665f, 2.800f, 2.887f, 1.500f, 1.500f },	/* a */
	{ 1.977f, 1.584f, 1.324f, 1.243f, 1.331f, 1.531f, 1.723f, 1.775f, 1.656f, 1.451f, 1.262f, 1.146f, 1.126f, 1.204f, 1.379f, 1.640f, 1.955f, 2.274f, 2.547f, 2.746f, 7.963f, 7.963f },	/* E */
	{ 2.963f, 2.944f, 2.928f, 2.919f, 2.912f, 2.883f, 2.785f, 2.562f, 2.185f, 1.707f, 1.249f, 0.918f, 0.747f, 0.725f, 0.851f, 1.135f, 1.564f, 2.052f, 2.475f, 2.757f, 7.994f, 7.994f },	/* e */
	{ 2.937f, 2.901f, 2.865f, 2.828f, 2.770f, 2.646f, 2.401f, 2.012f, 1.524f, 1.047f, 0.678f, 0.445f, 0.327f, 0.290f, 0.323f, 0.436f, 0.660f, 1.023f, 1.499f, 2.001f, 7.679f, 7.679f },	/* i */
	{ 2.998f, 2.998f, 2.997f, 2.996f, 2.993f, 2.986f, 2.962f, 2.901f, 2.770f, 2.529f, 2.165f, 1.717f, 1.279f, 0.936f, 0.725f, 0.637f, 0.663f, 0.806f, 1.079f, 1.473f, 0.483f, 0.483f },	/* y */
};
static const float voxCore_syrT[VOX_SYR][VOX_SYR] =
{
	{ 15.41f, 13.40f, 10.07f, 8.56f, 7.64f, 6.99f, 6.49f, 6.10f },
	{ 10.80f, 9.12f, 8.04f, 7.30f, 6.75f, 6.33f, 5.98f, 5.69f },
	{ 8.51f, 7.66f, 7.03f, 6.55f, 6.17f, 5.86f, 5.59f, 5.37f },
	{ 7.36f, 6.81f, 6.37f, 6.02f, 5.73f, 5.49f, 5.28f, 5.10f },
	{ 6.62f, 6.21f, 5.88f, 5.62f, 5.39f, 5.19f, 5.02f, 4.87f },
	{ 6.07f, 5.76f, 5.50f, 5.29f, 5.11f, 4.94f, 4.80f, 4.67f },
	{ 5.64f, 5.40f, 5.19f, 5.02f, 4.86f, 4.73f, 4.61f, 4.49f },
	{ 5.30f, 5.10f, 4.93f, 4.78f, 4.65f, 4.54f, 4.43f, 4.34f },
};
static const float voxCore_syrLo[VOX_SYR][VOX_SYR] =
{
	{ -0.686f, -0.686f, -0.820f, -0.922f, -1.006f, -1.078f, -1.140f, -1.195f },
	{ -0.462f, -0.653f, -0.785f, -0.888f, -0.972f, -1.044f, -1.106f, -1.162f },
	{ -0.441f, -0.625f, -0.755f, -0.856f, -0.940f, -1.012f, -1.075f, -1.131f },
	{ -0.422f, -0.600f, -0.727f, -0.827f, -0.911f, -0.983f, -1.045f, -1.101f },
	{ -0.404f, -0.577f, -0.702f, -0.800f, -0.883f, -0.955f, -1.017f, -1.073f },
	{ -0.388f, -0.556f, -0.678f, -0.776f, -0.857f, -0.928f, -0.991f, -1.047f },
	{ -0.374f, -0.537f, -0.656f, -0.752f, -0.833f, -0.903f, -0.966f, -1.021f },
	{ -0.361f, -0.520f, -0.636f, -0.731f, -0.811f, -0.880f, -0.942f, -0.997f },
};
static const float voxCore_syrHi[VOX_SYR][VOX_SYR] =
{
	{ 0.645f, 0.645f, 0.687f, 0.712f, 0.725f, 0.729f, 0.727f, 0.719f },
	{ 0.442f, 0.568f, 0.633f, 0.671f, 0.692f, 0.704f, 0.708f, 0.706f },
	{ 0.401f, 0.528f, 0.597f, 0.640f, 0.667f, 0.683f, 0.692f, 0.695f },
	{ 0.376f, 0.499f, 0.570f, 0.616f, 0.646f, 0.666f, 0.678f, 0.684f },
	{ 0.357f, 0.476f, 0.548f, 0.595f, 0.628f, 0.650f, 0.665f, 0.673f },
	{ 0.341f, 0.458f, 0.529f, 0.578f, 0.612f, 0.636f, 0.652f, 0.663f },
	{ 0.328f, 0.442f, 0.513f, 0.562f, 0.597f, 0.622f, 0.641f, 0.654f },
	{ 0.316f, 0.428f, 0.498f, 0.547f, 0.583f, 0.610f, 0.630f, 0.644f },
};

#define VOX_FST(fs)		((fs) * 0.5f)	/* tract rate */
#define VOX_GLOT_GAIN	1.f
#define VOX_SYR_GAIN	0.8f
#define VOX_CNS_GAIN	6.f
#define VOX_BREATH_GAIN	2.f

static inline float voxCore_bilin(const float t[VOX_SYR][VOX_SYR], const float b, const float a)
{
	const uint8_t bi = b >= VOX_SYR-1 ? VOX_SYR-2 : (uint8_t)b, ai = a >= VOX_SYR-1 ? VOX_SYR-2 : (uint8_t)a;
	const float fb = b - bi, fa = a - ai;
	return (t[bi][ai] * (1.f - fa) + t[bi][ai+1] * fa) * (1.f - fb) + (t[bi+1][ai] * (1.f - fa) + t[bi+1][ai+1] * fa) * fb;
}

/* the shape: vowel morph and length (control rate) */
static inline void voxCore_shape(VoxCore* v)
{
	uint8_t N = (uint8_t)ceilf(v->Nf - 1e-4f);
	if(N < VOX_MINN) N = VOX_MINN;
	if(N > VOX_MAXN) N = VOX_MAXN;
	for(uint8_t i=v->N;i<N;i++) v->R[i] = 0.f;
	v->N = N;
	v->lipF = v->Nf - (N - 1);
	if(v->lipF < 0.f) v->lipF = 0.f;
	if(v->lipF > 1.f) v->lipF = 1.f;
	const float pos = v->vowPos;
	const uint8_t a = pos >= VOX_VOWELS-1 ? VOX_VOWELS-2 : (uint8_t)pos;
	const float fv = pos - a;
	const float sc = (float)VOX_REFN / v->Nf;
	for(uint8_t i=0;i<N;i++)
	{
		float x = (i + 0.5f) * sc - 0.5f;
		if(x < 0.f) x = 0.f;
		uint8_t j = (uint8_t)x;
		if(j > VOX_REFN-2) j = VOX_REFN-2;
		float f = x - j; if(f > 1.f) f = 1.f;
		const float A0 = voxCore_vowels[a][j] * (1.f - f) + voxCore_vowels[a][j+1] * f;
		const float A1 = voxCore_vowels[a+1][j] * (1.f - f) + voxCore_vowels[a+1][j+1] * f;
		v->A[i] = A0 * (1.f - fv) + A1 * fv;
	}
	/* lips: the last section has the fitted lip area; the one before becomes the lips while
	 * the end moves towards it (lipF -> 0), so a new section starts where the lips were */
	const float lip = voxCore_vowels[a][VOX_REFN-1] * (1.f - fv) + voxCore_vowels[a+1][VOX_REFN-1] * fv;
	v->A[N-1] = lip;
	v->A[N-2] += (lip - v->A[N-2]) * (1.f - v->lipF);
	v->k[0] = 0.f;
	for(uint8_t i=1;i<N;i++)
		v->k[i] = (v->A[i-1] - v->A[i]) / (v->A[i-1] + v->A[i]);
	v->nv = (uint8_t)(v->Nf * 0.5f + 0.5f);
}

/* parameters (control rate); length and syrinx settle smoothly */
static inline void voxCore_setParams(VoxCore* v, const VoxParams* p)
{
	v->NfT = p->len < 64 ? VOX_MINN + (VOX_REFN - VOX_MINN) * (p->len / 64.f)
		: VOX_REFN + (VOX_MAXN - VOX_REFN) * ((p->len - 64) / 63.f);
	if(v->Nf < VOX_MINN) { v->Nf = v->NfT; v->N = 0; }	/* first call */
	const float vp = p->vow * ((VOX_VOWELS - 1) / 127.f);
	if(vp != v->vowPos || !v->N) { v->vowPos = vp; v->shapeDirty = 1; }
	/* velum area: measured so that each step of NAS changes the spectrum about equally */
	{
		static const float q[9] = { 0.f, 0.02f, 0.05f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.f };
		static const float ar[9] = { 0.f, 0.008f, 0.0196f, 0.04f, 0.108f, 0.294f, 0.736f, 1.374f, 2.f };
		const float n = p->nas * (1.f / 127.f);
		uint8_t i = 1;
		while(i < 8 && n > q[i]) i++;
		v->nA0 = ar[i-1] + (ar[i] - ar[i-1]) * (n - q[i-1]) / (q[i] - q[i-1]);
	}
	if(p->nas && !v->nasOn)
		for(uint8_t i=0;i<VOX_NASN;i++) v->nR[i] = 0.f;
	v->nasOn = p->nas != 0;
	{
		/* nasal tube: velum port, cavity, nostrils */
		static const float nasA[VOX_NASN] = { 0.f, 1.10f, 1.70f, 2.20f, 2.55f, 2.70f, 2.60f, 2.30f, 1.90f, 1.45f, 1.05f, 0.75f };
		float prev = v->nA0 > 0.001f ? v->nA0 : 0.001f;
		v->nk[0] = 0.f;
		for(uint8_t i=1;i<VOX_NASN;i++)
		{
			v->nk[i] = (prev - nasA[i]) / (prev + nasA[i]);
			prev = nasA[i];
		}
	}
	/* source */
	v->src = p->src != 0;
	v->cns = p->cns;
	const float t = p->tns / 127.f;
	v->oq = 0.85f - 0.5f * t;					/* open quotient */
	v->tilt = 0.75f - 0.65f * t;				/* lowpass of the source: soft = dark */
	const float b = p->bre * (1.f / 127.f);
	v->breath = VOX_BREATH_GAIN * b * b;
	v->gain = (1.f - 0.7f * b * b) / (0.3f + 0.7f * v->oq);	/* much breath: less voicing (whisper) */
	/* syrinx: drive -> alpha, tension -> beta (table indices; alpha from the 2nd column: always oscillating) */
	v->sa = 1.f + p->bre * ((VOX_SYR - 2) / 127.f);
	v->sb = p->tns * ((VOX_SYR - 1) / 127.f);
	v->sTT = voxCore_bilin(voxCore_syrT, v->sb, v->sa);
	v->sLoT = voxCore_bilin(voxCore_syrLo, v->sb, v->sa);
	v->sHiT = voxCore_bilin(voxCore_syrHi, v->sb, v->sa);
	if(v->sT == 0.f) { v->sT = v->sTT; v->sLo = v->sLoT; v->sHi = v->sHiT; }
}

static inline void voxCore_setPitch(VoxCore* v, const float f0, const float fs)
{
	v->f0 = f0;
	const float r = f0 / VOX_FST(fs);
	v->pn = sqrtf(r > 1e-4f ? r : 1e-4f);
}

/* a trigger: restarts the source (the tract keeps ringing) */
static inline void voxCore_trigger(VoxCore* v)
{
	v->phase = 0.f;
	v->bPos = 0;
	v->bLen = v->cns ? (uint16_t)(400.f + 1600.f * v->cns * v->cns * (1.f / 16129.f)) : 0;	/* 18 .. 90 ms */
	if(!v->rnd) v->rnd = 0x2545F491u;
	if(!(v->sx == v->sx) || v->sx == 0.f) { v->sx = 0.2f; v->sy = 0.f; }
	{
		float u = (v->sx - v->sLo) / (v->sHi - v->sLo + 1e-3f);
		u = u < 0.f ? 0.f : (u > 1.f ? 1.f : u);
		v->sPrev = u * u;	/* no step at the start */
	}
}

static inline void voxCore_syrRhs(const float x, const float y, const float a, const float b, float* dx, float* dy)
{
	*dx = y;
	*dy = -a - b * x - x * x * x - x * x * y + x * x - x * y;
}

/* the source at the tract rate: flow derivative into the tract */
static inline float voxCore_source(VoxCore* v, const float fst)
{
	v->rnd = v->rnd * 1664525u + 1013904223u;
	const float noise = (int32_t)v->rnd * (1.f / 2147483648.f);
	if(v->src)
	{
		/* settings glide (a step of lo/hi would click through the derivative) */
		v->sT += 0.01f * (v->sTT - v->sT);
		v->sLo += 0.01f * (v->sLoT - v->sLo);
		v->sHi += 0.01f * (v->sHiT - v->sHi);
		/* alpha 0.07..0.6, beta 0.1..1.3; tau advances by T * f0 / fs per sample */
		const float a = 0.07f + 0.0757f * v->sa, b = 0.1f + 0.1714f * v->sb;
		float dtau = v->sT * v->f0 / fst;
		uint8_t n = (uint8_t)(dtau * 4.f) + 1;		/* steps of at most 0.25 */
		if(n > 4) n = 4;
		const float h = dtau / n;
		float x = v->sx, y = v->sy + 1e-4f * noise;	/* a little noise: leaves a resting point */
		for(uint8_t i=0;i<n;i++)
		{
			float k1x, k1y, k2x, k2y, k3x, k3y, k4x, k4y;
			voxCore_syrRhs(x, y, a, b, &k1x, &k1y);
			voxCore_syrRhs(x + 0.5f * h * k1x, y + 0.5f * h * k1y, a, b, &k2x, &k2y);
			voxCore_syrRhs(x + 0.5f * h * k2x, y + 0.5f * h * k2y, a, b, &k3x, &k3y);
			voxCore_syrRhs(x + h * k3x, y + h * k3y, a, b, &k4x, &k4y);
			x += (h / 6.f) * (k1x + 2.f * k2x + 2.f * k3x + k4x);
			y += (h / 6.f) * (k1y + 2.f * k2y + 2.f * k3y + k4y);
		}
		if(!(x == x) || fabsf(x) > 10.f) { x = 0.2f; y = 0.f; }
		v->sx = x; v->sy = y;
		float u = (x - v->sLo) / (v->sHi - v->sLo + 1e-3f);
		u = u < 0.f ? 0.f : (u > 1.f ? 1.f : u);
		u *= u;
		float pn2 = v->pn * v->pn;
		if(pn2 < 0.005f) pn2 = 0.005f;
		const float d = (u - v->sPrev) * VOX_SYR_GAIN / pn2;
		v->sPrev = u;
		return d;
	}
	/* vocal folds: KLGLOTT88-like flow pulse u = 27/4 phi^2 (1 - phi) in the open phase */
	v->phase += v->f0 / fst;
	if(v->phase >= 1.f) v->phase -= (float)(int)v->phase;
	float d = 0.f, u = 0.f;
	if(v->phase < v->oq)
	{
		const float ph = v->phase / v->oq;
		d = 6.75f * ph * (2.f - 3.f * ph) * VOX_GLOT_GAIN;
		u = 6.75f * ph * ph * (1.f - ph);
	}
	v->tiltZ += (1.f - v->tilt) * (d - v->tiltZ);
	/* breath: aspiration noise, modulated by the flow (not chopped: half of it always there),
	 * highpassed (no rumble) */
	const float hn = noise - v->nzZ;
	v->nzZ += 0.3f * (noise - v->nzZ);
	return v->tiltZ * v->gain + v->breath * hn * (0.5f + 0.5f * u);
}

/* one tick of the tract (at the tract rate): returns the radiated sound.
 * Waves are volume velocities (Kelly-Lochbaum); right-going delayed, left-going not */
static inline float voxCore_tick(VoxCore* v, const float src)
{
	const float rg = 0.75f, rl = -0.85f, rn = -0.85f, loss = 0.996f;
	const uint8_t N = v->N, nv = v->nv;
	float* R = v->R;
	const float* k = v->k;
	/* lips: the end is between the last two sections (smooth length) */
	const float Rl = R[N-2] + v->lipF * (R[N-1] - R[N-2]);
	float out = Rl * (1.f + rl);
	float l = rl * Rl;
	float nl = 0.f;
	if(v->nasOn)
	{
		float* nR = v->nR;
		out += nR[VOX_NASN-1] * (1.f + rn);
		nl = rn * nR[VOX_NASN-1];
		for(uint8_t j=VOX_NASN-1;j>0;j--)
		{
			const float w = v->nk[j] * (nR[j-1] + nl);
			nR[j] = (nR[j-1] - w) * loss;
			nl = (nl + w) * loss;
		}
	}
	for(uint8_t j=N-1;j>0;j--)
	{
		if(j == nv && v->nasOn)
		{
			/* three-port junction (volume velocities): pharynx side (j-1), mouth side (j), velum */
			const float Al = v->A[j-1], Ar = v->A[j], An = v->nA0;
			const float S = 2.f * (R[j-1] + l + nl) / (Al + Ar + An);
			const float rOut = Ar * S - l;
			v->nR[0] = (An * S - nl) * loss;
			l = (Al * S - R[j-1]) * loss;
			R[j] = rOut * loss;
			continue;
		}
		const float w = k[j] * (R[j-1] + l);
		R[j] = (R[j-1] - w) * loss;
		l = (l + w) * loss;
	}
	R[0] = src + rg * l;
	return out;
}

/* n samples at fs (the tract runs at fs/2, linearly interpolated);
 * extra = the transient sample (int16, may be 0) as an additional source */
static inline void voxCore_render(VoxCore* v, const int16_t* extra, float* out, const uint8_t n, const float fs)
{
	const float fst = VOX_FST(fs);
	/* length glides (a few ms), the shape follows per block */
	if(v->Nf != v->NfT)
	{
		const float d = v->NfT - v->Nf;
		v->Nf = fabsf(d) < 0.01f ? v->NfT : v->Nf + 0.25f * d;
		v->shapeDirty = 1;
	}
	if(v->shapeDirty)
	{
		v->shapeDirty = 0;
		voxCore_shape(v);
	}
	for(uint8_t i=0;i<n;i++)
	{
		float y;
		if(!v->odd)
		{
			v->out0 = v->out1;
			float s = voxCore_source(v, fst);
			if(extra) s += (extra[i] + (i+1 < n ? extra[i+1] : 0)) * (1.f / 32768.f);
			if(v->bPos < v->bLen)
			{
				/* consonant: the source starts in the second half of the burst */
				const float e = 1.f - v->bPos / (float)v->bLen;
				if(2u * v->bPos < v->bLen) s = 0.f;
				v->rnd = v->rnd * 1664525u + 1013904223u;
				v->R[(3u * v->N) / 4u] += (int32_t)v->rnd * (1.f / 2147483648.f) * e * e * e * VOX_CNS_GAIN;
				v->bPos++;
			}
			v->out1 = voxCore_tick(v, s);
			y = 0.5f * (v->out0 + v->out1);
		}
		else
			y = v->out1;
		v->odd ^= 1;
		/* DC blocker */
		const float o = y - v->dcX + 0.995f * v->dcY;
		v->dcX = y; v->dcY = o;
		out[i] = o;
	}
	for(uint8_t i=0;i<v->N;i++)
		if(fabsf(v->R[i]) < 1e-15f) v->R[i] = 0.f;
}

#endif /* VOXCORE_H_ */
