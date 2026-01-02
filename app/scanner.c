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

#include "app/app.h"
#include "app/generic.h"
#include "app/menu.h"
#include "app/scanner.h"
#include "driver/bk4819.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

DCS_CodeType_t    gScanCssResultType;
uint8_t           gScanCssResultCode;
bool              gScanSingleFrequency; // scan CTCSS/DCS codes for current frequency
uint8_t           gScanChannel;
uint8_t           gScanBand;
uint32_t          gScanFrequency;
SCAN_CssState_t   gScanCssState;
uint8_t           gScanProgressIndicator;
bool              gScanUseCssResult;
uint8_t               gScanDelay_10ms;

STEP_Setting_t    stepSetting;
uint8_t           scanHitCount;

bool SCANNER_IsScanning(void)
{
	return gCssBackgroundScan || (gScreenToDisplay == DISPLAY_SCANNER);
}

void SCANNER_Stop(void)
{
	if(SCANNER_IsScanning()) {
		gVfoConfigureMode        = VFO_CONFIGURE_RELOAD;
		gFlagResetVfos           = true;
		            
		gCssBackgroundScan 			 = false;
		gScanUseCssResult = false;
		BK4819_StopScan();
	}
}

static void SCANNER_Key_EXIT(bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyHeld && bKeyPressed) { // short pressed
				SCANNER_Stop();
				gRequestDisplayScreen    = DISPLAY_MAIN;
		}
}

void SCANNER_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	switch (Key) {
		
		case KEY_1:
			gScanBand = 1; //VHF
			SCANNER_Start(0);
			break;
		case KEY_2:
			gScanBand = 2; //UHF
			SCANNER_Start(0);
			break;
		case KEY_3:
			gScanBand = 3; //ALL
			SCANNER_Start(0);
			break;
		case KEY_0:
		case KEY_4:
		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
		case KEY_MENU:
		case KEY_UP:
		case KEY_DOWN:
			break;
		case KEY_EXIT:
			SCANNER_Key_EXIT(bKeyPressed, bKeyHeld);
			break;
		case KEY_STAR:
			SCANNER_Start(gScanSingleFrequency);
			break;
		case KEY_PTT:
			GENERIC_Key_PTT(bKeyPressed);
			break;
		default:
			break;
	}
}

void SCANNER_Start(bool singleFreq)
{
	gScanSingleFrequency = singleFreq;
	gMonitor = false;
	BK4819_StopScan();
	RADIO_SelectVfos();
	uint8_t  backupStep      = gTxVfo->STEP_SETTING;
	uint16_t backupFrequency = gTxVfo->StepFrequency;

	RADIO_InitInfo(gTxVfo, gTxVfo->CHANNEL_SAVE, gTxVfo->pRX->Frequency);

	gTxVfo->STEP_SETTING  = backupStep;
	gTxVfo->StepFrequency = backupFrequency;

	RADIO_SetupRegisters(true);

	if (gScanSingleFrequency) {
		gScanCssState  = SCAN_CSS_STATE_SCANNING;
		gScanFrequency = gTxVfo->pRX->Frequency;
		stepSetting   = gTxVfo->STEP_SETTING;

		BK4819_PickRXFilterPathBasedOnFrequency(gScanFrequency);
		BK4819_SetScanFrequency(gScanFrequency);

		
	}
	else { //Close call
		gScanCssState  = SCAN_CSS_STATE_OFF;
		gScanFrequency = 0xFFFFFFFF;
		if (gScanBand == 1) BK4819_PickRXFilterPathBasedOnFrequency(10000000);
		else if (gScanBand == 2) BK4819_PickRXFilterPathBasedOnFrequency(50000000);
			 else BK4819_PickRXFilterPathBasedOnFrequency(0xFFFFFFFF);
		BK4819_EnableFrequencyScan();
	}
	gScanDelay_10ms        = scan_delay_10ms;
	gScanCssResultCode     = 0xFF;
	gScanCssResultType     = 0xFF;
	scanHitCount          = 0;
	gScanUseCssResult      = false;
	g_CxCSS_TAIL_Found     = false;
	g_CDCSS_Lost           = false;
	gCDCSSCodeType         = 0;
	g_CTCSS_Lost           = false;
	g_SquelchLost          = false;
	gScanProgressIndicator = 0;
}



void SCANNER_TimeSlice10ms(void)
{
	if (!SCANNER_IsScanning())
		return;

	if (gScanDelay_10ms > 0) {
		gScanDelay_10ms--;
		return;
	}
	uint16_t rssi;
	rssi = BK4819_GetRSSI();
	sLevelAttributes signal = GetSLevelAttributes(rssi, gTxVfo->freq_config_RX.Frequency);

	if(signal.overSquelch)
		BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
	else
		BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);

	switch (gScanCssState) {
		case SCAN_CSS_STATE_OFF: {
			// must be RF frequency scanning if we're here ?
			uint32_t result;
			if (!BK4819_GetFrequencyScanResult(&result))
				break;

			int32_t delta = result - gScanFrequency;
			gScanFrequency = result;

			if (delta < 0)
				delta = -delta;
			if (delta < 100)
				scanHitCount++;
			else
				scanHitCount = 0;

			BK4819_DisableFrequencyScan();
			if (scanHitCount < 3) {
				BK4819_EnableFrequencyScan();
			}
			else {
				BK4819_SetScanFrequency(gScanFrequency);
				gScanCssResultCode     = 0xFF;
				gScanCssResultType     = 0xFF;
				scanHitCount          = 0;
				gScanUseCssResult      = false;
				gScanProgressIndicator = 0;
				gScanCssState          = SCAN_CSS_STATE_SCANNING;

				if(!gCssBackgroundScan)
					GUI_SelectNextDisplay(DISPLAY_SCANNER);

				          
			}
			gScanDelay_10ms = scan_delay_10ms;
			break;
		}
		case SCAN_CSS_STATE_SCANNING: {
			uint32_t cdcssFreq;
			uint16_t ctcssFreq;
			BK4819_CssScanResult_t scanResult = BK4819_GetCxCSSScanResult(&cdcssFreq, &ctcssFreq);
			if (scanResult == BK4819_CSS_RESULT_NOT_FOUND)
				break;

			BK4819_Disable();

			if (scanResult == BK4819_CSS_RESULT_CDCSS) {
				const uint8_t Code = DCS_GetCdcssCode(cdcssFreq);
				if (Code != 0xFF)
				{
					gScanCssResultCode = Code;
					gScanCssResultType = CODE_TYPE_DIGITAL;
					gScanCssState      = SCAN_CSS_STATE_FOUND;
					gScanUseCssResult  = true;
					      
				}
			}
			else if (scanResult == BK4819_CSS_RESULT_CTCSS) {
				const uint8_t Code = DCS_GetCtcssCode(ctcssFreq);
				if (Code != 0xFF) {
					
					++scanHitCount;
					gScanCssState      = SCAN_CSS_STATE_FOUND;
					gScanUseCssResult  = true;
					      
					gScanCssResultType = CODE_TYPE_CONTINUOUS_TONE;
					gScanCssResultCode = Code;
				}
				else {
					scanHitCount = 0;
				}
			}

			if (gScanCssState < SCAN_CSS_STATE_FOUND) { // scanning or off
				BK4819_SetScanFrequency(gScanFrequency);
				gScanDelay_10ms = 1;
				break;
			}
				GUI_SelectNextDisplay(DISPLAY_SCANNER);


			break;
		}
		default:
			gCssBackgroundScan = false;
			break;
	}

}

void SCANNER_TimeSlice500ms(void)
{
	if (SCANNER_IsScanning() && gScanCssState < SCAN_CSS_STATE_FOUND) {
		gScanProgressIndicator++;

		if (gScanProgressIndicator > 250) {
			if (gScanCssState == SCAN_CSS_STATE_SCANNING && !gScanSingleFrequency)
				gScanCssState = SCAN_CSS_STATE_FOUND;
			else
				gScanCssState = SCAN_CSS_STATE_FAILED;

			
		}
		
	}
	else if(gCssBackgroundScan) {
		
	}
}

