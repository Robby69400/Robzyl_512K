//K5 Spectrum
// ============================================================
// SECTION: Includes
// ============================================================
#include "app/spectrum.h"
#include "scanner.h"
#include "driver/backlight.h"
#include "driver/eeprom.h"

#include "ui/helper.h"
#include "common.h"
#include "driver/spi.h"
#include "action.h"
#include "bands.h"
#include "ui/main.h"
//#include "debugging.h"

/*	
          /////////////////////////DEBUG//////////////////////////
          char str[64] = "";sprintf(str, "%d\r\n", Spectrum_state );//LogUart(str);
*/

#ifdef ENABLE_SCREENSHOT
  #include "screenshot.h"
#endif

// ============================================================
// SECTION: Compile-time configuration
// ============================================================
#define MAX_VISIBLE_LINES 6
#define MR_CHANNELS_LIST 15
#define NoisLvl 45
#define NoiseHysteresis 15

// ============================================================
// SECTION: State variables
// ============================================================
static volatile bool gSpectrumChangeRequested = false;
static volatile uint8_t gRequestedSpectrumState = 0;

#ifdef ENABLE_EEPROM_512K
  #define HISTORY_SIZE 100
#else
  #define HISTORY_SIZE 500
#endif

static uint8_t cachedValidScanListCount = 0;
static uint8_t cachedEnabledScanListCount = 0;
static bool scanListCountsDirty = true;

static uint16_t historyListIndex = 0;
static uint16_t indexFs = 0;
static int historyScrollOffset = 0;
static bool gHistoryScan = false; // Indicateur de scan de l'historique
static bool gHistorySortLongPressDone = false; //Żeby długie przytrzymanie nie sortowało w pętli co repeat

/////////////////////////////Parameters://///////////////////////////
//SEE parametersSelectedIndex
// see GetParametersText
static uint8_t DelayRssi = 3;                // case 0       
static uint16_t SpectrumDelay = 0;           // case 1      
static uint16_t MaxListenTime = 0;           // case 2
static uint32_t gScanRangeStart = 1400000;   // case 3      
static uint32_t gScanRangeStop = 13000000;   // case 4
//Step                                       // case 5      
//ListenBW                                   // case 6      
//Modulation                                 // case 7      
static bool Backlight_On_Rx = 0;             // case 8        
static uint16_t SpectrumSleepMs = 0;         // case 9
static uint8_t Noislvl_OFF = NoisLvl;        // case 10
static uint8_t Noislvl_ON = NoisLvl - NoiseHysteresis;
static uint16_t osdPopupSetting = 500;       // case 11
static uint16_t UOO_trigger = 15;            // case 12
static uint8_t AUTO_KEYLOCK = AUTOLOCK_OFF;  // case 13
static uint8_t GlitchMax = 20;               // case 14 
static bool    SoundBoost = 1;               // case 15 
static uint8_t PttEmission = 0;              // case 16   
//ClearHistory                               // case 17      
//ClearSettings                              // case 18      
#define PARAMETER_COUNT 19
////////////////////////////////////////////////////////////////////

static bool SettingsLoaded = false;
uint8_t  gKeylockCountdown = 0;
bool     gIsKeylocked = false;
static uint16_t osdPopupTimer = 0;
static uint32_t Fmax = 0;
static uint32_t spectrumElapsedCount = 0;
static uint32_t SpectrumPauseCount = 0;
static bool SPECTRUM_PAUSED;
static uint8_t IndexMaxLT = 0;
static const char *labels[] = {"OFF","3s","6s","10s","20s", "1m", "5m", "10m", "20m", "30m"};
static const uint16_t listenSteps[] = {0, 3, 6, 10, 20, 60, 300, 600, 1200, 1800}; //in s
#define LISTEN_STEP_COUNT 9

static uint8_t IndexPS = 0;
static const char *labelsPS[] = {"OFF","100ms","500ms", "1s", "2s", "5s"};
static const uint16_t PS_Steps[] = {0, 10, 50, 100, 200, 500}; //in 10 ms
#define PS_STEP_COUNT 5


static uint32_t lastReceivingFreq = 0;
static bool gIsPeak = false;
static bool historyListActive = false;
static bool gForceModulation = 0;
static bool classic = 1;
static uint8_t SpectrumMonitor = 0;
static uint8_t prevSpectrumMonitor = 0;
static bool Key_1_pressed = 0;
static uint16_t WaitSpectrum = 0; 
#define SQUELCH_OFF_DELAY 10;
static bool StorePtt_Toggle_Mode = 0;
static uint8_t ArrowLine = 1;
static void LoadActiveScanFrequencies(void);
static void ToggleRX(bool on);
static void NextScanStep();
static void BuildValidScanListIndices();
static void RenderHistoryList();
static void RenderScanListSelect();
static void RenderParametersSelect();
static void UpdateScan();
static uint8_t bandListSelectedIndex = 0;
static int bandListScrollOffset = 0;
static void RenderBandSelect();
static void ClearHistory();
static void DrawMeter(int);
static void SortHistoryByFrequencyAscending(void); //nowe
static uint8_t scanListSelectedIndex = 0;
static uint8_t scanListScrollOffset = 0;
static uint8_t parametersSelectedIndex = 0;
static uint8_t parametersScrollOffset = 0;
static uint8_t validScanListCount = 0;
static KeyboardState kbd = {KEY_INVALID, KEY_INVALID, 0,0};
struct FrequencyBandInfo {
    uint32_t lower;
    uint32_t upper;
    uint32_t middle;
};
static bool isBlacklistApplied;
static uint32_t cdcssFreq;
static uint16_t ctcssFreq;
//static uint8_t refresh = 0; // СУБТОНО ЗАПРОС ВСЕГДА
#define F_MAX frequencyBandTable[ARRAY_SIZE(frequencyBandTable) - 1].upper
#define Bottom_print 51 //Robby69
static Mode appMode;
#define UHF_NOISE_FLOOR 5

static uint16_t scanChannelsCount;
static void ToggleScanList();
static void SaveSettings();
static const uint16_t RSSI_MAX_VALUE = 255;
static uint16_t R30, R37, R3D, R43, R47, R48, R7E, R02, R3F, R7B, R12, R11, R14, R54, R55, R75;
static char String[100];
static char StringC[10];
static bool isKnownChannel = false;
static uint16_t  gChannel;
static char channelName[12];
ModulationMode_t  channelModulation;
static BK4819_FilterBandwidth_t channelBandwidth;
static bool isInitialized = false;
static bool isListening = true;
static bool newScanStart = true;
static bool audioState = true;
static uint8_t bl;
static State currentState = SPECTRUM, previousState = SPECTRUM;
static uint8_t Spectrum_state = 2; 
static PeakInfo peak;
static ScanInfo scanInfo;
static char latestScanListName[12];
static bool refreshScanListName = true;
static bool IsBlacklisted(uint32_t f);

/* ---- Unified list row descriptor ----
 * Single-line mode: left = left-aligned text, right = right-aligned text.
 * Two-line mode:    left = first display line,  right = second display line.
 */
typedef struct {
    char left[17];   /* Left-aligned text (16 chars + null) */
    char right[14];  /* Right-aligned / second line (13 chars + null) */
} ListRow;

typedef void (*GetListRowFn)(uint16_t index, ListRow *row);


/***************************BIG RAM******************************************/

static	uint32_t ScanFrequencies[MR_CHANNEL_LAST +1];
static uint32_t    HFreqs[HISTORY_SIZE];
static uint8_t     HCount[HISTORY_SIZE];
static bool  HBlacklisted[HISTORY_SIZE];

/****************************************************************************/

SpectrumSettings settings = {stepsCount: STEPS_128,
                             scanStepIndex: S_STEP_500kHz,
                             frequencyChangeStep: 80000,
                             rssiTriggerLevelUp: 20,
                             bw: BK4819_FILTER_BW_WIDE,
                             listenBw: BK4819_FILTER_BW_WIDE,
                             modulationType: false,
                             dbMin: -128,
                             dbMax: 10,
                             scanList: S_SCAN_LIST_ALL,
                             scanListEnabled: {0},
                             bandEnabled: {0}
                            };

static uint32_t currentFreq, tempFreq;
static uint8_t rssiHistory[128];
static int ShowLines = 1;  // СТРОКА ПО УМОЛЧАНИЮ
static uint8_t freqInputIndex = 0;
static uint8_t freqInputDotIndex = 0;
static KEY_Code_t freqInputArr[10];
char freqInputString[11];
static uint8_t nextBandToScanIndex = 0;
static void LookupChannelModulation();

#ifdef ENABLE_SCANLIST_SHOW_DETAIL
  static uint16_t scanListChannels[MR_CHANNEL_LAST+1]; // Array to store Channel indices for selected scanlist
  static uint16_t scanListChannelsCount = 0; // Number of Channels in selected scanlist
  static uint16_t scanListChannelsSelectedIndex = 0;
  static uint16_t scanListChannelsScrollOffset = 0;
  static uint16_t selectedScanListIndex = 0; // Which scanlist we're viewing Channels for
  static void BuildScanListChannels(uint8_t scanListIndex);
  static void RenderScanListChannels();
 // static void RenderScanListChannelsDoubleLines(const char* title, uint8_t numItems, uint8_t selectedIndex, uint8_t scrollOffset);
#endif

  #define MAX_VALID_SCANLISTS 15
static uint8_t validScanListIndices[MAX_VALID_SCANLISTS]; // stocke les index valides
#ifdef ENABLE_SPECTRUM_LINES
static void MyDrawShortHLine(uint8_t y, uint8_t x_start, uint8_t x_end, uint8_t step, bool white); //ПРОСТОЙ РЕЖИМ ЛИНИИ
static void MyDrawVLine(uint8_t x, uint8_t y_start, uint8_t y_end, uint8_t step); //ПРОСТОЙ РЕЖИМ ЛИНИИ
#endif

const RegisterSpec allRegisterSpecs[] = {
 //   {"10_LNAs",  0x10, 8, 0b11,  1},
 //   {"10_LNA",   0x10, 5, 0b111, 1},
 //   {"10_PGA",   0x10, 0, 0b111, 1},
 //   {"10_MIX",   0x10, 3, 0b11,  1},
 //   {"11_LNAs",  0x11, 8, 0b11,  1},
 //   {"11_LNA",   0x11, 5, 0b111, 1},
 //   {"11_PGA",   0x11, 0, 0b111, 1},
 //   {"11_MIX",   0x11, 3, 0b11,  1},
 //   {"12_LNAs",  0x12, 8, 0b11,  1},
 //   {"12_LNA",   0x12, 5, 0b111, 1},
 //   {"12_PGA",   0x12, 0, 0b111, 1},
 //   {"12_MIX",   0x12, 3, 0b11,  1},
    {"13_LNAs",  0x13, 8, 0b11,  1},
    {"13_LNA",   0x13, 5, 0b111, 1},
    {"13_PGA",   0x13, 0, 0b111, 1},
    {"13_MIX",   0x13, 3, 0b11,  1},
 //   {"14_LNAs",  0x14, 8, 0b11,  1},
 //   {"14_LNA",   0x14, 5, 0b111, 1},
 //   {"14_PGA",   0x14, 0, 0b111, 1},
 //   {"14_MIX",   0x14, 3, 0b11,  1},
    {"XTAL F Mode Select", 0x3C, 6, 0b11, 1},
//    {"OFF AF Rx de-emp", 0x2B, 8, 1, 1},
//    {"Gain after FM Demod", 0x43, 2, 1, 1},
    {"RF Tx Deviation", 0x40, 0, 0xFFF, 10},
    {"Compress AF Tx Ratio", 0x29, 14, 0b11, 1},
    {"Compress AF Tx 0 dB", 0x29, 7, 0x7F, 1},
    {"Compress AF Tx noise", 0x29, 0, 0x7F, 1},
    {"MIC AGC Disable", 0x19, 15, 1, 1},
    {"AFC Range Select", 0x73, 11, 0b111, 1},
    {"AFC Disable", 0x73, 4, 1, 1},
    {"AFC Speed", 0x73, 5, 0b111111, 1},
//   {"IF step100x", 0x3D, 0, 0xFFFF, 100},
//   {"IF step1x", 0x3D, 0, 0xFFFF, 1},
//   {"RFfiltBW1.7-4.5khz ", 0x43, 12, 0b111, 1},
//   {"RFfiltBWweak1.7-4.5khz", 0x43, 9, 0b111, 1},
//   {"BW Mode Selection", 0x43, 4, 0b11, 1},
//   {"XTAL F Low-16bits", 0x3B, 0, 0xFFFF, 1},
//   {"XTAL F Low-16bits 100", 0x3B, 0, 0xFFFF, 100},
//   {"XTAL F High-8bits", 0x3C, 8, 0xFF, 1},
//   {"XTAL F reserved flt", 0x3C, 0, 0b111111, 1},
//   {"XTAL Enable", 0x37, 1, 1, 1},
//   {"ANA LDO Selection", 0x37, 11, 1, 1},
//   {"VCO LDO Selection", 0x37, 10, 1, 1},
//   {"RF LDO Selection", 0x37, 9, 1, 1},
//   {"PLL LDO Selection", 0x37, 8, 1, 1},
//   {"ANA LDO Bypass", 0x37, 7, 1, 1},
//   {"VCO LDO Bypass", 0x37, 6, 1, 1},
//   {"RF LDO Bypass", 0x37, 5, 1, 1},
//   {"PLL LDO Bypass", 0x37, 4, 1, 1},
//   {"Freq Scan Indicator", 0x0D, 15, 1, 1},
//   {"F Scan High 16 bits", 0x0D, 0, 0xFFFF, 1},
//   {"F Scan Low 16 bits", 0x0E, 0, 0xFFFF, 1},
//   {"AGC fix", 0x7E, 15, 0b1, 1},
//   {"AGC idx", 0x7E, 12, 0b111, 1},
//   {"49", 0x49, 0, 0xFFFF, 100},
//   {"7B", 0x7B, 0, 0xFFFF, 100},
//   {"rssi_rel", 0x65, 8, 0xFF, 1},
//   {"agc_rssi", 0x62, 8, 0xFF, 1},
//   {"lna_peak_rssi", 0x62, 0, 0xFF, 1},
//   {"rssi_sq", 0x67, 0, 0xFF, 1},
//   {"weak_rssi 1", 0x0C, 7, 1, 1},
//   {"ext_lna_gain set", 0x2C, 0, 0b11111, 1},
//   {"snr_out", 0x61, 8, 0xFF, 1},
//   {"noise sq", 0x65, 0, 0xFF, 1},
//   {"glitch", 0x63, 0, 0xFF, 1},
//   {"soft_mute_en 1", 0x20, 12, 1, 1},
//   {"SNR Threshold SoftMut", 0x20, 0, 0b111111, 1},
//   {"soft_mute_atten", 0x20, 6, 0b11, 1},
//   {"soft_mute_rate", 0x20, 8, 0b11, 1},
//   {"Band Selection Thr", 0x3E, 0, 0xFFFF, 100},
//   {"chip_id", 0x00, 0, 0xFFFF, 1},
//   {"rev_id", 0x01, 0, 0xFFFF, 1},
//   {"aerror_en 0am 1fm", 0x30, 9, 1, 1},
//   {"bypass 1tx 0rx", 0x47, 0, 1, 1},
//   {"bypass tx gain 1", 0x47, 1, 1, 1},
//   {"bps afdac 3tx 9rx ", 0x47, 8, 0b1111, 1},
//   {"bps tx dcc=0 ", 0x7E, 3, 0b111, 1},
//   {"audio_tx_mute1", 0x50, 15, 1, 1},
//   {"audio_tx_limit_bypass1", 0x50, 10, 1, 1},
//  {"audio_tx_limit320", 0x50, 0, 0x3FF, 1},
//   {"audio_tx_limit reserved7", 0x50, 11, 0b1111, 1},
//   {"audio_tx_path_sel", 0x2D, 2, 0b11, 1},
//   {"AFTx Filt Bypass All", 0x47, 0, 1, 1},
   {"3kHz AF Resp K Tx", 0x74, 0, 0xFFFF, 100},
//   {"MIC Sensit Tuning", 0x7D, 0, 0b11111, 1},
//   {"DCFiltBWTxMICIn15-480hz", 0x7E, 3, 0b111, 1},
//   {"04 768", 0x04, 0, 0x0300, 1},
//   {"43 32264", 0x43, 0, 0x7E08, 1},
//   {"4b 58434", 0x4b, 0, 0xE442, 1},
//   {"73 22170", 0x73, 0, 0x569A, 1},
//   {"7E 13342", 0x7E, 0, 0x341E, 1},
//   {"47 26432 24896", 0x47, 0, 0x6740, 1},
//   {"03 49662 49137", 0x30, 0, 0xC1FE, 1},
//   {"Enable Compander", 0x31, 3, 1, 1},
//   {"Band-Gap Enable", 0x37, 0, 1, 1},
//   {"IF step100x", 0x3D, 0, 0xFFFF, 100},
//   {"IF step1x", 0x3D, 0, 0xFFFF, 1},
//   {"Band Selection Thr", 0x3E, 0, 0xFFFF, 1},
//   {"RF filt BW ", 0x43, 12, 0b111, 1},
//   {"RF filt BW weak", 0x43, 9, 0b111, 1},
//   {"BW Mode Selection", 0x43, 4, 0b11, 1},
//   {"AF Output Inverse", 0x47, 13, 1, 1},
//   {"AF ALC Disable", 0x4B, 5, 1, 1},
//   {"AGC Fix Mode", 0x7E, 15, 1, 1},
//   {"AGC Fix Index", 0x7E, 12, 0b111, 1},
//   {"Crystal vReg Bit", 0x1A, 12, 0b1111, 1},
//   {"Crystal iBit", 0x1A, 8, 0b1111, 1},
//   {"PLL CP bit", 0x1F, 0, 0b1111, 1},
//   {"PLL/VCO Enable", 0x30, 4, 0xF, 1},
//   {"Exp AF Rx Ratio", 0x28, 14, 0b11, 1},
//   {"Exp AF Rx 0 dB", 0x28, 7, 0x7F, 1},
//   {"Exp AF Rx noise", 0x28, 0, 0x7F, 1},
//   {"OFF AFRxHPF300 flt", 0x2B, 10, 1, 1},
//   {"OFF AF RxLPF3K flt", 0x2B, 9, 1, 1},
//   {"AF Rx Gain1", 0x48, 10, 0x11, 1},
//   {"AF Rx Gain2", 0x48, 4, 0b111111, 1},
//   {"AF DAC G after G1 G2", 0x48, 0, 0b1111, 1},
     {"300Hz AF Resp K Tx", 0x44, 0, 0xFFFF, 100},
     {"300Hz AF Resp K Tx", 0x45, 0, 0xFFFF, 100},
//   {"DC Filt BW Rx IF In", 0x7E, 0, 0b111, 1},
//   {"OFF AFTxHPF300filter", 0x2B, 2, 1, 1},
//   {"OFF AFTxLPF1filter", 0x2B, 1, 1, 1},
//   {"OFF AFTxpre-emp flt", 0x2B, 0, 1, 1},
//   {"PA Gain Enable", 0x30, 3, 1, 1},
//   {"PA Biasoutput 0~3", 0x36, 8, 0xFF, 1},
//   {"PA Gain1 Tuning", 0x36, 3, 0b111, 1},
//   {"PA Gain2 Tuning", 0x36, 0, 0b111, 1},
//   {"RF TxDeviation ON", 0x40, 12, 1, 1},
//   {"AFTxLPF2fltBW1.7-4.5khz", 0x43, 6, 0b111, 1}, 
     {"300Hz AF Resp K Rx", 0x54, 0, 0xFFFF, 100},
     {"300Hz AF Resp K Rx", 0x55, 0, 0xFFFF, 100},
     {"3kHz AF Resp K Rx", 0x75, 0, 0xFFFF, 100},
};

#define STILL_REGS_MAX_LINES 3
static uint8_t stillRegSelected = 0;
static uint8_t stillRegScroll = 0;
static bool stillEditRegs = false; // false = edycja czestotliwosci, true = edycja rejestrow

uint16_t statuslineUpdateTimer = 0;

static void RelaunchScan();
static void ResetInterrupts();
static char StringCode[10] = "";

static bool parametersStateInitialized = false;

//
static char osdPopupText[32] = "";

// 
static void ShowOSDPopup(const char *str)
{   osdPopupTimer = osdPopupSetting;
    strncpy(osdPopupText, str, sizeof(osdPopupText)-1);
    osdPopupText[sizeof(osdPopupText)-1] = '\0';
}

static uint32_t stillFreq = 0;
static uint32_t GetInitialStillFreq(void) {
    uint32_t f = 0;

    if (historyListActive) {
        f = HFreqs[historyListIndex];
    } else if (SpectrumMonitor) {
        f = lastReceivingFreq;
    } else if (gIsPeak) {
        f = peak.f;
    } else {
        f = scanInfo.f;
    }

    if (f < 1400000 || f > 130000000) {
        if (scanInfo.f >= 1400000 && scanInfo.f <= 130000000) return scanInfo.f;
        if (currentFreq >= 1400000 && currentFreq <= 130000000) return currentFreq;
        return gScanRangeStart; // ostateczny fallback
    }

    return f;
}

static uint16_t GetRegMenuValue(uint8_t st) {
  RegisterSpec s = allRegisterSpecs[st];
  return (BK4819_ReadRegister(s.num) >> s.offset) & s.mask;
}

static void SetRegMenuValue(uint8_t st, bool add) {
  uint16_t v = GetRegMenuValue(st);
  RegisterSpec s = allRegisterSpecs[st];

  uint16_t reg = BK4819_ReadRegister(s.num);
  if (add && v <= s.mask - s.inc) {
    v += s.inc;
  } else if (!add && v >= 0 + s.inc) {
    v -= s.inc;
  }
  reg &= ~(s.mask << s.offset);
  BK4819_WriteRegister(s.num, reg | (v << s.offset));
  
}

KEY_Code_t GetKey() {
  KEY_Code_t btn = KEYBOARD_Poll();
  // Gestion PTT existante
  if (btn == KEY_INVALID && !GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT)) {
    btn = KEY_PTT;
  }
  return btn;
}

static int clamp(int v, int min, int max) {
  return v <= min ? min : (v >= max ? max : v);
}

static void SetState(State state) {
  previousState = currentState;
  currentState = state;
  
  
}

// ============================================================
// SECTION: Radio / hardware functions
// ============================================================

static void BackupRegisters() {
  R30 = BK4819_ReadRegister(BK4819_REG_30);
  R37 = BK4819_ReadRegister(BK4819_REG_37);
  R3D = BK4819_ReadRegister(BK4819_REG_3D);
  R43 = BK4819_ReadRegister(BK4819_REG_43);
  R47 = BK4819_ReadRegister(BK4819_REG_47);
  R48 = BK4819_ReadRegister(BK4819_REG_48);
  R7E = BK4819_ReadRegister(BK4819_REG_7E);
  R02 = BK4819_ReadRegister(BK4819_REG_02);
  R3F = BK4819_ReadRegister(BK4819_REG_3F);
  R7B = BK4819_ReadRegister(BK4819_REG_7B);
  R12 = BK4819_ReadRegister(BK4819_REG_12);
  R11 = BK4819_ReadRegister(BK4819_REG_11);
  R14 = BK4819_ReadRegister(BK4819_REG_14);
  R54 = BK4819_ReadRegister(BK4819_REG_54);
  R55 = BK4819_ReadRegister(BK4819_REG_55);
  R75 = BK4819_ReadRegister(BK4819_REG_75);
}

static void RestoreRegisters() {
  BK4819_WriteRegister(BK4819_REG_30, R30);
  BK4819_WriteRegister(BK4819_REG_37, R37);
  BK4819_WriteRegister(BK4819_REG_3D, R3D);
  BK4819_WriteRegister(BK4819_REG_43, R43);
  BK4819_WriteRegister(BK4819_REG_47, R47);
  BK4819_WriteRegister(BK4819_REG_48, R48);
  BK4819_WriteRegister(BK4819_REG_7E, R7E);
  BK4819_WriteRegister(BK4819_REG_02, R02);
  BK4819_WriteRegister(BK4819_REG_3F, R3F);
  BK4819_WriteRegister(BK4819_REG_7B, R7B);
  BK4819_WriteRegister(BK4819_REG_12, R12);
  BK4819_WriteRegister(BK4819_REG_11, R11);
  BK4819_WriteRegister(BK4819_REG_14, R14);
  BK4819_WriteRegister(BK4819_REG_54, R54);
  BK4819_WriteRegister(BK4819_REG_55, R55);
  BK4819_WriteRegister(BK4819_REG_75, R75);
}

static void ToggleAFBit(bool on) {
  uint32_t reg = regs_cache[BK4819_REG_47]; //KARINA mod
  reg &= ~(1 << 8);
  if (on)
    reg |= on << 8;
  BK4819_WriteRegister(BK4819_REG_47, reg);
}

static void ToggleAFDAC(bool on) {
  uint32_t Reg = regs_cache[BK4819_REG_30]; //KARINA mod
  Reg &= ~(1 << 9);
  if (on)
    Reg |= (1 << 9);
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}

static void SetF(uint32_t sf) {
  uint32_t f = sf;
  if (f < 1400000 || f > 130000000) return;
  if (SPECTRUM_PAUSED) return;
  BK4819_SetFrequency(f);
  BK4819_PickRXFilterPathBasedOnFrequency(f);
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
  BK4819_WriteRegister(BK4819_REG_30, 0);
  BK4819_WriteRegister(BK4819_REG_30, reg);
}

static void ResetInterrupts()
{
  // disable interupts
  BK4819_WriteRegister(BK4819_REG_3F, 0);
  // reset the interrupt
  BK4819_WriteRegister(BK4819_REG_02, 0);
}

// scan step in 0.01khz
static uint32_t GetScanStep() { return scanStepValues[settings.scanStepIndex]; }

static uint16_t GetStepsCount() 
{ 
  if (appMode==CHANNEL_MODE)
  {
    return scanChannelsCount;
  }
  if(appMode==SCAN_RANGE_MODE) {
    return ((gScanRangeStop - gScanRangeStart) / GetScanStep()); //Robby69
  }
  if (appMode==SCAN_BAND_MODE) {return (gScanRangeStop - gScanRangeStart) / scanInfo.scanStep;}
  
  return 128 >> settings.stepsCount;
}

static uint32_t GetBW() { return GetStepsCount() * GetScanStep(); }

static uint16_t GetRandomChannelFromRSSI(uint16_t maxChannels) {
  uint32_t rssi = rssiHistory[1]*rssiHistory[maxChannels/2];
  if (maxChannels == 0 || rssi == 0) {
        return 1;  // Fallback to chanel 1 if invalid input
    }
    // Scale RSSI to [1, maxChannels]
    return 1 + (rssi % maxChannels);
}

static void DeInitSpectrum(bool ComeBack) {
  
  RestoreRegisters();
  gVfoConfigureMode = VFO_CONFIGURE;
  isInitialized = false;
  SetState(SPECTRUM);
  if(!ComeBack) {
    uint8_t Spectrum_state = 0; //Spectrum Not Active
    EEPROM_WriteBuffer(0x1D00, &Spectrum_state);
    ToggleRX(0);
    SYSTEM_DelayMs(50);
    }
    
  else {
    EEPROM_ReadBuffer(0x1D00, &Spectrum_state, 1);
	  Spectrum_state+=10;
    EEPROM_WriteBuffer(0x1D00, &Spectrum_state);
    StorePtt_Toggle_Mode = Ptt_Toggle_Mode;
    SYSTEM_DelayMs(50);
    Ptt_Toggle_Mode =0;
    }
}

/////////////////////////////EEPROM://///////////////////////////

static void TrimTrailingChars(char *str) {
    int len = strlen(str);
    while (len > 0) {
        unsigned char c = str[len - 1];
        if (c == '\0' || c == 0x20 || c == 0xFF)  // fin de chaîne, espace, EEPROM vide
            len--;
        else
            break;
    }
    str[len] = '\0';
}


static void ReadChannelName(uint16_t Channel, char *name) {
    EEPROM_ReadBuffer(ADRESS_NAMES + Channel * 16, (uint8_t *)name, 12);
    TrimTrailingChars(name);
}


static void DeleteHistoryItem(void) {
    if (!historyListActive || indexFs == 0) return;
    if (historyListIndex >= indexFs) {
        historyListIndex = (indexFs > 0) ? indexFs - 1 : 0;
        if (indexFs == 0) return;
    }
    uint16_t indexToDelete = historyListIndex;
    for (uint16_t i = indexToDelete; i < indexFs - 1; i++) {
        HFreqs[i]       = HFreqs[i + 1];
        HCount[i]       = HCount[i + 1];
        HBlacklisted[i] = HBlacklisted[i + 1];
    }
    indexFs--;
    
    HFreqs[indexFs]       = 0;
    HCount[indexFs]       = 0;
    HBlacklisted[indexFs] = 0xFF;

    if (historyListIndex >= indexFs && indexFs > 0) {
        historyListIndex = indexFs - 1;
    } else if (indexFs == 0) {
        historyListIndex = 0;
    }
    ShowOSDPopup("Deleted");
    
}


#include "settings.h"

static void SaveHistoryToFreeChannel(void) {
    if (!historyListActive) return;

    uint32_t f = HFreqs[historyListIndex];
    if (f < 1000000) return;
    char str[32];
    for (int i = 0; i < MR_CHANNEL_LAST; i++) {
        uint32_t freqInMem;
        EEPROM_ReadBuffer(ADRESS_FREQ_PARAMS + (i * 16), (uint8_t *)&freqInMem, 4);
        if (freqInMem != 0xFFFFFFFF && freqInMem == f) {
            sprintf(str, "Exist CH %d", i + 1);
            ShowOSDPopup(str);
            return;
        }
    }
    int freeCh = -1;
    for (int i = 0; i < MR_CHANNEL_LAST; i++) {
        uint8_t checkByte;
        EEPROM_ReadBuffer(ADRESS_FREQ_PARAMS + (i * 16), &checkByte, 1);
        if (checkByte == 0xFF) { 
            freeCh = i;
            break;
        }
    }

    if (freeCh != -1) {
        VFO_Info_t tempVFO;
        memset(&tempVFO, 0, sizeof(tempVFO)); 
        tempVFO.freq_config_RX.Frequency = f;
        tempVFO.freq_config_TX.Frequency = f; 
        tempVFO.TX_OFFSET_FREQUENCY = 0;
        tempVFO.Modulation = settings.modulationType;
        tempVFO.CHANNEL_BANDWIDTH = settings.listenBw; 
        tempVFO.OUTPUT_POWER = OUTPUT_POWER_LOW;
        tempVFO.STEP_SETTING = STEP_12_5kHz; 
        SETTINGS_SaveChannel(freeCh, &tempVFO, 2);
        LoadActiveScanFrequencies();
        sprintf(str, "SAVED TO CH %d", freeCh + 1);
        ShowOSDPopup(str);
    } else {
        ShowOSDPopup("MEMORY FULL");
    }
}

typedef struct HistoryStruct {
    uint32_t HFreqs;
    uint8_t HCount;
    uint8_t HBlacklisted;
} HistoryStruct;


#ifdef ENABLE_EEPROM_512K
static bool historyLoaded = false; // flaga stanu wczytania histotii spectrum

void ReadHistory(void) {
    HistoryStruct History = {0};
    for (uint16_t position = 0; position < HISTORY_SIZE; position++) {
        EEPROM_ReadBuffer(ADRESS_HISTORY + position * sizeof(HistoryStruct),
                          (uint8_t *)&History, sizeof(HistoryStruct));

        // Stop si marque de fin trouvée
        if (History.HBlacklisted == 0xFF) {
            indexFs = position;
            break;
        }
      if (History.HFreqs){
        HFreqs[position] = History.HFreqs;
        HCount[position] = History.HCount;
        HBlacklisted[position] = History.HBlacklisted;
        indexFs = position + 1;
      }
    }
}


void WriteHistory(void) {
    HistoryStruct History = {0};
    for (uint16_t position = 0; position < indexFs; position++) {
        History.HFreqs = HFreqs[position];
        History.HCount = HCount[position];
        History.HBlacklisted = HBlacklisted[position];
        EEPROM_WriteBuffer(ADRESS_HISTORY + position * sizeof(HistoryStruct),
                           (uint8_t *)&History);
    }

    // Marque de fin (HBlacklisted = 0xFF)
    History.HFreqs = 0;
    History.HCount = 0;
    History.HBlacklisted = 0xFF;
    EEPROM_WriteBuffer(ADRESS_HISTORY + indexFs * sizeof(HistoryStruct),
                       (uint8_t *)&History);
    
    ShowOSDPopup("HISTORY SAVED");
}
#endif

static void ExitAndCopyToVfo() {
  RestoreRegisters();
if (historyListActive){
      SETTINGS_SetVfoFrequency(HFreqs[historyListIndex]); 
      gTxVfo->Modulation = MODULATION_FM;
      gRequestSaveChannel = 1;
      DeInitSpectrum(0);
}
  switch (currentState) {
    case SPECTRUM:
        if (PttEmission ==1 && scanChannelsCount > 0){
            uint16_t randomChannel = GetRandomChannelFromRSSI(scanChannelsCount);
            uint32_t rndfreq = 0;
            uint16_t attempts = 0;
            SpectrumDelay = 0; //not compatible with ninja
                while (attempts < scanChannelsCount) {
                    rndfreq = ScanFrequencies[randomChannel];
                    if (rssiHistory[randomChannel] <= 120 && rndfreq) {break;}
                    attempts++;
                    randomChannel = (randomChannel + 1) % scanChannelsCount;
                }
                if (rndfreq) {
                    gCurrentVfo->freq_config_TX.Frequency = rndfreq;
                    gCurrentVfo->freq_config_RX.Frequency = rndfreq;
                    gEeprom.MrChannel     = randomChannel;
                    gEeprom.ScreenChannel = randomChannel;
                    gCurrentVfo->Modulation   = MODULATION_FM;
                    gCurrentVfo->STEP_SETTING = STEP_0_01kHz;
                    gRequestSaveChannel       = 1;
                }
        }
        if (PttEmission == 2) {
            SpectrumDelay = 0;
            uint16_t ExitCh = BOARD_gMR_fetchChannel(lastReceivingFreq);
            if (ExitCh == 0xFFFF) {
                SETTINGS_SetVfoFrequency(HFreqs[historyListIndex]);
                gTxVfo->STEP_SETTING = STEP_0_01kHz;
                gTxVfo->Modulation = MODULATION_FM;
                gTxVfo->OUTPUT_POWER = OUTPUT_POWER_HIGH;
                COMMON_SwitchToVFOMode();
            } else {
            gTxVfo->freq_config_RX.Frequency = HFreqs[historyListIndex];
            gEeprom.ScreenChannel = ExitCh;
            gEeprom.MrChannel = ExitCh;
            COMMON_SwitchToChannelMode();
            }
          gRequestSaveChannel = 1;
          }

      DeInitSpectrum(1);
      break;      
    
    default:
      DeInitSpectrum(0);
      break;
  }
  
    SYSTEM_DelayMs(200);
    isInitialized = false;
}

static uint16_t GetRssi(void) {
    uint16_t rssi;
    //BK4819_ReadRegister(0x63);
    if (isListening) SYSTICK_DelayUs(12000); 
    else SYSTICK_DelayUs(DelayRssi * 1000);
    rssi = BK4819_GetRSSI();
    if (FREQUENCY_GetBand(scanInfo.f) > BAND4_174MHz) {rssi += UHF_NOISE_FLOOR;}
    BK4819_ReadRegister(0x63);
  return rssi;
}

static void ToggleAudio(bool on) {
  if (on == audioState) {
    return;
  }
  audioState = on;
  if (on) {
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  } else {
    GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  }
}

static uint16_t CountValidHistoryItems() {
    return (indexFs > HISTORY_SIZE) ? HISTORY_SIZE : indexFs;
}

static void FillfreqHistory(void)
{
    uint32_t f = peak.f;
    if (f == 0 || f < 1400000 || f > 130000000) return;

    uint16_t count = CountValidHistoryItems();
    uint16_t found = HISTORY_SIZE;

    for (uint16_t i = 0; i < count; i++) {
        if (HFreqs[i] == f) {
            found = i;
            break;
        }
    }

    if (found < count) {
        uint8_t cnt = HCount[found];
        bool blacklisted = HBlacklisted[found];

        if (lastReceivingFreq != f && cnt < 255) {
            cnt++;
    }

        for (uint16_t i = found; i > 0; i--) {
        HFreqs[i]       = HFreqs[i - 1];
        HCount[i]       = HCount[i - 1];
        HBlacklisted[i] = HBlacklisted[i - 1];
    }

        HFreqs[0]       = f;
        HCount[0]       = cnt;
        HBlacklisted[0] = blacklisted;
    } else {
        uint16_t last = (count < HISTORY_SIZE) ? count : (HISTORY_SIZE - 1);

        for (uint16_t i = last; i > 0; i--) {
            HFreqs[i]       = HFreqs[i - 1];
            HCount[i]       = HCount[i - 1];
            HBlacklisted[i] = HBlacklisted[i - 1];
        }

        HFreqs[0]       = f;
        HCount[0]       = 1;
        HBlacklisted[0] = 0;

    if (count < HISTORY_SIZE) {
        indexFs = count + 1;
    } else {
        indexFs = HISTORY_SIZE;
    }
    }

    lastReceivingFreq = f;
    historyListIndex = 0;
    historyScrollOffset = 0;
} 

static void ToggleRX(bool on) {
    if (SPECTRUM_PAUSED) return;
    if(!on && SpectrumMonitor == 2) {isListening = 1;return;}
    isListening = on;

    if (on && isKnownChannel) {
        if(!gForceModulation) settings.modulationType = channelModulation;
        BK4819_InitAGCSpectrum(settings.modulationType);
    }
    else if(on && appMode == SCAN_BAND_MODE) {
            if (!gForceModulation) settings.modulationType = BParams[bl].modulationType;
            BK4819_InitAGCSpectrum(settings.modulationType);
          }
    
    if (on) { 
        Fmax = peak.f;
        SPI0_Init(64);
        BK4819_WriteRegister(BK4819_REG_37, 0x1D0F);
        SYSTEM_DelayMs(20);
        RADIO_SetModulation(settings.modulationType);
        BK4819_SetFilterBandwidth(settings.listenBw, false);
        BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_02_CxCSS_TAIL);

    } else { 
        SPI0_Init(2);
        RADIO_SetModulation(MODULATION_FM); //Test for Kolyan
        BK4819_SetFilterBandwidth(BK4819_FILTER_BW_WIDE, false); //Scan in 25K bandwidth
        //if(appMode!=CHANNEL_MODE) BK4819_WriteRegister(0x43, GetBWRegValueForScan());
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, 0);
    }
    if (on != audioState) {
        ToggleAudio(on);
        ToggleAFDAC(on);
        ToggleAFBit(on);
    }
    
}


static void ResetScanStats() {
  scanInfo.rssiMax = scanInfo.rssiMin + 20 ; 
}

static bool InitScan() {
    ResetScanStats();
    scanInfo.i = 0;
    peak.i = 0; // To check
    peak.f = 0; // To check
    
    bool scanInitializedSuccessfully = false;

    if (appMode == SCAN_BAND_MODE) {
        uint8_t checkedBandCount = 0;
        while (checkedBandCount < MAX_BANDS) { 
            if (settings.bandEnabled[nextBandToScanIndex]) {
                bl = nextBandToScanIndex; 
                scanInfo.f = BParams[bl].Startfrequency;
                scanInfo.scanStep = scanStepValues[BParams[bl].scanStep];
                settings.scanStepIndex = BParams[bl].scanStep; 
                if(BParams[bl].Startfrequency>0) gScanRangeStart = BParams[bl].Startfrequency;
                if(BParams[bl].Stopfrequency>0)  gScanRangeStop = BParams[bl].Stopfrequency;
                if (!gForceModulation) settings.modulationType = BParams[bl].modulationType;
                nextBandToScanIndex = (nextBandToScanIndex + 1) % MAX_BANDS;
                scanInitializedSuccessfully = true;
                break;
            }
            nextBandToScanIndex = (nextBandToScanIndex + 1) % MAX_BANDS;
            checkedBandCount++;
        }
    } else {
        if(gScanRangeStart > gScanRangeStop)
		    SWAP(gScanRangeStart, gScanRangeStop);
        scanInfo.f = gScanRangeStart;
        scanInfo.scanStep = GetScanStep();
        scanInitializedSuccessfully = true;
      }

    if (appMode == CHANNEL_MODE) {
        if (scanChannelsCount == 0) {
            return false;
        }
        scanInfo.f = ScanFrequencies[0];
        peak.f = scanInfo.f;
        peak.i = 0;
    }

    return scanInitializedSuccessfully;
}

// resets modifiers like blacklist, attenuation, normalization
static void ResetModifiers() {
  memset(StringC, 0, sizeof(StringC)); 
  for (int i = 0; i < 128; ++i) {
    if (rssiHistory[i] == RSSI_MAX_VALUE) rssiHistory[i] = 0;
  }
  LoadActiveScanFrequencies();
  RelaunchScan();
}

static void RelaunchScan() {
    InitScan();
    ToggleRX(false);
    scanInfo.rssiMin = RSSI_MAX_VALUE;
    gIsPeak = false;
}

static void UpdateNoiseOff(){
  if( BK4819_GetExNoiseIndicator() > Noislvl_OFF) {gIsPeak = false;ToggleRX(0);}		
}

static void UpdateNoiseOn(){
	if( BK4819_GetExNoiseIndicator() < Noislvl_ON) {gIsPeak = true;ToggleRX(1);}
}

static void UpdateScanInfo() {
  if (scanInfo.rssi > scanInfo.rssiMax) {
    scanInfo.rssiMax = scanInfo.rssi;
  }
  if (scanInfo.rssi < scanInfo.rssiMin && scanInfo.rssi > 0) {
    scanInfo.rssiMin = scanInfo.rssi;
  }
}
static void UpdateGlitch() {
    uint8_t glitch = BK4819_GetGlitchIndicator();
    if (glitch > GlitchMax) {gIsPeak = false;} 
    else {gIsPeak = true;}// if glitch is too high, receiving stopped
}

static void Measure() {
    static int16_t previousRssi = 0;
    static bool isFirst = true;
    uint16_t rssi = scanInfo.rssi = GetRssi();
    UpdateScanInfo();
    if (scanInfo.f % 1300000 == 0 || IsBlacklisted(scanInfo.f)) rssi = scanInfo.rssi = 0;

    if (isFirst) {
        previousRssi = rssi;
        gIsPeak      = false;
        isFirst      = false;
    }
    if (settings.rssiTriggerLevelUp == 50 && rssi > previousRssi + UOO_trigger) {
      peak.f = scanInfo.f;
      peak.i = scanInfo.i;
      FillfreqHistory();
    }

    if (!gIsPeak && rssi > previousRssi + settings.rssiTriggerLevelUp) {
        SYSTEM_DelayMs(10);
        
        uint16_t rssi2 = scanInfo.rssi = GetRssi();
        if (rssi2 > rssi+10) {
          peak.f = scanInfo.f;
            peak.i = scanInfo.i-1;
        }
        if (settings.rssiTriggerLevelUp < 50) {
            gIsPeak = true;
            UpdateNoiseOff();
            UpdateGlitch();
        } 
    } 
    if (!gIsPeak || !isListening) previousRssi = rssi;
    else if (rssi < previousRssi) previousRssi = rssi;

    uint16_t count = GetStepsCount();
    uint16_t i = scanInfo.i-1;

    if (count > 128) {
        uint16_t pixel = (uint32_t)i * 128 / count;
        if (pixel < 128) { rssiHistory[pixel] = rssi;}
    } else {
        uint16_t j;
          uint16_t base = 128 / count;
          uint16_t rem  = 128 % count;
        uint16_t start = i * base + (i < rem ? i : rem);
        uint16_t end   = (i + 1) * base + ((i + 1) < rem ? (i + 1) : rem);
        if (end > 128) end = 128;
        for (j = start; j < end; ++j) {
            rssiHistory[j] = rssi;
            //char str[64] = "";sprintf(str, "M %d %d %d\r\n", scanInfo.i,j, scanInfo.f);LogUart(str);
        }
    }
}

static void UpdateDBMaxAuto() { //Zoom
  static uint8_t z = 5;
  int newDbMax;
    if (scanInfo.rssiMax > 0) {
        newDbMax = clamp(Rssi2DBm(scanInfo.rssiMax), -80, 0);
        newDbMax = Rssi2DBm(scanInfo.rssiMax);

        if (newDbMax > settings.dbMax + z) {
            settings.dbMax = settings.dbMax + z;   // montée limitée
        } else if (newDbMax < settings.dbMax - z) {
            settings.dbMax = settings.dbMax - z;   // descente limitée
        } else {
            settings.dbMax = newDbMax;              // suivi normal
        }
    }

    if (scanInfo.rssiMin > 0) {
        settings.dbMin = clamp(Rssi2DBm(scanInfo.rssiMin), -160, -120);
        settings.dbMin = Rssi2DBm(scanInfo.rssiMin);
    }
}

static void AutoAdjustFreqChangeStep() {
  settings.frequencyChangeStep = gScanRangeStop - gScanRangeStart;
}

static void UpdateScanStep(bool inc) {
if (inc) {
    settings.scanStepIndex = (settings.scanStepIndex >= S_STEP_500kHz) 
                          ? S_STEP_0_01kHz 
                          : settings.scanStepIndex + 1;
} else {
    settings.scanStepIndex = (settings.scanStepIndex <= S_STEP_0_01kHz) 
                          ? S_STEP_500kHz 
                          : settings.scanStepIndex - 1;
}
  AutoAdjustFreqChangeStep();
  scanInfo.scanStep = settings.scanStepIndex;
}

static void UpdateCurrentFreq(bool inc) {
  if (inc && currentFreq < F_MAX) {
    gScanRangeStart += settings.frequencyChangeStep;
    gScanRangeStop += settings.frequencyChangeStep;
  } else if (!inc && currentFreq > settings.frequencyChangeStep) {
    gScanRangeStart -= settings.frequencyChangeStep;
    gScanRangeStop -= settings.frequencyChangeStep;
  } else {
    return;
  }
  ResetModifiers();
  
}

static void ToggleModulation() {
  if (settings.modulationType < MODULATION_UKNOWN - 1) {
    settings.modulationType++;
  } else {
    settings.modulationType = MODULATION_FM;
  }
  RADIO_SetModulation(settings.modulationType);
  BK4819_InitAGCSpectrum(settings.modulationType);
  gForceModulation = 1;
}

static void ToggleListeningBW(bool inc) {
  settings.listenBw = ACTION_NextBandwidth(settings.listenBw, false, inc);
  BK4819_SetFilterBandwidth(settings.listenBw, false);
  
}

static void ToggleStepsCount() {
  if (settings.stepsCount == STEPS_128) {
    settings.stepsCount = STEPS_16;
  } else {
    settings.stepsCount--;
  }
  AutoAdjustFreqChangeStep();
  ResetModifiers();
  
}

static void ResetFreqInput() {
  tempFreq = 0;
  for (int i = 0; i < 10; ++i) {
    freqInputString[i] = '-';
  }
}

static void FreqInput() {
  freqInputIndex = 0;
  freqInputDotIndex = 0;
  ResetFreqInput();
  SetState(FREQ_INPUT);
  Key_1_pressed = 1;
}

static void UpdateFreqInput(KEY_Code_t key) {
  if (key != KEY_EXIT && freqInputIndex >= 10) {
    return;
  }
  if (key == KEY_STAR) {
    if (freqInputIndex == 0 || freqInputDotIndex) {
      return;
    }
    freqInputDotIndex = freqInputIndex;
  }
  if (key == KEY_EXIT) {
    freqInputIndex--;
    if(freqInputDotIndex==freqInputIndex)
      freqInputDotIndex = 0;    
  } else {
    freqInputArr[freqInputIndex++] = key;
  }

  ResetFreqInput();

  uint8_t dotIndex =
      freqInputDotIndex == 0 ? freqInputIndex : freqInputDotIndex;

  KEY_Code_t digitKey;
  for (int i = 0; i < 10; ++i) {
    if (i < freqInputIndex) {
      digitKey = freqInputArr[i];
      freqInputString[i] = digitKey <= KEY_9 ? '0' + digitKey-KEY_0 : '.';
    } else {
      freqInputString[i] = '-';
    }
  }

  uint32_t base = 100000; // 1MHz in BK units
  for (int i = dotIndex - 1; i >= 0; --i) {
    tempFreq += (freqInputArr[i]-KEY_0) * base;
    base *= 10;
  }

  base = 10000; // 0.1MHz in BK units
  if (dotIndex < freqInputIndex) {
    for (int i = dotIndex + 1; i < freqInputIndex; ++i) {
      tempFreq += (freqInputArr[i]-KEY_0) * base;
      base /= 10;
    }
  }
  
}

static bool IsBlacklisted(uint32_t f) {
    for (uint16_t i = 0; i < HISTORY_SIZE; i++) {
        if (HFreqs[i] == f && HBlacklisted[i]) {
            return true;
        }
    }
    return false;
}

static void Blacklist() {
    if (peak.f == 0) return;
    gIsPeak = 0;
    ToggleRX(false);
    isBlacklistApplied = true;
    ResetScanStats();
    NextScanStep();
    for (uint16_t i = 0; i < HISTORY_SIZE; i++) {
        if (HFreqs[i] == peak.f) {
            HBlacklisted[i] = true;
            historyListIndex = i;
            gIsPeak = 0;
            return;
        }
    }

    HFreqs[indexFs]   = peak.f;
    HCount[indexFs]       = 1;
    HBlacklisted[indexFs] = true;
    historyListIndex = indexFs;
    if (++indexFs >= HISTORY_SIZE) {
      historyScrollOffset = 0;
      indexFs=0;
    }  
}


// ============================================================
// SECTION: Display / rendering helpers
// ============================================================
// applied x2 to prevent initial rounding
static uint16_t Rssi2PX(uint16_t rssi, uint16_t pxMin, uint16_t pxMax) {
  const int16_t DB_MIN = settings.dbMin << 1;
  const int16_t DB_MAX = settings.dbMax << 1;
  const int16_t DB_RANGE = DB_MAX - DB_MIN;
  const int16_t PX_RANGE = pxMax - pxMin;
  int dbm = clamp(rssi - (160 << 1), DB_MIN, DB_MAX);
  return ((dbm - DB_MIN) * PX_RANGE + DB_RANGE / 2) / DB_RANGE + pxMin;
}

static int16_t Rssi2Y(uint16_t rssi) {
  int delta = ArrowLine*8;
  return DrawingEndY + delta -Rssi2PX(rssi, delta, DrawingEndY);
}

static void DrawSpectrum(void) {
    int16_t y_baseline = Rssi2Y(0); 
    for (uint8_t i = 0; i < 128; i++) {
        int16_t y_curr = Rssi2Y(rssiHistory[i]);
        for (int16_t y = y_curr; y <= y_baseline; y++) {
                gFrameBuffer[y >> 3][i] |= (1 << (y & 7));
            }
        }
}

static void RemoveTrailZeros(char *s) {
    char *p;
    if (strchr(s, '.')) {
        p = s + strlen(s) - 1;
        while (p > s && *p == '0') {
            *p-- = '\0';
        }
        if (*p == '.') {
            *p = '\0';
        }
    }
}

static void DrawStatus() {
static const char* const scanStepNames[] = {"10", "100", "500", "1k", "2k5", "5k", "6k25", "8k33", "10k", "12k5", "25k", "100k", "500k"};
  int len=0;
  int pos=0;
   
switch(SpectrumMonitor) {
    case 0:
      len = sprintf(&String[pos],"");
      pos += len;
    break;

    case 1:
      len = sprintf(&String[pos],"FL ");
      pos += len;
    break;

    case 2:
      len = sprintf(&String[pos],"M ");
      pos += len;
    break;
  } 
  if (settings.rssiTriggerLevelUp == 50) len = sprintf(&String[pos],"");
  else len = sprintf(&String[pos],"DS%d ", settings.rssiTriggerLevelUp);
  pos += len;
  
  len = sprintf(&String[pos],"%dms %s BW%s ", DelayRssi, gModulationStr[settings.modulationType],bwNames[settings.listenBw]);
  pos += len;
  int16_t afcVal = BK4819_GetAFCValue();
  if (afcVal) {
      len = sprintf(&String[pos],"A%+d ", afcVal);
      pos += len;
  } else {
      len = sprintf(&String[pos], "ST%s", scanStepNames[settings.scanStepIndex]);
      pos += len;
  }
 
  GUI_DisplaySmallest(String, 0, 1, true,true);
  BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[gBatteryCheckCounter++ % 4]);

  uint16_t voltage = (gBatteryVoltages[0] + gBatteryVoltages[1] + gBatteryVoltages[2] +
             gBatteryVoltages[3]) /
            4 * 760 / gBatteryCalibration[3];

  unsigned perc = BATTERY_VoltsToPercent(voltage);
  sprintf(String,"%d%%", perc);
  GUI_DisplaySmallest(String, 112, 1, true,true);
}

// ------------------ Frequency string ------------------
static void FormatFrequency(uint32_t f, char *buf, size_t buflen) {
    snprintf(buf, buflen, "%u.%05u", f / 100000, f % 100000);
    RemoveTrailZeros(buf);
}

// ------------------ CSS detection ------------------
static void UpdateCssDetection(void) {
    // Проверяем только когда есть приём сигнала
    if (!isListening && !gIsPeak) {
        StringCode[0] = '\0';  // очищаем, если нет сигнала
        return;
    }

    // Включаем CxCSS детектор
    BK4819_WriteRegister(BK4819_REG_51,
        BK4819_REG_51_ENABLE_CxCSS |
        BK4819_REG_51_AUTO_CDCSS_BW_ENABLE |
        BK4819_REG_51_AUTO_CTCSS_BW_ENABLE |
        (51u << BK4819_REG_51_SHIFT_CxCSS_TX_GAIN1));

    BK4819_CssScanResult_t scanResult = BK4819_GetCxCSSScanResult(&cdcssFreq, &ctcssFreq);

    if (scanResult == BK4819_CSS_RESULT_CDCSS) {
        uint8_t code = DCS_GetCdcssCode(cdcssFreq);
        if (code != 0xFF) {
            snprintf(StringCode, sizeof(StringCode), "D%03oN", DCS_Options[code]); //субтон цифра
            return;
        }
    } else if (scanResult == BK4819_CSS_RESULT_CTCSS) {
        uint8_t code = DCS_GetCtcssCode(ctcssFreq);
        if (code < ARRAY_SIZE(CTCSS_Options)) {
            snprintf(StringCode, sizeof(StringCode), "%u.%uHz", // субтон аналог Hz
                     CTCSS_Options[code] / 10, CTCSS_Options[code] % 10);
            return;
        }
    }

    // Если ничего не нашли — очищаем
    StringCode[0] = '\0';
}

static void DrawF(uint32_t f) {
    if ((f == 0) || f < 1400000 || f > 130000000) return;
    char freqStr[18];
    if(isListening) {
    snprintf(freqStr, sizeof(freqStr), "%u.%05u", f / 100000, f % 100000);
    } else {
        snprintf(freqStr, sizeof(freqStr), "%u.%01u", f / 100000, (f % 100000) / 10000);
    }
    UpdateCssDetection(); // субтон новый
    uint16_t channelFd = BOARD_gMR_fetchChannel(f);
    isKnownChannel = (channelFd != 0xFFFF);
    char line1[19] = "";
    char line1b[19] = "";
    char line2[19] = "";
    sprintf(line1, "%s", freqStr);
    sprintf(line1b, "%s %s", freqStr, StringCode);
    
    if (gNextTimeslice_1s) {
        ReadChannelName(channelFd, channelName);
        gNextTimeslice_1s = 0;
    }
    char prefix[9] = "";
    if (appMode == SCAN_BAND_MODE) {
        snprintf(prefix, sizeof(prefix), "B%u ", bl + 1);
        if (isListening && isKnownChannel) {
            snprintf(line2, sizeof(line2), "%-3s%s ", prefix, channelName);
    } else {
            snprintf(line2, sizeof(line2), "%s%s", prefix, BParams[bl].BandName);
        }
    } else if (appMode == CHANNEL_MODE) {

        if (channelName[0] != '\0') {
            snprintf(line2, sizeof(line2), "%s%s ", prefix, channelName);
        } else {
            snprintf(line2, sizeof(line2), "%s", prefix);
        }
    } else {
        line2[0] = '\0';
    }
   
    if (classic) {
            if (ShowLines == 2) {
                UI_DisplayFrequency(line1, 2, 0, 0);  // BIG FREQUENCY
                ArrowLine = 2;
            }

            if (ShowLines == 1) {
                UI_PrintStringSmall(line1b, 1, LCD_WIDTH - 1, 0, 0);  // F + CSS
                UI_PrintStringSmall(line2,  1, LCD_WIDTH - 1, 1, 0);  // SL or BD + Name
                ArrowLine = 2;
}
    if (Fmax) 
      {
          FormatFrequency(Fmax, freqStr, sizeof(freqStr));
          GUI_DisplaySmallest(freqStr,  50, Bottom_print, false,true);
      }

    } else { //Not Classic

    DrawMeter(4);
#ifdef ENABLE_SPECTRUM_LINES
    MyDrawShortHLine(10, 0, 127, 2, false);
    MyDrawShortHLine(35, 0, 5, 1, false);
    MyDrawShortHLine(35, 122, 127, 1, false);
    MyDrawVLine(0,   35, 57, 1);
    MyDrawVLine(127, 35, 57, 1);         
#endif
    UI_DisplayFrequency(line1, 10, 2, 0);
    UI_PrintString(line2, 5, LCD_WIDTH - 1, 5, 8);
    char rssiText[16];
    sprintf(rssiText, "R:%3d", scanInfo.rssi);
    UI_PrintStringSmall(rssiText, 96, 1, 0, 0);  // x=96, y=0, BSmall

    if (StringCode[0]) {
        UI_PrintStringSmall(StringCode, 50, 1, 0, 0);  // ← подбери 100–110
    }
}
}

static void LookupChannelInfo() {
    gChannel = BOARD_gMR_fetchChannel(peak.f);
    isKnownChannel = gChannel == 0xFFFF ? false : true;
    if (isKnownChannel){LookupChannelModulation();}
  }

static void LookupChannelModulation() {
	  uint8_t tmp;
		uint8_t data[8];

		EEPROM_ReadBuffer(ADRESS_FREQ_PARAMS + gChannel * 16 + 8, data, sizeof(data));

		tmp = data[3] >> 4;
		if (tmp >= MODULATION_UKNOWN)
			tmp = MODULATION_FM;
		channelModulation = tmp;

		if (data[4] == 0xFF)
		{
			channelBandwidth = BK4819_FILTER_BW_WIDE;
		}
		else
		{
			const uint8_t d4 = data[4];
			channelBandwidth = !!((d4 >> 1) & 1u);
			if(channelBandwidth != BK4819_FILTER_BW_WIDE)
				channelBandwidth = ((d4 >> 5) & 3u) + 1;
		}	

}

static void UpdateScanListCountsCached(void) {
    if (!scanListCountsDirty) return;

    BuildValidScanListIndices();
    cachedValidScanListCount = validScanListCount;
    cachedEnabledScanListCount = 0;

    for (uint8_t i = 0; i < cachedValidScanListCount; i++) {
        uint8_t realIndex = validScanListIndices[i];
        if (settings.scanListEnabled[realIndex]) {
            cachedEnabledScanListCount++;
        }
    }

    scanListCountsDirty = false;
}

static void DrawNums() {
if (appMode==CHANNEL_MODE) 
{
  UpdateScanListCountsCached();

  uint8_t displayEnabled = (cachedEnabledScanListCount == 0)
      ? cachedValidScanListCount
      : cachedEnabledScanListCount;

  sprintf(String, "SL:%u/%u", displayEnabled, cachedValidScanListCount);
  GUI_DisplaySmallest(String, 2, Bottom_print, false, true);

  sprintf(String, "CH:%u", scanChannelsCount);
  GUI_DisplaySmallest(String, 96, Bottom_print, false, true);

  return;
}

  if(appMode!=CHANNEL_MODE){
    sprintf(String, "%u.%05u", gScanRangeStart / 100000, gScanRangeStart % 100000);
    GUI_DisplaySmallest(String, 2, Bottom_print, false, true);
 
    sprintf(String, "%u.%05u", gScanRangeStop / 100000, gScanRangeStop % 100000);
    GUI_DisplaySmallest(String, 90, Bottom_print, false, true);
  }
  
}

static void nextFrequencyinterlaced() {
    static uint16_t lastStep = 0;
    static uint16_t jumpSize = 2500;
    static uint16_t loops = 1;
    static uint32_t columns = 0;

    // Recalcul des constantes si le pas change
    if (scanInfo.scanStep != lastStep) {
        lastStep = scanInfo.scanStep;
        
        uint8_t idx = 0;
        for (uint8_t i = 0; i < sizeof(scanStepValues)/sizeof(scanStepValues[0]); i++) {
            if (scanStepValues[i] == lastStep) {
                idx = i;
                break;
            }
        }
        jumpSize = jumpSizes[idx];
        loops    = interlacedLoops[idx];
        columns = (GetStepsCount() + (loops - 1)) / loops;
        if (columns == 0) columns = 1;
    }
    uint32_t currentColumn = scanInfo.i % columns;
    uint32_t currentPass   = scanInfo.i / columns;
    scanInfo.f = gScanRangeStart + (currentColumn * jumpSize) + (currentPass * lastStep);
}
uint32_t f_linear;

static void NextScanStep() {
    spectrumElapsedCount = 0;
    uint16_t count = GetStepsCount();
    if (scanInfo.i >= count) {scanInfo.i = 0;}
    if (appMode == CHANNEL_MODE) { 
        if (scanChannelsCount == 0) {return;}
            scanInfo.f = ScanFrequencies[scanInfo.i];
        f_linear = scanInfo.f;
        //char str[64] = "";sprintf(str, "%d %d \r\n", scanInfo.i, scanInfo.f);LogUart(str);
    } else {
        if (scanInfo.scanStep < 2500 || scanInfo.scanStep == 1000) {
            nextFrequencyinterlaced();
        } else {
            scanInfo.f = gScanRangeStart + (scanInfo.i * scanInfo.scanStep);
        }
        f_linear = gScanRangeStart + (scanInfo.i * scanInfo.scanStep);
    }
        scanInfo.i++;
}

static void SortHistoryByFrequencyAscending(void) {
    uint16_t count = CountValidHistoryItems();

    if (count < 2) {
        historyListIndex = 0;
        historyScrollOffset = 0;
        return;
    }

    for (uint16_t i = 0; i < count - 1; i++) {
        for (uint16_t j = i + 1; j < count; j++) {
            if (HFreqs[j] != 0 && (HFreqs[i] == 0 || HFreqs[j] < HFreqs[i])) {
                uint32_t tf = HFreqs[i];
                uint8_t  tc = HCount[i];
                bool     tb = HBlacklisted[i];

                HFreqs[i] = HFreqs[j];
                HCount[i] = HCount[j];
                HBlacklisted[i] = HBlacklisted[j];

                HFreqs[j] = tf;
                HCount[j] = tc;
                HBlacklisted[j] = tb;
            }
        }
    }

    historyListIndex = 0;
    historyScrollOffset = 0;
    ShowOSDPopup("HISTORY SORTED");  //skrocic?
}

static void CompactHistory(void) {
    uint16_t w = 0;
    uint16_t limit = (indexFs > HISTORY_SIZE) ? HISTORY_SIZE : indexFs;

    for (uint16_t r = 0; r < limit; r++) {
        if (HFreqs[r] == 0) continue;
        if (w != r) {
            HFreqs[w]       = HFreqs[r];
            HCount[w]       = HCount[r];
            HBlacklisted[w] = HBlacklisted[r];
        }
        w++;
    }

    // wyczyść resztę
    for (uint16_t i = w; i < limit; i++) {
        HFreqs[i]       = 0;
        HCount[i]       = 0;
        HBlacklisted[i] = 0;
    }

    indexFs = w;
    if (indexFs == 0) {
        historyListIndex = 0;
        historyScrollOffset = 0;
    } else {
        if (historyListIndex >= indexFs) historyListIndex = indexFs - 1;
        if (historyScrollOffset >= indexFs) {
            historyScrollOffset = (indexFs > MAX_VISIBLE_LINES) ? (indexFs - MAX_VISIBLE_LINES) : 0;
        }
    }
}

static void Skip() {
  if (!SpectrumMonitor) {  
      WaitSpectrum = 0;
      spectrumElapsedCount = 0;
      gIsPeak = false;
      ToggleRX(false);

      if (appMode == CHANNEL_MODE) {
          if (scanChannelsCount == 0) return;
          NextScanStep();
          peak.f = scanInfo.f;
          peak.i = scanInfo.i;
          SetF(scanInfo.f);
          return;
      }

      NextScanStep();
      peak.f = scanInfo.f;
      peak.i = scanInfo.i;
      SetF(scanInfo.f);
  }
}

void NextAppMode(void) {
        // 0 = FR, 1 = SL, 2 = BD, 3 = RG
        if (++Spectrum_state > 3) {Spectrum_state = 0;}
        if(Spectrum_state == 1)LoadActiveScanFrequencies();
        if (!scanChannelsCount && Spectrum_state ==1) Spectrum_state++; //No SL skip SL mode
        char sText[32];
        const char* s[] = {"FREQ", "S LIST", "BAND", "RANGE"};
        sprintf(sText, "MODE: %s", s[Spectrum_state]);
        ShowOSDPopup(sText);
        gRequestedSpectrumState = Spectrum_state;
        gSpectrumChangeRequested = true;
        isInitialized = false;
        spectrumElapsedCount = 0;
        WaitSpectrum = 0;
        gIsPeak = false;
        SPECTRUM_PAUSED = false;
        SpectrumPauseCount = 0;
        newScanStart = true;
        ToggleRX(false);
}


static void SetTrigger50(){
  char triggerText[32];
  if (settings.rssiTriggerLevelUp == 50) {
      sprintf(triggerText, "DYN SQUELCH: OFF");
  }
  else {
      sprintf(triggerText, "DYN SQUELCH: %d", settings.rssiTriggerLevelUp);
  }
  ShowOSDPopup(triggerText);
}
static const uint8_t durations[] = {0, 20, 40, 60};

// ============================================================
// SECTION: Per-state keyboard handlers
// ============================================================

/* --- BAND_LIST_SELECT: navigate and toggle band enable flags --- */
static void HandleKeyBandList(uint8_t key) {
    switch (key) {
        case KEY_UP:
            if (bandListSelectedIndex > 0) {
                bandListSelectedIndex--;
                if (bandListSelectedIndex < bandListScrollOffset)
                    bandListScrollOffset = bandListSelectedIndex;
            } else {
                bandListSelectedIndex = ARRAY_SIZE(BParams) - 1;
            }
            break;
        case KEY_DOWN:
            if (bandListSelectedIndex < ARRAY_SIZE(BParams) - 1) {
                bandListSelectedIndex++;
                if (bandListSelectedIndex >= bandListScrollOffset + MAX_VISIBLE_LINES)
                    bandListScrollOffset = bandListSelectedIndex - MAX_VISIBLE_LINES + 1;
            } else {
                bandListSelectedIndex = 0;
            }
            break;
        case KEY_4: /* toggle selected band */
            if (bandListSelectedIndex < ARRAY_SIZE(BParams)) {
                settings.bandEnabled[bandListSelectedIndex] = !settings.bandEnabled[bandListSelectedIndex];
                nextBandToScanIndex = bandListSelectedIndex;
                bandListSelectedIndex++;
            }
            break;
        case KEY_5: /* select only this band */
            if (bandListSelectedIndex < ARRAY_SIZE(BParams)) {
                memset(settings.bandEnabled, 0, sizeof(settings.bandEnabled));
                settings.bandEnabled[bandListSelectedIndex] = true;
                nextBandToScanIndex = bandListSelectedIndex;
            }
            break;
        case KEY_MENU: /* select only this band and start scanning */
            if (bandListSelectedIndex < ARRAY_SIZE(BParams)) {
                memset(settings.bandEnabled, 0, sizeof(settings.bandEnabled));
                settings.bandEnabled[bandListSelectedIndex] = true;
                nextBandToScanIndex = bandListSelectedIndex;
                SetState(SPECTRUM);
                RelaunchScan();
            }
            break;
        case KEY_EXIT:
            SpectrumMonitor = 0;
            SetState(SPECTRUM);
            RelaunchScan();
            break;
        default:
            break;
    }
}

/* --- SCANLIST_SELECT: navigate and toggle scan-list enable flags --- */
static void HandleKeyScanList(uint8_t key) {
    switch (key) {
        case KEY_UP:
            if (scanListSelectedIndex > 0) {
                scanListSelectedIndex--;
                if (scanListSelectedIndex < scanListScrollOffset)
                    scanListScrollOffset = scanListSelectedIndex;
            } else {
                scanListSelectedIndex = validScanListCount - 1;
            }
            break;
        case KEY_DOWN:
            if (scanListSelectedIndex < validScanListCount - 1) {
                scanListSelectedIndex++;
                if (scanListSelectedIndex >= scanListScrollOffset + MAX_VISIBLE_LINES)
                    scanListScrollOffset = scanListSelectedIndex - MAX_VISIBLE_LINES + 1;
            } else {
                scanListSelectedIndex = 0;
            }
            break;
#ifdef ENABLE_SCANLIST_SHOW_DETAIL
        case KEY_STAR: /* drill-down into channel list for selected scanlist */
            selectedScanListIndex = scanListSelectedIndex;
            BuildScanListChannels(validScanListIndices[selectedScanListIndex]);
            scanListChannelsSelectedIndex = 0;
            scanListChannelsScrollOffset  = 0;
            SetState(SCANLIST_CHANNELS);
            break;
#endif
        case KEY_4: /* toggle selected list, advance cursor */
            ToggleScanList(validScanListIndices[scanListSelectedIndex], 0);
            if (scanListSelectedIndex < validScanListCount - 1)
                scanListSelectedIndex++;
            break;
        case KEY_5: /* activate only selected list */
            ToggleScanList(validScanListIndices[scanListSelectedIndex], 1);
            break;
        case KEY_MENU: /* activate selected list and start scanning */
            if (scanListSelectedIndex < MR_CHANNELS_LIST) {
                ToggleScanList(validScanListIndices[scanListSelectedIndex], 1);
                SetState(SPECTRUM);
                ResetModifiers();
                gForceModulation = 0;
            }
            break;
        case KEY_EXIT:
            SpectrumMonitor = 0;
            SetState(SPECTRUM);
            ResetModifiers();
            gForceModulation = 0;
            break;
        default:
            break;
    }
}

#ifdef ENABLE_SCANLIST_SHOW_DETAIL
/* --- SCANLIST_CHANNELS: scroll through channel detail list --- */
static void HandleKeyScanListChannels(uint8_t key) {
    switch (key) {
        case KEY_UP:
            if (scanListChannelsSelectedIndex > 0) {
                scanListChannelsSelectedIndex--;
                if (scanListChannelsSelectedIndex < scanListChannelsScrollOffset)
                    scanListChannelsScrollOffset = scanListChannelsSelectedIndex;
            }
            break;
        case KEY_DOWN:
            if (scanListChannelsSelectedIndex < scanListChannelsCount - 1) {
                scanListChannelsSelectedIndex++;
                if (scanListChannelsSelectedIndex >= scanListChannelsScrollOffset + 3)
                    scanListChannelsScrollOffset = scanListChannelsSelectedIndex - 3 + 1;
            }
            break;
        case KEY_EXIT:
            SetState(SCANLIST_SELECT);
            break;
        default:
            break;
    }
}
#endif /* ENABLE_SCANLIST_SHOW_DETAIL */

/* --- PARAMETERS_SELECT: navigate settings, edit values --- */
static void HandleKeyParameters(uint8_t key) {
    switch (key) {
        case KEY_UP:
            if (parametersSelectedIndex > 0) {
                parametersSelectedIndex--;
                if (parametersSelectedIndex < parametersScrollOffset)
                    parametersScrollOffset = parametersSelectedIndex;
            } else {
                parametersSelectedIndex = PARAMETER_COUNT - 1;
            }
            break;
        case KEY_DOWN:
            if (parametersSelectedIndex < PARAMETER_COUNT - 1) {
                parametersSelectedIndex++;
                if (parametersSelectedIndex >= parametersScrollOffset + MAX_VISIBLE_LINES)
                    parametersScrollOffset = parametersSelectedIndex - MAX_VISIBLE_LINES + 1;
            } else {
                parametersSelectedIndex = 0;
            }
            break;
        case KEY_3:
        case KEY_1: {
            bool isKey3 = (key == KEY_3);
            switch (parametersSelectedIndex) {
                case 0: /* RSSI Delay */
                    DelayRssi = isKey3 ?
                                (DelayRssi >= 12 ? 1  : DelayRssi + 1) :
                                (DelayRssi <= 1  ? 12 : DelayRssi - 1);
                    {
                        static const int rssiMap[] = {1, 5, 10, 15, 20};
                        settings.rssiTriggerLevelUp =
                            (DelayRssi >= 1 && DelayRssi <= 5) ? rssiMap[DelayRssi - 1] : 20;
                    }
                    break;
                case 1: /* Spectrum Delay */
                    if (isKey3) {
                        if (SpectrumDelay < 61000)
                            SpectrumDelay += (SpectrumDelay < 10000) ? 1000 : 5000;
                    } else if (SpectrumDelay >= 1000) {
                        SpectrumDelay -= (SpectrumDelay < 10000) ? 1000 : 5000;
                    }
                    break;
                case 2: /* Max listen time */
                    if (isKey3) {
                        if (++IndexMaxLT > LISTEN_STEP_COUNT) IndexMaxLT = 0;
                    } else {
                        if (IndexMaxLT == 0) IndexMaxLT = LISTEN_STEP_COUNT;
                        else IndexMaxLT--;
                    }
                    MaxListenTime = listenSteps[IndexMaxLT];
                    break;
                case 3: /* Scan range start */
                case 4: /* Scan range stop  */
                    appMode = SCAN_RANGE_MODE;
                    FreqInput();
                    break;
                case 5: /* Scan step */
                    UpdateScanStep(isKey3);
                    break;
                case 6: /* Listen BW */
                case 7: /* Modulation */
                    if (isKey3 || key == KEY_1) {
                        if (parametersSelectedIndex == 6)
                            ToggleListeningBW(isKey3 ? 0 : 1);
                        else
                            ToggleModulation();
                    }
                    break;
                case 8: /* RX Backlight */
                    Backlight_On_Rx = !Backlight_On_Rx;
                    break;
                case 9: /* Power Save */
                    if (isKey3) {
                        if (++IndexPS > PS_STEP_COUNT) IndexPS = 0;
                    } else {
                        if (IndexPS == 0) IndexPS = PS_STEP_COUNT;
                        else IndexPS--;
                    }
                    SpectrumSleepMs = PS_Steps[IndexPS];
                    break;
                case 10: /* Noise level OFF */
                    Noislvl_OFF = isKey3 ?
                                  (Noislvl_OFF >= 100 ? 30  : Noislvl_OFF + 1) :
                                  (Noislvl_OFF <= 30  ? 100 : Noislvl_OFF - 1);
                    Noislvl_ON = NoisLvl - NoiseHysteresis;
                    break;
                case 11: /* OSD popup duration */
                    osdPopupSetting = isKey3 ?
                                      (osdPopupSetting >= 5000 ? 0    : osdPopupSetting + 500) :
                                      (osdPopupSetting <= 0    ? 5000 : osdPopupSetting - 500);
                    break;
                case 12: /* Record trigger */
                    UOO_trigger = isKey3 ?
                                  (UOO_trigger >= 50 ? 0  : UOO_trigger + 1) :
                                  (UOO_trigger <= 0  ? 50 : UOO_trigger - 1);
                    break;
                case 13: /* Auto keylock */
                    AUTO_KEYLOCK = isKey3 ?
                                   (AUTO_KEYLOCK > 2  ? 0 : AUTO_KEYLOCK + 1) :
                                   (AUTO_KEYLOCK <= 0 ? 3 : AUTO_KEYLOCK - 1);
                    gKeylockCountdown = durations[AUTO_KEYLOCK];
                    break;
                case 14: /* Glitch max */
                    if (isKey3) { if (GlitchMax < 75) GlitchMax += 5; }
                    else        { if (GlitchMax > 10) GlitchMax -= 5; }
                    break;
                case 15: /* Sound boost */
                    SoundBoost = !SoundBoost;
                    break;
                case 16: /* PTT emission mode */
                    PttEmission = isKey3 ?
                                  (PttEmission >= 2 ? 0 : PttEmission + 1) :
                                  (PttEmission <= 0 ? 2 : PttEmission - 1);
                    break;
                case 17: /* Clear history */
                    if (isKey3) ClearHistory();
                    break;
                case 18: /* Reset to defaults */
                    if (isKey3) ClearSettings();
                    break;
            }
            break;
        }
        case KEY_7:
            SaveSettings();
            break;
        case KEY_EXIT:
            SetState(SPECTRUM);
            RelaunchScan();
            ResetModifiers();
            if (Key_1_pressed) APP_RunSpectrum(3);
            break;
        default:
            break;
    }
}

/* --- SPECTRUM state: main spectrum view keys, including list entry shortcuts --- */
static void HandleKeySpectrum(uint8_t key) {

    /* Shortcuts that open list sub-states from SPECTRUM */
    if (appMode == SCAN_BAND_MODE && key == KEY_4) {
        SetState(BAND_LIST_SELECT);
        bandListSelectedIndex = 0;
        bandListScrollOffset  = 0;
        return;
    }
    if (appMode == CHANNEL_MODE && key == KEY_4) {
        SetState(SCANLIST_SELECT);
        scanListSelectedIndex = 0;
        scanListScrollOffset  = 0;
        return;
    }
    if (key == KEY_5) {
        if (historyListActive) {
            gHistoryScan = !gHistoryScan;
            ShowOSDPopup(gHistoryScan ? "SCAN HISTORY ON" : "SCAN HISTORY OFF");
            if (gHistoryScan) { gIsPeak = false; SpectrumMonitor = 0; }
        } else {
            SetState(PARAMETERS_SELECT);
            if (!parametersStateInitialized) {
                parametersSelectedIndex    = 0;
                parametersScrollOffset     = 0;
                parametersStateInitialized = true;
            }
        }
        return;
    }

    switch (key) {
        case KEY_STAR: {
            int step = (settings.rssiTriggerLevelUp >= 20) ? 5 : 1;
            settings.rssiTriggerLevelUp =
                (settings.rssiTriggerLevelUp >= 50 ? 0 : settings.rssiTriggerLevelUp + step);
            SPECTRUM_PAUSED = true;
            Skip();
            SetTrigger50();
            break;
        }
        case KEY_F: {
            int step = (settings.rssiTriggerLevelUp <= 20) ? 1 : 5;
            settings.rssiTriggerLevelUp =
                (settings.rssiTriggerLevelUp <= 0 ? 50 : settings.rssiTriggerLevelUp - step);
            SPECTRUM_PAUSED = true;
            Skip();
            SetTrigger50();
            break;
        }
        case KEY_3:
            if (historyListActive) {
                DeleteHistoryItem();
            } else {
                ToggleListeningBW(1);
                char bwText[32];
                sprintf(bwText, "BW: %s", bwNames[settings.listenBw]);
                ShowOSDPopup(bwText);
            }
            break;
        case KEY_9: {
            ToggleModulation();
            char modText[32];
            sprintf(modText, "MOD: %s", gModulationStr[settings.modulationType]);
            ShowOSDPopup(modText);
            break;
        }
        case KEY_1:
            Skip();
            ShowOSDPopup("SKIPPED");
            break;
        case KEY_7:
            if (historyListActive) {
#ifdef ENABLE_EEPROM_512K
                WriteHistory();
#endif
            } else {
                SaveSettings();
            }
            break;
        case KEY_2:
            if (historyListActive) SaveHistoryToFreeChannel();
            else classic = !classic;
            break;
        case KEY_8:
            if (historyListActive) {
                memset(HFreqs,       0, sizeof(HFreqs));
                memset(HCount,       0, sizeof(HCount));
                memset(HBlacklisted, 0, sizeof(HBlacklisted));
                historyListIndex    = 0;
                historyScrollOffset = 0;
                indexFs             = 0;
                SpectrumMonitor     = 0;
            } else if (classic) {
                ShowLines++;
                if (ShowLines > 2 || ShowLines < 1) ShowLines = 1;
                char viewText[15];
                const char *viewName = (ShowLines == 2) ? "BIG" : "CLASSIC";
                sprintf(viewText, "VIEW: %s", viewName);
                ShowOSDPopup(viewText);
            }
            break;
        case KEY_UP:
            if (historyListActive) {
                uint16_t count = CountValidHistoryItems();
                SpectrumMonitor = 1;
                if (!count) return;
                if (historyListIndex == 0) {
                    historyListIndex    = count - 1;
                    historyScrollOffset = (count > MAX_VISIBLE_LINES) ? count - MAX_VISIBLE_LINES : 0;
                } else {
                    historyListIndex--;
                }
                if (historyListIndex < historyScrollOffset)
                    historyScrollOffset = historyListIndex;
                lastReceivingFreq = HFreqs[historyListIndex];
                SetF(lastReceivingFreq);
            } else if (appMode == SCAN_BAND_MODE) {
                ToggleScanList(bl, 1);
                settings.bandEnabled[bl + 1] = true;
                RelaunchScan();
            } else if (appMode == FREQUENCY_MODE) {
                UpdateCurrentFreq(true);
            } else if (appMode == CHANNEL_MODE) {
                BuildValidScanListIndices();
                scanListSelectedIndex = (scanListSelectedIndex < validScanListCount
                                         ? scanListSelectedIndex + 1 : 0);
                ToggleScanList(validScanListIndices[scanListSelectedIndex], 1);
                SetState(SPECTRUM);
                ResetModifiers();
            } else if (appMode == SCAN_RANGE_MODE) {
                uint32_t rstep = gScanRangeStop - gScanRangeStart;
                gScanRangeStop  += rstep;
                gScanRangeStart += rstep;
                RelaunchScan();
            } else {
                Skip();
            }
            break;
        case KEY_DOWN:
            if (historyListActive) {
                uint16_t count = CountValidHistoryItems();
                SpectrumMonitor = 1;
                if (!count) return;
                if (++historyListIndex >= count) {
                    historyListIndex    = 0;
                    historyScrollOffset = 0;
                }
                if (historyListIndex >= historyScrollOffset + MAX_VISIBLE_LINES)
                    historyScrollOffset = historyListIndex - MAX_VISIBLE_LINES + 1;
                lastReceivingFreq = HFreqs[historyListIndex];
                SetF(lastReceivingFreq);
            } else if (appMode == SCAN_BAND_MODE) {
                ToggleScanList(bl, 1);
                settings.bandEnabled[bl - 1] = true;
                RelaunchScan();
            } else if (appMode == FREQUENCY_MODE) {
                UpdateCurrentFreq(false);
            } else if (appMode == CHANNEL_MODE) {
                BuildValidScanListIndices();
                scanListSelectedIndex = (scanListSelectedIndex < 1
                                         ? validScanListCount - 1 : scanListSelectedIndex - 1);
                ToggleScanList(validScanListIndices[scanListSelectedIndex], 1);
                SetState(SPECTRUM);
                ResetModifiers();
            } else if (appMode == SCAN_RANGE_MODE) {
                uint32_t rstep = gScanRangeStop - gScanRangeStart;
                gScanRangeStop  -= rstep;
                gScanRangeStart -= rstep;
                RelaunchScan();
            } else {
                Skip();
            }
            break;
        case KEY_4:
            if (appMode != SCAN_RANGE_MODE) ToggleStepsCount();
            break;
        case KEY_0:
            if (kbd.counter > 22) {
                if (!gHistorySortLongPressDone) {
                    CompactHistory();
                    SortHistoryByFrequencyAscending();

            if (!historyListActive) {
                        historyListActive   = true;
                        prevSpectrumMonitor = SpectrumMonitor;
                    }

                    historyListIndex = 0;
                    historyScrollOffset = 0;
                    gHistorySortLongPressDone = true;
                }
            } else if (!historyListActive) {
                CompactHistory();
                historyListActive   = true;
                historyListIndex    = 0;
                historyScrollOffset = 0;
                prevSpectrumMonitor = SpectrumMonitor;
            }
            break;
  
     case KEY_6: // next mode
        NextAppMode();
            break;
        case KEY_SIDE1:
            if (SPECTRUM_PAUSED) return;
            if (++SpectrumMonitor > 2) SpectrumMonitor = 0;
            if (SpectrumMonitor == 1) {
                if (lastReceivingFreq < 1400000 || lastReceivingFreq > 130000000)
                    lastReceivingFreq = (scanInfo.f >= 1400000) ? scanInfo.f : gScanRangeStart;
                peak.f     = lastReceivingFreq;
                scanInfo.f = lastReceivingFreq;
                SetF(lastReceivingFreq);
            }
            if (SpectrumMonitor == 2) ToggleRX(1);
            {
                char monitorText[32];
                const char *modes[] = {"NORMAL", "FREQ LOCK", "MONITOR"};
                sprintf(monitorText, "MODE: %s", modes[SpectrumMonitor]);
                ShowOSDPopup(monitorText);
            }
            break;
        case KEY_SIDE2:
            if (historyListActive) {
                HBlacklisted[historyListIndex] = !HBlacklisted[historyListIndex];
                ShowOSDPopup(HBlacklisted[historyListIndex] ? "BL ADDED" : "BL REMOVED");
                RenderHistoryList();
                gIsPeak = 0;
                ToggleRX(false);
                isBlacklistApplied = true;
                ResetScanStats();
                NextScanStep();
            } else {
                Blacklist();
                WaitSpectrum = 0;
                ShowOSDPopup("BL ADD");
            }
            break;
        case KEY_PTT:
            ExitAndCopyToVfo();
            break;
        case KEY_MENU:
            if (historyListActive) scanInfo.f = HFreqs[historyListIndex];
            SetState(STILL);
            stillFreq = GetInitialStillFreq();
            if (stillFreq >= 1400000 && stillFreq <= 130000000) {
                scanInfo.f = stillFreq;
                peak.f     = stillFreq;
                SetF(stillFreq);
            }
            break;
        case KEY_EXIT:
            if (historyListActive) {
                gHistoryScan        = false;
                SetState(SPECTRUM);
                historyListActive   = false;
                SpectrumMonitor     = prevSpectrumMonitor;
                SetF(scanInfo.f);
                break;
            }
            if (WaitSpectrum) WaitSpectrum = 0;
            DeInitSpectrum(0);
            break;
        default:
            break;
    }
}

// ============================================================
// SECTION: Main keyboard dispatcher
// ============================================================

static void OnKeyDown(uint8_t key) {
    if (!gBacklightCountdown) { BACKLIGHT_TurnOn(); return; }
    BACKLIGHT_TurnOn();

    /* Key-lock guard: only KEY_F unlocks */
    if (gIsKeylocked) {
        if (key == KEY_F) {
            gIsKeylocked = false;
            ShowOSDPopup("Unlocked");
            gKeylockCountdown = durations[AUTO_KEYLOCK];
        } else {
            ShowOSDPopup("Unlock:F");
        }
        return;
    }
    gKeylockCountdown = durations[AUTO_KEYLOCK];

    /* Dispatch to the handler for the currently active state */
    switch (currentState) {
        case BAND_LIST_SELECT:  HandleKeyBandList(key);         break;
        case SCANLIST_SELECT:   HandleKeyScanList(key);         break;
#ifdef ENABLE_SCANLIST_SHOW_DETAIL
        case SCANLIST_CHANNELS: HandleKeyScanListChannels(key); break;
#endif
        case PARAMETERS_SELECT: HandleKeyParameters(key);       break;
        default:                HandleKeySpectrum(key);         break;
    }
}

static void OnKeyDownFreqInput(uint8_t key) {
  BACKLIGHT_TurnOn();
  switch (key) {
  case KEY_0:
  case KEY_1:
  case KEY_2:
  case KEY_3:
  case KEY_4:
  case KEY_5:
  case KEY_6:
  case KEY_7:
  case KEY_8:
  case KEY_9:
  case KEY_STAR:
    UpdateFreqInput(key);
    break;
  case KEY_EXIT: //EXIT from freq input
    if (freqInputIndex == 0) {
      SetState(previousState);
      WaitSpectrum = 0;
      break;
    }
    UpdateFreqInput(key);
    break;
  case KEY_MENU: //OnKeyDownFreqInput
    if (tempFreq > F_MAX) {
      break;
    }
    SetState(previousState);
    if (currentState == SPECTRUM) {
        currentFreq = tempFreq;
      ResetModifiers();
    }
    if (currentState == PARAMETERS_SELECT && parametersSelectedIndex == 3)
        gScanRangeStart = tempFreq;
    if (currentState == PARAMETERS_SELECT && parametersSelectedIndex == 4)
        gScanRangeStop = tempFreq;

    break;
  default:
    break;
  }
}

static int16_t storedScanStepIndex = -1;

static void OnKeyDownStill(KEY_Code_t key) {
  BACKLIGHT_TurnOn();
  switch (key) {
      case KEY_3:
         ToggleListeningBW(1);
      break;
     
      case KEY_9:
        ToggleModulation();
      break;
case KEY_UP:
    if (stillEditRegs) {
        SetRegMenuValue(stillRegSelected, true);
    } else if (SpectrumMonitor > 0) {
        uint32_t step = GetScanStep();
        stillFreq += step;
        scanInfo.f = stillFreq;
        peak.f = stillFreq;
        SetF(stillFreq);
    }
    break;
case KEY_DOWN:
    if (stillEditRegs) {
        SetRegMenuValue(stillRegSelected, false);
    } else if (SpectrumMonitor > 0) {
        uint32_t step = GetScanStep();
        if (stillFreq > step) stillFreq -= step;
        scanInfo.f = stillFreq;
        peak.f = stillFreq;
        SetF(stillFreq);
    }
    break;
      case KEY_2: // przewijanie w górę po liście rejestrów
          if (stillEditRegs && stillRegSelected > 0) {
            stillRegSelected--;
          }
      break;
      case KEY_8: // przewijanie w dół po liście rejestrów
          if (stillEditRegs && stillRegSelected < ARRAY_SIZE(allRegisterSpecs)-1) {
            stillRegSelected++;
          }
      break;
      case KEY_STAR:
            if (storedScanStepIndex == -1) {
                storedScanStepIndex = settings.scanStepIndex;
            }
            UpdateScanStep(1);
            break;
      case KEY_F:
            if (storedScanStepIndex == -1) {
                storedScanStepIndex = settings.scanStepIndex;
            }
            UpdateScanStep(0);
            break;
      case KEY_5:
      case KEY_0:
      case KEY_6:
      case KEY_7:
      break;
          
case KEY_SIDE1:
    SpectrumMonitor++;
    if (SpectrumMonitor > 2) SpectrumMonitor = 0;

    if (SpectrumMonitor == 1) {
        if (lastReceivingFreq < 1400000 || lastReceivingFreq > 130000000) {
            lastReceivingFreq = (stillFreq >= 1400000) ? stillFreq : scanInfo.f;
        }
        peak.f = lastReceivingFreq;
        scanInfo.f = lastReceivingFreq;
        SetF(lastReceivingFreq);
    }

    if (SpectrumMonitor == 2) ToggleRX(1);
    break;

      case KEY_SIDE2: 
            Blacklist();
            WaitSpectrum = 0; //don't wait if this frequency not interesting
      break;
      case KEY_PTT:
        if (storedScanStepIndex != -1) { // Restore scan step when exiting with PTT
            settings.scanStepIndex = storedScanStepIndex;
            scanInfo.scanStep = settings.scanStepIndex;
            storedScanStepIndex = -1;
        }
        ExitAndCopyToVfo();
        break;
      case KEY_MENU:
          stillEditRegs = !stillEditRegs;
      break;
      case KEY_EXIT: //EXIT from regs
        if (stillEditRegs) {
          stillEditRegs = false;
        break;
        }
        if (storedScanStepIndex != -1) {
            settings.scanStepIndex = storedScanStepIndex;
            storedScanStepIndex = -1;
            scanInfo.scanStep = settings.scanStepIndex;
        }
        SetState(SPECTRUM);
        WaitSpectrum = 0; //Prevent coming back to still directly
        
    break;
  default:
    break;
  }
}


static void RenderFreqInput() {
  UI_PrintString(freqInputString, 2, 127, 0, 8);
}

static void RenderStatus() {
  memset(gStatusLine, 0, sizeof(gStatusLine));
  DrawStatus();
  ST7565_BlitStatusLine();
}
#ifdef ENABLE_SPECTRUM_LINES
static uint16_t DBm2Rssi(int dbm)
{
    // Формула из оригинального кода: dbm ≈ rssi - 160 (примерно)
    // Мы делаем обратное: rssi ≈ dbm + 160
    // Ограничиваем в диапазоне 0..255 (как реальное RSSI от BK4819)
    int rssi = dbm + 160;
    if (rssi < 0) rssi = 0;
    if (rssi > 255) rssi = 255;
    return (uint16_t)rssi;
}


static void MyDrawHLine(uint8_t y, bool white)
{
    if (y >= 64) return;
    uint8_t byte_idx = y / 8;
    uint8_t bit_mask = 1U << (y % 8);
    for (uint8_t x = 0; x < 128; x++) {
        if (white) {
            gFrameBuffer[byte_idx][x] &= ~bit_mask;  // белая
        } else {
            gFrameBuffer[byte_idx][x] |= bit_mask;   // чёрная
        }
    }
}

// Короткая горизонтальная пунктирная линия
static void MyDrawShortHLine(uint8_t y, uint8_t x_start, uint8_t x_end, uint8_t step, bool white)
{
    if (y >= 64 || x_start >= x_end || x_end > 127) return;
    uint8_t byte_idx = y / 8;
    uint8_t bit_mask = 1U << (y % 8);

    for (uint8_t x = x_start; x <= x_end; x++) {
        if (step > 1 && (x % step) != 0) continue;  // пунктир

        if (white) {
            gFrameBuffer[byte_idx][x] &= ~bit_mask;  // белая
        } else {
            gFrameBuffer[byte_idx][x] |= bit_mask;   // чёрная
        }
    }
}

static void MyDrawVLine(uint8_t x, uint8_t y_start, uint8_t y_end, uint8_t step)
{
    if (x >= 128) return;
    for (uint8_t y = y_start; y <= y_end && y < 64; y++) {
        if (step > 1 && (y % step) != 0) continue;  // пунктир
        uint8_t byte_idx = y / 8;
        uint8_t bit_mask = 1U << (y % 8);
        gFrameBuffer[byte_idx][x] |= bit_mask;  // чёрная (для белой сделай отдельно или параметр)
    }
}

static void MyDrawFrameLines(void)
{
    MyDrawHLine(50, true);   // белая горизонтальная на y=50
    MyDrawHLine(49, false);  // чёрная горизонтальная на y=49
    MyDrawVLine(0,   21, 49, 1);  // левая вертикальная сплошная
    MyDrawVLine(127, 21, 49, 1);  // правая вертикальная сплошная
    MyDrawVLine(0,   0, 17, 1);  // левая вертикальная сплошная
    MyDrawVLine(127, 0, 17, 1);  // правая вертикальная сплошная
    MyDrawShortHLine(0, 0, 3, 1, false);  // верх кор лев
    MyDrawShortHLine(0, 4, 8, 2, false);  // верх кор лев
    MyDrawShortHLine(0, 124, 127, 1, false);  // верх кор прав
    MyDrawShortHLine(0, 118, 123, 2, false);  // верх кор прав
    MyDrawShortHLine(17, 0, 10, 1, false);  // верх кор лев
    MyDrawShortHLine(21, 0, 10, 1, false);  // верх кор лев
    MyDrawShortHLine(19, 11, 119, 2, false);  // центр длин
    MyDrawShortHLine(21, 120, 127, 1, false);  // кор прав
    MyDrawShortHLine(17, 120, 127, 1, false);  // кор прав
}
#endif


static void RenderSpectrum()
{
    if (classic) {
        DrawNums();
        UpdateDBMaxAuto();
        DrawSpectrum();
#ifdef ENABLE_SPECTRUM_LINES
 // === ЛИНИИ С ОТСТУПОМ ПО 4 ПИКСЕЛЯ (только линии, тики от края) ===
const int LEFT_MARGIN  = 4;
const int RIGHT_MARGIN = 4;

const int lineLevels[] = { -60, -40};   // твои уровни (меняй)
const int numLineLevels = ARRAY_SIZE(lineLevels);
const int dashStep = 2;                     // шаг пунктира

const int tickLevels[] = {-20, -50, -70, -100};  // тики (остаются от края)
const int numTickLevels = ARRAY_SIZE(tickLevels);
const int tickLength = 3;

int i, y, x;

// ПУНКТИРНЫЕ ЛИНИИ (с отступом 4 пикселя слева и справа)
for (i = 0; i < numLineLevels; i++) {
    y = Rssi2Y(DBm2Rssi(lineLevels[i]));
    if (y < 8 || y > DrawingEndY) continue;

    // Чёрная пунктирная линия (с отступами)
    for (x = LEFT_MARGIN; x < 128 - RIGHT_MARGIN; x += dashStep) {
        PutPixel(x, y, true);   // чёрный
    }

    // Белая пунктирная линия (сдвиг на 1, с отступами)
    for (x = LEFT_MARGIN + 1; x < 128 - RIGHT_MARGIN; x += dashStep) {
        PutPixel(x, y, false);  // белый
    }
}

// ТИКИ (остаются от самого края — без отступа)
for (i = 0; i < numTickLevels; i++) {
    y = Rssi2Y(DBm2Rssi(tickLevels[i]));
    if (y < 8 || y > DrawingEndY) continue;

    // Левый тик (от x=0)
    for (x = 0; x < tickLength; x++) {
        PutPixel(x, y, true);
    }

    // Правый тик (до x=127)
    for (x = 0; x < tickLength; x++) {
        PutPixel(127 - x, y, true);
    }
}
// === КОНЕЦ СЕТКИ ===
MyDrawFrameLines();
#endif
        
  }

    if(isListening) {
      DrawF(peak.f);
    }
    else {
      if (SpectrumMonitor) DrawF(lastReceivingFreq);
      else DrawF(scanInfo.f);
    }
}


// ВЫВОД БАРА В ПРОСТОМ РЕЖИМЕ — теперь высота 6 пикселей (убрали по 1 сверху и снизу)
static void DrawMeter(int line) {
    const uint8_t METER_PAD_LEFT = 7;
    const uint8_t NUM_SQUARES    = 23;          // чуть короче, чтобы точно влез
    const uint8_t SQUARE_SIZE    = 4;
    const uint8_t SQUARE_GAP     = 1;
    const uint8_t Y_START_BIT    = 2;

    settings.dbMax = 30; 
    settings.dbMin = -100;

    uint8_t max_width_px = NUM_SQUARES * (SQUARE_SIZE + SQUARE_GAP) - SQUARE_GAP;
    uint8_t fill_px      = Rssi2PX(scanInfo.rssi, 0, max_width_px);
    uint8_t fill_count   = fill_px / (SQUARE_SIZE + SQUARE_GAP);

    // Очистка строки
    for (uint8_t px = 0; px < 128; px++) {
        gFrameBuffer[line][px] = 0;
    }

    // Рисуем все квадратики с обводкой
    for (uint8_t sq = 0; sq < NUM_SQUARES; sq++) {
        uint8_t x_left  = METER_PAD_LEFT + sq * (SQUARE_SIZE + SQUARE_GAP);
        uint8_t x_right = x_left + SQUARE_SIZE - 1;

        if (x_right >= 128) break;

        // Верх и низ
        for (uint8_t x = x_left; x <= x_right; x++) {
            gFrameBuffer[line][x] |= (1 << Y_START_BIT);
            gFrameBuffer[line][x] |= (1 << (Y_START_BIT + SQUARE_SIZE - 1));
        }

        // Лево и право
        for (uint8_t bit = Y_START_BIT; bit < Y_START_BIT + SQUARE_SIZE; bit++) {
            gFrameBuffer[line][x_left]  |= (1 << bit);
            gFrameBuffer[line][x_right] |= (1 << bit);
        }
    }

    // Заполняем активные квадратики
    for (uint8_t sq = 0; sq < fill_count; sq++) {
        uint8_t x_left  = METER_PAD_LEFT + sq * (SQUARE_SIZE + SQUARE_GAP);
        uint8_t x_right = x_left + SQUARE_SIZE - 1;

        if (x_right >= 128) break;

        for (uint8_t x = x_left; x <= x_right; x++) {
            for (uint8_t bit = Y_START_BIT; bit < Y_START_BIT + SQUARE_SIZE; bit++) {
                gFrameBuffer[line][x] |= (1 << bit);
            }
        }
    }
}
//*******************подробный режим */
static void RenderStill() {
  classic=1;
  char freqStr[18];
  //if (SpectrumMonitor) FormatFrequency(HFreqs[historyListIndex], freqStr, sizeof(freqStr));
  //else
  FormatFrequency(stillFreq, freqStr, sizeof(freqStr));
  UI_DisplayFrequency(freqStr, 0, 0, 0);
  DrawMeter(2);
  sLevelAttributes sLevelAtt;
  sLevelAtt = GetSLevelAttributes(scanInfo.rssi, stillFreq);

  if(sLevelAtt.over > 0)
    snprintf(String, sizeof(String), "S%2d+%2d", sLevelAtt.sLevel, sLevelAtt.over);
  else
    snprintf(String, sizeof(String), "S%2d", sLevelAtt.sLevel);

  GUI_DisplaySmallest(String, 4, 25, false, true);
  snprintf(String, sizeof(String), "%d dBm", sLevelAtt.dBmRssi);
  GUI_DisplaySmallest(String, 40, 25, false, true);



  // --- lista rejestrów
  uint8_t total = ARRAY_SIZE(allRegisterSpecs);
  uint8_t lines = STILL_REGS_MAX_LINES;
  if (total < lines) lines = total;

  // Scroll logic
  if (stillRegSelected >= total) stillRegSelected = total-1;
  if (stillRegSelected < stillRegScroll) stillRegScroll = stillRegSelected;
  if (stillRegSelected >= stillRegScroll + lines) stillRegScroll = stillRegSelected - lines + 1;

  for (uint8_t i = 0; i < lines; ++i) {
    uint8_t idx = i + stillRegScroll;
    RegisterSpec s = allRegisterSpecs[idx];
    uint16_t v = GetRegMenuValue(idx);

    char buf[32];
    // Przygotuj tekst do wyświetlenia
    if (stillEditRegs && idx == stillRegSelected)
      snprintf(buf, sizeof(buf), ">%-18s %6u", s.name, v);
    else
      snprintf(buf, sizeof(buf), " %-18s %6u", s.name, v);

    uint8_t y = 32 + i * 8;
    if (stillEditRegs && idx == stillRegSelected) {
      // Najpierw czarny prostokąt na wysokość linii
      for (uint8_t px = 0; px < 128; ++px)
        for (uint8_t py = y; py < y + 6; ++py) // 6 = wysokość fontu 3x5
          PutPixel(px, py, true); // 
      // Następnie białe litery (fill = true)
      GUI_DisplaySmallest(buf, 0, y, false, false);
    } else {
      // Zwykły tekst: czarne litery na białym
      GUI_DisplaySmallest(buf, 0, y, false, true);
    }
  }
}


static void Render() {
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
  
  switch (currentState) {
  case SPECTRUM:
    if(historyListActive) RenderHistoryList();
    else RenderSpectrum();
    break;
  case FREQ_INPUT:
    RenderFreqInput();
    break;
  case STILL:
    RenderStill();
    break;
  
    case BAND_LIST_SELECT:
      RenderBandSelect();
    break;

    case SCANLIST_SELECT:
      RenderScanListSelect();
    break;
    case PARAMETERS_SELECT:
      RenderParametersSelect();
    break;
#ifdef ENABLE_SCANLIST_SHOW_DETAIL
    case SCANLIST_CHANNELS: // NOWY CASE
      RenderScanListChannels();
      break;
#endif // ENABLE_SCANLIST_SHOW_DETAIL
    
  }
  #ifdef ENABLE_SCREENSHOT
    getScreenShot(1);
  #endif
  ST7565_BlitFullScreen();
}

static void HandleUserInput(void) {
    kbd.prev = kbd.current;
    kbd.current = GetKey();
    // ---- Anti-rebond + répétition ----
    if (kbd.current != KEY_INVALID && kbd.current == kbd.prev) {
        kbd.counter++;
    } else {
        if (kbd.prev == KEY_0) {
            gHistorySortLongPressDone = false;
        }
          kbd.counter = 0;
      }

if (kbd.counter == 2 || (kbd.counter > 22 && (kbd.counter % 20 == 0))) {
       
        switch (currentState) {
            case SPECTRUM:
                OnKeyDown(kbd.current);
                break;
            case FREQ_INPUT:
                OnKeyDownFreqInput(kbd.current);
                break;
            case STILL:
                OnKeyDownStill(kbd.current);
                break;
            case BAND_LIST_SELECT:
                OnKeyDown(kbd.current);
                break;
            case SCANLIST_SELECT:
                OnKeyDown(kbd.current);
                break;
            case PARAMETERS_SELECT:
                OnKeyDown(kbd.current);
                break;
        #ifdef ENABLE_SCANLIST_SHOW_DETAIL
            case SCANLIST_CHANNELS:
                OnKeyDown(kbd.current);
                break;
        #endif
        }
    }
}

static void NextHistoryScanStep() {

    uint16_t count = CountValidHistoryItems();
    if (count == 0) return;

    uint16_t start = historyListIndex;
    
    // Boucle pour trouver le prochain élément non blacklisté
    do {
        historyListIndex++;
        if (historyListIndex >= count) historyListIndex = 0;
        
        // Sécurité : si on a fait un tour complet (tout est blacklisté), on s'arrête
        if (historyListIndex == start && HBlacklisted[historyListIndex]) return;
        
    } while (HBlacklisted[historyListIndex]);

    // Mise à jour de l'affichage (scroll) pour suivre le curseur
    if (historyListIndex < historyScrollOffset) {
        historyScrollOffset = historyListIndex;
    } else if (historyListIndex >= historyScrollOffset + 6) { // 6 = MAX_VISIBLE_LINES
        historyScrollOffset = historyListIndex - 6 + 1;
    }

    // Mise à jour de la fréquence pour le prochain cycle de mesure
    scanInfo.f = HFreqs[historyListIndex];
    
    // Reset du compteur de temps pour la logique de pause
    spectrumElapsedCount = 0;
}


static void UpdateScan() {
  if(SPECTRUM_PAUSED || gIsPeak || SpectrumMonitor || WaitSpectrum) return;

  SetF(scanInfo.f);
  Measure();
  if(gIsPeak || SpectrumMonitor || WaitSpectrum) return;
  if (gHistoryScan && historyListActive) {
      NextHistoryScanStep();
      return;
  }
  if (scanInfo.i <= GetStepsCount()) {
    NextScanStep();
    return;
  }
  
  newScanStart = true; 
  //Fmax = peak.f;
  
  if (SpectrumSleepMs) {
      BK4819_Sleep();
      BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);
      SPECTRUM_PAUSED = true;
      SpectrumPauseCount = SpectrumSleepMs;
  }
}


static void UpdateListening(void) { // called every 10ms
    static uint32_t stableFreq = 1;
    static uint16_t stableCount = 0;
    static bool SoundBoostsave = false; // Initialisation
    if (SoundBoost != SoundBoostsave) {
        if (SoundBoost) {
            BK4819_WriteRegister(0x54, 0x90D1);    // default is 0x9009
            BK4819_WriteRegister(0x55, 0x3271);    // default is 0x31a9
            BK4819_WriteRegister(0x75, 0xFC13);    // default is 0xF50B
        } 
        else {
            BK4819_WriteRegister(0x54, 0x9009);
            BK4819_WriteRegister(0x55, 0x31a9);
            BK4819_WriteRegister(0x75, 0xF50B);
        }
        SoundBoostsave = SoundBoost;
    }
    if (peak.f == stableFreq) {
        if (++stableCount >= 2) {  // ~600ms
            if (!SpectrumMonitor) FillfreqHistory();
            stableCount = 0;
            if (gEeprom.BACKLIGHT_MAX > 5)
                BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, 1);
            if(Backlight_On_Rx) BACKLIGHT_TurnOn();
        }
    } else {
        stableFreq = peak.f;
        stableCount = 0;
    }
    
    UpdateNoiseOff();
    if (!isListening) {
        UpdateNoiseOn();
        UpdateGlitch();
    }
        
    spectrumElapsedCount += 10; //in ms
    uint32_t maxCount = (uint32_t)MaxListenTime * 1000;

    if (MaxListenTime && spectrumElapsedCount >= maxCount && !SpectrumMonitor) {
        // délai max atteint → reset
        ToggleRX(false);
        Skip();
        return;
    }

    // --- Gestion du pic ---
    if (gIsPeak) {
        WaitSpectrum = SpectrumDelay;   // reset timer
        return;
    }

    if (WaitSpectrum > 61000)
        return;

    if (WaitSpectrum > 10) {
        WaitSpectrum -= 10;
        return;
    }
    // timer écoulé
    WaitSpectrum = 0;
    ResetScanStats();
}

static void Tick() {
  if (gNextTimeslice_500ms) {
    if (gBacklightCountdown > 0)
      if (--gBacklightCountdown == 0)	BACKLIGHT_TurnOff();
    gNextTimeslice_500ms = false;
    
    if (gKeylockCountdown > 0) {gKeylockCountdown--;}
    if (AUTO_KEYLOCK && !gKeylockCountdown) {
      if (!gIsKeylocked) ShowOSDPopup("Locked"); 
      gIsKeylocked = true;
	  }
  }

  if (gNextTimeslice_10ms) {
    HandleUserInput();
    gNextTimeslice_10ms = 0;
    if (isListening || SpectrumMonitor || WaitSpectrum) UpdateListening(); 
    if(SpectrumPauseCount) SpectrumPauseCount--;
    if (osdPopupTimer > 0) {
        UI_DisplayPopup(osdPopupText);
        ST7565_BlitFullScreen();
        osdPopupTimer -= 10; 
        if (osdPopupTimer <= 0) {osdPopupText[0] = '\0';}
        return;
    }

  }

  if (SPECTRUM_PAUSED && (SpectrumPauseCount == 0)) {
      // fin de la pause
      SPECTRUM_PAUSED = false;
      BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
      BK4819_RX_TurnOn(); //Wake up
      SYSTEM_DelayMs(10);
  }

  if(!isListening && gIsPeak && !SpectrumMonitor && !SPECTRUM_PAUSED) {
     LookupChannelInfo();
     SetF(peak.f);
     ToggleRX(true);
     return;
  }

  if (newScanStart) {
    newScanStart = false;
    InitScan();
  }

  if (!isListening) {UpdateScan();}
  
  if (gNextTimeslice_display) {
    //if (isListening || SpectrumMonitor || WaitSpectrum) UpdateListening(); // Kolyan test
    gNextTimeslice_display = 0;
    latestScanListName[0] = '\0';
    RenderStatus();
    Render();
  }
}


void APP_RunSpectrum(uint8_t Spectrum_state)
{
    for (;;) {
        Mode mode;
        appMode = CHANNEL_MODE; LoadActiveScanFrequencies();
        if      (Spectrum_state == 4) mode = FREQUENCY_MODE ;
        else if (Spectrum_state == 3) mode = SCAN_RANGE_MODE ;
        else if (Spectrum_state == 2) mode = SCAN_BAND_MODE ;
        else if (Spectrum_state == 1) mode = CHANNEL_MODE ;
        else mode = FREQUENCY_MODE;
        //BK4819_SetFilterBandwidth(BK4819_FILTER_BW_NARROW, false);  // принудительно узкий в спектре ЧИНИМ ВФО
        EEPROM_WriteBuffer(0x1D00, &Spectrum_state);
        if (!Key_1_pressed) LoadSettings();
        appMode = mode;
        ResetModifiers();
        if (appMode==FREQUENCY_MODE && !Key_1_pressed) {
            currentFreq = gTxVfo->pRX->Frequency;
            gScanRangeStart = currentFreq - (GetBW() >> 1);
            gScanRangeStop  = currentFreq + (GetBW() >> 1);
        }
        Key_1_pressed = 0;
        BackupRegisters();
        uint8_t CodeType = gTxVfo->pRX->CodeType;
        uint8_t Code     = gTxVfo->pRX->Code;
        BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(CodeType, Code));
        ResetInterrupts();
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
        isListening = true;
        newScanStart = true;
        AutoAdjustFreqChangeStep();
        RelaunchScan();
        for (int i = 0; i < 128; ++i) { rssiHistory[i] = 0; }
        isInitialized = true;
        historyListActive = false;
        while (isInitialized) {Tick();}

        if (gSpectrumChangeRequested) {
            Spectrum_state = gRequestedSpectrumState;
            gSpectrumChangeRequested = false;
            RestoreRegisters(); 
            continue;
        } else {
            RestoreRegisters();
            break;
        }

        break;
    } 
}

void MR_LoadChannelAttributesFromFlash(uint16_t channel_id, ChannelAttributes_t* attributes)
{
    if (channel_id >= (MR_CHANNEL_LAST + 7)) {
        attributes->__val = 0;
        return;
    }
    uint32_t flash_addr = ADRESS_ATTRIBUTES + (channel_id * sizeof(ChannelAttributes_t));
    EEPROM_ReadBuffer(flash_addr, attributes, sizeof(ChannelAttributes_t));
}

static void LoadActiveScanFrequencies(void)
{   if(appMode!=CHANNEL_MODE)return;
    memset(ScanFrequencies,0,sizeof(ScanFrequencies));
    scanChannelsCount = 0;
    ChannelAttributes_t cache;
    for (uint16_t ch = MR_CHANNEL_FIRST; ch <= MR_CHANNEL_LAST; ch++)
    {
        MR_LoadChannelAttributesFromFlash(ch, &cache);
        uint32_t freq = FetchChannelFrequency(ch);
        if (freq) {
            if (settings.scanListEnabled[cache.scanlist-1])
                {   ScanFrequencies[scanChannelsCount] = freq;
                    scanChannelsCount++;
                }
            }

    }
}

static void ToggleScanList(int scanListNumber, int single )
  {
    if (appMode == SCAN_BAND_MODE) {
      if (single) memset(settings.bandEnabled, 0, sizeof(settings.bandEnabled));
        else settings.bandEnabled[scanListNumber-1] = !settings.bandEnabled[scanListNumber-1];
      }
    if (appMode == CHANNEL_MODE) {
        if (single) {memset(settings.scanListEnabled, 0, sizeof(settings.scanListEnabled));}
        settings.scanListEnabled[scanListNumber] = !settings.scanListEnabled[scanListNumber];
        refreshScanListName = true;
      }
  }

// ============================================================
// SECTION: EEPROM / Settings persistence
// ============================================================
#include "index.h"

bool IsVersionMatching(void) {
    uint16_t stored,app_version;
    app_version = APP_VERSION;
    EEPROM_ReadBuffer(0x1D08, &stored, 2);
    if (stored != APP_VERSION) EEPROM_WriteBuffer(0x1D08, &app_version);
    return (stored == APP_VERSION);
}


typedef struct {
    int ShowLines;
    uint8_t DelayRssi;
    uint8_t PttEmission; 
    uint8_t listenBw;
    uint32_t bandListFlags;            // Bits 0-31: bandEnabled[0..31]
    uint32_t scanListFlags;            // Bits 0-31: scanListEnabled[0..31]
    int16_t Trigger;
    uint32_t RangeStart;
    uint32_t RangeStop;
    ScanStep scanStepIndex;
    uint16_t R40;                      // RF TX Deviation
    uint16_t R29;                      // AF TX noise compressor, AF TX 0dB compressor, AF TX compression ratio
    uint16_t R19;                      // Disable MIC AGC
    uint16_t R73;                      // AFC range select
    uint16_t R10;
    uint16_t R11;
    uint16_t R12;
    uint16_t R13;
    uint16_t R14;
    uint16_t R3C;
    uint16_t R43;
    uint16_t R2B;
    uint16_t SpectrumDelay;
    uint8_t IndexMaxLT;
    uint8_t IndexPS;
    uint8_t Noislvl_OFF;
    uint16_t UOO_trigger;
    uint16_t osdPopupSetting;
    uint8_t GlitchMax;  
    bool Backlight_On_Rx;
    bool SoundBoost;  
} SettingsEEPROM;


void LoadSettings()
{
  if(SettingsLoaded) return;
  SettingsEEPROM  eepromData  = {0};
  EEPROM_ReadBuffer(0x1D10, &eepromData, sizeof(eepromData));
  
  BK4819_WriteRegister(BK4819_REG_10, eepromData.R10);
  BK4819_WriteRegister(BK4819_REG_11, eepromData.R11);
  BK4819_WriteRegister(BK4819_REG_12, eepromData.R12);
  BK4819_WriteRegister(BK4819_REG_13, eepromData.R13);
  BK4819_WriteRegister(BK4819_REG_14, eepromData.R14);
  if(!IsVersionMatching()) ClearSettings();
  for (int i = 0; i < MR_CHANNELS_LIST; i++) {
    settings.scanListEnabled[i] = (eepromData.scanListFlags >> i) & 0x01;
  }
  settings.rssiTriggerLevelUp = eepromData.Trigger;
  settings.listenBw = eepromData.listenBw;
  BK4819_SetFilterBandwidth(settings.listenBw, false);
  if (eepromData.RangeStart >= 1400000) gScanRangeStart = eepromData.RangeStart;
  if (eepromData.RangeStop >= 1400000) gScanRangeStop = eepromData.RangeStop;
  settings.scanStepIndex = eepromData.scanStepIndex;
  for (int i = 0; i < MAX_BANDS; i++) {
      settings.bandEnabled[i] = (eepromData.bandListFlags >> i) & 0x01;
    }
  DelayRssi = eepromData.DelayRssi;
  if (DelayRssi > 12) DelayRssi =12;
  PttEmission = eepromData.PttEmission;
  validScanListCount = 0;
  ShowLines = eepromData.ShowLines;
  if (ShowLines < 1 || ShowLines > 2) ShowLines = 1;
  SpectrumDelay = eepromData.SpectrumDelay;
  
  IndexMaxLT = eepromData.IndexMaxLT;
  MaxListenTime = listenSteps[IndexMaxLT];
  
  IndexPS = eepromData.IndexPS;
  SpectrumSleepMs = PS_Steps[IndexPS];
  Noislvl_OFF = eepromData.Noislvl_OFF;
  Noislvl_ON  = Noislvl_OFF - NoiseHysteresis; 
  UOO_trigger = eepromData.UOO_trigger;
  osdPopupSetting = eepromData.osdPopupSetting;
  Backlight_On_Rx = eepromData.Backlight_On_Rx;
  GlitchMax = eepromData.GlitchMax;    
  SoundBoost = eepromData.SoundBoost;    
  BK4819_WriteRegister(BK4819_REG_40, eepromData.R40);
  BK4819_WriteRegister(BK4819_REG_29, eepromData.R29);
  BK4819_WriteRegister(BK4819_REG_19, eepromData.R19);
  BK4819_WriteRegister(BK4819_REG_73, eepromData.R73);
  BK4819_WriteRegister(BK4819_REG_3C, eepromData.R3C);
  BK4819_WriteRegister(BK4819_REG_43, eepromData.R43);
  BK4819_WriteRegister(BK4819_REG_2B, eepromData.R2B);
  
  
#ifdef ENABLE_EEPROM_512K
  if (!historyLoaded) {
     ReadHistory();
     historyLoaded = true;
  }
#endif
SettingsLoaded = true;
}

static void SaveSettings() 
{
  SettingsEEPROM  eepromData  = {0};
  for (int i = 0; i < MR_CHANNELS_LIST; i++) {
    if (settings.scanListEnabled[i]) eepromData.scanListFlags |= (1 << i);
  }
  eepromData.Trigger = settings.rssiTriggerLevelUp;
  eepromData.listenBw = settings.listenBw;
  eepromData.RangeStart = gScanRangeStart;
  eepromData.RangeStop = gScanRangeStop;
  eepromData.DelayRssi = DelayRssi;
  eepromData.PttEmission = PttEmission;
  eepromData.scanStepIndex = settings.scanStepIndex;
  eepromData.ShowLines = ShowLines;
  eepromData.SpectrumDelay = SpectrumDelay;
  eepromData.IndexMaxLT = IndexMaxLT;
  eepromData.IndexPS = IndexPS;
  eepromData.Backlight_On_Rx = Backlight_On_Rx;
  eepromData.Noislvl_OFF = Noislvl_OFF;
  eepromData.UOO_trigger = UOO_trigger;
  eepromData.osdPopupSetting = osdPopupSetting;
  eepromData.GlitchMax = 10;
  eepromData.GlitchMax  = GlitchMax;    
  eepromData.SoundBoost = SoundBoost;
  
  for (int i = 0; i < MAX_BANDS; i++) { 
      if (settings.bandEnabled[i]) eepromData.bandListFlags |= (1 << i);
    }
  eepromData.R40 = BK4819_ReadRegister(BK4819_REG_40);
  eepromData.R29 = BK4819_ReadRegister(BK4819_REG_29);
  eepromData.R19 = BK4819_ReadRegister(BK4819_REG_19);
  eepromData.R73 = BK4819_ReadRegister(BK4819_REG_73);
  eepromData.R10 = BK4819_ReadRegister(BK4819_REG_10);
  eepromData.R11 = BK4819_ReadRegister(BK4819_REG_11);
  eepromData.R12 = BK4819_ReadRegister(BK4819_REG_12);
  eepromData.R13 = BK4819_ReadRegister(BK4819_REG_13);
  eepromData.R14 = BK4819_ReadRegister(BK4819_REG_14);
  eepromData.R3C = BK4819_ReadRegister(BK4819_REG_3C);
  eepromData.R43 = BK4819_ReadRegister(BK4819_REG_43);
  eepromData.R2B = BK4819_ReadRegister(BK4819_REG_2B);
  // Write in 8-byte chunks
  for (uint16_t addr = 0; addr < sizeof(eepromData); addr += 8) 
    EEPROM_WriteBuffer(addr + 0x1D10, ((uint8_t*)&eepromData) + addr);
  
  ShowOSDPopup("PARAMS SAVED");
}

static void ClearHistory() 
{
  memset(HFreqs,0,sizeof(HFreqs));
  memset(HCount,0,sizeof(HCount));
  memset(HBlacklisted,0,sizeof(HBlacklisted));
  historyListIndex = 0;
  historyScrollOffset = 0;
  #ifdef ENABLE_EEPROM_512K
  indexFs = HISTORY_SIZE;
  WriteHistory();
  #endif
  indexFs = 0;
  SaveSettings(); 
}

void ClearSettings() 
{
  for (int i = 1; i < MR_CHANNELS_LIST; i++) {settings.scanListEnabled[i] = 0;}
  settings.scanListEnabled[0] = 1;
  settings.rssiTriggerLevelUp = 5;
  settings.listenBw = 1;
  gScanRangeStart = 43000000;
  gScanRangeStop  = 44000000;
  DelayRssi = 3;
  PttEmission = 2;
  settings.scanStepIndex = S_STEP_10_0kHz;
  ShowLines = 1;
  SpectrumDelay = 0;
  MaxListenTime = 0;
  IndexMaxLT = 0;
  IndexPS = 0;
  Backlight_On_Rx = 1;
  Noislvl_OFF = NoisLvl; 
  Noislvl_ON = NoisLvl - NoiseHysteresis;  
  UOO_trigger = 15;
  osdPopupSetting = 500;
  GlitchMax = 20;  
  Spectrum_state = 2; 
  SoundBoost = 1;  
  settings.bandEnabled[0] = 1;
  BK4819_WriteRegister(BK4819_REG_10, 0x0145);
  BK4819_WriteRegister(BK4819_REG_11, 0x01B5);
  BK4819_WriteRegister(BK4819_REG_12, 0x0393);
  BK4819_WriteRegister(BK4819_REG_13, 0x03BE);
  BK4819_WriteRegister(BK4819_REG_14, 0x0019);
  BK4819_WriteRegister(BK4819_REG_40, 13520);
  BK4819_WriteRegister(BK4819_REG_29, 43840);
  BK4819_WriteRegister(BK4819_REG_19, 4161);
  BK4819_WriteRegister(BK4819_REG_73, 18066);
  BK4819_WriteRegister(BK4819_REG_3C, 20360);
  BK4819_WriteRegister(BK4819_REG_43, 13896);
  BK4819_WriteRegister(BK4819_REG_2B, 49152);
  
  ShowOSDPopup("DEFAULT SETTINGS");
  SaveSettings();
}

// ============================================================
// SECTION: List item text helpers
// ============================================================

static bool GetScanListLabel(uint8_t scanListIndex, char* bufferOut) {
    ChannelAttributes_t att;
    char channel_name[12];
    uint16_t first_channel = 0xFFFF;
    uint16_t channel_count = 0;

    for (uint16_t ch = MR_CHANNEL_FIRST; ch <= MR_CHANNEL_LAST; ch++) {
        att = gMR_ChannelAttributes[ch];
        if (att.scanlist == scanListIndex + 1) {
            if (first_channel == 0xFFFF) first_channel = ch;
            channel_count++;
        }
    }
    if (first_channel == 0xFFFF) return false; 

    SETTINGS_FetchChannelName(channel_name, first_channel);

    char nameOrFreq[13];
    if (channel_name[0] == '\0') {
        uint32_t freq = gMR_ChannelFrequencyAttributes[first_channel].Frequency;
        if (freq < 1400000) {
            return false;
        }

        sprintf(nameOrFreq, "%u.%05u", freq / 100000, freq % 100000);
        RemoveTrailZeros(nameOrFreq);
    } else {
        strncpy(nameOrFreq, channel_name, 12);
        nameOrFreq[12] = '\0';
    }

    if (settings.scanListEnabled[scanListIndex]) {
      sprintf(bufferOut, "%d:%-11s*", scanListIndex + 1, nameOrFreq);
    } else {
        sprintf(bufferOut, "%d:%-11s", scanListIndex + 1, nameOrFreq);
    }

    return true;
}

static void BuildValidScanListIndices() {
    uint8_t ScanListCount = 0;
    char tempName[17];
    for (uint8_t i = 0; i < MR_CHANNELS_LIST; i++) {

        if (GetScanListLabel(i, tempName)) {
            validScanListIndices[ScanListCount++] = i;
        }
    }
    validScanListCount = ScanListCount;
}


static void GetFilteredScanListText(uint16_t displayIndex, char* buffer) {
    uint8_t realIndex = validScanListIndices[displayIndex];
    GetScanListLabel(realIndex, buffer);
}


// ============================================================
// SECTION: Unified list renderer
// ============================================================

/* Approximate starting X pixel for right-aligned text.
 * Uses 7px/char estimate (font char_width=6 + inter-char spacing=1). */
#define CHAR_WIDTH_PX 7
static uint8_t ListRightX(const char *s) {
    size_t len = strlen(s);
    return (len > 0 && len * CHAR_WIDTH_PX < 128)
           ? (uint8_t)(128 - len * CHAR_WIDTH_PX) : 1;
}

/* Fill one display line with a solid black bar (selected-row highlight). */
static void ListDrawSelectedBg(uint8_t line) {
    for (uint8_t x = 0; x < LCD_WIDTH; x++)
        for (uint8_t y = (uint8_t)(line * 8); y < (uint8_t)((line + 1) * 8); y++)
            PutPixel(x, y, true);
}

/* Draw a single list row.
 *   left  - left-aligned text starting at x=1
 *   right - right-aligned text (omitted when empty)
 *   inv   - true for selected row (white chars on black)
 *
 * The background is drawn manually via ListDrawSelectedBg so that
 * UI_PrintStringSmall is always called with bg=0 (no font-level fill).
 * This avoids rendering artefacts where the font renderer's background
 * fill can spill outside the intended row or conflict with the manual bar. */
static void ListDrawRow(uint8_t line, const char *left, const char *right, bool inv) {
    if (inv) ListDrawSelectedBg(line);
    uint8_t bg = inv ? 1 : 0;
    if (inv) ListDrawSelectedBg(line);
    UI_PrintStringSmall(left, 1, 0, line, bg);
    if (right[0])
        UI_PrintStringSmall(right, ListRightX(right), 0, line, bg);
}

/* Unified list renderer.
 *
 * Header (line 0):
 *   useMeter=true  -> draw signal-strength meter bar when in FL/Monitor mode.
 *   useMeter=false -> draw title text.
 *
 * Single-line mode (twoLineMode=false):
 *   Up to MAX_VISIBLE_LINES (6) items, one display line each.
 *   row.left  = left-aligned text
 *   row.right = right-aligned text (omitted when empty)
 *
 * Two-line mode (twoLineMode=true):
 *   Up to 3 items; each occupies 2 consecutive display lines.
 *   row.left  = first line text  (left-aligned)
 *   row.right = second line text (left-aligned)
 */
static void RenderUnifiedList(
    const char  *title,
    bool         useMeter,
    uint16_t     numItems,
    uint16_t     selectedIndex,
    uint16_t     scrollOffset,
    bool         invertSelected,
    bool         twoLineMode,
    GetListRowFn getRow)
{
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

    /* Header row */
    if (useMeter && historyListActive && SpectrumMonitor > 0)
        DrawMeter(0);
    else if (title)
        UI_PrintStringSmall(title, 1, LCD_WIDTH - 1, 0, 0);

    const uint8_t maxItems = twoLineMode ? 3 : MAX_VISIBLE_LINES;

    /* Clamp scroll offset */
    if (numItems <= maxItems)
        scrollOffset = 0;
    else if (selectedIndex < scrollOffset)
        scrollOffset = selectedIndex;
    else if (selectedIndex >= scrollOffset + maxItems)
        scrollOffset = selectedIndex - maxItems + 1;

    for (uint8_t i = 0; i < maxItems; i++) {
        uint16_t itemIndex = i + scrollOffset;
        if (itemIndex >= numItems) break;

        ListRow row;
        row.left[0]  = '\0';
        row.right[0] = '\0';
        getRow(itemIndex, &row);

        bool sel = (itemIndex == selectedIndex);

        if (!twoLineMode) {
            if (row.left[0] == '\0') continue; /* skip empty items */
            ListDrawRow((uint8_t)(1 + i), row.left, row.right, sel && invertSelected);
        } else {
            /* Two-line mode: item occupies lines (1+i*2) and (2+i*2) */
            uint8_t line1 = (uint8_t)(1 + i * 2);
            uint8_t line2 = (uint8_t)(2 + i * 2);
            bool inv = sel && invertSelected;
            if (inv) {
                ListDrawSelectedBg(line1);
                ListDrawSelectedBg(line2);
            }
            UI_PrintStringSmall(row.left,  1, 0, line1, inv ? 1 : 0);
            UI_PrintStringSmall(row.right, 1, 0, line2, inv ? 1 : 0);
        }
    }
    ST7565_BlitFullScreen();
}

/* ---- GetRow callbacks for each list type ---- */

/* History list: frequency[+channel name] on left, TX count on right */
static void GetHistoryRow(uint16_t index, ListRow *row) {
    row->left[0]  = '\0';
    row->right[0] = '\0';
    uint32_t f = HFreqs[index];
    if (!f) return;

    char freqStr[10];
    snprintf(freqStr, sizeof(freqStr), "%u.%05u", f / 100000, f % 100000);
    RemoveTrailZeros(freqStr);
    snprintf(row->right, sizeof(row->right), ":%u", HCount[index]);

    char Name[12] = "";
    uint16_t ch = BOARD_gMR_fetchChannel(f);
    if (ch != 0xFFFF) {
        ReadChannelName(ch, Name);
        Name[10] = '\0';
    }
    const char *prefix = HBlacklisted[index] ? "#" : "";
    if (Name[0])
        snprintf(row->left, sizeof(row->left), "%s%s %s", prefix, freqStr, Name);
    else
        snprintf(row->left, sizeof(row->left), "%s%s", prefix, freqStr);
}

/* Scanlist multiselect: "N:name" on left, "*" on right when enabled */
static void GetScanListRow(uint16_t displayIndex, ListRow *row) {
    char buf[20];
    GetFilteredScanListText(displayIndex, buf);
    /* Strip trailing '*' marker and padding spaces added by GetScanListLabel */
    size_t len = strlen(buf);
    bool enabled = (len > 0 && buf[len - 1] == '*');
    if (enabled) buf[--len] = '\0';
    while (len > 0 && buf[len - 1] == ' ') buf[--len] = '\0';
    snprintf(row->left, sizeof(row->left), "%s", buf);
    if (enabled) { row->right[0] = '*'; row->right[1] = '\0'; }
    else           row->right[0] = '\0';
}

/* Band multiselect: "N:bandname" on left, "*" on right when enabled */
static void GetBandRow(uint16_t index, ListRow *row) {
    snprintf(row->left, sizeof(row->left), "%d:%s", index + 1, BParams[index].BandName);
    if (settings.bandEnabled[index]) { row->right[0] = '*'; row->right[1] = '\0'; }
    else                               row->right[0] = '\0';
}

/* Parameters list: setting name on left, current value on right */
static void GetParametersRow(uint16_t index, ListRow *row) {
    row->right[0] = '\0';
    switch (index) {
        case 0:
            snprintf(row->left,  sizeof(row->left),  "RSSI Delay:");
            snprintf(row->right, sizeof(row->right), "%dms", DelayRssi);
            break;
        case 1:
            snprintf(row->left, sizeof(row->left), "Spectrum Delay:");
            if (SpectrumDelay < 65000)
                snprintf(row->right, sizeof(row->right), "%us", SpectrumDelay / 1000);
            else
                strncpy(row->right, "OFF", sizeof(row->right) - 1);
            break;
        case 2:
            snprintf(row->left,  sizeof(row->left),  "MaxListenTime:");
            snprintf(row->right, sizeof(row->right), "%s", labels[IndexMaxLT]);
            break;
        case 3: {
            /* Preserve full frequency precision with trailing-zero removal */
            char tmp[12];
            snprintf(tmp, sizeof(tmp), "%u.%05u",
                     gScanRangeStart / 100000, gScanRangeStart % 100000);
            RemoveTrailZeros(tmp);
            snprintf(row->left,  sizeof(row->left),  "Fstart:");
            snprintf(row->right, sizeof(row->right), "%s", tmp);
            break;
        }
        case 4: {
            /* Preserve full frequency precision with trailing-zero removal */
            char tmp[12];
            snprintf(tmp, sizeof(tmp), "%u.%05u",
                     gScanRangeStop / 100000, gScanRangeStop % 100000);
            RemoveTrailZeros(tmp);
            snprintf(row->left,  sizeof(row->left),  "Fstop:");
            snprintf(row->right, sizeof(row->right), "%s", tmp);
            break;
        }
        case 5: {
            uint32_t step = GetScanStep();
            snprintf(row->left, sizeof(row->left), "Step:");
            snprintf(row->right, sizeof(row->right),
                     step % 100 ? "%uk%02u" : "%uk", step / 100, step % 100);
            break;
        }
        case 6:
            snprintf(row->left,  sizeof(row->left),  "Listen BW:");
            snprintf(row->right, sizeof(row->right), "%s", bwNames[settings.listenBw]);
            break;
        case 7:
            snprintf(row->left,  sizeof(row->left),  "Modulation:");
            snprintf(row->right, sizeof(row->right), "%s", gModulationStr[settings.modulationType]);
            break;
        case 8:
            snprintf(row->left, sizeof(row->left), "RX Backlight:");
            strncpy(row->right, Backlight_On_Rx ? "ON" : "OFF", sizeof(row->right) - 1);
            break;
        case 9:
            snprintf(row->left,  sizeof(row->left),  "Power Save:");
            snprintf(row->right, sizeof(row->right), "%s", labelsPS[IndexPS]);
            break;
        case 10:
            snprintf(row->left,  sizeof(row->left),  "Nois LVL OFF:");
            snprintf(row->right, sizeof(row->right), "%d", Noislvl_OFF);
            break;
        case 11:
            snprintf(row->left, sizeof(row->left), "Popups:");
            if (osdPopupSetting) {
                uint8_t sec = osdPopupSetting / 1000;
                uint8_t dec = (osdPopupSetting % 1000) / 100;
                if (dec) snprintf(row->right, sizeof(row->right), "%d.%ds", sec, dec);
                else     snprintf(row->right, sizeof(row->right), "%ds", sec);
            } else {
                strncpy(row->right, "OFF", sizeof(row->right) - 1);
            }
            break;
        case 12:
            snprintf(row->left,  sizeof(row->left),  "Record Trig:");
            snprintf(row->right, sizeof(row->right), "%d", UOO_trigger);
            break;
        case 13:
            if (AUTO_KEYLOCK) {
                snprintf(row->left,  sizeof(row->left),  "Keylock:");
                snprintf(row->right, sizeof(row->right), "%ds", durations[AUTO_KEYLOCK] / 2);
            } else {
                snprintf(row->left, sizeof(row->left), "Key Unlocked");
            }
            break;
        case 14:
            snprintf(row->left,  sizeof(row->left),  "GlitchMax:");
            snprintf(row->right, sizeof(row->right), "%d", GlitchMax);
            break;
        case 15:
            snprintf(row->left, sizeof(row->left), "SoundBoost:");
            strncpy(row->right, SoundBoost ? "ON" : "OFF", sizeof(row->right) - 1);
            break;
        case 16:
            snprintf(row->left, sizeof(row->left), "PTT:");
            if      (PttEmission == 0) strncpy(row->right, "VFO Freq", sizeof(row->right) - 1);
            else if (PttEmission == 1) strncpy(row->right, "NINJA",    sizeof(row->right) - 1);
            else                       strncpy(row->right, "Last RX",  sizeof(row->right) - 1);
            break;
        case 17:
            snprintf(row->left, sizeof(row->left), "Clear History");
            strncpy(row->right, ">", sizeof(row->right) - 1);
            break;
        case 18:
            snprintf(row->left, sizeof(row->left), "Reset Default");
            strncpy(row->right, ">", sizeof(row->right) - 1);
            break;
        default:
            row->left[0] = '\0';
            break;
    }
}

#ifdef ENABLE_SCANLIST_SHOW_DETAIL
/* ScanList channel detail (two-line mode):
 *   row.left  = "NNN: channel_name"  (line 1)
 *   row.right = "    freq"           (line 2) */
static void GetScanListChannelRow(uint16_t index, ListRow *row) {
    uint16_t channelIndex = scanListChannels[index];
    char channel_name[12];
    SETTINGS_FetchChannelName(channel_name, channelIndex);
    uint32_t freq = gMR_ChannelFrequencyAttributes[channelIndex].Frequency;
    char freqStr[16];
    sprintf(freqStr, " %u.%05u", freq / 100000, freq % 100000);
    RemoveTrailZeros(freqStr);
    snprintf(row->left,  sizeof(row->left),  "%3d: %s", channelIndex + 1, channel_name);
    snprintf(row->right, sizeof(row->right), "    %s", freqStr);
}
#endif

// ============================================================
// SECTION: List screen render functions
// ============================================================

static void RenderScanListSelect() {
    if (refreshScanListName) {
        BuildValidScanListIndices();
        refreshScanListName = false;
    }
    uint8_t selectedCount = 0;
    for (uint8_t i = 0; i < validScanListCount; i++) {
        if (settings.scanListEnabled[validScanListIndices[i]]) selectedCount++;
    }
    char title[24];
    snprintf(title, sizeof(title), "SCANLISTS: %u/%u", selectedCount, validScanListCount);
    RenderUnifiedList(title, false, validScanListCount, scanListSelectedIndex,
                      scanListScrollOffset, true, false, GetScanListRow);
}

static void RenderParametersSelect() {
    RenderUnifiedList("PARAMETERS:", false, PARAMETER_COUNT, parametersSelectedIndex,
                      parametersScrollOffset, true, false, GetParametersRow);
}


#ifdef ENABLE_FULL_BAND
void RenderBandSelect() {
    RenderUnifiedList("BANDS:", false, ARRAY_SIZE(BParams), bandListSelectedIndex,
                      bandListScrollOffset, true, false, GetBandRow);
}
#endif

static void RenderHistoryList() {
    uint16_t count = CountValidHistoryItems();
    char title[24];
    sprintf(title, "HISTORY: %d", count);
    /* title shown in normal mode; signal meter shown in FL/Monitor mode via useMeter=true */
    RenderUnifiedList(title, true, count, historyListIndex,
                      historyScrollOffset, true, false, GetHistoryRow);
}

#ifdef ENABLE_SCANLIST_SHOW_DETAIL
static void BuildScanListChannels(uint8_t scanListIndex) {
    scanListChannelsCount = 0;
    ChannelAttributes_t att;
    for (uint16_t i = 0; i < MR_CHANNEL_LAST + 1; i++) {
        att = gMR_ChannelAttributes[i];
        if (att.scanlist == scanListIndex + 1) {
            if (scanListChannelsCount < MR_CHANNEL_LAST + 1)
                scanListChannels[scanListChannelsCount++] = i;
        }
    }
}

/* Two-line detail view: 3 items visible, each occupying 2 display lines.
 * Header shows the scanlist number; items rendered via RenderUnifiedList. */
static void RenderScanListChannels() {
    char headerString[24];
    uint8_t realScanListIndex = validScanListIndices[selectedScanListIndex];
    sprintf(headerString, "SL %d CHANNELS:", realScanListIndex + 1);
    RenderUnifiedList(headerString, false, scanListChannelsCount,
                      scanListChannelsSelectedIndex, scanListChannelsScrollOffset,
                      true, true, GetScanListChannelRow);
}
#endif /* ENABLE_SCANLIST_SHOW_DETAIL */
