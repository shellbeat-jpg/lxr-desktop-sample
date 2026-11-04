/*
 * VoiceSlot.c
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "Soundengine/DSPAudio/VoiceSlot.h"
#include <string.h>
#include "SampleRom/sampleFlash.h"	//MOD: loop flag (slot_stopLoops)
#include "Preset/KitEngines.h"			//MOD: track 7 settings
#include <math.h>

INDTCMZ VoiceSlot voiceSlots[NUM_VOICE_SLOTS];

//roles:	drum				snare				cymbal / hi-hat
// 0		mod osc wave		-					mod osc 1 wave
// 1		pitch EG decay		pitch EG decay		mod osc 2 wave
// 2		pitch EG amount		pitch EG amount		mod osc 2 freq
// 3		pitch EG slope		pitch EG slope		mod osc 2 gain
// 4		FM amount			tone/noise mix		mod osc 1 gain
// 5		FM freq				noise freq			mod osc 1 freq
// 6		mix/FM mode			repeat				repeat (cymbal) / - (hi-hat)
const uint8_t slot_rolePar[NUM_VOICE_SLOTS][NUM_SLOT_ROLES] =
{
	{ PAR_MOD_WAVE_DRUM1, PAR_MOD_EG1, PAR_MODAMNT1, PAR_PITCH_SLOPE1, PAR_FMAMNT1, PAR_FM_FREQ1, PAR_MIX_MOD_1 },
	{ PAR_MOD_WAVE_DRUM2, PAR_MOD_EG2, PAR_MODAMNT2, PAR_PITCH_SLOPE2, PAR_FMAMNT2, PAR_FM_FREQ2, PAR_MIX_MOD_2 },
	{ PAR_MOD_WAVE_DRUM3, PAR_MOD_EG3, PAR_MODAMNT3, PAR_PITCH_SLOPE3, PAR_FMAMNT3, PAR_FM_FREQ3, PAR_MIX_MOD_3 },
	{ PAR_UNUSED_3, PAR_MOD_EG4, PAR_MODAMNT4, PAR_PITCH_SLOPE4, PAR_MIX1, PAR_NOISE_FREQ1, PAR_REPEAT4 },
	{ PAR_WAVE2_CYM, PAR_WAVE3_CYM, PAR_MOD_OSC_F2_CYM, PAR_MOD_OSC_GAIN2_CYM, PAR_MOD_OSC_GAIN1_CYM, PAR_MOD_OSC_F1_CYM, PAR_REPEAT5 },
	{ PAR_WAVE2_HH, PAR_WAVE3_HH, PAR_MOD_OSC_F2, PAR_MOD_OSC_GAIN2, PAR_MOD_OSC_GAIN1, PAR_MOD_OSC_F1, PAR_UNUSED_4 },
};
//---------------------------------------------------
uint8_t slot_stockEngine(const uint8_t slot)
{
	switch(slot)
	{
	case 3:		return ENGINE_SNARE;
	case 4:		return ENGINE_CYMBAL;
	case 5:		return ENGINE_HIHAT;
	default:	return ENGINE_DRUM;
	}
}
//---------------------------------------------------
static void slot_initEngine(VoiceSlot* s, const uint8_t slot)
{
	switch(s->engine)
	{
	case ENGINE_SNARE:	Snare_init(&s->u.snare, slot);		break;
	case ENGINE_CYMBAL:	Cymbal_init(&s->u.cymbal, slot);	break;
	case ENGINE_HIHAT:	HiHat_init(&s->u.hat, slot);		break;
	default:			Drum_init(&s->u.drum, slot);		break;
	}
}
//---------------------------------------------------
void slot_init(void)
{
	for(uint8_t i=0;i<NUM_VOICE_SLOTS;i++)
	{
		voiceSlots[i].engine = slot_stockEngine(i);
		slot_initEngine(&voiceSlots[i], i);
	}
}
//---------------------------------------------------
void slot_setEngine(const uint8_t slot, const uint8_t engine)
{
	if(slot >= NUM_VOICE_SLOTS || engine >= NUM_ENGINES)
		return;

	VoiceSlot* s = &voiceSlots[slot];
	if(s->engine == engine)
		return;

	//the audio block is rendered in an interrupt: don't let it see a half initialized voice
	const uint32_t primask = __get_PRIMASK();
	__disable_irq();

	const Lfo lfo = s->u.c.lfo;
	const double faderVolume = s->u.c.FaderVolumeFilter.b;

	//start from zero like at power-on, so no field keeps bytes of the previous engine
	memset(&s->u, 0, sizeof(s->u));
	s->engine = engine;
	slot_initEngine(s, slot);

	//keep the LFO (settings, target, phase) and the current fader level
	s->u.c.lfo = lfo;
	s->u.c.FaderVolumeFilter.b = faderVolume;

	__set_PRIMASK(primask);
}
//---------------------------------------------------
//---------------------------------------------------
//MOD: choke group
#define SLOT_CHOKE_FADE		96		//samples (~2 ms)
#define SLOT_CHOKE_OFF		(-1)	//not choked
uint8_t slot_chokeGroup = 0;
uint8_t slot_t7Playing = 0;	//MOD: track 7 sends
uint8_t slot_cueOnly[NUM_VOICE_SLOTS];	//MOD: cue
uint8_t slot_triggerCueOnly = 0;
void slot_clearCueOnly(const uint8_t track)
{
	if(track >= 7)	memset(slot_cueOnly, 0, sizeof(slot_cueOnly));
	else			slot_cueOnly[track < NUM_VOICE_SLOTS ? track : NUM_VOICE_SLOTS-1] = 0;
}
static int16_t slot_choke[NUM_VOICE_SLOTS] = { SLOT_CHOKE_OFF, SLOT_CHOKE_OFF, SLOT_CHOKE_OFF, SLOT_CHOKE_OFF, SLOT_CHOKE_OFF, SLOT_CHOKE_OFF };

void slot_applyChoke(const uint8_t slot, int16_t* buf, const uint8_t size)
{
	int16_t c = slot_choke[slot];
	if(c == SLOT_CHOKE_OFF)
		return;
	for(uint8_t i=0;i<size;i++)
	{
		buf[i] = (int16_t)(((int32_t)buf[i] * c) / SLOT_CHOKE_FADE);
		if(c > 0) c--;
	}
	slot_choke[slot] = c;
}
static void slot_chokeOthers(const uint8_t track, const uint8_t slot)
{
	slot_choke[slot] = SLOT_CHOKE_OFF;		//the triggered slot plays again
	if(!(slot_chokeGroup & (1u << track)))
		return;
	for(uint8_t t=0;t<7;t++)
	{
		const uint8_t other = t < NUM_VOICE_SLOTS ? t : NUM_VOICE_SLOTS-1;
		if(t == track || other == slot || !(slot_chokeGroup & (1u << t)))
			continue;		//tracks 6/7 share a slot: they cut each other anyway
		if(slot_choke[other] == SLOT_CHOKE_OFF)
			slot_choke[other] = SLOT_CHOKE_FADE;
	}
}
void slot_stopLoops(void)
{
	for(uint8_t s=0;s<NUM_VOICE_SLOTS;s++)
	{
		const OscInfo* o = &voiceSlots[s].u.c.osc;
		const uint8_t w = o->smpWave;	//the sample latched at the trigger
		if(o->waveform < NUM_OSC_WAVEFORMS || o->waveform >= OSC_WAVE_USER
				|| w < NUM_OSC_WAVEFORMS || w >= OSC_WAVE_USER)
			continue;		//no sample playing
		const SampleFlashEntry* e = sampleFlash_entry((uint8_t)(w - NUM_OSC_WAVEFORMS));
		if(e != NULL && (e->flags & SF_FLAG_LOOP) && slot_choke[s] == SLOT_CHOKE_OFF)
			slot_choke[s] = SLOT_CHOKE_FADE;
	}
}
//---------------------------------------------------
void slot_trigger(const uint8_t track, const uint8_t vel, const uint8_t note)
{
	const uint8_t slot = track < NUM_VOICE_SLOTS ? track : NUM_VOICE_SLOTS-1;
	const uint8_t second = track >= NUM_VOICE_SLOTS;	//track 7
	VoiceSlot* s = &voiceSlots[slot];

	slot_chokeOthers(track, slot);	//MOD
	slot_cueOnly[slot] = slot_triggerCueOnly;	//MOD: cue

	//MOD: track 7 can play its own waveform and pitch on the shared voice of slot 6
	{
		OscInfo* o = &s->u.c.osc;
		o->altWave = 0;
		o->altPitch = 0.f;
		if(second)
		{
			o->altWave = kitEngines[KIT_T7_OFFSET];
			const int8_t semi = (int8_t)kitEngines[KIT_T7_OFFSET + 1] - KIT_T7_COARSE_ZERO;
			if(semi) o->altPitch = powf(2.f, semi / 12.f);
		}
	}

	switch(s->engine)
	{
	case ENGINE_SNARE:	Snare_trigger(&s->u.snare, vel, note);				break;
	case ENGINE_CYMBAL:	Cymbal_trigger(&s->u.cymbal, vel, note);			break;
	case ENGINE_HIHAT:	HiHat_trigger(&s->u.hat, vel, second, note);		break;
	default:			Drum_trigger(&s->u.drum, vel, note);				break;
	}
	lfo_extraRetrigger(slot, second);	//MOD: 2nd LFOs, track 7 LFO

	//tracks 6 and 7 share slot 6 but each has its own amp decay. Set after the
	//trigger, which applies the velocity modulation (it may target the decay).
	if(slot == NUM_VOICE_SLOTS-1)
	{
		s->u.c.oscVolEg.decay = s->decay[second];
		slot_t7Playing = second;
		//MOD: track 7's own pan (the engines took the voice's pan at the trigger)
		const uint8_t pan = kitEngines[KIT_T7_OFFSET + KIT_T7_PAN];
		if(second && pan)
		{
			const float p = (float)(pan - 1u);
			s->u.c.triggeredPanL = sqrtf((127.f - p) / 127.f);
			s->u.c.triggeredPanR = sqrtf(p / 127.f);
		}
	}
}
//---------------------------------------------------
static inline float slot_calcPitchModAmount(const uint8_t data2)
{
	const float val = data2/127.f;
	return val*val*PITCH_AMOUNT_FACTOR;
}
//---------------------------------------------------
//A role can be a 0-127 setting for one engine and a waveform for another, so
//clamp. MOD: the samples S1..S31 and user waves W1..W8 are waveforms of the FM
//oscillators too (as in the menu; before they were turned into PWM); values above
//them become the last built-in waveform. A modulation selects among the built-in ones.
static uint8_t slot_waveform(const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	if(isFloatModulator)
	{
		const uint8_t wave = NUM_OSC_WAVEFORMS * flt;
		return wave >= NUM_OSC_WAVEFORMS ? NUM_OSC_WAVEFORMS-1 : wave;
	}
	return itg >= OSC_WAVE_USER + OSC_NUM_USER_WAVES ? NUM_OSC_WAVEFORMS-1 : itg;
}
//---------------------------------------------------
//set the coarse tuning (upper byte of midiFreq) of an oscillator
static void slot_setCoarse(OscInfo* osc, const uint8_t itg)
{
	osc->midiFreq &= 0x00ff;
	osc->midiFreq |= itg << 8;
	osc_recalcFreq(osc);
}
//---------------------------------------------------
static void slot_setDrumRole(DrumVoice* v, const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	switch(role)
	{
	case 0:	v->modOsc.waveform = slot_waveform(itg, flt, isFloatModulator);	break;
	case 1:	DecayEg_setDecay(&v->oscPitchEg, itg);							break;
	case 2:	v->egPitchModAmount = slot_calcPitchModAmount(itg);				break;
	case 3:	DecayEg_setSlope(&v->oscPitchEg, itg);							break;
	case 4:	v->fmModAmount = flt;											break;
	case 5:	slot_setCoarse(&v->modOsc, itg);								break;
	default: v->mixOscs = itg != 0;											break;
	}
}
//---------------------------------------------------
static void slot_setSnareRole(SnareVoice* v, const uint8_t role, const uint8_t itg, const float flt)
{
	switch(role)
	{
	case 0:																	break;
	case 1:	DecayEg_setDecay(&v->oscPitchEg, itg);							break;
	case 2:	v->egPitchModAmount = slot_calcPitchModAmount(itg);				break;
	case 3:	DecayEg_setSlope(&v->oscPitchEg, itg);							break;
	case 4:	v->mix = flt;													break;
	case 5:	v->noiseOsc.freq = flt*22000;									break;
	default: v->oscVolEg.repeat = itg;										break;
	}
}
//---------------------------------------------------
//cymbal and hi-hat: 2 modulator oscillators
static void slot_setFmRole(OscInfo* modOsc, OscInfo* modOsc2, float* modAmount1, float* modAmount2,
		const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	switch(role)
	{
	case 0:	modOsc->waveform = slot_waveform(itg, flt, isFloatModulator);	break;
	case 1:	modOsc2->waveform = slot_waveform(itg, flt, isFloatModulator);	break;
	case 2:	slot_setCoarse(modOsc2, itg);									break;
	case 3:	*modAmount2 = flt;												break;
	case 4:	*modAmount1 = flt;												break;
	default: slot_setCoarse(modOsc, itg);									break;
	}
}
//---------------------------------------------------
uint8_t slot_setRoleParameter(const uint16_t par, const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	for(uint8_t slot=0;slot<NUM_VOICE_SLOTS;slot++)
	{
		for(uint8_t role=0;role<NUM_SLOT_ROLES;role++)
		{
			if(slot_rolePar[slot][role] != par)
				continue;

			VoiceSlot* s = &voiceSlots[slot];
			switch(s->engine)
			{
			case ENGINE_SNARE:
				slot_setSnareRole(&s->u.snare, role, itg, flt);
				break;
			case ENGINE_CYMBAL:
				if(role == 6)
					s->u.cymbal.oscVolEg.repeat = itg;
				else
					slot_setFmRole(&s->u.cymbal.modOsc, &s->u.cymbal.modOsc2, &s->u.cymbal.fmModAmount1,
							&s->u.cymbal.fmModAmount2, role, itg, flt, isFloatModulator);
				break;
			case ENGINE_HIHAT:
				if(role != 6)
					slot_setFmRole(&s->u.hat.modOsc, &s->u.hat.modOsc2, &s->u.hat.fmModAmount1,
							&s->u.hat.fmModAmount2, role, itg, flt, isFloatModulator);
				break;
			default:
				slot_setDrumRole(&s->u.drum, role, itg, flt, isFloatModulator);
				break;
			}
			return 1;
		}
	}
	return 0;
}
