/* Original work Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
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
#include "app/scanner.h"
#include "bitmaps.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"
#include "ui/battery.h"
#include "ui/helper.h"
#include "ui/ui.h"
#include "ui/status.h"
#include "driver/si473x.h"
#include "helper/rds.h"  // dostęp do rds oraz SI47XX_GetLocalTime()


void UI_DisplayStatus()
{
	uint8_t     *line = gStatusLine;
	unsigned int x    = 0;
	unsigned int x1   = 0;
	memset(gStatusLine, 0, sizeof(gStatusLine));

	// **************
	// DODANE: szeroka ikona STEREO po lewej (jeśli aktywne)
	if (gFmRadioMode && gFmIsStereo)
	{
		// rysujemy szeroką ikonę stereo na samym lewym skraju
		memmove(line + x, BITMAP_STEREO_BIG, sizeof(BITMAP_STEREO_BIG));
		x += sizeof(BITMAP_STEREO_BIG);
		x1 = x;
		// dodajemy mały odstęp (2 kolumny)
		x += 2;
	}

	// POWER-SAVE indicator
	if (gCurrentFunction == FUNCTION_TRANSMIT)
	{
		memmove(line + x, BITMAP_TX, sizeof(BITMAP_TX));
		x1 = x + sizeof(BITMAP_TX);
	}
	else
	if (gCurrentFunction == FUNCTION_RECEIVE ||
	    gCurrentFunction == FUNCTION_MONITOR ||
	    gCurrentFunction == FUNCTION_INCOMING)
	{
		memmove(line + x, BITMAP_RX, sizeof(BITMAP_RX));
		x1 = x + sizeof(BITMAP_RX);
	}
	else
	if (gCurrentFunction == FUNCTION_POWER_SAVE)
	{
		memmove(line + x, BITMAP_POWERSAVE, sizeof(BITMAP_POWERSAVE));
		x1 = x + sizeof(BITMAP_POWERSAVE);
	}
	x += sizeof(BITMAP_POWERSAVE);
	x += 7;  // font character width
	x++;
	x = MAX(x, 61u);
	x1 = x;

	// KEY-LOCK indicator
	if (gEeprom.KEY_LOCK)
	{
		memmove(line + x, BITMAP_KeyLock, sizeof(BITMAP_KeyLock));
		x += sizeof(BITMAP_KeyLock);
		x1 = x;
	}
	else
	if (gWasFKeyPressed)
	{
		memmove(line + x, BITMAP_F_Key, sizeof(BITMAP_F_Key));
		x += sizeof(BITMAP_F_Key);
		x1 = x;
	}

	{	// battery voltage or percentage
		char         s[8];
		unsigned int space_needed;
		
		//unsigned int x2 = LCD_WIDTH - sizeof(BITMAP_BatteryLevel1) - 3;
		unsigned int x2 = LCD_WIDTH - 3;
		switch (gSetting_battery_text)
		{
			default:
			case 0:
				break;
	
			case 1:		// voltage
			{
				const uint16_t voltage = (gBatteryVoltageAverage <= 999) ? gBatteryVoltageAverage : 999; // limit to 9.99V
				sprintf(s, "%u.%02uV", voltage / 100, voltage % 100);
				space_needed = (7 * strlen(s));
				if (x2 >= (x1 + space_needed))
				{
					UI_PrintStringSmallBuffer(s, line + x2 - space_needed);
				}
				break;
			}
			
			case 2:		// percentage
			{
				sprintf(s, "%u%%", BATTERY_VoltsToPercent(gBatteryVoltageAverage));
				space_needed = (7 * strlen(s));
				if (x2 >= (x1 + space_needed))
					UI_PrintStringSmallBuffer(s, line + x2 - space_needed);
				break;
			}
		}
	
	
		//char         s[16]; TIME rds
		// --- najpierw rysujemy czas RDS (zarezerwowane miejsce przed ikoną baterii) ---
		// rezerwujemy miejsce na "HH:MM" (5 znaków, używamy ~7px na znak jak w reszcie)
		const unsigned int TIME_CHARS = 5; // "HH:MM"
		const unsigned int CHAR_W = 7;
		const unsigned int TIME_SPACE = TIME_CHARS * CHAR_W; // np. 35px

		// wyznacz pozycję, gdzie narysować czas (tuż przed ikoną baterii)
		unsigned int x_time_pos;
	//	bool drewTime = false;

	//if (x2 > (x1 + TIME_SPACE + 2))
    {
        x_time_pos = x2 - TIME_SPACE - space_needed;
        if (gRdsTimeValid)
        {
            sprintf(s, "%02u:%02u:%02u", gRdsLocalTime.hour, gRdsLocalTime.minute, gRdsLocalTime.second);
        }
        else
        {
            strcpy(s, "-:--");
        }
        UI_PrintStringSmallBuffer(s, line + x_time_pos);
        //drewTime = true;
    }

	}	
	// move to right side of the screen
	//x = LCD_WIDTH - sizeof(BITMAP_BatteryLevel1);

	// BATTERY LEVEL indicator
	//UI_DrawBattery(line + x, gBatteryDisplayLevel, gLowBatteryBlink);
	
	// **************

	ST7565_BlitStatusLine();
}
