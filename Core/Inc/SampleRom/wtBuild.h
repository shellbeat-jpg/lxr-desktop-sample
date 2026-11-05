/*
 * wtBuild.h
 *
 * MOD (OE02-Samples+): builds the frames of the wavetable engine at the sample
 * import. A source cycle (any length) is analysed into its harmonics (1..127),
 * turned so that its fundamental starts at phase 0 (all frames of a set line up),
 * and rendered as one frame: 256 points with 127 harmonics plus band limited
 * versions with 63 (128 points), 31, 15, 7 and 3 harmonics (64 points each, so
 * that the linear interpolation of the short ones adds little aliasing); the
 * engine picks them by pitch. Frames between two source cycles are made by
 * interpolating the harmonics' levels and phases ("real" in-between waves).
 * Pure math, no hardware: also used by the host tests.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef WTBUILD_H_
#define WTBUILD_H_

#include <stdint.h>

#define WT_POINTS		256u	/* points of the full frame */
#define WT_LEVELS		6u		/* 127, 63, 31, 15, 7, 3 harmonics */
#define WT_FRAME_SIZE	640u	/* int16 per frame, all levels (1280 bytes) */
#define WT_HARM			127u	/* harmonics of the full frame */
#define WT_MAX_FRAMES	64u		/* frames per set incl. the in-between ones */
#define WT_MAX_INTER	7u		/* in-between frames per pair of source cycles */

/* offset (int16), length (2^bits) and harmonics of a level in a frame */
#define WT_LEVEL_BITS(l)	((uint8_t)((l) < 2u ? 8u - (l) : 6u))
#define WT_LEVEL_LEN(l)		((uint16_t)(1u << WT_LEVEL_BITS(l)))
#define WT_LEVEL_OFS(l)		((uint16_t)((l) < 2u ? 512u - (512u >> (l)) : 384u + 64u * ((l) - 2u)))
#define WT_LEVEL_HARM(l)	((uint16_t)((128u >> (l)) - 1u))

typedef struct
{
	float mag[WT_HARM];
	float ph[WT_HARM];	/* phase of sin(h*x + ph) */
} WtSpectrum;

/* working memory of the analysis (the import borrows the recorder's buffer) */
typedef struct
{
	float cs[WT_HARM], sn[WT_HARM], c[WT_HARM], s[WT_HARM], re[WT_HARM], im[WT_HARM];
	float tmp[WT_POINTS];
} WtWork;
void wtBuild_setWork(WtWork* w);

/* analysing one cycle of n points: begin, n times add, end */
void wtBuild_begin(uint32_t n);
void wtBuild_add(float x);
void wtBuild_end(WtSpectrum* out);

/* in-between spectrum, t = 0 (a) .. 1 (b) */
void wtBuild_mix(const WtSpectrum* a, const WtSpectrum* b, float t, WtSpectrum* out);

/* the frame (all levels), normalized to a peak of about -0.8 dB */
void wtBuild_render(const WtSpectrum* s, int16_t* frame);

/* frames of a set from n source cycles: source cycles used, in-between frames
 * per pair, frames in total. More than WT_MAX_FRAMES cycles: evenly picked. */
void wtBuild_plan(uint32_t n, uint8_t maxInter, uint32_t* used, uint8_t* inter, uint32_t* total);
/* the source cycle of used cycle i */
uint32_t wtBuild_source(uint32_t n, uint32_t used, uint32_t i);

#endif /* WTBUILD_H_ */
