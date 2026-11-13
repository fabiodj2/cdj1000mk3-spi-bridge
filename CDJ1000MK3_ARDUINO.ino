/**
 * ==============================================================================
 * PIONEER CDJ-1000 MK3 HARDWARE DEBUG & EMULATION SUITE FOR ARDUINO UNO
 * ==============================================================================
 * Author    : Hanif Bagus Saputra
 * Repository: https://github.com/rayocta303/cdj1000mk3-spi-bridge
 * Target    : Arduino UNO (ATmega328P - 5V)
 * Baud Rate : 115200 bps
 * 
 * Hardware Connections (CDJ Top Panel CN2002 Connector):
 *   - CDJ Pin 13 (SCLK)  --> Arduino Uno Pin 13 (Hardware SCK - Clock input dari CDJ)
 *   - CDJ Pin 14 (SOUT)  --> Arduino Uno Pin 11 (Hardware MOSI - Data input dari CDJ)
 *   - CDJ Pin 15 (SIN)   --> Arduino Uno Pin 12 (Hardware MISO - Respon output ke CDJ)
 *   - CDJ Pin 16 (XCS)   --> Arduino Uno Pin 10 (Hardware SS - Frame Sync dari CDJ)
 *   - CDJ Pin GND        --> Arduino Uno Pin GND
 *   - Level Tegangan     : 5V Langsung (Direct, tanpa level shifter)
 * ==============================================================================
 */

#include <Arduino.h>
#include "CDJ1000MK3_Uno_Config.h"
#include "src/CDJ1000MK3.h"

CDJ1000MK3_Panel cdj;

// -----------------------------------------------------------------------------
// TELEMETRY & DIAGNOSTIC CONFIGURATION
// -----------------------------------------------------------------------------
bool streamRawHex     = false;
bool streamEvents     = true;
bool streamAnalog     = false;

// -----------------------------------------------------------------------------
// PIONEER DECK STATE MACHINE VARIABLES
// -----------------------------------------------------------------------------
bool s_isPlaying       = false;
bool s_cueActive       = true;
bool s_loopInSet       = false;
bool s_loopActive      = false;
bool s_vinylMode       = true;
bool s_recMode         = false;
uint8_t s_hotCueColor[3] = { (uint8_t)HOTCUE_OFF, (uint8_t)HOTCUE_OFF, (uint8_t)HOTCUE_OFF };
uint8_t s_tempoRange   = 1;     // 0 = +/-6%, 1 = +/-10%, 2 = +/-16%, 3 = WIDE (+/-100%)
bool s_masterTempo     = false;
bool s_tempoReset      = false;
bool s_reverseActive   = false;
uint8_t s_trackNumber  = 1;     // 1..99
uint8_t s_folderNumber = 1;     // 1..99
uint8_t s_memoryBank   = 1;     // 1..10
bool s_memorySaved[10] = { false };
bool s_remainTime      = false;
uint8_t s_textMode     = 0;     // 0 = Title, 1 = Artist, 2 = Album
bool s_searchFwdActive = false;
bool s_searchRevActive = false;

// Time Engine (75 frames per second = Standard CD Audio subcode)
uint8_t s_timeMinutes  = 0;
uint8_t s_timeSeconds  = 0;
uint8_t s_timeFrames   = 0;
uint8_t s_cueMinutes   = 0;
uint8_t s_cueSeconds   = 0;
uint8_t s_cueFrames    = 0;
uint32_t s_lastTimeTick = 0;

// Jog Platter Tracking & Vinyl Scratch Simulation
uint8_t jogRingPos        = 1;
uint8_t s_jogPlayCursor   = 1;
int32_t s_jogAccumulator  = 0;
bool    s_jogTouched      = false;

// Pitch & Tempo Tracking
float s_currentPitchPct   = 0.0f;

// Non-blocking Animation State Machines
bool isSweepingJog        = false;
uint8_t sweepPos          = 1;
uint32_t lastSweepTime    = 0;

bool isSelfTesting        = false;
uint8_t selfTestStep      = 0;
uint32_t lastSelfTestTime = 0;

bool s_isEjecting         = false;
uint32_t s_lastEjectTime  = 0;

// Performance Metrics
uint32_t frameCount       = 0;
uint32_t lastFpsCheck     = 0;
float    currentFps       = 0.0f;

// -----------------------------------------------------------------------------
// HELPER: Padded Button Name Output for Aligned Serial Logging
// -----------------------------------------------------------------------------
void printPaddedButtonName(CDJ_ButtonId_t btn) {
    const char* name = CDJ1000MK3_Panel::getButtonName(btn);
    Serial.print(name);
    int len = strlen(name);
    for (int i = len; i < 20; i++) {
        Serial.print(' ');
    }
}

// -----------------------------------------------------------------------------
// UPDATE ALL LED STATES ON HARDWARE PANEL
// -----------------------------------------------------------------------------
void updateHotCueLeds() {
    for (uint8_t i = 0; i < 3; i++) {
        if (s_recMode) {
            if (s_hotCueColor[i] == (uint8_t)HOTCUE_OFF) {
                cdj.setHotCue(i, HOTCUE_RED);
            } else {
                cdj.setHotCue(i, HOTCUE_ORANGE);
            }
        } else {
            cdj.setHotCue(i, (CDJ_HotCueColor_t)s_hotCueColor[i]);
        }
    }
}

void applyCurrentDeckStateToLeds() {
    cdj.setPlayLed(s_isPlaying);
    cdj.setCueLed(s_cueActive);
    cdj.setLoopInLed(s_loopInSet);
    cdj.setLoopOutLed(s_loopActive);
    cdj.setReloopExitLed(s_loopActive);
    cdj.setVinylLed(s_vinylMode);
    cdj.setCdjLed(!s_vinylMode);
    cdj.setMasterTempoLed(s_masterTempo);
    cdj.setTempoResetLed(s_tempoReset);
    cdj.setReverseLed(s_reverseActive);

    // [DISABLED] Kode LCD: Track, Time, & Tempo Display
    // cdj.setTrackDisplay(s_trackNumber);
    // cdj.setTimeDisplay(s_timeMinutes, s_timeSeconds, s_timeFrames);
    // cdj.setTempoDisplay(s_currentPitchPct);

    updateHotCueLeds();
    cdj.setJogPosition(jogRingPos);
    cdj.commitLeds();
}

void handleHotCuePress(uint8_t index) {
    if (s_recMode) {
        // Record Hot Cue at current playback time
        s_hotCueColor[index] = (uint8_t)HOTCUE_GREEN;
        s_recMode = false;
        updateHotCueLeds();
        cdj.commitLeds();
        if (streamEvents) {
            Serial.print(F("[BUTTON]   HOT CUE "));
            Serial.print((char)('A' + index));
            Serial.println(F("          -> PRESSED   Point Recorded at Current Time"));
        }
    } else {
        if (s_hotCueColor[index] != (uint8_t)HOTCUE_OFF) {
            // Jump to Hot Cue
            if (streamEvents) {
                Serial.print(F("[BUTTON]   HOT CUE "));
                Serial.print((char)('A' + index));
                Serial.println(F("          -> PRESSED   Jump to Hot Cue Point"));
            }
        } else {
            // Quick Assign
            s_hotCueColor[index] = (uint8_t)HOTCUE_GREEN;
            updateHotCueLeds();
            cdj.commitLeds();
            if (streamEvents) {
                Serial.print(F("[BUTTON]   HOT CUE "));
                Serial.print((char)('A' + index));
                Serial.println(F("          -> PRESSED   Quick Assigned (Green LED ON)"));
            }
        }
    }
}

// -----------------------------------------------------------------------------
// EVENT CALLBACK: BUTTONS & SWITCHES
// -----------------------------------------------------------------------------
void onButtonEvent(CDJ_ButtonId_t btn, bool pressed) {
    if (!pressed) {
        if (streamEvents) {
            Serial.print(F("[BUTTON]   "));
            printPaddedButtonName(btn);
            Serial.println(F(" -> RELEASED"));
        }
        if (btn == CDJ_ID_SEARCH_FWD) s_searchFwdActive = false;
        if (btn == CDJ_ID_SEARCH_REV) s_searchRevActive = false;
        return;
    }

    switch (btn) {
        case CDJ_ID_PLAY:
            s_isPlaying = !s_isPlaying;
            s_cueActive = !s_isPlaying;
            cdj.setPlayLed(s_isPlaying);
            cdj.setCueLed(s_cueActive);
            cdj.commitLeds();
            if (streamEvents) {
                Serial.print(F("[BUTTON]   PLAY/PAUSE           -> PRESSED   State: "));
                Serial.println(s_isPlaying ? F("PLAYING (Green LED ON)") : F("PAUSED (Cue LED ON)"));
            }
            break;

        case CDJ_ID_CUE:
            if (s_isPlaying) {
                s_isPlaying = false;
                s_cueActive = true;
                // [DISABLED] Time Reset
                // s_timeMinutes = s_cueMinutes;
                // s_timeSeconds = s_cueSeconds;
                // s_timeFrames  = s_cueFrames;
                // cdj.setTimeDisplay(s_timeMinutes, s_timeSeconds, s_timeFrames);
                s_jogPlayCursor = 1;
                jogRingPos = 1;
                cdj.setJogPosition(1);
                cdj.setPlayLed(false);
                cdj.setCueLed(true);
                cdj.commitLeds();
                if (streamEvents) Serial.println(F("[BUTTON]   CUE                  -> PRESSED   Paused & Cue Point Ready"));
            } else {
                s_cueActive = true;
                cdj.setCueLed(true);
                cdj.commitLeds();
                if (streamEvents) Serial.println(F("[BUTTON]   CUE                  -> PRESSED   Cue Point Active"));
            }
            break;

        case CDJ_ID_HOTCUE_A:
            handleHotCuePress(0);
            break;

        case CDJ_ID_HOTCUE_B:
            handleHotCuePress(1);
            break;

        case CDJ_ID_HOTCUE_C:
            handleHotCuePress(2);
            break;

        case CDJ_ID_REC_MODE:
            s_recMode = !s_recMode;
            updateHotCueLeds();
            cdj.commitLeds();
            if (streamEvents) {
                Serial.print(F("[BUTTON]   REC MODE             -> PRESSED   Hot Cue Record: "));
                Serial.println(s_recMode ? F("ARMED (Red)") : F("OFF"));
            }
            break;

        case CDJ_ID_LOOP_IN:
            s_loopInSet = true;
            cdj.setLoopInLed(true);
            cdj.commitLeds();
            if (streamEvents) Serial.println(F("[BUTTON]   LOOP IN              -> PRESSED   Loop In Point Set (Amber LED ON)"));
            break;

        case CDJ_ID_LOOP_OUT:
            if (s_loopInSet) {
                s_loopActive = true;
                cdj.setLoopOutLed(true);
                cdj.setReloopExitLed(true);
                cdj.commitLeds();
                if (streamEvents) Serial.println(F("[BUTTON]   LOOP OUT             -> PRESSED   Seamless Loop Active (Out/Reloop LED ON)"));
            } else {
                if (streamEvents) Serial.println(F("[BUTTON]   LOOP OUT             -> PRESSED   Ignored (Press Loop In First)"));
            }
            break;

        case CDJ_ID_RELOOP_EXIT:
            if (s_loopActive) {
                s_loopActive = false;
                cdj.setLoopOutLed(false);
                cdj.setReloopExitLed(false);
                cdj.commitLeds();
                if (streamEvents) Serial.println(F("[BUTTON]   RELOOP/EXIT          -> PRESSED   Loop Exited (LEDs OFF)"));
            } else if (s_loopInSet) {
                s_loopActive = true;
                cdj.setLoopOutLed(true);
                cdj.setReloopExitLed(true);
                cdj.commitLeds();
                if (streamEvents) Serial.println(F("[BUTTON]   RELOOP/EXIT          -> PRESSED   Reloop Jumped (LEDs ON)"));
            }
            break;

        case CDJ_ID_JOG_MODE:
            s_vinylMode = !s_vinylMode;
            cdj.setVinylLed(s_vinylMode);
            cdj.setCdjLed(!s_vinylMode);
            cdj.commitLeds();
            if (streamEvents) {
                Serial.print(F("[BUTTON]   JOG MODE             -> PRESSED   Mode: "));
                Serial.println(s_vinylMode ? F("VINYL (Blue LED ON)") : F("CDJ (Green LED ON)"));
            }
            break;

        case CDJ_ID_TRACK_FWD:
            s_trackNumber = (s_trackNumber < 99) ? s_trackNumber + 1 : 1;
            // [DISABLED] Kode LCD
            // cdj.setTrackDisplay(s_trackNumber);
            // cdj.commitLeds();
            if (streamEvents) {
                Serial.print(F("[BUTTON]   TRACK SEARCH >>      -> PRESSED   Track: "));
                if (s_trackNumber < 10) Serial.print('0');
                Serial.println(s_trackNumber);
            }
            break;

        case CDJ_ID_TRACK_REV:
            s_trackNumber = (s_trackNumber > 1) ? s_trackNumber - 1 : 99;
            // [DISABLED] Kode LCD
            // cdj.setTrackDisplay(s_trackNumber);
            // cdj.commitLeds();
            if (streamEvents) {
                Serial.print(F("[BUTTON]   TRACK SEARCH <<      -> PRESSED   Track: "));
                if (s_trackNumber < 10) Serial.print('0');
                Serial.println(s_trackNumber);
            }
            break;

        case CDJ_ID_SEARCH_FWD:
            s_searchFwdActive = true;
            if (streamEvents) Serial.println(F("[BUTTON]   SEARCH >>            -> PRESSED   Fast Forward Active"));
            break;

        case CDJ_ID_SEARCH_REV:
            s_searchRevActive = true;
            if (streamEvents) Serial.println(F("[BUTTON]   SEARCH <<            -> PRESSED   Fast Rewind Active"));
            break;

        case CDJ_ID_FOLDER_FWD:
            s_folderNumber = (s_folderNumber < 99) ? s_folderNumber + 1 : 1;
            if (streamEvents) {
                Serial.print(F("[BUTTON]   MP3 FOLDER >         -> PRESSED   Folder: "));
                if (s_folderNumber < 10) Serial.print('0');
                Serial.println(s_folderNumber);
            }
            break;

        case CDJ_ID_FOLDER_REV:
            s_folderNumber = (s_folderNumber > 1) ? s_folderNumber - 1 : 99;
            if (streamEvents) {
                Serial.print(F("[BUTTON]   MP3 FOLDER <         -> PRESSED   Prev Folder: "));
                if (s_folderNumber < 10) Serial.print('0');
                Serial.println(s_folderNumber);
            }
            break;

        case CDJ_ID_TEMPO_RANGE:
            s_tempoRange = (s_tempoRange + 1) % 4;
            if (streamEvents) {
                Serial.print(F("[BUTTON]   TEMPO RANGE          -> PRESSED   Pitch Range: "));
                switch (s_tempoRange) {
                    case 0: Serial.println(F("+/- 6%")); break;
                    case 1: Serial.println(F("+/- 10%")); break;
                    case 2: Serial.println(F("+/- 16%")); break;
                    case 3: Serial.println(F("WIDE (+/- 100%)")); break;
                }
            }
            break;

        case CDJ_ID_MASTER_TEMPO:
            s_masterTempo = !s_masterTempo;
            cdj.setMasterTempoLed(s_masterTempo);
            cdj.commitLeds();
            if (streamEvents) {
                Serial.print(F("[BUTTON]   MASTER TEMPO         -> PRESSED   Key Lock: "));
                Serial.println(s_masterTempo ? F("ON (Red LED ON)") : F("OFF"));
            }
            break;

        case CDJ_ID_TEMPO_RESET:
            s_tempoReset = !s_tempoReset;
            cdj.setTempoResetLed(s_tempoReset);
            cdj.commitLeds();
            if (streamEvents) {
                Serial.print(F("[BUTTON]   TEMPO RESET          -> PRESSED   Tempo 0%: "));
                Serial.println(s_tempoReset ? F("LOCKED (Green LED ON)") : F("OFF"));
            }
            break;

        case CDJ_ID_CALL_FWD:
            s_memoryBank = (s_memoryBank < 10) ? s_memoryBank + 1 : 1;
            if (streamEvents) {
                Serial.print(F("[BUTTON]   CALL >               -> PRESSED   Memory Bank: #"));
                Serial.print(s_memoryBank);
                Serial.println(s_memorySaved[s_memoryBank - 1] ? F(" [STORED]") : F(" [EMPTY]"));
            }
            break;

        case CDJ_ID_CALL_REV:
            s_memoryBank = (s_memoryBank > 1) ? s_memoryBank - 1 : 10;
            if (streamEvents) {
                Serial.print(F("[BUTTON]   < CALL               -> PRESSED   Memory Bank: #"));
                Serial.print(s_memoryBank);
                Serial.println(s_memorySaved[s_memoryBank - 1] ? F(" [STORED]") : F(" [EMPTY]"));
            }
            break;

        case CDJ_ID_MEMORY:
            s_memorySaved[s_memoryBank - 1] = true;
            if (streamEvents) {
                Serial.print(F("[BUTTON]   MEMORY               -> PRESSED   Cue Point Saved to Bank #"));
                Serial.println(s_memoryBank);
            }
            break;

        case CDJ_ID_DELETE:
            s_memorySaved[s_memoryBank - 1] = false;
            if (streamEvents) {
                Serial.print(F("[BUTTON]   DELETE               -> PRESSED   Bank #"));
                Serial.print(s_memoryBank);
                Serial.println(F(" Cleared"));
            }
            break;

        case CDJ_ID_TIME_MODE:
            s_remainTime = !s_remainTime;
            if (streamEvents) {
                Serial.print(F("[BUTTON]   TIME MODE/AUTO CUE   -> PRESSED   Time Display: "));
                Serial.println(s_remainTime ? F("REMAIN") : F("ELAPSED"));
            }
            break;

        case CDJ_ID_TEXT_MODE:
            s_textMode = (s_textMode + 1) % 3;
            if (streamEvents) {
                Serial.print(F("[BUTTON]   TEXT MODE            -> PRESSED   Text Mode: "));
                switch (s_textMode) {
                    case 0: Serial.println(F("TRACK TITLE")); break;
                    case 1: Serial.println(F("ARTIST NAME")); break;
                    case 2: Serial.println(F("ALBUM NAME")); break;
                }
            }
            break;

        case CDJ_ID_EJECT:
            s_isEjecting = true;
            s_lastEjectTime = millis();
            cdj.setJogPosition(CDJ_JOG_POS_EJECT_ANIM);
            cdj.commitLeds();
            if (streamEvents) Serial.println(F("[BUTTON]   EJECT                -> PRESSED   Eject Sequence Triggered (Ring Anim 138)"));
            break;

        case CDJ_ID_REVERSE_SWITCH:
            s_reverseActive = !s_reverseActive;
            cdj.setReverseLed(s_reverseActive);
            cdj.commitLeds();
            if (streamEvents) {
                Serial.print(F("[SWITCH]   DIRECTION SWITCH     -> "));
                Serial.println(s_reverseActive ? F("REVERSE (Red Reverse LED ON)") : F("FORWARD (Normal)"));
            }
            break;

        case CDJ_ID_JOG_TOUCH:
            if (streamEvents) Serial.println(F("[TOUCH]    JOG TOUCH PLATTER    -> TOUCHED   (Hand On Metallic Platter)"));
            break;

        default:
            break;
    }
}

// -----------------------------------------------------------------------------
// EVENT CALLBACK: JOG WHEEL ENCODER & TOUCH SENSOR (REAL-TIME NON-BLOCKING)
// -----------------------------------------------------------------------------
void onJogEvent(int16_t delta, bool touched, uint16_t speed) {
    (void)speed;

    // Platter Touch State (Scratch Pause)
    bool touchStateChanged = (touched != s_jogTouched);
    s_jogTouched = touched;
    cdj.setTouchRing(touched);

    if (touchStateChanged) {
        cdj.commitLeds();
    }

    // Jog Turn: Direct Platter Position Tracking (1:1 Encoder Reading)
    if (delta != 0) {
        // [DISABLED] Jog Turn Time Scrubbing
        /*
        int32_t totalFrames = ((int32_t)s_timeMinutes * 60 + s_timeSeconds) * 75 + s_timeFrames;
        totalFrames += (delta / 4);
        if (totalFrames < 0) totalFrames = 0;
        s_timeMinutes = (totalFrames / 75) / 60;
        s_timeSeconds = (totalFrames / 75) % 60;
        s_timeFrames  = totalFrames % 75;
        if (s_timeMinutes > 99) s_timeMinutes = 99;
        cdj.setTimeDisplay(s_timeMinutes, s_timeSeconds, s_timeFrames);
        */

        // High-precision 3600 PPR rotation accumulator mapped to 1..135 ring position
        s_jogAccumulator += delta;
        while (s_jogAccumulator >= 3600) s_jogAccumulator -= 3600;
        while (s_jogAccumulator < 0)    s_jogAccumulator += 3600;

        uint8_t nextPos = 1 + (uint8_t)(((uint32_t)s_jogAccumulator * 135UL) / 3600UL);
        if (nextPos > CDJ_JOG_POS_MAX) nextPos = CDJ_JOG_POS_MAX;

        jogRingPos = nextPos;
        s_jogPlayCursor = nextPos;
        cdj.setJogPosition(jogRingPos);
        cdj.commitLeds();
    }

    // Rate-limited non-blocking Serial monitor output (max 10 Hz)
    if (streamEvents) {
        static uint32_t lastJogPrint = 0;
        if (millis() - lastJogPrint >= 100) {
            Serial.print(F("[JOG]      Pos: "));
            if (jogRingPos < 10) Serial.print(F("  "));
            else if (jogRingPos < 100) Serial.print(' ');
            Serial.print(jogRingPos);
            Serial.print(F("/135 | Delta: "));
            if (delta >= 0) Serial.print('+');
            Serial.print(delta);
            Serial.print(F(" | Speed: "));
            Serial.print(speed);
            Serial.print(F(" | Touch: "));
            Serial.println(touched ? F("YES") : F("NO"));
            lastJogPrint = millis();
        }
    }
}

// -----------------------------------------------------------------------------
// EVENT CALLBACK: PITCH FADER (RATE-LIMITED NON-BLOCKING)
// -----------------------------------------------------------------------------
void onPitchEvent(uint16_t pitchValue) {
    float rangePct = 10.0f;
    switch (s_tempoRange) {
        case 0: rangePct = 6.0f; break;
        case 1: rangePct = 10.0f; break;
        case 2: rangePct = 16.0f; break;
        case 3: rangePct = 100.0f; break;
    }

    s_currentPitchPct = ((float)pitchValue - 8192.0f) / 8192.0f * rangePct;

    // [DISABLED] Kode LCD Tempo Display
    // cdj.setTempoDisplay(s_currentPitchPct);
    // cdj.commitLeds();

    if (streamEvents) {
        static uint32_t lastPitchPrint = 0;
        if (millis() - lastPitchPrint >= 100) {
            Serial.print(F("[PITCH]    Slider: "));
            if (s_currentPitchPct >= 0.0f) Serial.print('+');
            Serial.print(s_currentPitchPct, 2);
            Serial.print(F("% (ADC: "));
            Serial.print(pitchValue);
            Serial.println(F(")"));
            lastPitchPrint = millis();
        }
    }
}

// -----------------------------------------------------------------------------
// MENU & SNAPSHOT UTILITIES
// -----------------------------------------------------------------------------
void printMenu() {
    Serial.println(F("\n========================================================"));
    Serial.println(F(" PIONEER CDJ-1000 MK3 ADVANCED EMULATOR (ARDUINO UNO)   "));
    Serial.println(F("========================================================"));
    Serial.println(F(" Display Features:"));
    Serial.println(F("   - Custom VFD Text : HANIF@UNXCHR"));
    Serial.println(F("   - 100-Bar Waveform: Full dynamic random audio wave"));
    Serial.println(F("   - 7-Segment Timer : Real-time Min:Sec:Frame (75 fps)"));
    Serial.println(F("   - Tempo Display   : Live Pitch % (+/-10.00%)"));
    Serial.println(F(" Jog Behavior:"));
    Serial.println(F("   - Playing         : Jog LED spins smoothly at 33 1/3 RPM"));
    Serial.println(F("   - Touch Platter   : Pauses time & spinning (Vinyl Scratch)"));
    Serial.println(F("   - Rotate Platter  : Scrubs time forward / backward"));
    Serial.println(F(" Keyboard Commands:"));
    Serial.println(F("   [1] Toggle Event Stream      [2] Toggle Raw Hex Stream"));
    Serial.println(F("   [3] Toggle Analog Stream     [w] Re-roll Random Waveform"));
    Serial.println(F("   [p] Toggle Play/Pause        [c] Trigger Cue Reset"));
    Serial.println(F("   [j] Run Jog Sweep            [t] Run LED Self-Test"));
    Serial.println(F("   [s] Print Status Snapshot    [?] Print Menu"));
    Serial.println(F("========================================================\n"));
}

void printStatusSnapshot() {
    const CDJ_PanelState_t& st = cdj.getState();
    Serial.println(F("\n--- HARDWARE STATUS SNAPSHOT ---"));
    Serial.print(F("SPI Communication  : "));
    Serial.print(currentFps, 1);
    Serial.println(F(" Frames/Sec (Hz)"));

    Serial.print(F("Playback State     : "));
    Serial.print(s_isPlaying ? F("PLAYING") : F("PAUSED"));
    Serial.print(F(" | Touch Hold: "));
    Serial.println(s_jogTouched ? F("HOLD (Scratching)") : F("NO"));

    Serial.print(F("Track Time         : "));
    if (s_timeMinutes < 10) Serial.print('0');
    Serial.print(s_timeMinutes);
    Serial.print(':');
    if (s_timeSeconds < 10) Serial.print('0');
    Serial.print(s_timeSeconds);
    Serial.print('.');
    if (s_timeFrames < 10) Serial.print('0');
    Serial.println(s_timeFrames);

    Serial.print(F("Pitch / Tempo      : "));
    if (s_currentPitchPct >= 0.0f) Serial.print('+');
    Serial.print(s_currentPitchPct, 2);
    Serial.print(F("% (ADC: "));
    Serial.print(st.pitchFader);
    Serial.println(F(")"));

    Serial.print(F("Active Track / Fldr: Track "));
    Serial.print(s_trackNumber);
    Serial.print(F(" | Folder "));
    Serial.println(s_folderNumber);

    Serial.print(F("Jog Wheel Pulses   : "));
    Serial.print(st.jogPulseCounter);
    Serial.print(F(" | Speed: "));
    Serial.print(st.jogSpeed);
    Serial.print(F(" | Ring Position: "));
    Serial.print(jogRingPos);
    Serial.println(F("/135"));

    Serial.print(F("Direction Switch   : "));
    Serial.println(st.reverseSwitch ? F("FORWARD") : F("REVERSE"));
    Serial.println(F("--------------------------------\n"));
}

void startJogSweep() {
    Serial.println(F("[ANIM] Starting Non-Blocking Jog Ring Sweep (1..135)..."));
    isSweepingJog = true;
    sweepPos = 1;
    lastSweepTime = millis();
}

void startLedSelfTest() {
    Serial.println(F("\n[TEST] Executing Non-Blocking Full Hardware LED Test..."));
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
    cdj.setSdCardLed(true);
    cdj.setTouchRing(true);
    // [DISABLED] Kode LCD Test Display
    // cdj.setTrackDisplay(88);
    // cdj.setTimeDisplay(88, 88, 88);
    cdj.commitLeds();

    selfTestStep = 1;
    lastSelfTestTime = millis();
    isSelfTesting = true;
}

// -----------------------------------------------------------------------------
// ARDUINO SETUP
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(DEBUG_BAUD_RATE);
    while (!Serial && millis() < 3000);

    Serial.println(F("\n========================================================"));
    Serial.println(F("   PIONEER CDJ-1000 MK3 ADVANCED EMULATOR SUITE         "));
    Serial.println(F("========================================================"));
    Serial.print(F("Board Target       : "));
    Serial.println(F(CDJ_BOARD_NAME));
    Serial.print(F("SPI Pin Assignment : SCLK=Pin "));
    Serial.print(PIN_UNO_SCLK);
    Serial.print(F(", MISO=Pin "));
    Serial.print(PIN_UNO_MISO);
    Serial.print(F(", MOSI=Pin "));
    Serial.print(PIN_UNO_MOSI);
    Serial.print(F(", CS/XCS=Pin "));
    Serial.println(PIN_UNO_CS);
    Serial.println(F("Initializing SPI Driver in Multi-Package Screen Refresh Mode..."));

    if (!cdj.begin(CDJ_ROLE_SLAVE, PIN_UNO_CS)) {
        Serial.println(F("[FATAL ERROR] Failed to initialize SPI interface!"));
        while (1);
    }

    // 1. [DISABLED] Enable Full Screen Multi-Package Refresh (Packages 0..7)
    // cdj.enableMultiPackageMode(true);

    // 2. [DISABLED] Set Custom VFD Dot-Matrix Text: "HANIF@UNXCHR"
    // cdj.setCustomText("HANIF@UNXCHR");

    // 3. [DISABLED] Generate 100-Bar Dynamic Random Audio Waveform
    // cdj.setRandomWaveform();

    // 4. Register event listeners
    cdj.onButton(onButtonEvent);
    cdj.onJog(onJogEvent);
    cdj.onPitch(onPitchEvent);

    // 5. Apply initial default deck state
    applyCurrentDeckStateToLeds();

    printMenu();
    Serial.println(F("System Ready. Waiting for SPI Clock from CDJ Display Board...\n"));
}

// -----------------------------------------------------------------------------
// MAIN LOOP (100% NON-BLOCKING)
// -----------------------------------------------------------------------------
void loop() {
    // 1. Process CDJ-1000 MK3 SPI communications
    if (cdj.update()) {
        frameCount++;

        if (millis() - lastFpsCheck >= 1000) {
            currentFps = (float)frameCount * 1000.0f / (float)(millis() - lastFpsCheck);
            frameCount = 0;
            lastFpsCheck = millis();
        }

        // Live Raw Hex Packet Sniffer Stream
        if (streamRawHex) {
            static uint32_t lastRawPrint = 0;
            if (millis() - lastRawPrint >= 100) {
                Serial.print(F("[RAW RX] "));
                const uint8_t* raw = cdj.getRawRx();
                for (uint8_t i = 0; i < CDJ_FRAME_SIZE; i++) {
                    if (raw[i] < 0x10) Serial.print('0');
                    Serial.print(raw[i], HEX);
                    Serial.print(' ');
                }
                Serial.println();
                lastRawPrint = millis();
            }
        }

        // Live Analog & Jog Telemetry Stream
        if (streamAnalog) {
            static uint32_t lastAnalogPrint = 0;
            if (millis() - lastAnalogPrint >= 150) {
                const CDJ_PanelState_t& st = cdj.getState();
                Serial.print(F("[ANALOG] Pitch: "));
                Serial.print(st.pitchFader);
                Serial.print(F(" | CenterPot: "));
                Serial.print(st.pitchCenterPot);
                Serial.print(F(" | TBrake: "));
                Serial.print(st.touchBrake);
                Serial.print(F(" | RStart: "));
                Serial.print(st.releaseStart);
                Serial.print(F(" | JogPulses: "));
                Serial.print(st.jogPulseCounter);
                Serial.print(F(" | Touch: "));
                Serial.println(st.jogTouched ? F("1") : F("0"));
                lastAnalogPrint = millis();
            }
        }
    }

    // 2. [DISABLED] Real-Time Timecode Engine & Jog LED Playing (75 FPS Audio Time)
    /*
    if (s_isPlaying && !s_jogTouched) {
        if (millis() - s_lastTimeTick >= 13) { // 13.33 ms = 1/75 sec
            s_lastTimeTick = millis();

            // Advance Timecode
            s_timeFrames++;
            if (s_timeFrames >= 75) {
                s_timeFrames = 0;
                s_timeSeconds++;
                if (s_timeSeconds >= 60) {
                    s_timeSeconds = 0;
                    s_timeMinutes++;
                    if (s_timeMinutes >= 100) s_timeMinutes = 0;
                }
            }
            cdj.setTimeDisplay(s_timeMinutes, s_timeSeconds, s_timeFrames);

            // Jog LED Playing: Spins smoothly at 33 1/3 RPM (~1.8s per 135 positions)
            s_jogPlayCursor++;
            if (s_jogPlayCursor > CDJ_JOG_POS_MAX) s_jogPlayCursor = 1;
            jogRingPos = s_jogPlayCursor;
            cdj.setJogPosition(jogRingPos);
            cdj.commitLeds();
        }
    }
    */

    // 3. Authentic Pioneer Play LED Blinking when Paused (Non-blocking)
    if (!s_isPlaying && !isSelfTesting) {
        static uint32_t lastPlayBlink = 0;
        static bool playBlinkState = false;
        if (millis() - lastPlayBlink >= 300) {
            lastPlayBlink = millis();
            playBlinkState = !playBlinkState;
            cdj.setPlayLed(playBlinkState);
            cdj.commitLeds();
        }
    }

    // 4. Non-blocking Jog Ring Sweep Animation Machine
    if (isSweepingJog) {
        if (millis() - lastSweepTime >= 15) {
            lastSweepTime = millis();
            sweepPos += 2;
            if (sweepPos > 135) {
                isSweepingJog = false;
                sweepPos = 1;
                cdj.setJogPosition(jogRingPos);
                cdj.commitLeds();
                Serial.println(F("[ANIM] Jog Ring Sweep Complete"));
            } else {
                cdj.setJogPosition(sweepPos);
                cdj.commitLeds();
            }
        }
    }

    // 5. Non-blocking LED Self-Test State Machine
    if (isSelfTesting) {
        if (millis() - lastSelfTestTime >= 600) {
            lastSelfTestTime = millis();
            selfTestStep++;
            switch (selfTestStep) {
                case 2:
                    Serial.println(F("[TEST] Phase 2: Green LEDs Only"));
                    cdj.setPlayLed(true);
                    cdj.setCueLed(false);
                    cdj.setVinylLed(false);
                    cdj.setCdjLed(true);
                    cdj.setReverseLed(false);
                    cdj.setHotCue(0, HOTCUE_GREEN);
                    cdj.setHotCue(1, HOTCUE_GREEN);
                    cdj.setHotCue(2, HOTCUE_GREEN);
                    cdj.commitLeds();
                    break;
                case 3:
                    Serial.println(F("[TEST] Phase 3: Amber/Red LEDs"));
                    cdj.setPlayLed(false);
                    cdj.setCueLed(true);
                    cdj.setVinylLed(true);
                    cdj.setCdjLed(false);
                    cdj.setReverseLed(true);
                    cdj.setHotCue(0, HOTCUE_RED);
                    cdj.setHotCue(1, HOTCUE_RED);
                    cdj.setHotCue(2, HOTCUE_RED);
                    cdj.commitLeds();
                    break;
                case 4:
                    Serial.println(F("[TEST] Phase 4: Self-Test Complete. Restoring Deck State..."));
                    isSelfTesting = false;
                    selfTestStep = 0;
                    applyCurrentDeckStateToLeds();
                    break;
            }
        }
    }

    // 6. Non-blocking Eject Animation Recovery
    if (s_isEjecting) {
        if (millis() - s_lastEjectTime >= 1000) {
            s_isEjecting = false;
            cdj.setJogPosition(jogRingPos);
            cdj.commitLeds();
            if (streamEvents) Serial.println(F("[EJECT] Sequence Complete. Ready."));
        }
    }

    // 7. Serial Interactive Console Parser
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == '\r' || cmd == '\n' || cmd == ' ') return;
        switch (cmd) {
            case '1':
                streamEvents = !streamEvents;
                Serial.print(F("Event Stream: "));
                Serial.println(streamEvents ? F("ENABLED") : F("MUTED"));
                break;
            case '2':
                streamRawHex = !streamRawHex;
                Serial.print(F("Raw Hex Stream: "));
                Serial.println(streamRawHex ? F("ENABLED") : F("MUTED"));
                break;
            case '3':
                streamAnalog = !streamAnalog;
                Serial.print(F("Analog Stream: "));
                Serial.println(streamAnalog ? F("ENABLED") : F("MUTED"));
                break;
            case 'w':
                // Serial.println(F("[WAVEFORM] Generating New Random Waveform across 100 bars..."));
                // cdj.setRandomWaveform();
                break;
            case 'p':
                s_isPlaying = !s_isPlaying;
                s_cueActive = !s_isPlaying;
                cdj.setPlayLed(s_isPlaying);
                cdj.setCueLed(s_cueActive);
                cdj.commitLeds();
                Serial.print(F("Play State: "));
                Serial.println(s_isPlaying ? F("PLAYING (Green LED ON)") : F("PAUSED"));
                break;
            case 'c':
                s_cueActive = true;
                s_isPlaying = false;
                s_timeMinutes = s_cueMinutes;
                s_timeSeconds = s_cueSeconds;
                s_timeFrames  = s_cueFrames;
                // cdj.setTimeDisplay(s_timeMinutes, s_timeSeconds, s_timeFrames);
                s_jogPlayCursor = 1;
                jogRingPos = 1;
                cdj.setJogPosition(1);
                cdj.setPlayLed(false);
                cdj.setCueLed(true);
                cdj.commitLeds();
                Serial.println(F("Cue Reset: Time reset to 00:00.00 & Paused"));
                break;
            case 'l':
                s_loopActive = !s_loopActive;
                s_loopInSet  = s_loopActive;
                cdj.setLoopInLed(s_loopActive);
                cdj.setLoopOutLed(s_loopActive);
                cdj.setReloopExitLed(s_loopActive);
                cdj.commitLeds();
                Serial.print(F("Loop LEDs: "));
                Serial.println(s_loopActive ? F("ALL ON") : F("ALL OFF"));
                break;
            case 'a':
                s_hotCueColor[0] = (s_hotCueColor[0] + 1) % 4;
                cdj.setHotCue(0, (CDJ_HotCueColor_t)s_hotCueColor[0]);
                cdj.commitLeds();
                Serial.print(F("Hot Cue A Color: "));
                Serial.println(s_hotCueColor[0]);
                break;
            case 'b':
                s_hotCueColor[1] = (s_hotCueColor[1] + 1) % 4;
                cdj.setHotCue(1, (CDJ_HotCueColor_t)s_hotCueColor[1]);
                cdj.commitLeds();
                Serial.print(F("Hot Cue B Color: "));
                Serial.println(s_hotCueColor[1]);
                break;
            case 'x':
                s_hotCueColor[2] = (s_hotCueColor[2] + 1) % 4;
                cdj.setHotCue(2, (CDJ_HotCueColor_t)s_hotCueColor[2]);
                cdj.commitLeds();
                Serial.print(F("Hot Cue C Color: "));
                Serial.println(s_hotCueColor[2]);
                break;
            case 'v':
                s_vinylMode = !s_vinylMode;
                cdj.setVinylLed(s_vinylMode);
                cdj.setCdjLed(!s_vinylMode);
                cdj.commitLeds();
                Serial.print(F("Mode: "));
                Serial.println(s_vinylMode ? F("VINYL (Blue LED)") : F("CDJ (Green LED)"));
                break;
            case 'j':
                startJogSweep();
                break;
            case 't':
                startLedSelfTest();
                break;
            case 's':
                printStatusSnapshot();
                break;
            case '?':
            case 'h':
                printMenu();
                break;
        }
    }
}
