/*
 * voiceMorph.h
 *
 * MOD (XT): morph the sound of one voice towards another active
 * voice, like the kit morph but between two voices of the active kit.
 * Performance page: "vmv" selects the pair (source > target), "vmo" the
 * amount (0 = the source as set, 127 = the target's values). Not saved.
 *
 * Modified firmware - not an official Erica Synths release.
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef VOICEMORPH_H_
#define VOICEMORPH_H_

#include <stdint.h>
#include "Parameters.h"

//the parameters PAR_VMORPH_PAIR / PAR_VMORPH_AMT are defined in Parameters.h

//source/target slot of a pair value (1..30); returns 0 for "off"
uint8_t voiceMorph_decode(const uint8_t pair, uint8_t* src, uint8_t* dst);

//send the morphed values of the source voice to the sound engine (or its own
//values if the morph is off); call when pair or amount change
void voiceMorph_apply(void);

//switch point (1..127) of a source slot: from this amount on, switch-like values
//are the target's (global settings PAR_VMORPH_SW1..6, default 64)
uint8_t voiceMorph_switchPoint(const uint8_t srcSlot);

//sends the pending values in small steps; call from the main loop
void voiceMorph_tick(void);

//a sound parameter was edited: if it belongs to the source or target voice of
//an active morph, the morph is applied again
void voiceMorph_paramEdited(const uint16_t par);

#endif /* VOICEMORPH_H_ */
