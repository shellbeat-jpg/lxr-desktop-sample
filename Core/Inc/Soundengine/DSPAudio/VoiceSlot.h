/*
 * VoiceSlot.h
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * The 6 voice slots. Each slot holds one voice of any engine (drum, snare,
 * cymbal, hi-hat) in a union; the fields all engines share (VoiceCommon) are
 * at the same place whatever engine the slot runs. Tracks 1-6 trigger slots
 * 1-6, track 7 is the second trigger of slot 6 with its own amp decay.
 */

#ifndef VOICESLOT_H_
#define VOICESLOT_H_

#include "Soundengine/DSPAudio/VoiceCommon.h"
#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/DSPAudio/HiHat.h"
#include "Parameters.h"

#define NUM_VOICE_SLOTS	6
#define NUM_SLOT_ROLES	7

//engine numbers are stored in kits and sent via MIDI: never change them
enum VoiceEngine
{
	ENGINE_DRUM		= 0,
	ENGINE_SNARE	= 1,
	ENGINE_CYMBAL	= 2,
	ENGINE_HIHAT	= 3,
	NUM_ENGINES
};

typedef struct
{
	union
	{
		VoiceCommon	c;		//the fields of every engine
		DrumVoice	drum;
		SnareVoice	snare;
		CymbalVoice	cymbal;
		HiHatVoice	hat;
	} u;
	uint8_t		engine;		//enum VoiceEngine
	float		decay[2];	//slot 6 only: amp decay of track 6 [0] and track 7 [1]
} VoiceSlot;

extern VoiceSlot voiceSlots[NUM_VOICE_SLOTS];

//The engine specific settings of a slot are stored in 7 parameters per slot,
//indexed by role: analogous controls of the engines share a role, so a knob or
//MIDI CC keeps a similar meaning when the engine changes. For the stock engine
//of a slot these are exactly its stock parameters. This table is part of the
//kit format: never change it.
extern const uint8_t slot_rolePar[NUM_VOICE_SLOTS][NUM_SLOT_ROLES];

//write-only command (NRPN 98, not stored in kits): value = slot*4 + engine
#define PAR_SLOT_ENGINE	PAR_UNUSED_6

//the engine every slot has in the stock firmware (and in kits without engine data)
uint8_t slot_stockEngine(const uint8_t slot);

//initialize all slots with their stock engines
void slot_init(void);

//change the engine of a slot. The voice is re-initialized (LFO and fader
//smoothing are kept); its sound parameters have to be sent again afterwards.
void slot_setEngine(const uint8_t slot, const uint8_t engine);

//MOD: choke group (PAR_CHOKE_GROUP): bit n = track n. A triggered track of the
//group fades out the slots of the other tracks of the group (~2 ms) and keeps
//them silent until they are triggered again.
extern uint8_t slot_chokeGroup;
void slot_applyChoke(const uint8_t slot, int16_t* buf, const uint8_t size);
//MOD: fade out (~2 ms) every slot that plays a loop sample, until its next trigger;
//on sequencer stop (with the amp decay at 127 a loop never ends by itself)
void slot_stopLoops(void);
//MOD: 1 while slot 6 plays a trigger of track 7 (the mixer takes its sends then)
extern uint8_t slot_t7Playing;
//MOD: cue: slot_cueOnly[slot] = 1 while the slot plays a trigger of a muted, cued track
//(heard on the cue only); the sequencer sets slot_triggerCueOnly around such a trigger
extern uint8_t slot_cueOnly[NUM_VOICE_SLOTS];
extern uint8_t slot_triggerCueOnly;
void slot_clearCueOnly(const uint8_t track);	//the track was unmuted (7: all)

//trigger track 0-6 (track 6 = 2nd trigger of slot 5)
void slot_trigger(const uint8_t track, const uint8_t vel, const uint8_t note);

//if par is one of the engine specific parameters (slot_rolePar), apply it to
//the engine the slot runs and return 1; otherwise return 0
uint8_t slot_setRoleParameter(const uint16_t par, const uint8_t itg, const float flt, const uint8_t isFloatModulator);

#endif /* VOICESLOT_H_ */
