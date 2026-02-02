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

#ifndef UI_UI_H
#define UI_UI_H

#include <stdbool.h>
#include <stdint.h>

void UI_GenerateChannelString(char *pString, const uint16_t Channel);
void UI_GenerateChannelStringEx(char *pString, const bool bShowPrefix, const uint16_t ChannelNumber);
void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width);
void UI_PrintStringSmall(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t background);
void UI_PrintStringBSmall(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t background);
void UI_PrintStringSmallBuffer(const char *pString, uint8_t *buffer);
void UI_PrintStringBSmallBuffer(const char *pString, uint8_t *buffer);
void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center);
#endif

void UI_DisplayPopup(const char *string);

void UI_DrawPixelBuffer(uint8_t (*buffer)[128], uint8_t x, uint8_t y, bool black);
void UI_DrawLineBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black);
void UI_DrawRectangleBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black);
void UI_DrawDottedLineBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black, int dotSpacing);

void PutPixel(uint8_t x, uint8_t y, bool fill);
void PutPixelStatus(uint8_t x, uint8_t y, bool fill);
void DrawVLine(int sy, int ey, int nx, bool fill);
void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y, bool statusbar, bool fill);
void GUI_DisplaySmallestDark(const char *pString, uint8_t x, uint8_t y, bool statusbar, bool wide_spacing); // ИНВЕРСИЯ МЕЛКОГО ТЕКСТА INVERSION FONT SMALL* // false, true шаг между символами

#ifdef ENABLE_PL_BAND
void UI_PrintStringSmallScrolling(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t scrollOffset);
#endif
