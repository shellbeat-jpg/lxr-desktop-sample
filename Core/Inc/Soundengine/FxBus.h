/*
 * FxBus.h
 *
 *  Created on: 26.08.2020
 *      Author: julian
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */

#ifndef INC_SOUNDENGINE_FXBUS_H_
#define INC_SOUNDENGINE_FXBUS_H_

#include "Soundengine/globals.h"
#include "Soundengine/config.h"
#include "Soundengine/DSPAudio/FX/Distortion.h"
#include "Soundengine/DSPAudio/FX/Ringmod.h"
#include "Soundengine/DSPAudio/FX/Compressor.h"
#include "Soundengine/DSPAudio/FX/Delay.h"
#include "Soundengine/DSPAudio/FX/Reverb.h"

#define USE_REVERB 0

enum FxTypeEnum
{
	FX_OFF,
	FX_DISTORTION,
	FX_RING,
	FX_COMPRESSOR,
	FX_DELAY,
#if USE_REVERB
	FX_REVERB
#endif
};

typedef struct FxStruct
{

	uint8_t fxType;
	uint8_t audioRouting;
	float dryWet;

	float parameter[4];
	uint8_t sync;		//MOD: delay tempo sync (0 = off, FXBUS_NUM_SYNC values)


	MasterDistortion distortion;
	Ringmod ringmod;
	Compressor compressor;
	Delay delay;



#if USE_REVERB
	Reverb reverb;
#endif

} FxBus;

extern INDTCMZ FxBus fxBus;

//MOD: second FX slot and the order of the slots
enum FxBusModeEnum
{
	FXBUS_MODE_A_B,		//A then B
	FXBUS_MODE_B_A,		//B then A
	FXBUS_MODE_PARALLEL,	//side by side
	FXBUS_NUM_MODES
};
#define FXBUS_NUM_SYNC	13	//off, then the delay time from long to short (FxBus.c)
extern FxBus fxBusB;
extern uint8_t fxBus_mode;
void FxBus_setParameterB(uint8_t idx, float value);
void FxBus_updateSlot(FxBus* b);
void FxBus_setSync(FxBus* b, const uint8_t sync);
extern float fxBus_inputGain[2];	//MOD: input gain of slot A / B

void FxBus_init();
void FxBus_process(int16_t *fxBuffer, int16_t *fxBufferB, int32_t* outL,int32_t* outR,int32_t* outL2, int32_t* outR2, uint32_t size);
void FxBus_setParameter(uint8_t idx, float value);
void FxBus_updateParameter();
//to prevent NaN pollution from delay
void FxBus_prepareFxChange();


#endif /* INC_SOUNDENGINE_FXBUS_H_ */
