/* Original work Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Modified work Copyright 2024 kamilsss655
 * https://github.com/kamilsss655
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <string.h>

#include "app/action.h"
#include "app/app.h"
#include "app/common.h"
#include "app/fm.h"
#include "app/scanner.h"

#include "bsp/dp32g030/gpio.h"
#include "driver/bk1080.h"

#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/backlight.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"
#include "spectrum.h"
//#include "debugging.h"

static void ACTION_FlashLight(void)
{
	switch (gFlashLightState)
	{
		case 0:
			gFlashLightState++;
			GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
			break;
		case 1:
		case 2:
			gFlashLightState++;
			break;
		default:
			gFlashLightState = 0;
			GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
	}
}

//Robby69 
static void ACTION_TOGGLE_PTT(void) {Ptt_Toggle_Mode = !Ptt_Toggle_Mode;}

void ACTION_Power(void)
{
	if (++gTxVfo->OUTPUT_POWER > OUTPUT_POWER_HIGH)gTxVfo->OUTPUT_POWER = OUTPUT_POWER_LOW;
	gRequestSaveChannel = 1;
	gRequestDisplayScreen = gScreenToDisplay;
}

void ACTION_Monitor(void)
{
	if (gCurrentFunction != FUNCTION_MONITOR) { // enable the monitor
		RADIO_SelectVfos();

		RADIO_SetupRegisters(true);
		APP_StartListening(FUNCTION_MONITOR);
		return;
	}

	gMonitor = false;
	


	RADIO_SetupRegisters(true);

	gRequestDisplayScreen = gScreenToDisplay;
}


#if defined(ENABLE_TX1750)
	static void ACTION_AlarmOr1750(const bool b1750)
	{
		(void)b1750;
		gInputBoxIndex = 0;

		#if defined(ENABLE_TX1750)
			gAlarmState = ALARM_STATE_TX1750;
		#endif

		gFlagPrepareTX = true;

		if (gScreenToDisplay != DISPLAY_MENU)     // 1of11 .. don't close the menu
			gRequestDisplayScreen = DISPLAY_MAIN;
	}
#endif



	void ACTION_FM(void)
	{
		if (gCurrentFunction != FUNCTION_TRANSMIT && gCurrentFunction != FUNCTION_MONITOR)
		{
			if (gFmRadioMode)
			{
				FM_TurnOff();

				gInputBoxIndex        = 0;
				gFlagReconfigureVfos  = true;

				gRequestDisplayScreen = DISPLAY_MAIN;
				return;
			}

			gMonitor = false;

			RADIO_SelectVfos();
			RADIO_SetupRegisters(true);

			FM_Start();

			gInputBoxIndex = 0;

			gRequestDisplayScreen = DISPLAY_FM;
		}
	}



void ACTION_SwitchDemodul(void)
{
	gTxVfo->Modulation++;
	if(gTxVfo->Modulation == MODULATION_UKNOWN)
		gTxVfo->Modulation = MODULATION_FM;
	
	gRequestSaveChannel = 1;
}

BK4819_FilterBandwidth_t ACTION_NextBandwidth(BK4819_FilterBandwidth_t currentBandwidth, const bool dynamic, bool increase)
{
    BK4819_FilterBandwidth_t nextBandwidth =
        (increase && currentBandwidth == BK4819_FILTER_BW_NARROWEST) ? BK4819_FILTER_BW_WIDE :
        (!increase && currentBandwidth == BK4819_FILTER_BW_WIDE)     ? BK4819_FILTER_BW_NARROWEST :
        (increase ? currentBandwidth + 1 : currentBandwidth - 1);

    BK4819_SetFilterBandwidth(nextBandwidth, dynamic);
    gRequestSaveChannel = 1;
    return nextBandwidth;
}


#ifdef ENABLE_BLMIN_TMP_OFF
void ACTION_BlminTmpOff(void)
{
	if(++gEeprom.BACKLIGHT_MIN_STAT == BLMIN_STAT_UNKNOWN)
	{
		gEeprom.BACKLIGHT_MIN_STAT = BLMIN_STAT_ON;
		BACKLIGHT_SetBrightness(gEeprom.BACKLIGHT_MIN);
	} else
	{
		BACKLIGHT_SetBrightness(0);
	}
}
#endif

void ACTION_Handle(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	uint8_t funcShort = ACTION_OPT_NONE;
	uint8_t funcLong  = ACTION_OPT_NONE;
	switch(Key) {
		case KEY_SIDE1:
			funcShort = gEeprom.KEY_1_SHORT_PRESS_ACTION;
			funcLong  = gEeprom.KEY_1_LONG_PRESS_ACTION;
			break;
		case KEY_SIDE2:
			funcShort = gEeprom.KEY_2_SHORT_PRESS_ACTION;
			funcLong  = gEeprom.KEY_2_LONG_PRESS_ACTION;
			break;
		default:
			break;
	}

	if (!bKeyHeld && bKeyPressed) // button pushed
	{
		return;
	}

	// held or released beyond this point

	if (bKeyHeld || bKeyPressed) // held
	{
		funcShort = funcLong;

		if (!bKeyPressed) //ignore release if held
			return;
	}

	// held or released after short press beyond this point

	switch (funcShort)
	{
		default:
		case ACTION_OPT_NONE:
			break;
		case ACTION_OPT_FLASHLIGHT:
			ACTION_FlashLight();
			break;
		case ACTION_OPT_TOGGLE_PTT:
			ACTION_TOGGLE_PTT();
			break;
		case ACTION_OPT_POWER:
			ACTION_Power();
			break;
		case ACTION_OPT_MONITOR:
			ACTION_Monitor();
			break;
		case ACTION_OPT_FM:
			ACTION_FM();
			break;
		case ACTION_OPT_1750:
			#ifdef ENABLE_TX1750
				ACTION_AlarmOr1750(true);
			#endif
			break;
		case ACTION_OPT_KEYLOCK:
			COMMON_KeypadLockToggle();
			break;
		case ACTION_OPT_VFO_MR:
			COMMON_SwitchVFOMode();
			break;
		case ACTION_OPT_SWITCH_DEMODUL:
			ACTION_SwitchDemodul();
			break;
		case ACTION_OPT_BANDWIDTH:
			gTxVfo->CHANNEL_BANDWIDTH =
				ACTION_NextBandwidth(gTxVfo->CHANNEL_BANDWIDTH, gTxVfo->Modulation != MODULATION_AM, 1);
			break;
#ifdef ENABLE_BLMIN_TMP_OFF
		case ACTION_OPT_BLMIN_TMP_OFF:
			ACTION_BlminTmpOff();
			break;
#endif
	}
}
