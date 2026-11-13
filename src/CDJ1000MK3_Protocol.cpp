/**
 * @file CDJ1000MK3_Protocol.cpp
 * @brief Implementation of CDJ-1000 MK3 Protocol Parser, Builder, and Checksum
 */

#include "CDJ1000MK3_Protocol.h"
#include <string.h>

// Default template for Package 7 (Status, LEDs, Jog, Display)
// Package ID = 168 (0xA8). Bytes 1..16 are standard display zeros and spaces.
static const uint8_t CDJ_DefaultPkg7[CDJ_FRAME_SIZE] = {
    168,  119,  119,  0,    119,  119,  0,    0,    0,    1,    
    176,  0,    0,    0,    0,    0,    0,    0x0C, 0,    0x20, 
    0,    0,    0,    88,   0,    1,    0
};

bool CDJ_VerifyRxFrame(const uint8_t *rawBytes) {
    if (!rawBytes) return false;
    // Check headers
    if (rawBytes[0] != 0x01 || rawBytes[1] != 0x10) {
        return false;
    }
    // Checksum verification
    uint8_t expectedCrc = CDJ_CalculateRxChecksum(rawBytes);
    return (rawBytes[19] == expectedCrc);
}

bool CDJ_ParseRxFrame(const uint8_t *rawBytes, CDJ_PanelState_t *outState, bool verifyCrc) {
    if (!rawBytes || !outState) return false;

    // Header validation: Pioneer CDJ-1000 MK3 always sends 0x01 0x10 as frame start
    if (rawBytes[0] != 0x01 || rawBytes[1] != 0x10) {
        return false;
    }

    if (verifyCrc) {
        if (!CDJ_VerifyRxFrame(rawBytes)) {
            return false;
        }
    }

    const CDJ_RxFrame_t *frame = (const CDJ_RxFrame_t *)rawBytes;

    // Analog Controls
    outState->touchBrake   = frame->touchBrakeAdc;
    outState->releaseStart = frame->releaseStartAdc;

    // 14-bit Pitch Fader: Combine MSB and upper 6 bits of LSB
    uint16_t newPitch = ((uint16_t)frame->pitchMsb << 6) | (frame->pitchLsb >> 2);
    outState->pitchFader = newPitch;

    // 14-bit Center Potentiometer
    outState->pitchCenterPot = ((uint16_t)frame->centerPotMsb << 6) | (frame->centerPotLsb >> 2);

    // Jog Pulse Counter (16-bit)
    uint16_t newPulses = ((uint16_t)frame->jogPulseMsb << 8) | frame->jogPulseLsb;
    outState->jogPulseDelta = (int16_t)(newPulses - outState->jogPulseCounter);
    outState->jogPulseCounter = newPulses;

    // Jog Speed (16-bit, 65535 when stopped)
    outState->jogSpeed = ((uint16_t)frame->jogSpeedMsb << 8) | frame->jogSpeedLsb;

    // Jog Status & Switches (Byte 12)
    outState->ejectLocked     = (frame->jogStatus & CDJ_SW_EJECT_LOCK) != 0;
    outState->reverseSwitch   = (frame->jogStatus & CDJ_SW_DIRECTION) != 0;
    outState->jogTouched      = (frame->jogStatus & CDJ_JOG_TOUCH_EN2) != 0;
    outState->jogDirectionFwd = (frame->jogStatus & CDJ_JOG_ROT_FWD) != 0;
    outState->jogRotating     = (frame->jogStatus & CDJ_JOG_ROTATING) != 0;

    // Transport & Hot Cue Buttons (Byte 14)
    outState->play            = (frame->buttons14 & CDJ_BTN_PLAY) != 0;
    outState->cue             = (frame->buttons14 & CDJ_BTN_CUE) != 0;
    outState->loopIn          = (frame->buttons14 & CDJ_BTN_LOOP_IN) != 0;
    outState->loopOut         = (frame->buttons14 & CDJ_BTN_LOOP_OUT) != 0;
    outState->reloopExit      = (frame->buttons14 & CDJ_BTN_RELOOP_EXIT) != 0;
    outState->hotCueA         = (frame->buttons14 & CDJ_BTN_HOTCUE_A) != 0;
    outState->hotCueB         = (frame->buttons14 & CDJ_BTN_HOTCUE_B) != 0;
    outState->hotCueC         = (frame->buttons14 & CDJ_BTN_HOTCUE_C) != 0;

    // Search & Track Buttons (Byte 16)
    outState->recMode         = (frame->buttons16 & CDJ_BTN_REC_MODE) != 0;
    outState->trackRev        = (frame->buttons16 & CDJ_BTN_TRACK_REV) != 0;
    outState->trackFwd        = (frame->buttons16 & CDJ_BTN_TRACK_FWD) != 0;
    outState->searchRev       = (frame->buttons16 & CDJ_BTN_SEARCH_REV) != 0;
    outState->searchFwd       = (frame->buttons16 & CDJ_BTN_SEARCH_FWD) != 0;

    // Folder & Tempo Buttons (Byte 17)
    outState->folderRev       = (frame->buttons17 & CDJ_BTN_FOLDER_REV) != 0;
    outState->folderFwd       = (frame->buttons17 & CDJ_BTN_FOLDER_FWD) != 0;
    outState->jogMode         = (frame->buttons17 & CDJ_BTN_JOG_MODE) != 0;
    outState->tempoRange      = (frame->buttons17 & CDJ_BTN_TEMPO_RANGE) != 0;
    outState->masterTempo     = (frame->buttons17 & CDJ_BTN_MASTER_TEMPO) != 0;
    outState->tempoReset      = (frame->buttons17 & CDJ_BTN_TEMPO_RESET) != 0;

    // Memory & Mode Buttons (Byte 18)
    outState->callFwd         = (frame->buttons18 & CDJ_BTN_CALL_FWD) != 0;
    outState->callRev         = (frame->buttons18 & CDJ_BTN_CALL_REV) != 0;
    outState->memory          = (frame->buttons18 & CDJ_BTN_MEMORY) != 0;
    outState->del             = (frame->buttons18 & CDJ_BTN_DELETE) != 0;
    outState->eject           = (frame->buttons18 & CDJ_BTN_EJECT) != 0;
    outState->timeMode        = (frame->buttons18 & CDJ_BTN_TIME_MODE) != 0;
    outState->textMode        = (frame->buttons18 & CDJ_BTN_TEXT_MODE) != 0;

    return true;
}

void CDJ_InitTxPackage7(uint8_t *rawBytes) {
    if (!rawBytes) return;
    memcpy(rawBytes, CDJ_DefaultPkg7, CDJ_FRAME_SIZE);
    rawBytes[26] = CDJ_CalculateTxChecksum(rawBytes);
}

void CDJ_SetTxLed(uint8_t *rawBytes, uint8_t byteIndex, uint8_t bitMask, bool enable) {
    if (!rawBytes || byteIndex >= CDJ_FRAME_SIZE) return;
    if (enable) {
        rawBytes[byteIndex] |= bitMask;
    } else {
        rawBytes[byteIndex] &= ~bitMask;
    }
    rawBytes[26] = CDJ_CalculateTxChecksum(rawBytes);
}

void CDJ_SetTxHotCue(uint8_t *rawBytes, uint8_t cueIndex, CDJ_HotCueColor_t color) {
    if (!rawBytes || cueIndex > 2) return;

    // Clear all bits for this Hot Cue first
    switch (cueIndex) {
        case 0: // Hot Cue A
            rawBytes[17] &= ~(CDJ_LED_HOTCUE_A_RED | CDJ_LED_HOTCUE_A_GREEN);
            rawBytes[18] &= ~CDJ_LED_HOTCUE_A_ORANGE;
            if (color == HOTCUE_GREEN)  rawBytes[17] |= CDJ_LED_HOTCUE_A_GREEN;
            if (color == HOTCUE_RED)    rawBytes[17] |= CDJ_LED_HOTCUE_A_RED;
            if (color == HOTCUE_ORANGE) rawBytes[18] |= CDJ_LED_HOTCUE_A_ORANGE;
            break;

        case 1: // Hot Cue B
            rawBytes[18] &= ~(CDJ_LED_HOTCUE_B_RED | CDJ_LED_HOTCUE_B_GREEN | CDJ_LED_HOTCUE_B_ORANGE);
            if (color == HOTCUE_GREEN)  rawBytes[18] |= CDJ_LED_HOTCUE_B_GREEN;
            if (color == HOTCUE_RED)    rawBytes[18] |= CDJ_LED_HOTCUE_B_RED;
            if (color == HOTCUE_ORANGE) rawBytes[18] |= CDJ_LED_HOTCUE_B_ORANGE;
            break;

        case 2: // Hot Cue C
            rawBytes[18] &= ~(CDJ_LED_HOTCUE_C_RED | CDJ_LED_HOTCUE_C_GREEN | CDJ_LED_HOTCUE_C_ORANGE);
            if (color == HOTCUE_GREEN)  rawBytes[18] |= CDJ_LED_HOTCUE_C_GREEN;
            if (color == HOTCUE_RED)    rawBytes[18] |= CDJ_LED_HOTCUE_C_RED;
            if (color == HOTCUE_ORANGE) rawBytes[18] |= CDJ_LED_HOTCUE_C_ORANGE;
            break;
    }

    rawBytes[26] = CDJ_CalculateTxChecksum(rawBytes);
}

void CDJ_SetTxJogRing(uint8_t *rawBytes, uint8_t position, uint8_t cuePosition) {
    if (!rawBytes) return;
    if (position >= CDJ_JOG_POS_MIN && position <= CDJ_JOG_POS_EJECT_ANIM) {
        rawBytes[25] = position;
    }
    if (cuePosition <= CDJ_JOG_CUE_MAX) {
        rawBytes[21] = cuePosition;
    }
    rawBytes[26] = CDJ_CalculateTxChecksum(rawBytes);
}

void CDJ_SetTxTouchRingVFD(uint8_t *rawBytes, bool showTouchRing) {
    if (!rawBytes) return;
    if (showTouchRing) {
        rawBytes[23] |= (1 << 5);  // VFD touch ring on
    } else {
        rawBytes[23] &= ~(1 << 5); // VFD touch ring off
    }
    rawBytes[26] = CDJ_CalculateTxChecksum(rawBytes);
}

void CDJ_SetTxTrackDisplay(uint8_t *rawBytes, uint8_t trackNumber) {
    if (!rawBytes) return;
    if (trackNumber > 99) trackNumber = 99;
    rawBytes[1] = CDJ_DigitTo7Segment('0' + (trackNumber / 10));
    rawBytes[2] = CDJ_DigitTo7Segment('0' + (trackNumber % 10));
    rawBytes[26] = CDJ_CalculateTxChecksum(rawBytes);
}

