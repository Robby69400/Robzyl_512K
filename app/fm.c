/* Original work Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 *   zylka(c)
 * https://k5.2je.eu/index.php?topic=119
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


#include "app/action.h"
#include "app/fm.h"
#include "app/generic.h"

#include "driver/bk1080.h"
#include "misc.h"
#include "settings.h"
#include "ui/ui.h"
#include "driver/gpio.h"
#include "driver/si473x.h"
#include <string.h>
#include "external/printf/printf.h"

const uint16_t FM_RADIO_MAX_FREQ = 10800; // 108  Mhz
const uint16_t FM_RADIO_MIN_FREQ = 8750;  // was 87.5 Mhz

// ZMIANA: Zakresy dla trybu SW (AM/SSB) w jednostkach 10Hz (było 100Hz)
const uint32_t AM_RADIO_MAX_FREQ_10HZ = 3000000; // 30 MHz
const uint32_t AM_RADIO_MIN_FREQ_10HZ = 15000;   // 150 kHz

// Próg siły sygnału (RSSI) do zatrzymania skanera (0-100%)
#define SCAN_RSSI_THRESHOLD 30 
// Ilość cykli pętli do odczekania po zmianie częstotliwości przed pomiarem RSSI
#define SCAN_DWELL_TIME     8 

// --- Manual Frequency Entry (Manual Tuning) ---

uint8_t freqEntryState = FREQ_ENTRY_NONE;  // Zmieniono z static na extern
char freqEntryBuf[8];                       // max 7 cyfr + null terminator (extern)
uint8_t freqEntryLen = 0;                   // (extern)

bool              gFmRadioMode;
uint8_t           gFmRadioCountdown_500ms;
volatile uint16_t gFmPlayCountdown_10ms;
volatile int8_t   gFM_ScanState;
uint16_t          gFM_RestoreCountdown_10ms;
uint16_t          gFmCurrentFrequency;

// FM settings variables
uint8_t           gFmGain;
int8_t            gFmVolume;
bool              gAgcEnabled;
uint8_t           gFmMuteState;

/* Nowa zmienna przechowująca siłę sygnału w procentach 0.. 100 */
uint8_t           gFmSignalStrength = 0; // 0.. 100 percent

/* Nowa zmienna:  czy odbierana stacja stereo */
bool              gFmIsStereo = false;

/* ZMIENNE DLA OBSŁUGI AM/SSB */
// ZMIANA:  Jednostki 10Hz.  Start 7000 kHz = 700000 * 10Hz
static uint32_t   gAmCurrentFrequency_10Hz = 700000; 

// Dostępne kroki strojenia dla AM/SSB (w jednostkach 10Hz)
// 1=10Hz, 10=100Hz, 100=1kHz, 1000=10kHz, 10000=100kHz, 100000=1MHz
const uint32_t    gAmStepsUnits[] = {1, 10, 100, 1000, 10000, 100000};
uint8_t           gAmStepIndex = 2; // Domyślnie 1kHz

// Zmienna pomocnicza do opóźnienia skanera
static uint8_t    gScanDelayCounter = 0;

// Bandwidth control for AM/SSB
// Indexes used to cycle through available bandwidth options via KEY_4
uint8_t gAmBandwidthIndex  = 2; // default -> around 3kHz (index in amBandwidthOptions)
uint8_t gSsbBandwidthIndex = 2; // default -> 3kHz (index in ssbBandwidthOptions)

// Arrays of available bandwidth options (used by SI47XX functions)
static const SI47XX_FilterBW amBandwidthOptions[] = {
    SI47XX_BW_6_kHz,
    SI47XX_BW_4_kHz,
    SI47XX_BW_3_kHz,
    SI47XX_BW_2_kHz,
    SI47XX_BW_1_kHz,
    SI47XX_BW_1_8_kHz,
    SI47XX_BW_2_5_kHz
};

static const SI47XX_SsbFilterBW ssbBandwidthOptions[] = {
    SI47XX_SSB_BW_0_5_kHz,
    SI47XX_SSB_BW_1_0_kHz,
    SI47XX_SSB_BW_1_2_kHz,
    SI47XX_SSB_BW_2_2_kHz,
    SI47XX_SSB_BW_3_kHz,
    SI47XX_SSB_BW_4_kHz
};
void FM_TuneAm(void);

// Gettery dla UI (zwracamy teraz wartość 10Hz!)
uint32_t FM_GetAmFrequency(void) { return gAmCurrentFrequency_10Hz; }
uint8_t  FM_GetAmStepIndex(void) { return gAmStepIndex; }

/* Minimalny parser wpisywanej częstotliwości - bez printf */
static void ApplyManualFrequency(void) {
    uint32_t freq = 0;
    for (uint8_t i = 0; i < freqEntryLen; i++)
        freq = freq * 10 + (freqEntryBuf[i] - '0');

#if defined ENABLE_4732
    if (si4732mode == SI47XX_FM) {
        // FM:   wpisanie w jednostkach kHz (np. 10250 = 102.50 MHz)
        if (freq >= FM_RADIO_MIN_FREQ && freq <= FM_RADIO_MAX_FREQ) {
            gFmCurrentFrequency = freq;
            SI47XX_SetFreq(gFmCurrentFrequency);
            gEeprom. FM_FrequencyPlaying = gFmCurrentFrequency;
        }
    } else {
        // AM/SSB: jednostki 10Hz (np. 710000 = 7.1 MHz)
        if (freq >= AM_RADIO_MIN_FREQ_10HZ && freq <= AM_RADIO_MAX_FREQ_10HZ) {
            gAmCurrentFrequency_10Hz = freq;
            FM_TuneAm();
            gEeprom.FM_FrequencyPlaying = gAmCurrentFrequency_10Hz / 100;
        }
    }
#else
    // BK1080 (tylko FM)
    if (freq >= FM_RADIO_MIN_FREQ && freq <= FM_RADIO_MAX_FREQ) {
        gFmCurrentFrequency = freq;
        BK1080_SetFrequency(freq/10);
        gEeprom.FM_FrequencyPlaying = gFmCurrentFrequency;
    }
#endif
    FM_UpdateSignalStrength();
    gUpdateDisplay = true;
}

/* Obsługa wpisywania:   cyfry 0-9, EXIT (anuluj), ENT (zatwierdź), SIDE2 (delete) */
static void FM_ManualEntry_KeyHandler(KEY_Code_t Key) {
    // Dodaj cyfrę jeśli jest miejsce w buforze
    // POPRAWKA: Usunęło warunkowanie Key >= KEY_0 (zawsze true dla unsigned)
    if (Key <= KEY_9 && freqEntryLen < 7) {
        freqEntryBuf[freqEntryLen++] = '0' + (Key - KEY_0);
        freqEntryBuf[freqEntryLen] = 0;
        gUpdateDisplay = true;
        return;
    }
    // EXIT - anuluj
    if (Key == KEY_EXIT) {
        freqEntryState = FREQ_ENTRY_NONE;
        freqEntryLen = 0;
        gUpdateDisplay = true;
        return;
    }
    // ENT - akceptuj i zastosuj
    if (Key == KEY_MENU) {
        if (freqEntryLen > 0) ApplyManualFrequency();
        freqEntryState = FREQ_ENTRY_NONE;
        freqEntryLen = 0;
        gUpdateDisplay = true;
        return;
    }
    // SIDE2 - delete ostatniej cyfry (backspace)
    if (Key == KEY_SIDE2 && freqEntryLen > 0) {
        freqEntryLen--;
        freqEntryBuf[freqEntryLen] = 0;
        gUpdateDisplay = true;
        return;
    }
}

/* Helper do ustawiania częstotliwości AM/SSB z uwzględnieniem trybu i BFO */
void FM_TuneAm(void) {
#if defined ENABLE_4732
    // Obliczamy część całkowitą kHz dla głównej pętli PLL
    uint16_t freq_kHz = gAmCurrentFrequency_10Hz / 100; 
    
    // Obliczamy resztę (0-990 Hz) dla BFO
    // Reszta z dzielenia przez 100 daje jednostki 10Hz, mnożymy przez 10 aby mieć Hz
    int16_t bfo_Hz = (gAmCurrentFrequency_10Hz % 100) * 10;

    static uint16_t last_kHz = 0;
    static bool firstRun = true;

    // Przestrajamy PLL tylko jeśli zmienił się 1kHz lub to pierwsze uruchomienie
    if (freq_kHz != last_kHz || firstRun) {
        SI47XX_SetFreq(freq_kHz);
        last_kHz = freq_kHz;
        firstRun = false;
    }

    // W trybie SSB używamy BFO do dokładnego dostrojenia
    if (si4732mode == SI47XX_LSB || si4732mode == SI47XX_USB) {
        SI47XX_SetBFO(bfo_Hz);
    } else {
        // W AM BFO zwykle 0, chyba że chcemy fine-tune
        SI47XX_SetBFO(0); 
    }
#endif
}

/* Funkcja aktualizująca siłę sygnału i tryb stereo.  */
void FM_UpdateSignalStrength(void)
{
	int percent = 0;
	bool stereo = false;

#if defined ENABLE_4732
	// Wypełnia rsqStatus (pole resp)
	RSQ_GET();
	int raw = rsqStatus.resp. RSSI; // 0.. 127 (dBµV)
	percent = (raw * 100) / 127;
	
	if (si4732mode == SI47XX_FM) {
		stereo = (rsqStatus.resp.PILOT != 0);
	} else {
		stereo = false;
	}
#else
	// BK1080 logic... 
	int raw = BK1080_GetRSSI(); 
	const int BK_MAX = 63; 
	if (raw < 0) raw = 0;
	if (raw > BK_MAX) raw = BK_MAX;
	percent = (raw * 100) / BK_MAX;
	
	#ifdef BK1080_IsStereo
		stereo = BK1080_IsStereo();
	#else
		stereo = false; 
	#endif
#endif

	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;

	// Aktualizacja zmiennych globalnych
	gFmSignalStrength = (uint8_t)percent;
	gFmIsStereo = stereo;
    
    // Jeśli nie skanujemy, odświeżamy ekran tylko przy znaczącej zmianie
    if (gFM_ScanState == FM_SCAN_OFF) {
        static int oldPercent = 0;
        static bool oldStereo = false;
        // Odświeżaj częściej dla SSB/AM aby widzieć zmiany RSSI płynniej
        if ( (percent > oldPercent + 1) || (percent + 1 < oldPercent) || (stereo != oldStereo) )
        {
            oldPercent = percent;
            oldStereo = stereo;
            gUpdateDisplay = true;
        }
    }
}

void FM_TurnOff(void)
{
	gFmRadioMode              = false;
	gFM_ScanState             = FM_SCAN_OFF;
	gFM_RestoreCountdown_10ms = 0;

	AUDIO_AudioPathOff();

	gEnableSpeaker = false;

#if defined ENABLE_4732
	SI47XX_PowerDown();
#endif
	BK1080_Init(0, false);

	//gUpdateStatus  = true;
}

static void Key_EXIT()
{
	ACTION_FM();
	SETTINGS_SaveVfoIndices();
	return;
}

void FM_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
    // TRYB WPISYWANIA - obsługuj tylko klawisze dla wpisywania
    if (freqEntryState == FREQ_ENTRY_ACTIVE) {
        if (bKeyPressed && ! bKeyHeld) {
            FM_ManualEntry_KeyHandler(Key);
        }
        return;  // Pomiń normalną obsługę klawiszy
    }

    // Handle frequency change on initial press and hold
    if (bKeyPressed || bKeyHeld) {
		switch (Key) {
			case KEY_UP:
                // Jeśli skanowanie jest aktywne, przerwij je lub zmień kierunek
                if (gFM_ScanState != FM_SCAN_OFF) {
                    gFM_ScanState = FM_SCAN_OFF;
                    gUpdateDisplay = true;
                    return;
                }

#if defined ENABLE_4732
				if (si4732mode != SI47XX_FM) {
					uint32_t step = gAmStepsUnits[gAmStepIndex];
					if ((gAmCurrentFrequency_10Hz + step) <= AM_RADIO_MAX_FREQ_10HZ) {
						gAmCurrentFrequency_10Hz += step;
					} else {
						gAmCurrentFrequency_10Hz = AM_RADIO_MIN_FREQ_10HZ;
					}
					FM_TuneAm();
				} else 
#endif
				{
					if (gFmCurrentFrequency < FM_RADIO_MAX_FREQ) {
						gFmCurrentFrequency += 10; // 0.1 MHz
					} else {
						gFmCurrentFrequency = FM_RADIO_MIN_FREQ;
					}
#if defined ENABLE_4732
					SI47XX_SetFreq(gFmCurrentFrequency);
#else
					BK1080_SetFrequency(gFmCurrentFrequency/10);
#endif	
				}
				gEeprom.FM_FrequencyPlaying = gFmCurrentFrequency;
				FM_UpdateSignalStrength();
				gUpdateDisplay = true;
				return; 

			case KEY_DOWN:
                if (gFM_ScanState != FM_SCAN_OFF) {
                    gFM_ScanState = FM_SCAN_OFF;
                    gUpdateDisplay = true;
                    return;
                }

#if defined ENABLE_4732
				if (si4732mode != SI47XX_FM) {
					uint32_t step = gAmStepsUnits[gAmStepIndex];
					if ((gAmCurrentFrequency_10Hz) >= (AM_RADIO_MIN_FREQ_10HZ + step)) {
						gAmCurrentFrequency_10Hz -= step;
					} else {
						gAmCurrentFrequency_10Hz = AM_RADIO_MAX_FREQ_10HZ;
					}
					FM_TuneAm();
				} else 
#endif
				{
					if (gFmCurrentFrequency > FM_RADIO_MIN_FREQ) {
						gFmCurrentFrequency -= 10; // 0.1 MHz
					} else {
						gFmCurrentFrequency = FM_RADIO_MAX_FREQ;
					}
#if defined ENABLE_4732
					SI47XX_SetFreq(gFmCurrentFrequency);
#else
					BK1080_SetFrequency(gFmCurrentFrequency/10);
#endif
				}
                gEeprom.FM_FrequencyPlaying = gFmCurrentFrequency;
				FM_UpdateSignalStrength();
				gUpdateDisplay = true;
				return; 
			default:
				break;
		}
	}
			
	// Handle other keys on release (short press)
	if (!bKeyPressed && ! bKeyHeld) {
		switch (Key)
		{
            case KEY_EXIT:
                if (gFM_ScanState != FM_SCAN_OFF) {
                    gFM_ScanState = FM_SCAN_OFF;
                    gUpdateDisplay = true;
                } else {
                    Key_EXIT();
                }
                break;
                
            case KEY_PTT:  // PTT też przerywa
                 if (gFM_ScanState != FM_SCAN_OFF) {
                    gFM_ScanState = FM_SCAN_OFF;
                    gUpdateDisplay = true;
                }
                break;

#if defined ENABLE_4732
   
    case KEY_F:   // 
        // Inicjuj tryb ręcznego wpisywania
        freqEntryState = FREQ_ENTRY_ACTIVE;
        freqEntryLen = 0;
        freqEntryBuf[0] = 0;
        gUpdateDisplay = true;
        break;

			case KEY_STAR:  // Przełączanie FM <-> SW (AM/SSB)
                if (gFM_ScanState != FM_SCAN_OFF) { gFM_ScanState = FM_SCAN_OFF; return; }

				if (si4732mode == SI47XX_FM) {
					SI47XX_MODE nextMode = SI47XX_AM; 
					SI47XX_SwitchMode(nextMode);
					if (gAmCurrentFrequency_10Hz < AM_RADIO_MIN_FREQ_10HZ || gAmCurrentFrequency_10Hz > AM_RADIO_MAX_FREQ_10HZ)
						gAmCurrentFrequency_10Hz = 700000;
					FM_TuneAm();
				} else {
					SI47XX_SwitchMode(SI47XX_FM);
					gFmCurrentFrequency = gEeprom.FM_FrequencyPlaying;
					if (gFmCurrentFrequency < FM_RADIO_MIN_FREQ || gFmCurrentFrequency > FM_RADIO_MAX_FREQ)
						gFmCurrentFrequency = 9600; 
					SI47XX_SetFreq(gFmCurrentFrequency);
				}
				gUpdateDisplay = true;
				break;

			case KEY_0:  // W trybie SW:  Cykl AM -> LSB -> USB -> AM.  
				if (si4732mode != SI47XX_FM) {
					SI47XX_MODE next = SI47XX_AM;
					if (si4732mode == SI47XX_AM) next = SI47XX_LSB;
					else if (si4732mode == SI47XX_LSB) next = SI47XX_USB;
					else if (si4732mode == SI47XX_USB) next = SI47XX_AM;
					
					SI47XX_SwitchMode(next);
					FM_TuneAm();
					gUpdateDisplay = true;
				}
				break;

			case KEY_6: // Toggle AGC
				gAgcEnabled = !gAgcEnabled;
				SI47XX_SetAutomaticGainControl(! gAgcEnabled, gFmGain);
				gUpdateDisplay = true;
				break;
			
			case KEY_SIDE1:  // STEP UP
				if (si4732mode != SI47XX_FM) {
                    // Mamy teraz 6 kroków (0.. 5)
					gAmStepIndex = (gAmStepIndex + 1) % 6;
					gUpdateDisplay = true;
				} else {
					ACTION_Handle(Key, bKeyPressed, bKeyHeld);
				}
				break;

			case KEY_SIDE2: // STEP DOWN
				if (si4732mode != SI47XX_FM) {
					if (gAmStepIndex == 0) gAmStepIndex = 5;
					else gAmStepIndex--;
					gUpdateDisplay = true;
				} else {
					ACTION_Handle(Key, bKeyPressed, bKeyHeld);
				}
				break;
#endif

			case KEY_1: // Scan Up
				if (gFM_ScanState == FM_SCAN_OFF) {
					gFM_ScanState = FM_SCAN_UP;
                    gScanDelayCounter = 0; 
                    gUpdateDisplay = true;
				} else {
                    gFM_ScanState = FM_SCAN_OFF;
                    gUpdateDisplay = true;
                }
				break;
                
			case KEY_7: // Scan Down
				if (gFM_ScanState == FM_SCAN_OFF) {
					gFM_ScanState = FM_SCAN_DOWN;
                    gScanDelayCounter = 0;
                    gUpdateDisplay = true;
				} else {
                     gFM_ScanState = FM_SCAN_OFF;
                     gUpdateDisplay = true;
                }
				break;
			
			case KEY_2: // Volume Up
				if (gFmVolume < 63) {
					gFmVolume++;
#if defined ENABLE_4732
					setVolume(gFmVolume);
#endif
				}
				break;
			case KEY_8: // Volume Down
				if (gFmVolume > 0) {
					gFmVolume--;
#if defined ENABLE_4732
					setVolume(gFmVolume);
#endif
				}
				break;
			case KEY_3: // Manual Gain Up
				if (! gAgcEnabled && gFmGain < 27) { 
					gFmGain++;
#if defined ENABLE_4732
					SI47XX_SetAutomaticGainControl(true, gFmGain); 
#endif
					gUpdateDisplay = true;
				}
				break;
			case KEY_9: // Manual Gain Down
				if (! gAgcEnabled && gFmGain > 0) { 
					gFmGain--;
#if defined ENABLE_4732
					SI47XX_SetAutomaticGainControl(true, gFmGain); 
#endif
					gUpdateDisplay = true;
				}
				break;
			case KEY_5: // Cycle Mute State
#if defined ENABLE_4732
				gFmMuteState = (gFmMuteState + 1) % 4; 
				sendProperty(PROP_RX_HARD_MUTE, gFmMuteState);				
#else
				gFmMuteState = (gFmMuteState + 1) % 2; 
				BK1080_Mute(gFmMuteState);	
#endif
				break;

            case KEY_4: // NEW:  Cycle audio bandwidth in AM/SSB modes
                if (gFM_ScanState != FM_SCAN_OFF) { gFM_ScanState = FM_SCAN_OFF; gUpdateDisplay = true; break; }

                if (si4732mode != SI47XX_FM) {
                    if (SI47XX_IsSSB()) {
                        // cycle SSB bandwidth
                        gSsbBandwidthIndex = (gSsbBandwidthIndex + 1) % (sizeof(ssbBandwidthOptions)/sizeof(ssbBandwidthOptions[0]));
                        SI47XX_SetSsbBandwidth(ssbBandwidthOptions[gSsbBandwidthIndex]);
                        // Optional UI feedback could be added to show ssbBandwidthNames[gSsbBandwidthIndex]
                    } else {
                        // cycle AM channel filter bandwidth
                        gAmBandwidthIndex = (gAmBandwidthIndex + 1) % (sizeof(amBandwidthOptions)/sizeof(amBandwidthOptions[0]));
                        // AM channel filter also takes AMPLFLT param (enable power line noise rejection)
                        SI47XX_SetBandwidth(amBandwidthOptions[gAmBandwidthIndex], true);
                        // Optional UI feedback:  amBandwidthNames[gAmBandwidthIndex]
                    }
                    gUpdateDisplay = true;
                }
                break;
			
			default:
				break;
		}
	}
}

void FM_CheckScan(void)
{
	if (gFM_ScanState == FM_SCAN_OFF) return;

    // Krok 1: Jeśli licznik opóźnienia > 0, czekamy
    if (gScanDelayCounter > 0) {
        gScanDelayCounter--;
        if (gScanDelayCounter == 0) {
			uint8_t snr = SI47XX_GetSNR();
            uint8_t threshold = 12; // FM:  12dB
            if (si4732mode != SI47XX_FM) {
                threshold = 8; // AM/SSB: 8dB
            }

            if (snr >= threshold) {
                gFM_ScanState = FM_SCAN_OFF;
                if (si4732mode == SI47XX_FM) {
                    gEeprom.FM_FrequencyPlaying = gFmCurrentFrequency;
                }
                gUpdateDisplay = true;
#if defined ENABLE_4732
                gEeprom.FM_FrequencyPlaying = gFmCurrentFrequency;
#else
                gEeprom.FM_FrequencyPlaying = gFmCurrentFrequency; 
#endif
                return;
            }
        } else {
            return; // Czekamy dalej
        }
    }
    
    // Krok 2: Zmiana częstotliwości
#if defined ENABLE_4732
    if (si4732mode != SI47XX_FM) {
        // --- SCAN AM/SSB ---
        uint32_t step = gAmStepsUnits[gAmStepIndex];
        
        if (gFM_ScanState == FM_SCAN_UP) {
            if ((gAmCurrentFrequency_10Hz + step) <= AM_RADIO_MAX_FREQ_10HZ)
                gAmCurrentFrequency_10Hz += step;
            else
                gAmCurrentFrequency_10Hz = AM_RADIO_MIN_FREQ_10HZ; 
        } else { // DOWN
            if (gAmCurrentFrequency_10Hz >= (AM_RADIO_MIN_FREQ_10HZ + step))
                gAmCurrentFrequency_10Hz -= step;
            else
                gAmCurrentFrequency_10Hz = AM_RADIO_MAX_FREQ_10HZ;
        }
        
        FM_TuneAm(); 
        
    } else 
#endif
    {
        // --- SCAN FM ---
        uint16_t step = 10; 
        if (gFM_ScanState == FM_SCAN_UP) {
            if (gFmCurrentFrequency + step <= FM_RADIO_MAX_FREQ)
                gFmCurrentFrequency += step;
            else
                gFmCurrentFrequency = FM_RADIO_MIN_FREQ;
        } else { // DOWN
            if (gFmCurrentFrequency >= FM_RADIO_MIN_FREQ + step)
                gFmCurrentFrequency -= step;
            else
                gFmCurrentFrequency = FM_RADIO_MAX_FREQ;
        }

#if defined ENABLE_4732
        SI47XX_SetFreq(gFmCurrentFrequency);
#else
        BK1080_SetFrequency(gFmCurrentFrequency/10);
#endif
        gEeprom.FM_FrequencyPlaying = gFmCurrentFrequency;
    }

    gScanDelayCounter = SCAN_DWELL_TIME; 
    gUpdateDisplay = true;
}

void FM_Start(void)
{
	gFmRadioMode              = true;
	gFM_ScanState             = FM_SCAN_OFF;
	gFM_RestoreCountdown_10ms = 0;

	gFmPlayCountdown_10ms = 0;
	gScheduleFM           = false;
	gAskToSave            = false;
	
	gFmGain                   = 4;   
	gFmVolume                 = 55;   
	gAgcEnabled               = true; 
	gFmMuteState              = 0;    

#if defined ENABLE_4732
	si4732mode = SI47XX_FM;
	SI47XX_PowerUp();
	setVolume(gFmVolume);
	sendProperty(PROP_FM_DEEMPHASIS, 0x0001);
	SI47XX_SetAutomaticGainControl(! gAgcEnabled, gFmGain); 
	AUDIO_AudioPathOn();	
	gFmCurrentFrequency = gEeprom.FM_FrequencyPlaying;
	SI47XX_SetFreq(gFmCurrentFrequency);
	if(gFmCurrentFrequency>FM_RADIO_MAX_FREQ) gFmCurrentFrequency=gFmCurrentFrequency*10;
		enableRDS();
#else
	BK1080_Init(gEeprom.FM_FrequencyPlaying/10, true);
	AUDIO_AudioPathOn();
	gFmCurrentFrequency = gEeprom.FM_FrequencyPlaying;
#endif

	// Apply default AM/SSB bandwidths if we're not in FM mode now. 
#if defined ENABLE_4732
    if (si4732mode != SI47XX_FM) {
        // if AM
        if (! SI47XX_IsSSB()) {
            SI47XX_SetBandwidth(amBandwidthOptions[gAmBandwidthIndex], true);
        } else {
            SI47XX_SetSsbBandwidth(ssbBandwidthOptions[gSsbBandwidthIndex]);
        }
    }
#endif

	FM_UpdateSignalStrength();

	gDualWatchActive     = false;
	gEnableSpeaker       = true;
	//gUpdateStatus        = true;
	GUI_SelectNextDisplay(DISPLAY_FM);
}