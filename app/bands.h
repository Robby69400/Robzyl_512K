/*Frequencies are 10Hz: 100M is written 10000000
  Max bandname length is 11
  AVAILABLE STEPS ARE 
  S_STEP_0_01kHz,  S_STEP_0_1kHz,  S_STEP_0_5kHz,  S_STEP_1_0kHz,  S_STEP_2_5kHz,  S_STEP_5_0kHz,  S_STEP_6_25kHz,
  S_STEP_8_33kHz,  S_STEP_10_0kHz,  S_STEP_12_5kHz,  S_STEP_25_0kHz,  S_STEP_100kHz,  S_STEP_500kHz, */

#define MAX_BANDS 49

#ifdef ENABLE_FULL_BAND
static const bandparameters BParams[MAX_BANDS] = {
    {"AIR-CIV",       11800000, 13700000, S_STEP_8_33kHz, MODULATION_AM}, // Commercial aviation
    {"HAM-2M",        14400000, 14800000, S_STEP_12_5kHz, MODULATION_FM}, // Global 2m (EU/US/AS)
    {"HAM-70CM",      43000000, 45000000, S_STEP_12_5kHz, MODULATION_FM}, // Global 70cm band
    {"DMR VHF",       14600000, 17000000, S_STEP_12_5kHz, MODULATION_FM},
    {"DMR UHF1",      45000000, 46600000, S_STEP_12_5kHz, MODULATION_FM},
    {"DMR UHF2",      46620000, 47000000, S_STEP_12_5kHz, MODULATION_FM}, //Pocsag FR removed 466-466.2
    {"CB",            2651500,  2830500,  S_STEP_5_0kHz,  MODULATION_AM}, // Enlarged Citizen Band
    {"SATCOM-LO",     24000000, 25000000, S_STEP_5_0kHz,  MODULATION_FM}, // Sat downlink (low)
    {"SATCOM-HI",     25050000, 27500000, S_STEP_5_0kHz,  MODULATION_FM}, // Sat downlink (high)
    {"MARINE-VHF",    15550000, 16215000, S_STEP_25_0kHz, MODULATION_FM}, // Intl maritime radio
    {"MARINE-16M",    1636000,  1741000,  S_STEP_1_0kHz,  MODULATION_AM}, // High-seas ship traffic
    {"HF-HAM-160M",   1810000,  2000000,  S_STEP_1_0kHz,  MODULATION_AM}, // Night-time long distance
    {"HF-HAM-80M",    3500000,  3800000,  S_STEP_1_0kHz,  MODULATION_AM}, // Regional night comms
    {"HF-HAM-40M",    7000000,  7200000,  S_STEP_1_0kHz,  MODULATION_AM}, // Very active global band
    {"HF-HAM-20M",    1400000,  1435000,  S_STEP_1_0kHz,  MODULATION_AM}, // Primary daytime DX band
    {"HF-HAM-17M",    1806800,  1816800,  S_STEP_1_0kHz,  MODULATION_AM}, // Upper HF propagation
    {"HF-HAM-15M",    2100000,  2145000,  S_STEP_1_0kHz,  MODULATION_AM}, // Solar cycle dependent DX
    {"HF-HAM-12M",    2489000,  2499000,  S_STEP_1_0kHz,  MODULATION_AM}, // High HF band
    {"HAM-10M",       2800000,  2970000,  S_STEP_1_0kHz,  MODULATION_FM}, // Technical FM/DX mix
    {"BC-19M",        1510000,  1580000,  S_STEP_5_0kHz,  MODULATION_AM}, // International news/radio
    {"BC-16M",        1748000,  1790000,  S_STEP_5_0kHz,  MODULATION_AM}, // Global daytime radio
    {"AIR-HF-17M",    1790000,  1803000,  S_STEP_2_5kHz,  MODULATION_AM}, // Long-range aero comms
    {"HAM-6M",        5000000,  5400000,  S_STEP_10_0kHz, MODULATION_FM}, // "Magic band" VHF
    {"HAM-4M",        7000000,  7050000,  S_STEP_12_5kHz, MODULATION_FM}, // European 70MHz band
    {"AIR-MIL-VHF",   13800000, 14400000, S_STEP_25_0kHz, MODULATION_AM}, // Military tactical VHF
    {"AIR-MIL-UHF",   22500000, 40000000, S_STEP_25_0kHz, MODULATION_AM}, // NATO Military UHF
    {"ISS-SPACE",     14580000, 14582500, S_STEP_5_0kHz,  MODULATION_FM}, // Space Station voice/data
    {"FIRE-EMERG",    14866250, 14933750, S_STEP_25_0kHz, MODULATION_FM}, // Emergency services
    {"FREENET-DE",    14902500, 14911250, S_STEP_12_5kHz, MODULATION_FM}, // German free-use VHF
    {"RAILWAY-EU",    15000000, 15550000, S_STEP_25_0kHz, MODULATION_FM}, // European train comms
    {"WEATHER-US",    16240000, 16255000, S_STEP_25_0kHz, MODULATION_FM}, // NOAA weather (USA)
    {"PRO-VHF",       16215000, 16900000, S_STEP_12_5kHz, MODULATION_FM}, // Commercial VHF
    {"POLICE-VHF",    17200000, 17397500, S_STEP_25_0kHz, MODULATION_FM}, // Legacy police/security
    {"EPIRB-DIST",    40600000, 40610000, S_STEP_1_0kHz,  MODULATION_FM}, // Intl distress beacons
    {"HAM-1.25M",     22200000, 22500000, S_STEP_12_5kHz, MODULATION_FM}, // USA amateur band
    {"LPD-433",       43307500, 43477500, S_STEP_25_0kHz, MODULATION_FM}, // Low power devices (EU)
    {"PMR-446",       44600625, 44619375, S_STEP_12_5kHz, MODULATION_FM}, // Consumer walkie-talkie
    {"FRS-GMRS-US",   46256250, 46771250, S_STEP_12_5kHz, MODULATION_FM}, // USA walkie-talkie
    {"PRO-UHF",       45000000, 47000000, S_STEP_12_5kHz, MODULATION_FM}, // Commercial UHF
    {"SRD-868",       86400000, 87000000, S_STEP_12_5kHz, MODULATION_FM}, // EU IoT / Smart home
    {"ISM-915-US",    90200000, 92800000, S_STEP_12_5kHz, MODULATION_FM}, // USA IoT / Remotes
    {"GSM-900-DN",    93500000, 96000000, S_STEP_12_5kHz, MODULATION_FM}, // Mobile network tower
    {"GSM-900-UP",    89000000, 91500000, S_STEP_12_5kHz, MODULATION_FM}, // Mobile network phone
    {"HAM-23CM",      124000000,130000000,S_STEP_25_0kHz, MODULATION_FM}, // 1.2GHz amateur
    {"MURS-USA",      15182000, 15460000, S_STEP_12_5kHz, MODULATION_FM}, // Multi-use Radio (USA)
    {"RIVER-CHN-1",   30002500, 30051250, S_STEP_5_0kHz,  MODULATION_FM}, // River navigation 1
    {"RIVER-CHN-2",   33601250, 34000050, S_STEP_5_0kHz,  MODULATION_FM}, // River navigation 2
    {"ARMY-RU",       30051300, 33000000, S_STEP_5_0kHz,  MODULATION_FM}, // Tactical RU comms
    {"FULL-SCAN",     1400000,  130000000, S_STEP_500kHz, MODULATION_FM}, // Rapid spectrum overview
}; 
#endif


