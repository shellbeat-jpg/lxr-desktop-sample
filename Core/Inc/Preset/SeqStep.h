/*
 * SeqStep.h
 *
 * Created: 26.06.2012 16:46:31
 *  Author: Julian
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */ 


#ifndef SEQSTEP_H_
#define SEQSTEP_H_
#include "main.h"

/** a copy of the step struct from the sequencer.h file from the cortex src
used to load step data from sequencer and write it to the pattern preset*/
typedef struct StepStruct
{
	//uint8_t  	active;		// bit 0: on/off
	uint8_t 	volume;		// 0-127 volume -> 0x7f => lower 7 bit, upper bit => active <- active c an probably be removed
	uint8_t  	prob;		//step probability 7 bit [0:100]
	uint8_t		note;		// 7 bit

	//parameter automation
	uint8_t 	param1Nr;
	uint8_t 	param1Val; //probably 7

	uint8_t 	param2Nr;
	uint8_t 	param2Val; //probably 7
}Step;

//MOD: random octave of a step (STEP_OCT_x) in the free upper bits of prob (bit 0),
//param1Val (bit 1) and param2Val (bit 2); these fields are 7 bit values, read them
//with STEP_PROB / STEP_VAL and set them with STEP_KEEP_OCT
enum { STEP_OCT_VOICE, STEP_OCT_OFF, STEP_OCT_PM1, STEP_OCT_PM2, STEP_OCT_PM3, STEP_OCT_UP1, STEP_OCT_DOWN1, STEP_OCT_NUM };
#define STEP_PROB(s)			((uint8_t)((s)->prob & 0x7fu))
#define STEP_VAL(v)				((uint8_t)((v) & 0x7fu))
#define STEP_KEEP_OCT(old, v)	((uint8_t)(((old) & 0x80u) | ((v) & 0x7fu)))
#define STEP_OCT(s)				((uint8_t)(((s)->prob >> 7) | (((s)->param1Val >> 7) << 1) | (((s)->param2Val >> 7) << 2)))
static inline void step_setOct(Step* s, const uint8_t oct)
{
	s->prob			= (uint8_t)((s->prob & 0x7fu) | ((oct & 1u) << 7));
	s->param1Val	= (uint8_t)((s->param1Val & 0x7fu) | (((oct >> 1) & 1u) << 7));
	s->param2Val	= (uint8_t)((s->param2Val & 0x7fu) | (((oct >> 2) & 1u) << 7));
}

typedef struct StepTiming
{
	uint8_t flam:4; // [0:5] => off, x2, x3, x4, x6, x8
	uint8_t shift:4; // [-7:7]
}StepTiming;


typedef enum StepPositionEnum
{
	STEP_PREV,
	STEP_CURRENT,
	STEP_NEXT
}StepPosition;

/*
typedef union
{
	uint32_t completeData;
	struct
	{
		uint8_t unused;
		uint8_t prevStep;
		uint8_t currentStep;
		uint8_t nextStep;
	}StepData;
}StepPattern;
*/





#endif /* SEQSTEP_H_ */
