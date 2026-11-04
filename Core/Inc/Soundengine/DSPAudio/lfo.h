/*
 * lfo.h
 *
 *  Created on: 14.01.2013
 *
 *      Non bandlimited low freq. oscillator
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */


#ifndef LFO_H_
#define LFO_H_
//-------------------------------------------------------------
#include "stm32f7xx_hal.h"
#include "Soundengine/DSPAudio/Oscillator.h"
#include "Soundengine/DSPAudio/wavetable.h"	//sine
#include "Soundengine/config.h"
#include "Soundengine/Sequencer/sequencer.h"
#include "Soundengine/DSPAudio/modulationNode.h"
//-------------------------------------------------------------
enum
{
	LFO_SINE,
	LFO_TRI,
	LFO_SAW_UP,
	LFO_SAW_DOWN,
	LFO_REC,
	LFO_NOISE,
	LFO_EXP_UP,
	LFO_EXP_DOWN,
	LFO_CYCLE,		//MOD: steps through mod+1 values, one step per trigger
	LFO_EXP_TRI,	//MOD: exponential rise, then exponential fall
	//MOD: one-shot versions: run one cycle after a trigger (of the rtg voice,
	//without rtg of the LFO's own voice), then hold; the offset is a start delay
	LFO_SINE_1,
	LFO_TRI_1,
	LFO_SAW_UP_1,
	LFO_SAW_DOWN_1,
	LFO_REC_1,		//inverted: on first, then off; during the delay off
	LFO_NOISE_1,	//one random value per trigger
	LFO_EXP_UP_1,
	LFO_EXP_DOWN_1,
	LFO_EXP_TRI_1,
	//MOD: the user waves W1..W8 (/SAMPLES/WAVES) as LFO shapes, running and one-shot
	LFO_USER,
	LFO_USER_1 = LFO_USER + 8,
	NUM_LFO_WAVEFORMS = LFO_USER_1 + 8
};
//MOD: one-shot waveforms (run one cycle per trigger, then hold)
#define lfo_isOneShot(w)	(((w) >= LFO_SINE_1 && (w) < LFO_USER) || (w) >= LFO_USER_1)


#define LFO_MAX_F 		200 //[Hz]
#define LFO_SR 			(REAL_FS/(float)OUTPUT_DMA_SIZE)
//-------------------------------------------------------------
typedef struct LfoStruct
{
	uint32_t 	phase;		// the current phase of the LFO (upper bits cnt 0-255)
	uint32_t 	phaseInc;	// the phase increment controls the LFO frequency
	uint8_t 	waveform;	// selects the waveform
	uint8_t 	retrigger;	// defines the voice nr that retriggers the LFO (0=no retrigger)
	uint32_t 	phaseOffset;// the phase value to which the LFO is retriggered
	float 		rnd;
	uint8_t 	sync;
	float 		freq;
	ModulationNode modTarget;
	float		modNodeValue;
	uint32_t	cycleCount;	//MOD: triggers counted by LFO_CYCLE
	uint32_t	osDelay;	//MOD: one-shot: remaining start delay (phase units)
	uint8_t		osDone;		//MOD: one-shot: the cycle has ended
	uint8_t		starts;		//MOD: counts the starts of the waveform (LFO -> track)
	uint8_t		startCause;	//MOD: voice slot whose trigger caused the last start (0xff = the LFO itself)
} Lfo;
//-------------------------------------------------------------
void lfo_init(Lfo *lfo);
void lfo_dispatchNextValue(Lfo* lfo);
float lfo_calc(Lfo *lfo);	//MOD: the next value (without dispatching it)
void lfo_setFreq(Lfo *lfo, float f);
void lfo_setSync(Lfo* lfo, uint8_t sync);
void lfo_recalcSync();
void lfo_retrigger(uint8_t voiceNr);
//MOD: number of waveform starts of the LFO of a voice slot (wraps) and the voice
//slot whose trigger caused the last one (0xff: the LFO's own cycle)
uint8_t lfo_getStarts(uint8_t slot, uint8_t* cause);

//MOD: extra LFOs: 0..5 = the 2nd LFO of voice 1..6, LFO_T7 = the LFO of track 7. Their
//settings are kit data (KitEngines.h, KIT_LFOX_OFFSET), taken over when they change.
//An LFO without a target is off. The track 7 LFO acts while slot 6 plays a trigger of
//track 7, instead of the LFO of voice 6 (which acts again with the next trigger of track 6).
#define LFO_NUM_EXTRA	7
#define LFO_T7			6
extern Lfo lfo_extra[LFO_NUM_EXTRA];
void lfo_extraCalc(void);			//before modNode_resetTargets: settings, next values
void lfo_extraDispatch(void);		//after it: the values to their targets
void lfo_extraResetTargets(void);	//the targets back to their own values
uint8_t lfo_t7Active(void);			//the track 7 LFO acts now (instead of the LFO of voice 6)
void lfo_extraRetrigger(const uint8_t slot, const uint8_t second);	//slot_trigger (second = track 7)

#endif /* LFO_H_ */
