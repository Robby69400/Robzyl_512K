/* Original work Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
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
//#ifndef ENABLE_4732_H
//#define ENABLE_4732_H


#include "driver/keyboard.h"

extern const uint16_t FM_RADIO_MAX_FREQ;
extern const uint16_t FM_RADIO_MIN_FREQ;

enum {
	FM_SCAN_OFF = 0,
	FM_SCAN_UP = 1,
	FM_SCAN_DOWN = -1
};

extern bool              gFmRadioMode;
extern uint8_t           gFmRadioCountdown_500ms;
extern volatile uint16_t gFmPlayCountdown_10ms;
extern volatile int8_t   gFM_ScanState;
extern uint8_t           gFM_ChannelPosition;
// Doubts about          whether this should be signed or not
extern uint16_t          gFM_FrequencyDeviation;
extern uint16_t          gFM_RestoreCountdown_10ms;
extern uint16_t          gFmCurrentFrequency;

// FM settings variables
extern uint8_t           gFmGain;
extern int8_t            gFmVolume;
extern bool              gAgcEnabled;
extern uint8_t           gFmMuteState;
extern char              gRdsTime[6];
extern char              gRdsDebugText[24];

/* Nowa zmienna: siła sygnału (0..100 procent) */
extern uint8_t           gFmSignalStrength;

/* Dodatkowe metryki */
extern uint8_t           gFmSnr;
extern uint8_t           gFmMult;
extern int8_t            gFmFreqOffset;

/* Nowa zmienna: czy odbierana jest stereo */
extern bool              gFmIsStereo;



void    FM_TurnOff(void);

void    FM_CheckScan(void);

void    FM_Tune(uint16_t Frequency, int8_t Step);
void    FM_PlayAndUpdate(void);
void    FM_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);

void    FM_Play(void);
void    FM_Start(void);
void FM_UpdateSignalStrength(void);