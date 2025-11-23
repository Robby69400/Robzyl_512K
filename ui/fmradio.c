/* Original work Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 *
 *   zylka(c)
 * https://k5.2je.eu/index.php?topic=119
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
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "settings.h"
#include "ui/helper.h"

void UI_DisplayFM(void)
{
	char         String[16];

	memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

	memset(String, 0, sizeof(String));
#if defined ENABLE_4732
	strcpy(String, "FM SI4732");			
#else
	strcpy(String, "FM BK1080");
#endif
	UI_PrintString(String, 0, 127, 0, 12);

	memset(String, 0, sizeof(String));
	
		// Display Gain and Mute status
	memset(String, 0, sizeof(String));
	const char *mute_str;
	#if defined ENABLE_4732
	switch (gFmMuteState) {
		case 1: mute_str = " L"; break;
		case 2: mute_str = " R"; break;
		case 3: mute_str = "All"; break;
		default: mute_str = "Off"; break;
	}			
#else
	switch (gFmMuteState) {
		case 1: mute_str = "On"; break;
		default: mute_str = "Off"; break;
	}	
#endif
if (!gAgcEnabled){
sprintf(String, "T:%2u M:%s", gFmGain, mute_str);}
else{ 
sprintf(String, "AGC M:%s", mute_str);}
	//UI_PrintString(String, 0, 127, 5, 8);

	UI_PrintString(String, 0, 127, 2, 10);

	memset(String, 0, sizeof(String));

	sprintf(String, "%3d.%d", gFmCurrentFrequency / 100, gFmCurrentFrequency % 100);
	UI_DisplayFrequency(String, 32, 4, true);			

	ST7565_BlitFullScreen();
}