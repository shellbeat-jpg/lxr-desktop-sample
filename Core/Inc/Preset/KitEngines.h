/*
 * KitEngines.h
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * The voice engine of each slot in the active kit (UI side). Changing an engine
 * follows the same rules wherever it comes from (MIDI now, the menu later):
 * all sound parameters of the slot (incl. volume, pan, output, LFO) start from
 * the new engine's defaults, modulation targets on its engine specific
 * parameters are turned off, and the slot's parameters are sent to the sound
 * engine again.
 */

#ifndef KITENGINES_H_
#define KITENGINES_H_

#include "stm32f7xx_hal.h"
#include "Soundengine/DSPAudio/VoiceSlot.h"

//Kit files (.snd, also in projects and the tmp folder) and tmp.snd carry the
//engines in a block appended after the 247 parameter bytes:
//	"OE02", payload size, payload: the engine of each slot (6 bytes)
//Later versions may only append to the payload. Files without the block (from
//the stock firmware or anywhere else) load with the stock engines, and the
//stock firmware ignores the block.
#define KIT_ENGINES_BLOCK_SIZE	(5 + NUM_VOICE_SLOTS)
//MOD: per kit data after the engines: the send levels of each slot into FX slot A
//and B (0..127, independent of the voice output) and the order of the FX slots.
//Arrays of kit data (kitEngines, kitEngines_ram, ...) hold KIT_DATA_SIZE bytes:
//[0..5] engines, [6..11] sends A, [12] order, [13..18] sends B. In the file the
//block continues after the engines with tagged parts (3 letters, count, data):
//"SNA" sends A, "ORD" order, "SNB" sends B. Unknown parts are skipped (the
//"SND" of the 1.2.8 test versions: a level for the former "1+A" outputs).
#define KIT_SEND_OFFSET			NUM_VOICE_SLOTS
#define KIT_FXMODE_OFFSET		(2*NUM_VOICE_SLOTS)		//order of the FX slots (FXBUS_MODE_x)
#define KIT_SENDB_OFFSET		(2*NUM_VOICE_SLOTS + 1)
//MOD: FX3 (master bus of St1, MasterBus.h): 9 settings, part "FX3"
#define KIT_FX3_OFFSET			(3*NUM_VOICE_SLOTS + 1)
#define KIT_FX3_SIZE			21
#define KIT_T7_OFFSET			(KIT_FX3_OFFSET + KIT_FX3_SIZE)	//MOD: track 7: waveform + 1 (0 = as track 6), coarse offset + 24,
#define KIT_T7_SIZE				5	//send A, send B and pan + 1 (0 = as track 6), part "TR7"
#define KIT_T7_SENDA			2
#define KIT_T7_SENDB			3
#define KIT_T7_PAN				4
#define KIT_T7_COARSE_ZERO		24
//MOD: extra LFOs (lfo.h): 0..5 = the 2nd LFO of voice 1..6, 6 = the LFO of track 7;
//8 settings each in the order of the LFO page (the values of the stock LFO
//parameters), part "LFX"
#define KIT_LFOX_OFFSET			(KIT_T7_OFFSET + KIT_T7_SIZE)
#define KIT_LFOX_STRIDE			8
#define KIT_LFOX_NUM			7
#define KIT_LFOX_SIZE			(KIT_LFOX_STRIDE * KIT_LFOX_NUM)
enum { LFOX_FRQ, LFOX_SYN, LFOX_AMT, LFOX_WAV, LFOX_RTG, LFOX_OFS, LFOX_VOI, LFOX_DST };
//MOD: the slots that don't follow the global sample rate (PAR_VOICE_DECIMATION_ALL), bit = slot, part "OSX"
#define KIT_OSX_OFFSET			(KIT_LFOX_OFFSET + KIT_LFOX_SIZE)
//MOD: random octave of each track 1..7 (OCT_RND_x, VoiceSlot.h), part "OCT"
#define KIT_OCT_OFFSET			(KIT_OSX_OFFSET + 1)
#define KIT_OCT_SIZE			7
//MOD: choke groups B and C (bit n = track n; group A is PAR_CHOKE_GROUP), part "CHK"
#define KIT_CHK_OFFSET			(KIT_OCT_OFFSET + KIT_OCT_SIZE)
#define KIT_CHK_SIZE			2
#define KIT_DATA_SIZE			(3*NUM_VOICE_SLOTS + 1 + KIT_FX3_SIZE + KIT_T7_SIZE + KIT_LFOX_SIZE + 1 + KIT_OCT_SIZE + KIT_CHK_SIZE)
#define KIT_SEND_DEFAULT		0
//engine block, "SNA" + count + levels, "ORD" + 1 + order, "SNB" + count + levels
#define KIT_DATA_BLOCK_SIZE		(KIT_ENGINES_BLOCK_SIZE + 4 + NUM_VOICE_SLOTS + 5 + 4 + NUM_VOICE_SLOTS + 4 + KIT_FX3_SIZE + 4 + KIT_T7_SIZE + 4 + KIT_LFOX_SIZE + 5 + 4 + KIT_OCT_SIZE + 4 + KIT_CHK_SIZE)

//version of this interface (for the host tests, which also build older trees)
#define KIT_ENGINES_API		3

extern uint8_t kitEngines[KIT_DATA_SIZE];							//the active kit
extern uint8_t kitEngines_ram[NUM_KIT_SLOTS][KIT_DATA_SIZE];		//the kits in RAM (mainParameters)

//all kits: stock engines
void kitEngines_init(void);
void kitEngines_setStock(uint8_t* engines);

//change the engine of a slot in the active kit
void kitEngines_assign(const uint8_t slot, const uint8_t engine);

//load a voice from another kit (its parameters kit[], END_OF_SOUND_PARAMETERS)
//into the same slot of the active kit, with its engine
void kitEngines_loadVoice(const uint8_t slot, const uint8_t engine, const uint8_t* kit);

//MOD voice INIT: reset a slot of the active kit to its engine's defaults (the
//engine stays); the factory kit (init kit values, stock engines); copy the
//sound parameters of a slot (incl. LFO/velocity targets, decay B, sample
//length) from one parameter set to another
void kitEngines_initVoice(const uint8_t slot);
void kitEngines_factoryKit(uint8_t* params, uint8_t* engines);
void kitEngines_copyVoice(const uint8_t slot, uint8_t* dst, const uint8_t* src);
//MOD: the kit data of a slot besides its engine (sends, 2nd LFO; slot 6: track 7's settings and LFO)
void kitEngines_copySlotData(const uint8_t slot, uint8_t* dst, const uint8_t* src);

//MOD: the sound parameters of a slot, entry i has the same function in every
//slot (list must hold KITENGINES_MAX_SLOT_PARAMS); returns their number
#define KITENGINES_MAX_SLOT_PARAMS	40
uint8_t kitEngines_getSlotParams(const uint8_t slot, uint8_t* list);

//set the sound engine's slots to the active kit's engines (before the kit's
//parameters are sent)
void kitEngines_apply(void);

//the slot whose engine specific parameter par is, NUM_VOICE_SLOTS if none
uint8_t kitEngines_rolePar2Slot(const uint16_t par);
//MOD: the role (0..6) of an engine specific parameter of a slot running one of
//the added engines (modal, FM2, wavetable: *engine), -1 for any other parameter
int8_t kitEngines_ownRole(const uint16_t par, uint8_t* engine);

//the slot whose sound parameter par is (incl. its modulation targets and slot 6
//decay B), NUM_VOICE_SLOTS if none
uint8_t kitEngines_slotOfParam(const uint16_t par);

//the parameter of slot toSlot with the same function as par of slot fromSlot
//(slot 6 decay B maps to decay); par itself if it isn't a sound parameter of fromSlot
uint16_t kitEngines_mapParam(uint16_t par, const uint8_t fromSlot, const uint8_t toSlot);

//the slot whose stock engine engine is (where its defaults and menu pages come from)
uint8_t kitEngines_stockSlot(const uint8_t engine);

//engines -> block, returns its size (KIT_ENGINES_BLOCK_SIZE)
uint8_t kitEngines_encode(const uint8_t* engines, uint8_t* block);
//block (size bytes read after the parameters, maybe 0) -> engines; the stock
//engines if it isn't a valid block
void kitEngines_decode(const uint8_t* block, const uint16_t size, uint8_t* engines);

#endif /* KITENGINES_H_ */
