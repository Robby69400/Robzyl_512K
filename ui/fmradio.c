#include <string.h>

#include "app/fm.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "settings.h"
#include "ui/helper.h"
#include "helper/rds.h"   
#include "driver/si473x.h" 

// Deklaracje zewnętrzne getterów z fm.c
extern uint32_t FM_GetAmFrequency(void);
extern uint8_t  FM_GetAmStepIndex(void);
extern uint8_t  gAmBandwidthIndex;
extern uint8_t  gSsbBandwidthIndex;
extern uint8_t  freqEntryState;
extern char     freqEntryBuf[];
extern uint8_t  freqEntryLen;

// Nazwy szerokości pasma dla AM/SSB
static const char * const amBandwidthNames[] = {
    "6k", "4k", "3k", "2k", "1k", "1.8k", "2.5k"
};

static const char * const ssbBandwidthNames[] = {
    "0.5k", "1.0k", "1.2k", "2.2k", "3k", "4k"
};

void UI_DisplayFM(void)
{
	char         String[32];

	memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

	// --- LINE 0: Station Name / Mode (big font) ---
#if defined ENABLE_4732
	if (si4732mode != SI47XX_FM) {
		if (si4732mode == SI47XX_LSB) strcpy(String, "LSB SI4732");
		else if (si4732mode == SI47XX_USB) strcpy(String, "USB SI4732");
		else strcpy(String, "AM SI4732");
		
		UI_PrintString(String, 0, 127, 0, 12);
	}
	else 
	{
		// RDS or Standard FM label
		char ps[16] = {0};
		if (rds. RDSSignal) {
			memcpy(ps, rds.programService, sizeof(rds.programService));
			ps[sizeof(rds.programService)] = '\0';
			char *start = ps;
			while (*start && *start == ' ') start++;
			char *end = start + strlen(start);
			while (end > start && end[-1] == ' ') end--;
			*end = '\0';
			if (*start == '\0') start = NULL;
			
			if (start) {
				strncpy(String, start, sizeof(String)-1);
				String[sizeof(String)-1] = '\0';
			} else {
				String[0] = '\0';
			}
		} else {
			String[0] = '\0';
		}

		// scrolling logic
		static char prevPS[16] = {0};
		static unsigned int scroll_offset = 0;
		static unsigned int scroll_tick = 0;
		const unsigned int scroll_delay = 6; 

		if (strcmp(prevPS, String) != 0) {
			strncpy(prevPS, String, sizeof(prevPS)-1);
			prevPS[sizeof(prevPS)-1] = '\0';
			scroll_offset = 0;
			scroll_tick = 0;
		}

		if (String[0] != '\0') {
			const unsigned int char_width = 12; 
			const unsigned int area_start = 0;
			const unsigned int area_end = 127;
			const unsigned int vis_chars = (area_end - area_start) / char_width; 
			size_t len = strlen(String);

			if (len <= vis_chars || vis_chars == 0) {
				UI_PrintString(String, area_start, area_end, 0, char_width);
			} else {
				if (++scroll_tick >= scroll_delay) {
					scroll_tick = 0;
					scroll_offset++;
					if (scroll_offset >= len) scroll_offset = 0;
				}
				char sub[32];
				for (unsigned int i = 0; i < vis_chars; ++i) {
					sub[i] = String[(scroll_offset + i) % len];
				}
				sub[vis_chars] = '\0';
				UI_PrintString(sub, area_start, area_end, 0, char_width);
			}
		}
		else
		{
			strcpy(String, "FM SI4732");
			UI_PrintString(String, 0, 127, 0, 12);
		}
	}
#else
	strcpy(String, "FM BK1080");
	UI_PrintString(String, 0, 127, 0, 12);
#endif

	// --- LINE 2: Status / Gain / Step / Signal ---
	memset(String, 0, sizeof(String));
	
#if defined ENABLE_4732
	if (si4732mode != SI47XX_FM) {
		// ZMIANA: Lista kroków uwzględniająca 10Hz
		const char *stepNames[] = {"10Hz", "0.1k", "1k", "10k", "100k", "1M"};
		uint8_t idx = FM_GetAmStepIndex();
		if (idx > 5) idx = 2; // Default 1k

		char gainInfo[10];
		if (! gAgcEnabled) sprintf(gainInfo, "G:%u", gFmGain);
		else strcpy(gainInfo, "AGC");

		sprintf(String, "Stp:%s S:%u%% %s", stepNames[idx], gFmSignalStrength, gainInfo);
		UI_PrintStringSmallBuffer(String, gFrameBuffer[2]);
	}
	else {
		// Tryb FM
		const char *mute_str;
		switch (gFmMuteState) {
			case 1: mute_str = "L"; break;
			case 2: mute_str = "R"; break;
			case 3: mute_str = "A"; break;
			default: mute_str = "Off"; break;
		}
		
		char gainInfo[8];
		if (!gAgcEnabled) sprintf(gainInfo, "T:%u", gFmGain);
		else strcpy(gainInfo, "AGC");

		sprintf(String, "%s M:%s S:%u%%", gainInfo, mute_str, gFmSignalStrength);
		UI_PrintStringSmallBuffer(String, gFrameBuffer[2]);
	}
#else
	// BK1080 fallback
	sprintf(String, "Vol:%u", gFmVolume);
	UI_PrintString(String, 0, 127, 2, 10);
#endif

    // --- LINE 3: Extended Signal Info (NEW) ---
#if defined ENABLE_4732
    memset(String, 0, sizeof(String));
    
    if (si4732mode == SI47XX_FM) {
        // FM:  pełne informacje
        sprintf(String, "R:%d S:%d M:%d O:%d", 
            rsqStatus.resp.RSSI, 
            rsqStatus.resp. SNR, 
            rsqStatus.resp.MULT, 
            (int8_t)rsqStatus.resp.FREQOFF);
    } else {
        // AM/SSB: tylko RSSI i SNR, plus BW
        const char *bwString;
        if (SI47XX_IsSSB()) {
            bwString = ssbBandwidthNames[gSsbBandwidthIndex];
        } else {
            bwString = amBandwidthNames[gAmBandwidthIndex];
        }
        sprintf(String, "R:%d S:%d BW:%s", 
            rsqStatus.resp.RSSI, 
            rsqStatus. resp.SNR,
            bwString);
    }
    UI_PrintStringSmallBuffer(String, gFrameBuffer[3]);
#endif


	// --- Display frequency (Lines 4-5) ---
	memset(String, 0, sizeof(String));
#if defined ENABLE_4732
	if (si4732mode != SI47XX_FM) {
		// ZMIANA: Formatowanie 10Hz (XX. XXX. XX)
        // gAmCurrentFrequency_10Hz to np. 700005 (7000.05 kHz)
		uint32_t amFreq = FM_GetAmFrequency(); 
		
		uint32_t mhz = amFreq / 100000;      // 7
		uint32_t rem = amFreq % 100000;      // 00005
		uint32_t khz = rem / 100;            // 000
		uint32_t hz10 = rem % 100;           // 05 (0.05 kHz)

		sprintf(String, "%2u.%03u. %02u", mhz, khz, hz10);
	} else {
		// Format FM
		sprintf(String, "%3d.%d", gFmCurrentFrequency / 100, gFmCurrentFrequency % 100);
	}
#else
	sprintf(String, "%3d.%d", gFmCurrentFrequency / 100, gFmCurrentFrequency % 100);
#endif
	UI_DisplayFrequency(String, 0, 4, false);

	// --- POPUP:  Wyświetlanie wpisywanej częstotliwości ---
#if defined ENABLE_4732
	if (freqEntryState == FREQ_ENTRY_ACTIVE) {
		char popupStr[16];
		if (freqEntryLen > 0) {
			// Skopiuj zawartość bufora
			strncpy(popupStr, freqEntryBuf, freqEntryLen);
			popupStr[freqEntryLen] = '\0';
		} else {
			strcpy(popupStr, "Wpisz czest.");
		}
		UI_DisplayPopup(popupStr);
	}
#endif

	ST7565_BlitFullScreen();
}