/**
 * @file CDJ1000MK3_Protocol.h
 * @brief Pioneer CDJ-1000 MK3 Serial/SPI Protocol Definitions, Structs & Bitmasks
 * 
 * Reverse-engineered SPI communication format between Main Assy and Display/Jog Board.
 * Reference: Anatsko Andrei & vandoeselaar.
 */

#ifndef CDJ1000MK3_PROTOCOL_H
#define CDJ1000MK3_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// PROTOCOL CONSTANTS
// =============================================================================
#define CDJ_FRAME_SIZE              27    // Bytes per SPI transaction
#define CDJ_PPR                     3600  // Jog wheel pulses per single 360-deg rotation
#define CDJ_JOG_POS_MIN             1     // Jog ring cursor index start
#define CDJ_JOG_POS_MAX             135   // Jog ring cursor index end (135 segments)
#define CDJ_JOG_POS_FILL_CIRCLE     136   // Turn on full circle
#define CDJ_JOG_POS_LOAD_ANIM       137   // Loading animation
#define CDJ_JOG_POS_EJECT_ANIM      138   // Eject animation
#define CDJ_JOG_CUE_MIN             1     // Cue cursor min
#define CDJ_JOG_CUE_MAX             85    // Cue cursor max

// SPI Specifications
#define CDJ_SPI_MODE                3     // CPOL=1, CPHA=1 (Data captured on rising edge)
#define CDJ_SPI_BIT_ORDER           1     // MSB First
#define CDJ_SPI_CLOCK_HZ            1000000 // 1 MHz nominal (500 kHz - 2 MHz supported)

// Package Identifiers (Tbuffer[0])
#define CDJ_PKG_WAVE_0              0     // 0x00: Waveform 0
#define CDJ_PKG_WAVE_1              24    // 0x18: Waveform 1
#define CDJ_PKG_WAVE_2              48    // 0x30: Waveform 2
#define CDJ_PKG_WAVE_3              72    // 0x48: Waveform 3
#define CDJ_PKG_WAVE_4              96    // 0x60: Waveform 4 & Progress bar
#define CDJ_PKG_BAR_5               120   // 0x78: Cue/Time Progress bars
#define CDJ_PKG_TEXT_6              144   // 0x90: Track #, Folder #, VFD Text
#define CDJ_PKG_STATUS_7            168   // 0xA8: LEDs, Jog, Pitch, BPM, Time

// =============================================================================
// INPUT BITMASKS: Display Board => Main Assy (Rbuffer[27])
// =============================================================================

// Byte 12: Jog State & Mechanical Switches
#define CDJ_SW_EJECT_LOCK           (1 << 0)  // 1 = Unlock, 0 = Lock
#define CDJ_SW_DIRECTION            (1 << 1)  // 1 = Forward, 0 = Reverse
#define CDJ_JOG_TOUCH_EN1           (1 << 4)  // Touch sensor enable bit 1
#define CDJ_JOG_TOUCH_EN2           (1 << 5)  // Touch sensor enable bit 2 (hand on platter)
#define CDJ_JOG_ROT_FWD             (1 << 6)  // 1 = Forward rotation, 0 = Reverse rotation
#define CDJ_JOG_ROTATING            (1 << 7)  // 1 = Platter moving / rotation detected

// Byte 14: Transport & Hot Cue Buttons (Active High: 1 = Pressed, 0 = Released)
#define CDJ_BTN_PLAY                (1 << 0)  // Play / Pause
#define CDJ_BTN_CUE                 (1 << 1)  // Cue
#define CDJ_BTN_LOOP_IN             (1 << 2)  // Loop In / Realtime Cue
#define CDJ_BTN_LOOP_OUT            (1 << 3)  // Loop Out
#define CDJ_BTN_RELOOP_EXIT         (1 << 4)  // Reloop / Exit
#define CDJ_BTN_HOTCUE_A            (1 << 5)  // Hot Cue A
#define CDJ_BTN_HOTCUE_B            (1 << 6)  // Hot Cue B
#define CDJ_BTN_HOTCUE_C            (1 << 7)  // Hot Cue C

// Byte 16: Search & Track Buttons
#define CDJ_BTN_REC_MODE            (1 << 0)  // Rec Mode (Hot Cue Record)
#define CDJ_BTN_TRACK_REV           (1 << 1)  // |<< Track Search Back
#define CDJ_BTN_TRACK_FWD           (1 << 2)  // >>| Track Search Fwd
#define CDJ_BTN_SEARCH_REV          (1 << 3)  // << Search Rewind
#define CDJ_BTN_SEARCH_FWD          (1 << 4)  // >> Search Fast Forward

// Byte 17: Navigation & Tempo Buttons
#define CDJ_BTN_FOLDER_REV          (1 << 0)  // < Folder Search Back
#define CDJ_BTN_FOLDER_FWD          (1 << 1)  // > Folder Search Fwd
#define CDJ_BTN_JOG_MODE            (1 << 2)  // Jog Mode (Vinyl / CDJ toggle)
#define CDJ_BTN_TEMPO_RANGE         (1 << 3)  // Tempo (+-6, +-10, +-16, WIDE)
#define CDJ_BTN_MASTER_TEMPO        (1 << 4)  // Master Tempo (Key Lock)
#define CDJ_BTN_TEMPO_RESET         (1 << 5)  // Tempo Reset

// Byte 18: Memory & Utility Buttons
#define CDJ_BTN_CALL_FWD            (1 << 0)  // Memory Call >
#define CDJ_BTN_CALL_REV            (1 << 1)  // Memory < Call
#define CDJ_BTN_MEMORY              (1 << 2)  // Memory Set
#define CDJ_BTN_DELETE              (1 << 3)  // Memory Delete
#define CDJ_BTN_EJECT               (1 << 4)  // Eject Button
#define CDJ_BTN_TIME_MODE           (1 << 5)  // Time Mode / Auto Cue
#define CDJ_BTN_TEXT_MODE           (1 << 6)  // Text Mode

// Unified Button Identifier Enum (for event-driven callbacks)
typedef enum {
    CDJ_ID_NONE = 0,
    CDJ_ID_PLAY,
    CDJ_ID_CUE,
    CDJ_ID_LOOP_IN,
    CDJ_ID_LOOP_OUT,
    CDJ_ID_RELOOP_EXIT,
    CDJ_ID_HOTCUE_A,
    CDJ_ID_HOTCUE_B,
    CDJ_ID_HOTCUE_C,
    CDJ_ID_REC_MODE,
    CDJ_ID_TRACK_REV,
    CDJ_ID_TRACK_FWD,
    CDJ_ID_SEARCH_REV,
    CDJ_ID_SEARCH_FWD,
    CDJ_ID_FOLDER_REV,
    CDJ_ID_FOLDER_FWD,
    CDJ_ID_JOG_MODE,
    CDJ_ID_TEMPO_RANGE,
    CDJ_ID_MASTER_TEMPO,
    CDJ_ID_TEMPO_RESET,
    CDJ_ID_CALL_FWD,
    CDJ_ID_CALL_REV,
    CDJ_ID_MEMORY,
    CDJ_ID_DELETE,
    CDJ_ID_EJECT,
    CDJ_ID_TIME_MODE,
    CDJ_ID_TEXT_MODE,
    CDJ_ID_JOG_TOUCH,
    CDJ_ID_REVERSE_SWITCH,
    CDJ_ID_MAX
} CDJ_ButtonId_t;

// =============================================================================
// OUTPUT BITMASKS: Main Assy => Display Board (Tbuffer[27] - Package 7)
// =============================================================================

// Byte 17: LEDs 1
#define CDJ_LED_PLAY                (1 << 0)  // Green Play LED
#define CDJ_LED_CUE                 (1 << 1)  // Amber Cue LED
#define CDJ_LED_LOOP_IN             (1 << 2)  // Amber Loop In LED
#define CDJ_LED_LOOP_OUT            (1 << 3)  // Amber Loop Out LED
#define CDJ_LED_RELOOP_EXIT         (1 << 4)  // Amber Reloop/Exit LED
#define CDJ_LED_REVERSE             (1 << 5)  // Red Reverse Diode
#define CDJ_LED_HOTCUE_A_RED        (1 << 6)  // Hot Cue A Red
#define CDJ_LED_HOTCUE_A_GREEN      (1 << 7)  // Hot Cue A Green

// Byte 18: LEDs 2
#define CDJ_LED_HOTCUE_A_ORANGE     (1 << 0)  // Hot Cue A Orange
#define CDJ_LED_HOTCUE_B_RED        (1 << 1)  // Hot Cue B Red
#define CDJ_LED_HOTCUE_B_GREEN      (1 << 2)  // Hot Cue B Green
#define CDJ_LED_HOTCUE_B_ORANGE     (1 << 3)  // Hot Cue B Orange
#define CDJ_LED_HOTCUE_C_RED        (1 << 4)  // Hot Cue C Red
#define CDJ_LED_HOTCUE_C_GREEN      (1 << 5)  // Hot Cue C Green
#define CDJ_LED_HOTCUE_C_ORANGE     (1 << 6)  // Hot Cue C Orange

// Byte 19: LEDs 3
#define CDJ_LED_MASTER_TEMPO        (1 << 3)  // Master Tempo Red LED
#define CDJ_LED_TEMPO_RESET         (1 << 4)  // Tempo Reset Green LED
#define CDJ_LED_VINYL               (1 << 5)  // Vinyl Blue LED
#define CDJ_LED_CDJ                 (1 << 6)  // CDJ Green LED
#define CDJ_LED_SD_CARD             (1 << 7)  // SD Card Green LED

// Hot Cue Color States
typedef enum {
    HOTCUE_OFF = 0,
    HOTCUE_GREEN,
    HOTCUE_ORANGE,
    HOTCUE_RED
} CDJ_HotCueColor_t;

// Pitch Range Settings
typedef enum {
    CDJ_PITCH_6PCT = 0,
    CDJ_PITCH_10PCT,
    CDJ_PITCH_16PCT,
    CDJ_PITCH_WIDE
} CDJ_PitchRange_t;

// =============================================================================
// RAW PACKET STRUCTS (27 Bytes each)
// =============================================================================

#pragma pack(push, 1)

/**
 * @brief Raw Frame received from Display Board (Display Board => Main Assy)
 */
typedef struct {
    uint8_t  header0;         // Byte 0: Always 0x01
    uint8_t  header1;         // Byte 1: Always 0x10
    uint8_t  touchBrakeAdc;   // Byte 2: Touch/Brake Knob ADC (0..255)
    uint8_t  releaseStartAdc; // Byte 3: Release/Start Knob ADC (0..255)
    uint8_t  pitchMsb;        // Byte 4: Pitch Fader 14-bit MSB
    uint8_t  pitchLsb;        // Byte 5: Pitch Fader 14-bit LSB (upper 6 bits)
    uint8_t  centerPotMsb;    // Byte 6: Center Potentiometer MSB (14-bit)
    uint8_t  centerPotLsb;    // Byte 7: Center Potentiometer LSB
    uint8_t  jogPulseMsb;     // Byte 8: Jog Pulse Counter MSB (0..65535, 3600 PPR)
    uint8_t  jogPulseLsb;     // Byte 9: Jog Pulse Counter LSB
    uint8_t  jogSpeedMsb;     // Byte 10: Jog Speed MSB (65535 when stopped)
    uint8_t  jogSpeedLsb;     // Byte 11: Jog Speed LSB
    uint8_t  jogStatus;       // Byte 12: Direction switch, touch sensor, rot detect
    uint8_t  reserved13;      // Byte 13: 0x00
    uint8_t  buttons14;       // Byte 14: Play, Cue, Loops, Hot Cues
    uint8_t  reserved15;      // Byte 15: 0x00
    uint8_t  buttons16;       // Byte 16: Track & Search buttons
    uint8_t  buttons17;       // Byte 17: Folder & Tempo buttons
    uint8_t  buttons18;       // Byte 18: Memory & Mode buttons
    uint8_t  checksum;        // Byte 19: Checksum (Sum bytes 1..18 % 256)
    uint8_t  tail20;          // Byte 20: 0xF1
    uint8_t  tail21;          // Byte 21: 0x64
    uint8_t  tail22;          // Byte 22: 0x76
    uint8_t  tail23;          // Byte 23: 0x00
    uint8_t  tail24;          // Byte 24: 0x00
    uint8_t  tail25;          // Byte 25: 0x00
    uint8_t  tail26;          // Byte 26: 0x00
} CDJ_RxFrame_t;

/**
 * @brief Raw Frame transmitted to Display Board (Main Assy => Display Board)
 */
typedef struct {
    uint8_t  packageId;       // Byte 0: Package 0, 24, 48, 72, 96, 120, 144, 168
    uint8_t  data[24];        // Bytes 1..24: Display payload (Package 7 contains LEDs/VFD)
    uint8_t  jogPosition;     // Byte 25: Jog position cursor (1..135)
    uint8_t  checksum;        // Byte 26: Checksum (Sum bytes 1..25 % 256)
} CDJ_TxFrame_t;

#pragma pack(pop)

// =============================================================================
// PARSED HIGH-LEVEL DATA STRUCTURES
// =============================================================================

typedef struct {
    // Buttons (true = currently held pressed)
    bool play;
    bool cue;
    bool loopIn;
    bool loopOut;
    bool reloopExit;
    bool hotCueA;
    bool hotCueB;
    bool hotCueC;
    bool recMode;
    bool trackRev;
    bool trackFwd;
    bool searchRev;
    bool searchFwd;
    bool folderRev;
    bool folderFwd;
    bool jogMode;
    bool tempoRange;
    bool masterTempo;
    bool tempoReset;
    bool callFwd;
    bool callRev;
    bool memory;
    bool del;
    bool eject;
    bool timeMode;
    bool textMode;

    // Platter & Mechanical Switches
    bool jogTouched;          // Hand touching top metallic surface
    bool jogRotating;         // Jog platter moving
    bool jogDirectionFwd;     // True = CW (clockwise), False = CCW
    bool reverseSwitch;       // True = Forward, False = Reverse play switch
    bool ejectLocked;         // True = Unlocked, False = Locked

    // Analog Controls
    uint16_t pitchFader;      // 14-bit ADC (0..16383, center ~8192)
    uint16_t pitchCenterPot;  // 14-bit Center Potentiometer ADC
    uint8_t  touchBrake;      // 8-bit (0..255)
    uint8_t  releaseStart;    // 8-bit (0..255)

    // Jog Encoder
    uint16_t jogPulseCounter; // 16-bit cumulative pulse counter (0..65535)
    int16_t  jogPulseDelta;   // Difference in pulses since last read
    uint16_t jogSpeed;        // Speed value (65535 = stopped)
} CDJ_PanelState_t;

// =============================================================================
// PROTOCOL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Calculate Transmit Checksum (for Tbuffer)
 * Formula: Sum of bytes 1 to 25 modulo 256
 */
static inline uint8_t CDJ_CalculateTxChecksum(const uint8_t *frame) {
    uint16_t sum = 0;
    for (uint8_t i = 1; i <= 25; i++) {
        sum += frame[i];
    }
    return (uint8_t)(sum & 0xFF);
}

/**
 * @brief Calculate Receive Checksum (for Rbuffer)
 * Formula: Sum of bytes 1 to 18 modulo 256
 */
static inline uint8_t CDJ_CalculateRxChecksum(const uint8_t *frame) {
    uint16_t sum = 0;
    for (uint8_t i = 1; i <= 18; i++) {
        sum += frame[i];
    }
    return (uint8_t)(sum & 0xFF);
}

/**
 * @brief Convert standard ASCII digit ('0'-'9') to Pioneer CDJ 7-segment byte
 */
static inline uint8_t CDJ_DigitTo7Segment(char digit) {
    switch (digit) {
        case '0': return 119;
        case '1': return 18;
        case '2': return 107;
        case '3': return 91;
        case '4': return 30;
        case '5': return 93;
        case '6': return 125;
        case '7': return 19;
        case '8': return 127;
        case '9': return 95;
        default:  return 0; // Blank
    }
}

#ifdef __cplusplus
}
#endif

#endif // CDJ1000MK3_PROTOCOL_H

