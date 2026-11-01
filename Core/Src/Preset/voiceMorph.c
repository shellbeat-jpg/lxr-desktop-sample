/*
 * voiceMorph.c
 *
 * MOD (OE02-Samples): voice morph, see voiceMorph.h.
 *
 * Every slot has the same list of sound parameters (kitEngines_slotParams,
 * entry i has the same function in every slot), so the source voice's entry i
 * is morphed towards the target voice's entry i. Like the kit morph, the audio
 * output routing isn't morphed, and engine specific parameters only when both
 * voices run the same engine. Switch-like values (menus, on/off) change at
 * half the amount. The UI values (activeParameters) stay as set; only the
 * sound engine gets the morphed values.
 *
 * Modified firmware - not an official Erica Synths release.
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "Preset/voiceMorph.h"
#include "Preset/KitEngines.h"
#include "menu/menu.h"
#include "Soundengine/MIDI/MidiParser.h"
#include "Soundengine/MIDI/MidiMessages.h"
#include "frontPanelParser_ui.h"

static uint8_t vm_activeSrc = 0xFF;		//source slot whose engine values are morphed

uint8_t voiceMorph_decode(const uint8_t pair, uint8_t* src, uint8_t* dst)
{
	if(pair == 0 || pair > VMORPH_NUM_PAIRS)
		return 0;
	const uint8_t k = (uint8_t)(pair - 1u);
	*src = (uint8_t)(k / 5u);
	const uint8_t d = (uint8_t)(k % 5u);
	*dst = d >= *src ? (uint8_t)(d + 1u) : d;
	return 1;
}
//---------------------------------------------------
//to the sound engine, the same way the kit morph does it (not to the UI values)
static void vm_send(const uint16_t par, const uint8_t val)
{
	MidiMsg msg = {0};
	msg.data2 = val;
	if(par < 128)
	{
		msg.status = MIDI_CC;
		msg.data1 = (uint8_t)((par + 1u) & 0x7f);
	}
	else
	{
		msg.status = CC_2;
		msg.data1 = (uint8_t)((par - 128u) & 0x7f);
	}
	midiParser_ccHandler(msg, 1, 0, 0.f);
}
//---------------------------------------------------
static uint8_t vm_isSwitch(const uint16_t par)
{
	const uint16_t dtype = menu_paramDtype(par) & 0xFF;
	return dtype == DTYPE_MENU || dtype == DTYPE_ON_OFF || dtype == DTYPE_0b1;
}
//---------------------------------------------------
static void vm_send_slot(const uint8_t src, const uint8_t dst, const uint8_t amount)
{
	uint8_t ls[KITENGINES_MAX_SLOT_PARAMS], ld[KITENGINES_MAX_SLOT_PARAMS];
	const uint8_t n = kitEngines_getSlotParams(src, ls);
	kitEngines_getSlotParams(dst, ld);
	const uint8_t sameEngine = kitEngines[src] == kitEngines[dst];
	for(uint8_t i=0;i<n;i++)
	{
		const uint16_t p = ls[i];
		const uint8_t a = activeParameters[p];
		uint8_t v = a;
		if(amount && !(p >= PAR_AUDIO_OUT1 && p <= PAR_AUDIO_OUT6)
				&& (sameEngine || kitEngines_rolePar2Slot(p) != src))
		{
			const uint8_t b = activeParameters[ld[i]];
			if(vm_isSwitch(p))
				v = amount >= 64 ? b : a;
			else
				v = (uint8_t)(a + (((int16_t)b - (int16_t)a) * (int16_t)amount) / 127);
		}
		vm_send(p, v);
	}
}
//---------------------------------------------------
void voiceMorph_apply(void)
{
	uint8_t src, dst;
	const uint8_t on = voiceMorph_decode(activeParameters[PAR_VMORPH_PAIR], &src, &dst);

	//another source (or off): the previous source gets its own values again
	if(vm_activeSrc < NUM_VOICE_SLOTS && (!on || vm_activeSrc != src))
	{
		vm_send_slot(vm_activeSrc, vm_activeSrc, 0);
		vm_activeSrc = 0xFF;
	}
	if(!on)
		return;
	vm_send_slot(src, dst, activeParameters[PAR_VMORPH_AMT] > 127 ? 127 : activeParameters[PAR_VMORPH_AMT]);
	vm_activeSrc = src;
}
//---------------------------------------------------
void voiceMorph_paramEdited(const uint16_t par)
{
	uint8_t src, dst;
	if(!voiceMorph_decode(activeParameters[PAR_VMORPH_PAIR], &src, &dst) || activeParameters[PAR_VMORPH_AMT] == 0)
		return;
	const uint8_t slot = kitEngines_slotOfParam(par);
	if(slot == src || slot == dst)
		voiceMorph_apply();
}
