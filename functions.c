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


	#include "app/fm.h"


#include "bsp/dp32g030/gpio.h"
#include "dcs.h"
#include "driver/backlight.h"
#include "driver/bk1080.h"
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/st7565.h"
#include "frequencies.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/status.h"
#include "ui/ui.h"
#include "app/main.h"


FUNCTION_Type_t gCurrentFunction;

void FUNCTION_Init(void)
{

	{
		gCurrentCodeType = (gTxVfo->Modulation != MODULATION_FM) ? CODE_TYPE_OFF : gTxVfo->pRX->CodeType;
	}


	g_CxCSS_TAIL_Found = false;
	g_CDCSS_Lost       = false;
	g_CTCSS_Lost       = false;
	g_SquelchLost      = false;
	gFlagTailNoteEliminationComplete   = false;
	gTailNoteEliminationCountdown_10ms = 0;
	gFoundCTCSS                        = false;
	gFoundCDCSS                        = false;
	gFoundCTCSSCountdown_10ms          = 0;
	gFoundCDCSSCountdown_10ms          = 0;
	gEndOfRxDetectedMaybe              = false;
	
}

void FUNCTION_Select(FUNCTION_Type_t Function)
{
	const FUNCTION_Type_t PreviousFunction = gCurrentFunction;
	const bool            bWasPowerSave    = (PreviousFunction == FUNCTION_POWER_SAVE);

	gCurrentFunction = Function;

	if (bWasPowerSave && Function != FUNCTION_POWER_SAVE)
	{
		BK4819_Conditional_RX_TurnOn_and_GPIO6_Enable();
		gRxIdleMode = false;
		UI_DisplayStatus();
	}

	switch (Function)
	{
		case FUNCTION_FOREGROUND:
			if (PreviousFunction == FUNCTION_TRANSMIT)
			{
				ST7565_FixInterfGlitch();
				gVFO_RSSI_bar_level[0] = 0;
				gVFO_RSSI_bar_level[1] = 0;
			}
			else
			if (PreviousFunction != FUNCTION_RECEIVE)
				break;

			#if defined(ENABLE_FMRADIO)
				if (gFmRadioMode)
					gFM_RestoreCountdown_10ms = fm_restore_countdown_10ms;
			#endif
			

			
			return;

		case FUNCTION_MONITOR:
			gMonitor = true;
			break;

		case FUNCTION_INCOMING:
		case FUNCTION_RECEIVE:
			rxTimeSeconds = 0;
			break;

		case FUNCTION_POWER_SAVE:
			gPowerSave_10ms            = gEeprom.BATTERY_SAVE * 20;
			gPowerSaveCountdownExpired = false;

			gRxIdleMode = true;

			gMonitor = false;
			BK4819_Sleep();

			BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);

			

			if (gScreenToDisplay != DISPLAY_MENU)     // 1of11 .. don't close the menu
				GUI_SelectNextDisplay(DISPLAY_MAIN);

			return;

		case FUNCTION_TRANSMIT:
			if (gFmRadioMode)BK1080_Init(0, false);
			

			//GUI_DisplayScreen();

			RADIO_SetTxParameters();

			// turn the RED LED on
			BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);

			#if defined(ENABLE_TX1750)
				if (gAlarmState != ALARM_STATE_OFF)
				{
					#ifdef ENABLE_TX1750
						if (gAlarmState == ALARM_STATE_TX1750)
							BK4819_TransmitTone(true, 1750);
					#endif
					SYSTEM_DelayMs(2);
					GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
					gEnableSpeaker = true;
					break;
				}
			#endif

			if (gCurrentVfo->SCRAMBLING_TYPE > 0 && gSetting_ScrambleEnable)
				BK4819_EnableScramble(gCurrentVfo->SCRAMBLING_TYPE - 1);
			else
				BK4819_DisableScramble();

			if (gSetting_backlight_on_tx_rx == BACKLIGHT_ON_TR_TX || 
			    gSetting_backlight_on_tx_rx == BACKLIGHT_ON_TR_TXRX)
				BACKLIGHT_TurnOn();

			break;

		case FUNCTION_BAND_SCOPE:
			break;
	}

	gBatterySaveCountdown_10ms = battery_save_count_10ms;
	gSchedulePowerSave         = false;
	gFM_RestoreCountdown_10ms = 0;
}
