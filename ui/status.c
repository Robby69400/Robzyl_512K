/* Original work Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Modified work 2024 kamilsss655
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
#include <stdio.h>  // для sprintf

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
#include "app/main.h"  // для txTimeSeconds, rxTimeSeconds, isTxActive

uint16_t BK4819_GetRSSI(void);
void UI_DisplayStatus()
{
	char time_str[6];
	const uint8_t TIME_POS_X = 16;  // редактируемая позиция времени (X в пикселях, можно менять)
	const uint8_t POS_FL = 63;  // Позиция для иконки фонарика (правее "B")

	memset(gStatusLine, 0, sizeof(gStatusLine));

	// === ОТДЕЛЬНЫЕ ПОЗИЦИИ ДЛЯ КАЖДОГО ИНДИКАТОРА ===
	const uint8_t POS_TX   = 0;    // начало "TX" при передаче
	const uint8_t POS_RX   = 0;    // начало "RX" при приёме
	const uint8_t POS_PS   = 0;    // начало "PS" при Power Save
	const uint8_t POS_LOCK = 81;   // Замок
	const uint8_t POS_F    = 81;   // "F"
	const uint8_t POS_B    = 72;   // "подсветка"

		// === Индикаторы TX / RX / PS — мелким шрифтом как у батареи (позиция по POS_...) ===
	if (gCurrentFunction == FUNCTION_TRANSMIT)
	{
		UI_PrintStringSmallBuffer("TX", gStatusLine + POS_TX);

		sprintf(time_str, "%02u:%02u", txTimeSeconds / 60, txTimeSeconds % 60);
		UI_PrintStringSmallBuffer(time_str, gStatusLine + TIME_POS_X);
	}
	else if (gCurrentFunction == FUNCTION_RECEIVE ||
	         gCurrentFunction == FUNCTION_MONITOR ||
	         gCurrentFunction == FUNCTION_INCOMING)
	{
		UI_PrintStringSmallBuffer("RX", gStatusLine + POS_RX);

		sprintf(time_str, "%02u:%02u", rxTimeSeconds / 60, rxTimeSeconds % 60);
		UI_PrintStringSmallBuffer(time_str, gStatusLine + TIME_POS_X);
	}
	else if (gCurrentFunction == FUNCTION_POWER_SAVE)
	{
		UI_PrintStringSmallBuffer("PS", gStatusLine + POS_PS);
	}

		// === Индикатор функции фонарика при приеме "L" ===
if (gEeprom.FlashlightOnRX)
{
gStatusLine[POS_FL + 0] |= 0x00;
gStatusLine[POS_FL + 1] |= 0x00;
gStatusLine[POS_FL + 2] |= 0x00;
gStatusLine[POS_FL + 3] |= 0x00;
gStatusLine[POS_FL + 4] |= 0x00;
gStatusLine[POS_FL + 5] |= 0x00;
gStatusLine[POS_FL + 6] |= 0x00;
gStatusLine[POS_FL + 7] |= 0x00;
gStatusLine[POS_FL + 8] |= 0x70;
gStatusLine[POS_FL + 9] |= 0x7E;
gStatusLine[POS_FL + 10] |= 0x61;
gStatusLine[POS_FL + 11] |= 0x61;
gStatusLine[POS_FL + 12] |= 0x61;
gStatusLine[POS_FL + 13] |= 0x7E;
gStatusLine[POS_FL + 14] |= 0x70;
    
}

	// === Индикатор блокировки клавиатуры (замок) ===
	if (gEeprom.KEY_LOCK)
	{
	gStatusLine[POS_LOCK + 0] |= 0x00;
gStatusLine[POS_LOCK + 1] |= 0x00;
gStatusLine[POS_LOCK + 2] |= 0x00;
gStatusLine[POS_LOCK + 3] |= 0x00;
gStatusLine[POS_LOCK + 4] |= 0x00;
gStatusLine[POS_LOCK + 5] |= 0x00;
gStatusLine[POS_LOCK + 6] |= 0x00;
gStatusLine[POS_LOCK + 7] |= 0x00;
gStatusLine[POS_LOCK + 8] |= 0x7C;
gStatusLine[POS_LOCK + 9] |= 0x7A;
gStatusLine[POS_LOCK + 10] |= 0x79;
gStatusLine[POS_LOCK + 11] |= 0x49;
gStatusLine[POS_LOCK + 12] |= 0x79;
gStatusLine[POS_LOCK + 13] |= 0x7A;
gStatusLine[POS_LOCK + 14] |= 0x7C;
	}

	// === Индикатор нажатой F-клавиши ===
	if (gWasFKeyPressed)
	{
		gStatusLine[POS_F + 0] |= 0x00;
gStatusLine[POS_F + 1] |= 0x00;
gStatusLine[POS_F + 2] |= 0x00;
gStatusLine[POS_F + 3] |= 0x00;
gStatusLine[POS_F + 4] |= 0x00;
gStatusLine[POS_F + 5] |= 0x00;
gStatusLine[POS_F + 6] |= 0x00;
gStatusLine[POS_F + 7] |= 0x00;
gStatusLine[POS_F + 8] |= 0x7F;
gStatusLine[POS_F + 9] |= 0x41;
gStatusLine[POS_F + 10] |= 0x75;
gStatusLine[POS_F + 11] |= 0x75;
gStatusLine[POS_F + 12] |= 0x75;
gStatusLine[POS_F + 13] |= 0x7D;
gStatusLine[POS_F + 14] |= 0x7F;
	}

	// === Индикатор постоянной подсветки "B" ===
	if (gBacklightAlwaysOn)
	{
	gStatusLine[POS_B + 0] |= 0x00;
gStatusLine[POS_B + 1] |= 0x00;
gStatusLine[POS_B + 2] |= 0x00;
gStatusLine[POS_B + 3] |= 0x00;
gStatusLine[POS_B + 4] |= 0x00;
gStatusLine[POS_B + 5] |= 0x00;
gStatusLine[POS_B + 6] |= 0x00;
gStatusLine[POS_B + 7] |= 0x00;
gStatusLine[POS_B + 8] |= 0x0C;
gStatusLine[POS_B + 9] |= 0x12;
gStatusLine[POS_B + 10] |= 0x65;
gStatusLine[POS_B + 11] |= 0x79;
gStatusLine[POS_B + 12] |= 0x65;
gStatusLine[POS_B + 13] |= 0x12;
gStatusLine[POS_B + 14] |= 0x0C;
	}


        // === S-МЕТР В СТАТУСБАРЕ (при приёме) ===
    if (gCurrentFunction == FUNCTION_RECEIVE ||
        gCurrentFunction == FUNCTION_MONITOR ||
        gCurrentFunction == FUNCTION_INCOMING)
    {
        uint16_t rssi = BK4819_GetRSSI();
        if (rssi > 50) {  // показываем только при нормальном сигнале
            sLevelAttributes sLevelAtt = GetSLevelAttributes(rssi, gTxVfo->freq_config_RX.Frequency);

             char meterStr[8];
    sprintf(meterStr, "S%d", (sLevelAtt.sLevel >= 9 || sLevelAtt.over > 0) ? 9 : sLevelAtt.sLevel);

            // ПОЗИЦИЯ — МЕНЯЙ ЗДЕСЬ
            unsigned int meter_x = 53;  // ← основная позиция (70 — слева от батареи)
            unsigned int space_needed = 7 * strlen(meterStr);
            unsigned int start_x = meter_x;

            // Автоматическая защита от переполнения (чтобы не затирать батарею)
            if (start_x + space_needed > LCD_WIDTH - 20) {  // оставляем место под батарею
                start_x = LCD_WIDTH - 20 - space_needed;
            }

            UI_PrintStringSmallBuffer(meterStr, gStatusLine + start_x);
        }
    }
	// === Battery voltage / percentage ===
	{
		char s[8];
		unsigned int space_needed;
		unsigned int x2 = LCD_WIDTH - 3;

		switch (gSetting_battery_text)
		{
			case 1: // voltage
			{
				const uint16_t voltage = (gBatteryVoltageAverage <= 999) ? gBatteryVoltageAverage : 999;
				sprintf(s, "%u.%02uV", voltage / 100, voltage % 100);
				space_needed = 7 * strlen(s);
				if (x2 >= space_needed)
					UI_PrintStringSmallBuffer(s, gStatusLine + x2 - space_needed);
				break;
			}
			case 2: // percentage
			{
				sprintf(s, "%u%%", BATTERY_VoltsToPercent(gBatteryVoltageAverage));
				space_needed = 7 * strlen(s);
				if (x2 >= space_needed)
					UI_PrintStringSmallBuffer(s, gStatusLine + x2 - space_needed);
				break;
			}
		}
	}

	/*/ === Вертикальные линии ===
	uint8_t line_x[] = {0, 127};
	uint8_t num_lines = sizeof(line_x) / sizeof(line_x[0]);
	uint8_t dash_step = 2;

	for (uint8_t i = 0; i < num_lines; i++) {
		uint8_t px = line_x[i];
		if (dash_step == 1) {
			gStatusLine[px] = 0xFF;
		} else {
			uint8_t pattern = 0;
			for (uint8_t bit = 0; bit < 8; bit += dash_step) {
				pattern |= (1 << bit);
			}
			gStatusLine[px] |= pattern;
		}
	}*/

	

	ST7565_BlitStatusLine();
}