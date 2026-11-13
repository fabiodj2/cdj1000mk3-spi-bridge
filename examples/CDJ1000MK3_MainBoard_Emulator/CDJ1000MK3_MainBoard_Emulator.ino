/**
 * ==============================================================================
 * CDJ-1000 MK3 Main Board Emulator / Automated Panel Tester
 * ==============================================================================
 * Emulates the CDJ-1000 MK3 top panel / Display Board towards the Main Assy
 * motherboard. Useful for bench-testing CDJ motherboards, repairing units
 * with broken top panels, or automated test rigs.
 * 
 * Hardware Connections:
 *  - Connect Arduino SPI pins to CDJ Main Assy CN2002 (Display connector).
 *  - Arduino sends simulated button presses (Play, Cue, Jog, Pitch) to Main Board.
 * ==============================================================================
 */

#include <Arduino.h>
#include "CDJ1000MK3.h"

CDJ1000MK3_SPI spiBus;

uint8_t simulatedRxFrame[CDJ_FRAME_SIZE];
uint8_t receivedTxFrame[CDJ_FRAME_SIZE];

// Simulated state
uint16_t simPulses = 0;
uint16_t simPitch = 8192; // Center
bool simPlay = false;
bool simCue = false;

void initSimulatedPanel() {
    memset(simulatedRxFrame, 0, CDJ_FRAME_SIZE);

    simulatedRxFrame[0]  = 0x01; // Header 0
    simulatedRxFrame[1]  = 0x10; // Header 1
    simulatedRxFrame[2]  = 128;  // Touch/Brake center
    simulatedRxFrame[3]  = 128;  // Release/Start center
    simulatedRxFrame[4]  = (simPitch >> 6) & 0xFF; // Pitch MSB
    simulatedRxFrame[5]  = (simPitch << 2) & 0xFC; // Pitch LSB
    simulatedRxFrame[6]  = 128;  // Center Pot MSB
    simulatedRxFrame[7]  = 0;
    simulatedRxFrame[8]  = 0;    // Pulse counter MSB
    simulatedRxFrame[9]  = 0;    // Pulse counter LSB
    simulatedRxFrame[10] = 0xFF; // Speed MSB (65535 = stopped)
    simulatedRxFrame[11] = 0xFF; // Speed LSB
    simulatedRxFrame[12] = CDJ_SW_EJECT_LOCK | CDJ_SW_DIRECTION; // Unlocked, Forward
    simulatedRxFrame[19] = CDJ_CalculateRxChecksum(simulatedRxFrame);

    simulatedRxFrame[20] = 0xF1;
    simulatedRxFrame[21] = 0x64;
    simulatedRxFrame[22] = 0x76;
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    Serial.println(F("=============================================="));
    Serial.println(F("   CDJ-1000 MK3 MAIN BOARD EMULATOR / TESTER  "));
    Serial.println(F("=============================================="));

    initSimulatedPanel();

    // Start in Master Mode if driving Main Assy, using CDJ1000MK3_Pins.h
    spiBus.begin(CDJ_ROLE_MASTER, CDJ_PIN_CS, 1000000);

    Serial.println(F("Ready! Type commands in Serial Monitor:"));
    Serial.println(F("  'p' = Press & Release PLAY button"));
    Serial.println(F("  'c' = Press & Release CUE button"));
    Serial.println(F("  '+' = Rotate Jog CW (+100 pulses)"));
    Serial.println(F("  '-' = Rotate Jog CCW (-100 pulses)"));
    Serial.println(F("  'f' = Move pitch fader"));
}

void loop() {
    // Send 27-byte frame periodically to keep Main Assy alive
    static uint32_t lastPoll = 0;
    if (millis() - lastPoll >= 20) { // 50 Hz poll rate
        lastPoll = millis();

        // Update checksum
        simulatedRxFrame[19] = CDJ_CalculateRxChecksum(simulatedRxFrame);

        // Perform 27-byte full-duplex exchange
        spiBus.transferMaster(simulatedRxFrame, receivedTxFrame);

        // Parse any LED responses from Main Board
        if (receivedTxFrame[0] == CDJ_PKG_STATUS_7) {
            bool playLed = (receivedTxFrame[17] & CDJ_LED_PLAY) != 0;
            bool cueLed  = (receivedTxFrame[17] & CDJ_LED_CUE) != 0;
            static bool prevPlayLed = false;
            if (playLed != prevPlayLed) {
                Serial.print(F("[MAIN BOARD FEEDBACK] Play LED is now: "));
                Serial.println(playLed ? F("LIT") : F("OFF"));
                prevPlayLed = playLed;
            }
        }
    }

    // Serial console injection
    if (Serial.available()) {
        char ch = Serial.read();
        if (ch == 'p') {
            Serial.println(F("Injecting PLAY button press..."));
            simulatedRxFrame[14] |= CDJ_BTN_PLAY;
            simulatedRxFrame[19] = CDJ_CalculateRxChecksum(simulatedRxFrame);
            spiBus.transferMaster(simulatedRxFrame, receivedTxFrame);
            delay(100);
            simulatedRxFrame[14] &= ~CDJ_BTN_PLAY;
            simulatedRxFrame[19] = CDJ_CalculateRxChecksum(simulatedRxFrame);
            spiBus.transferMaster(simulatedRxFrame, receivedTxFrame);
        } else if (ch == 'c') {
            Serial.println(F("Injecting CUE button press..."));
            simulatedRxFrame[14] |= CDJ_BTN_CUE;
            simulatedRxFrame[19] = CDJ_CalculateRxChecksum(simulatedRxFrame);
            spiBus.transferMaster(simulatedRxFrame, receivedTxFrame);
            delay(100);
            simulatedRxFrame[14] &= ~CDJ_BTN_CUE;
            simulatedRxFrame[19] = CDJ_CalculateRxChecksum(simulatedRxFrame);
            spiBus.transferMaster(simulatedRxFrame, receivedTxFrame);
        } else if (ch == '+') {
            simPulses += 100;
            simulatedRxFrame[8] = (simPulses >> 8) & 0xFF;
            simulatedRxFrame[9] = simPulses & 0xFF;
            simulatedRxFrame[12] |= (CDJ_JOG_ROTATING | CDJ_JOG_ROT_FWD);
            Serial.print(F("Jog rotated CW. Pulses: "));
            Serial.println(simPulses);
        } else if (ch == '-') {
            simPulses -= 100;
            simulatedRxFrame[8] = (simPulses >> 8) & 0xFF;
            simulatedRxFrame[9] = simPulses & 0xFF;
            simulatedRxFrame[12] |= CDJ_JOG_ROTATING;
            simulatedRxFrame[12] &= ~CDJ_JOG_ROT_FWD;
            Serial.print(F("Jog rotated CCW. Pulses: "));
            Serial.println(simPulses);
        }
    }
}

