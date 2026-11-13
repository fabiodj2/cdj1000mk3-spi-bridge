/**
 * ==============================================================================
 * CDJ-1000 MK3 Panel Reader & Serial Monitor Test
 * ==============================================================================
 * This sketch interfaces an Arduino with a physical Pioneer CDJ-1000 MK3 top panel.
 * 
 * Konfigurasi pinout sekarang TERPISAH di file: "src/CDJ1000MK3_Pins.h"
 * Anda dapat memilih board atau menentukan pin kustom di CDJ1000MK3_Pins.h!
 * ==============================================================================
 */

#include <Arduino.h>
#include "CDJ1000MK3.h"

CDJ1000MK3_Panel cdj;

// State tracking for LED toggles
bool playLedState = false;
bool cueLedState  = false;
bool vinylLedState = true;
uint8_t jogPosition = 1;

void handleButtonEvent(CDJ_ButtonId_t btn, bool pressed) {
    Serial.print(F("[BUTTON] "));
    Serial.print(CDJ1000MK3_Panel::getButtonName(btn));
    Serial.println(pressed ? F(" -> PRESSED") : F(" -> RELEASED"));

    // Interactive LED feedback: Light up Play / Cue when pressed
    if (btn == CDJ_ID_PLAY && pressed) {
        playLedState = !playLedState;
        cdj.setPlayLed(playLedState);
        Serial.print(F("[LED] Play LED: "));
        Serial.println(playLedState ? F("ON") : F("OFF"));
    } else if (btn == CDJ_ID_CUE && pressed) {
        cueLedState = !cueLedState;
        cdj.setCueLed(cueLedState);
        Serial.print(F("[LED] Cue LED: "));
        Serial.println(cueLedState ? F("ON") : F("OFF"));
    } else if (btn == CDJ_ID_JOG_MODE && pressed) {
        vinylLedState = !vinylLedState;
        cdj.setVinylLed(vinylLedState);
        cdj.setCdjLed(!vinylLedState);
        Serial.print(F("[LED] Jog Mode: "));
        Serial.println(vinylLedState ? F("VINYL") : F("CDJ"));
    } else if (btn == CDJ_ID_HOTCUE_A && pressed) {
        static uint8_t hotA = 0;
        hotA = (hotA + 1) % 4;
        cdj.setHotCue(0, (CDJ_HotCueColor_t)hotA);
    } else if (btn == CDJ_ID_HOTCUE_B && pressed) {
        static uint8_t hotB = 0;
        hotB = (hotB + 1) % 4;
        cdj.setHotCue(1, (CDJ_HotCueColor_t)hotB);
    } else if (btn == CDJ_ID_HOTCUE_C && pressed) {
        static uint8_t hotC = 0;
        hotC = (hotC + 1) % 4;
        cdj.setHotCue(2, (CDJ_HotCueColor_t)hotC);
    }

    // Immediately push LED update to panel
    cdj.commitLeds();
}

void handleJogEvent(int16_t delta, bool touched, uint16_t speed) {
    // Advance jog cursor ring display (1..135)
    int nextPos = (int)jogPosition + (delta / 8);
    while (nextPos > CDJ_JOG_POS_MAX) nextPos -= (CDJ_JOG_POS_MAX - CDJ_JOG_POS_MIN + 1);
    while (nextPos < CDJ_JOG_POS_MIN) nextPos += (CDJ_JOG_POS_MAX - CDJ_JOG_POS_MIN + 1);
    jogPosition = (uint8_t)nextPos;

    cdj.setJogPosition(jogPosition);
    cdj.setTouchRing(touched);
    cdj.commitLeds();

    // Print to Serial (throttled)
    static uint32_t lastJogPrint = 0;
    if (millis() - lastJogPrint > 50) {
        Serial.print(F("[JOG] Delta: "));
        Serial.print(delta);
        Serial.print(F(" | Pulses: "));
        Serial.print(cdj.getJogPulses());
        Serial.print(F(" | Touch: "));
        Serial.print(touched ? F("YES") : F("NO"));
        Serial.print(F(" | Pos: "));
        Serial.println(jogPosition);
        lastJogPrint = millis();
    }
}

void handlePitchEvent(uint16_t pitchValue) {
    // Pitch fader is 14-bit (0..16383). Center is ~8192.
    float percent = ((float)pitchValue - 8192.0f) / 8192.0f * 100.0f;
    Serial.print(F("[PITCH] Raw: "));
    Serial.print(pitchValue);
    Serial.print(F(" | Center: "));
    Serial.print(cdj.getPitchCenterPot());
    Serial.print(F(" | Percent: "));
    Serial.print(percent, 2);
    Serial.println(F("%"));
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    Serial.println(F("\n=============================================="));
    Serial.println(F("   PIONEER CDJ-1000 MK3 PANEL ARDUINO EMULATOR "));
    Serial.println(F("=============================================="));
    Serial.print(F("Board Target : "));
    Serial.println(F(CDJ_BOARD_NAME));
    Serial.print(F("Pinout (SPI) : SCLK=Pin "));
    Serial.print(CDJ_PIN_SCLK);
    Serial.print(F(" | SOUT/MISO=Pin "));
    Serial.print(CDJ_PIN_MISO);
    Serial.print(F(" | SIN/MOSI=Pin "));
    Serial.print(CDJ_PIN_MOSI);
    Serial.print(F(" | XCS/CS=Pin "));
    Serial.println(CDJ_PIN_CS);
    Serial.println(F("Initializing SPI Slave Mode (Connecting to CDJ Top Panel)..."));

    // Uses pin configuration from CDJ1000MK3_Pins.h
    if (!cdj.begin(CDJ_ROLE_SLAVE, CDJ_PIN_CS)) {
        Serial.println(F("[ERROR] Failed to initialize SPI interface!"));
        while (1);
    }

    // Register event callbacks
    cdj.onButton(handleButtonEvent);
    cdj.onJog(handleJogEvent);
    cdj.onPitch(handlePitchEvent);

    // Initial LED setup
    cdj.setVinylLed(true);
    cdj.setPlayLed(false);
    cdj.setCueLed(true);
    cdj.setJogPosition(1);
    cdj.commitLeds();

    Serial.println(F("System Ready! Waiting for CDJ-1000 MK3 SPI clock from panel..."));
    Serial.println(F("Commands available via Serial:"));
    Serial.println(F("  'p' = Toggle Play LED"));
    Serial.println(F("  'c' = Toggle Cue LED"));
    Serial.println(F("  'v' = Toggle Vinyl / CDJ Mode"));
    Serial.println(F("  'a' = Animate Jog ring"));
    Serial.println(F("  't' = Test all LEDs"));
}

void loop() {
    // Process CDJ-1000 MK3 SPI communication
    cdj.update();

    // Handle serial console commands
    if (Serial.available()) {
        char ch = Serial.read();
        if (ch == 'p') {
            playLedState = !playLedState;
            cdj.setPlayLed(playLedState);
            cdj.commitLeds();
            Serial.println(playLedState ? F("Play LED ON") : F("Play LED OFF"));
        } else if (ch == 'c') {
            cueLedState = !cueLedState;
            cdj.setCueLed(cueLedState);
            cdj.commitLeds();
            Serial.println(cueLedState ? F("Cue LED ON") : F("Cue LED OFF"));
        } else if (ch == 'v') {
            vinylLedState = !vinylLedState;
            cdj.setVinylLed(vinylLedState);
            cdj.setCdjLed(!vinylLedState);
            cdj.commitLeds();
            Serial.println(vinylLedState ? F("Vinyl Mode ON") : F("CDJ Mode ON"));
        } else if (ch == 'a') {
            Serial.println(F("Running Jog Ring spin animation..."));
            for (uint8_t i = 1; i <= 135; i += 2) {
                cdj.setJogPosition(i);
                cdj.commitLeds();
                delay(10);
            }
        } else if (ch == 't') {
            Serial.println(F("Testing all LEDs ON for 2 seconds..."));
            cdj.setPlayLed(true);
            cdj.setCueLed(true);
            cdj.setLoopInLed(true);
            cdj.setLoopOutLed(true);
            cdj.setReloopExitLed(true);
            cdj.setReverseLed(true);
            cdj.setHotCue(0, HOTCUE_GREEN);
            cdj.setHotCue(1, HOTCUE_ORANGE);
            cdj.setHotCue(2, HOTCUE_RED);
            cdj.setMasterTempoLed(true);
            cdj.setTempoResetLed(true);
            cdj.setVinylLed(true);
            cdj.setCdjLed(true);
            cdj.commitLeds();
            delay(2000);
            cdj.setPlayLed(false);
            cdj.setCueLed(false);
            cdj.setLoopInLed(false);
            cdj.setLoopOutLed(false);
            cdj.setReloopExitLed(false);
            cdj.setReverseLed(false);
            cdj.setHotCue(0, HOTCUE_OFF);
            cdj.setHotCue(1, HOTCUE_OFF);
            cdj.setHotCue(2, HOTCUE_OFF);
            cdj.setMasterTempoLed(false);
            cdj.setTempoResetLed(false);
            cdj.setVinylLed(true);
            cdj.setCdjLed(false);
            cdj.commitLeds();
            Serial.println(F("LED test complete."));
        }
    }
}

