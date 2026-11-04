/*
 * voiceLoad.c
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "menu/voiceLoad.h"
#include "menu/menu.h"
#include "menu/projectBrowse.h"
#include "Preset/PresetManager.h"
#include "Preset/KitEngines.h"
#include "buttonHandler.h"
#include "ledHandler.h"
#include "Soundengine/Soundengine.h"
#include <stdio.h>
#include <string.h>

static uint8_t voiceLoad_voice;		//track 0-6 (6 = 2nd trigger of slot 6)
static uint8_t voiceLoad_kit;		//position in the kit list of the listed project

static const char* const voiceLoad_voiceNames[7] = { "Drum 1", "Drum 2", "Drum 3", "Snare", "Cymbal", "CL HH", "OP HH" };
static const char* const voiceLoad_engineNames[NUM_ENGINES] = { "drm", "snr", "cym", "hht" };

//MOD voice INIT: an "INIT" row between "Change Project" and kit 0. Its menu
//loads the voice's INIT (project template, else global template, else the
//factory values of its engine), saves the voice into a template or removes it.
enum { VI_LOAD, VI_SAVE_PROJ, VI_SAVE_GLOB, VI_DEL_PROJ, VI_DEL_GLOB, VI_BACK, VI_NUM_ACTIONS };
enum { VI_SRC_PROJ, VI_SRC_GLOB, VI_SRC_FACT };
static const char* const vi_actionNames[VI_NUM_ACTIONS] = { "Load INIT", "Save to Project", "Save to Global", "Del Project INIT", "Del Global INIT", "Back" };
static const char* const vi_srcNames[3] = { "Project", "Global", "Factory" };
static uint8_t vi_row;			//the INIT row is selected
static uint8_t vi_menu;			//its action menu is open
static uint8_t vi_action;
static uint8_t vi_confirm;		//a delete waits for the 2nd click
static const char* vi_msg;		//result, shown until the encoder moves
static uint8_t vi_maskProj, vi_maskGlob;
//---------------------------------------------------
static uint8_t voiceLoad_slot(void)
{
	return voiceLoad_voice < NUM_VOICE_SLOTS ? voiceLoad_voice : NUM_VOICE_SLOTS-1;
}
//---------------------------------------------------
static void vi_refresh(void)
{
	const uint8_t bg = soundengine_backgroundAudioBegin();
	vi_maskProj = preset_initTemplateMask(projectBrowse.project);
	vi_maskGlob = preset_initTemplateMask(PRESET_INIT_GLOBAL);
	soundengine_backgroundAudioEnd(bg);
}
static uint8_t vi_source(void)
{
	const uint8_t bit = 1u << voiceLoad_slot();
	return (vi_maskProj & bit) ? VI_SRC_PROJ : (vi_maskGlob & bit) ? VI_SRC_GLOB : VI_SRC_FACT;
}
//---------------------------------------------------
static FRESULT vi_load(void)
{
	static uint8_t params[END_OF_SOUND_PARAMETERS];
	uint8_t engines[KIT_DATA_SIZE];
	const uint8_t src = vi_source();
	if(src == VI_SRC_FACT)
	{
		kitEngines_initVoice(voiceLoad_slot());
		return FR_OK;
	}
	const uint8_t bg = soundengine_backgroundAudioBegin();
	const FRESULT res = preset_readInitTemplate(src == VI_SRC_PROJ ? projectBrowse.project : PRESET_INIT_GLOBAL, params, engines);
	soundengine_backgroundAudioEnd(bg);
	if(res == FR_OK)
	{
		kitEngines_loadVoice(voiceLoad_slot(), engines[voiceLoad_slot()], params);
		kitEngines[KIT_SEND_OFFSET + voiceLoad_slot()] = engines[KIT_SEND_OFFSET + voiceLoad_slot()];	//MOD: FX send
			kitEngines[KIT_SENDB_OFFSET + voiceLoad_slot()] = engines[KIT_SENDB_OFFSET + voiceLoad_slot()];
			kitEngines[KIT_LSYNC_OFFSET + voiceLoad_slot()] = engines[KIT_LSYNC_OFFSET + voiceLoad_slot()];
	}
	return res;
}
//the voice of the active kit goes into the template, the other voices stay
static FRESULT vi_save(const uint8_t project)
{
	static uint8_t params[END_OF_SOUND_PARAMETERS];
	uint8_t engines[KIT_DATA_SIZE];
	const uint8_t slot = voiceLoad_slot();
	const uint8_t bg = soundengine_backgroundAudioBegin();
	uint8_t mask = preset_initTemplateMask(project);
	if(!mask || preset_readInitTemplate(project, params, engines) != FR_OK)
	{
		mask = 0;
		kitEngines_factoryKit(params, engines);
	}
	kitEngines_copyVoice(slot, params, activeParameters);
	engines[slot] = kitEngines[slot];
	engines[KIT_SEND_OFFSET + slot] = kitEngines[KIT_SEND_OFFSET + slot];	//MOD: FX sends
	engines[KIT_SENDB_OFFSET + slot] = kitEngines[KIT_SENDB_OFFSET + slot];
	engines[KIT_LSYNC_OFFSET + slot] = kitEngines[KIT_LSYNC_OFFSET + slot];
	const FRESULT res = preset_writeInitTemplate(project, params, engines, (uint8_t)(mask | (1u << slot)));
	soundengine_backgroundAudioEnd(bg);
	return res;
}
//remove the voice from the template, the file when it was the last one
static FRESULT vi_delete(const uint8_t project)
{
	static uint8_t params[END_OF_SOUND_PARAMETERS];
	uint8_t engines[KIT_DATA_SIZE];
	const uint8_t bg = soundengine_backgroundAudioBegin();
	const uint8_t mask = preset_initTemplateMask(project) & (uint8_t)~(1u << voiceLoad_slot());
	FRESULT res;
	if(!mask)
		res = preset_deleteInitTemplate(project);
	else
	{
		res = preset_readInitTemplate(project, params, engines);
		if(res == FR_OK)
			res = preset_writeInitTemplate(project, params, engines, mask);
	}
	soundengine_backgroundAudioEnd(bg);
	return res;
}
static void vi_click(void)
{
	const uint8_t bit = 1u << voiceLoad_slot();
	FRESULT res = FR_OK;
	vi_msg = NULL;
	switch(vi_action)
	{
	case VI_LOAD:
		res = vi_load();
		if(res == FR_OK) vi_msg = "INIT loaded";
		break;
	case VI_SAVE_PROJ:
		res = vi_save(projectBrowse.project);
		if(res == FR_NO_FILE || res == FR_NO_PATH) { vi_msg = "Save proj first"; res = FR_OK; }
		else if(res == FR_OK) vi_msg = "Saved to Project";
		break;
	case VI_SAVE_GLOB:
		res = vi_save(PRESET_INIT_GLOBAL);
		if(res == FR_OK) vi_msg = "Saved to Global";
		break;
	case VI_DEL_PROJ:
	case VI_DEL_GLOB:
	{
		const uint8_t isProj = vi_action == VI_DEL_PROJ;
		if(!((isProj ? vi_maskProj : vi_maskGlob) & bit)) { vi_msg = "No INIT there"; break; }
		if(!vi_confirm) { vi_confirm = 1; vi_msg = "Delete? Click"; return; }
		res = vi_delete(isProj ? projectBrowse.project : PRESET_INIT_GLOBAL);
		if(res == FR_OK) vi_msg = "Deleted";
		break;
	}
	default:	//back to the list
		vi_menu = 0;
		break;
	}
	vi_confirm = 0;
	if(res != FR_OK)
		menu_showSdError(res);
	vi_refresh();
}
//---------------------------------------------------
void voiceLoad_open(const uint8_t voice)
{
	const uint8_t wasOpen = menu_activePage == VOICE_LOAD_PAGE;
	if(wasOpen)
		led_setBlinkLed(led_idx2StepLed(voiceLoad_voice), 0);
	voiceLoad_voice = voice;

	//opened again for another voice: stay in the project and at the kit, so
	//several voices of one kit can be loaded one after the other
	if(!wasOpen)
	{
		projectBrowse_reset();
		voiceLoad_kit = menu_selectedPresetNr[SAVE_TYPE_KIT];
		vi_row = 0;

		//like the load kit menu: load mode, step buttons don't show the steps
		buttonHandler_setMode(SELECT_MODE_LOAD);
		led_setMode2(SELECT_MODE_LOAD);
		led_clearAllBlinkLeds();
		menu_switchPage(VOICE_LOAD_PAGE);
		led_clearSequencerLeds();
	}
	led_setBlinkLed(led_idx2StepLed(voiceLoad_voice), 1);
	vi_menu = 0;
	vi_confirm = 0;
	vi_msg = NULL;
	if(vi_row)
		vi_refresh();
	menu_repaintAll();
}
//---------------------------------------------------
void voiceLoad_leave(void)
{
	led_setBlinkLed(led_idx2StepLed(voiceLoad_voice), 0);
}
//---------------------------------------------------
void voiceLoad_encoder(const int8_t inc)
{
	vi_msg = NULL;
	vi_confirm = 0;
	if(vi_menu)
	{
		const int16_t a = vi_action + inc;
		vi_action = (uint8_t)(a < 0 ? 0 : a >= VI_NUM_ACTIONS ? VI_NUM_ACTIONS-1 : a);
		menu_repaintAll();
		return;
	}
	if(vi_row)
	{
		vi_row = 0;
		if(inc > 0)
			voiceLoad_kit = 0;
		else
			projectBrowse.changeRow = 1;
		menu_repaintAll();
		return;
	}
	const uint8_t wasChangeRow = projectBrowse.changeRow && !projectBrowse.selecting;
	if(!projectBrowse.selecting && !projectBrowse.changeRow && voiceLoad_kit == 0 && inc < 0)
	{
		vi_row = 1;		//kit 0 -> INIT -> Change Project
		vi_refresh();
	}
	else if(projectBrowse_encoder(inc, &voiceLoad_kit))
	{
		if(wasChangeRow && !projectBrowse.changeRow)
		{
			vi_row = 1;	//down from Change Project: INIT before kit 0
			vi_refresh();
		}
	}
	else
	{
		const int16_t kit = voiceLoad_kit + inc;
		voiceLoad_kit = (uint8_t)(kit < 0 ? 0 : kit >= NUM_KIT_SLOTS ? NUM_KIT_SLOTS-1 : kit);
	}
	menu_repaintAll();
}
//---------------------------------------------------
void voiceLoad_click(void)
{
	if(vi_menu)
	{
		vi_click();
	}
	else if(vi_row)
	{
		vi_menu = 1;
		vi_action = VI_LOAD;
		vi_msg = NULL;
	}
	else if(projectBrowse_click(0))
	{
		if(!projectBrowse.selecting && !projectBrowse.changeRow)
			voiceLoad_kit = 0;	//a project was chosen: start at its first kit
	}
	else if(projectBrowse_kitExists(voiceLoad_kit))
	{
		static uint8_t params[END_OF_SOUND_PARAMETERS];
		uint8_t engines[KIT_DATA_SIZE];
		const FRESULT res = projectBrowse_readKit(voiceLoad_kit, params, engines);
		if(res == FR_OK)
		{
			kitEngines_loadVoice(voiceLoad_slot(), engines[voiceLoad_slot()], params);
			kitEngines[KIT_SEND_OFFSET + voiceLoad_slot()] = engines[KIT_SEND_OFFSET + voiceLoad_slot()];	//MOD: FX send
			kitEngines[KIT_SENDB_OFFSET + voiceLoad_slot()] = engines[KIT_SENDB_OFFSET + voiceLoad_slot()];
			kitEngines[KIT_LSYNC_OFFSET + voiceLoad_slot()] = engines[KIT_LSYNC_OFFSET + voiceLoad_slot()];
		}
		else
			menu_showSdError(res);
	}
	menu_repaintAll();
}
//---------------------------------------------------
void voiceLoad_repaint(char lines[2][17])
{
	snprintf(lines[0], 17, "Load: %s", voiceLoad_voiceNames[voiceLoad_voice]);
	projectBrowse_repaintProjectNr(lines[0]);
	if(projectBrowse_repaint(lines))
		return;
	if(vi_menu)
	{
		snprintf(lines[0], 17, "INIT %s", voiceLoad_voiceNames[voiceLoad_voice]);
		projectBrowse_repaintProjectNr(lines[0]);
		if(vi_msg)
			snprintf(lines[1], 17, "%s", vi_msg);
		else if(vi_action == VI_LOAD)
			snprintf(lines[1], 17, "Load %s", vi_srcNames[vi_source()]);
		else
			snprintf(lines[1], 17, "%s", vi_actionNames[vi_action]);
		return;
	}
	if(vi_row)
	{
		snprintf(lines[1], 17, "INIT: %s", vi_srcNames[vi_source()]);
		return;
	}

	if(!projectBrowse_kitExists(voiceLoad_kit))
		snprintf(lines[1], 17, "%2u Empty", voiceLoad_kit);
	else	//the engine the voice has in that kit at the end
		snprintf(lines[1], 17, "%2u %-8.8s  %s", voiceLoad_kit, projectBrowse_kitName(voiceLoad_kit),
				voiceLoad_engineNames[projectBrowse_kitEngine(voiceLoad_kit, voiceLoad_slot())]);
}
