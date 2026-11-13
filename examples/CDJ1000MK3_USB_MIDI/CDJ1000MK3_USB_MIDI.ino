/**
 * ==============================================================================
 * CDJ-1000 MK3 USB MIDI Controller for Rekordbox, Traktor & VirtualDJ
 * ==============================================================================
 * Connects a physical Pioneer CDJ-1000 MK3 panel directly to your PC as a class-compliant
 * USB MIDI Controller.
 * 
 * Target Boards with Native USB:
 *  - Arduino Pro Micro / Leonardo (ATmega32U4)
 *  - Teensy 3.2 / 4.0 / 4.1 (Set USB Type to 'MIDI')
 *  - Raspberry Pi Pico / RP2040
 *  - ESP32-S2 / ESP32-S3
 * 
 * Required Arduino Library (for ATmega32U4 / SAMD):
 *  - 'MIDIUSB' by Arduino (Install via Arduino Library Manager)
 * 
 * Hardware Connections (CDJ Top Panel Connector):
 *  - Pin 13 (SCLK) --> Arduino SCK  (Pin 15 on Pro Micro, Pin 13 on Teensy)
 *  - Pin 14 (SOUT) --> Arduino MISO (Pin 14 on Pro Micro, Pin 12 on Teensy)
 *  - Pin 15 (SIN)  --> Arduino MOSI (Pin 16 on Pro Micro, Pin 11 on Teensy)
 *  - Pin 16 (XCS)  --> Arduino SS   (Pin 10 on Pro Micro, Pin 10 on Teensy)
 *  - GND           --> Arduino GND
 *  - Logic Level: 5V (For 3.3V boards, use logic level shifter or 74HC245).
 * ==============================================================================
 */

#include <Arduino.h>
#include "CDJ1000MK3.h"
#include "CDJ1000MK3_MidiMap.h"

#if defined(CORE_TEENSY)
    // Teensy has built-in usbMIDI
    #define USE_TEENSY_MIDI 1
#elif defined(USBCON) || defined(__AVR_ATmega32U4__) || defined(ARDUINO_ARCH_RP2040)
    #include <MIDIUSB.h>
    #define USE_MIDIUSB 1
#endif

// Select Deck Number (1, 2, 3, or 4)
// Deck 1 = MIDI Channel 1, Deck 2 = MIDI Channel 2
uint8_t midiChannel = 1;

CDJ1000MK3_Panel cdj;
uint8_t jogPosition = 1;

// -----------------------------------------------------------------------------
// MIDI SEND HELPERS
// -----------------------------------------------------------------------------

void sendMidiNote(uint8_t note, uint8_t velocity) {
    uint8_t status = (velocity > 0) ? (0x90 | (midiChannel - 1)) : (0x80 | (midiChannel - 1));

#if defined(USE_TEENSY_MIDI)
    if (velocity > 0) {
        usbMIDI.sendNoteOn(note, velocity, midiChannel);
    } else {
        usbMIDI.sendNoteOff(note, 0, midiChannel);
    }
#elif defined(USE_MIDIUSB)
    midiEventPacket_t event = { (uint8_t)((status >> 4) & 0x0F), status, note, velocity };
    MidiUSB.sendMIDI(event);
    MidiUSB.flush();
#endif
}

void sendMidiCC(uint8_t cc, uint8_t value) {
    uint8_t status = 0xB0 | (midiChannel - 1);

#if defined(USE_TEENSY_MIDI)
    usbMIDI.sendControlChange(cc, value, midiChannel);
#elif defined(USE_MIDIUSB)
    midiEventPacket_t event = { 0x0B, status, cc, value };
    MidiUSB.sendMIDI(event);
    MidiUSB.flush();
#endif
}

// -----------------------------------------------------------------------------
// EVENT CALLBACKS FROM CDJ HARDWARE
// -----------------------------------------------------------------------------

void onHardwareButton(CDJ_ButtonId_t btn, bool pressed) {
    uint8_t vel = pressed ? 127 : 0;

    switch (btn) {
        case CDJ_ID_PLAY:
            sendMidiNote(MIDI_NOTE_PLAY_PAUSE, vel);
            break;
        case CDJ_ID_CUE:
            sendMidiNote(MIDI_NOTE_CUE, vel);
            break;
        case CDJ_ID_LOOP_IN:
            sendMidiNote(MIDI_NOTE_LOOP_IN, vel);
            break;
        case CDJ_ID_LOOP_OUT:
            sendMidiNote(MIDI_NOTE_LOOP_OUT, vel);
            break;
        case CDJ_ID_RELOOP_EXIT:
            sendMidiNote(MIDI_NOTE_RELOOP_EXIT, vel);
            break;
        case CDJ_ID_HOTCUE_A:
            sendMidiNote(MIDI_NOTE_HOTCUE_A, vel);
            break;
        case CDJ_ID_HOTCUE_B:
            sendMidiNote(MIDI_NOTE_HOTCUE_B, vel);
            break;
        case CDJ_ID_HOTCUE_C:
            sendMidiNote(MIDI_NOTE_HOTCUE_C, vel);
            break;
        case CDJ_ID_TRACK_REV:
            sendMidiNote(MIDI_NOTE_TRACK_REV, vel);
            break;
        case CDJ_ID_TRACK_FWD:
            sendMidiNote(MIDI_NOTE_TRACK_FWD, vel);
            break;
        case CDJ_ID_SEARCH_REV:
            sendMidiNote(MIDI_NOTE_SEARCH_REV, vel);
            break;
        case CDJ_ID_SEARCH_FWD:
            sendMidiNote(MIDI_NOTE_SEARCH_FWD, vel);
            break;
        case CDJ_ID_FOLDER_REV:
            sendMidiNote(MIDI_NOTE_FOLDER_REV, vel);
            break;
        case CDJ_ID_FOLDER_FWD:
            sendMidiNote(MIDI_NOTE_FOLDER_FWD, vel);
            break;
        case CDJ_ID_MASTER_TEMPO:
            sendMidiNote(MIDI_NOTE_MASTER_TEMPO, vel);
            break;
        case CDJ_ID_TEMPO_RANGE:
            sendMidiNote(MIDI_NOTE_TEMPO_RANGE, vel);
            break;
        case CDJ_ID_REVERSE_SWITCH:
            sendMidiNote(MIDI_NOTE_REVERSE, vel);
            break;
        case CDJ_ID_JOG_MODE:
            sendMidiNote(MIDI_NOTE_SLIP_MODE, vel);
            break;
        case CDJ_ID_JOG_TOUCH:
            // Send platter touch event (enables vinyl scratch mode)
            sendMidiNote(MIDI_NOTE_JOG_TOUCH, vel);
            cdj.setTouchRing(pressed);
            cdj.commitLeds();
            break;
        default:
            break;
    }
}

void onHardwareJog(int16_t delta, bool touched, uint16_t speed) {
    (void)speed;

    // Send Relative MIDI CC for Jog (Center = 64)
    // Clockwise: 64 + delta (e.g. 65..127)
    // Counter-Clockwise: 64 - delta (e.g. 63..0)
    int8_t clampedDelta = constrain(delta / 4, -63, 63);
    uint8_t ccVal = (uint8_t)(64 + clampedDelta);

    if (touched) {
        // Scratch Mode
        sendMidiCC(MIDI_CC_JOG_SCRATCH, ccVal);
    } else {
        // Pitch Bend / Nudge Mode
        sendMidiCC(MIDI_CC_JOG_PITCH_BEND, ccVal);
    }

    // Update Jog Ring Display cursor (1..135)
    int nextPos = (int)jogPosition + (delta / 8);
    while (nextPos > CDJ_JOG_POS_MAX) nextPos -= (CDJ_JOG_POS_MAX - CDJ_JOG_POS_MIN + 1);
    while (nextPos < CDJ_JOG_POS_MIN) nextPos += (CDJ_JOG_POS_MAX - CDJ_JOG_POS_MIN + 1);
    jogPosition = (uint8_t)nextPos;

    cdj.setJogPosition(jogPosition);
    cdj.commitLeds();
}

void onHardwarePitch(uint16_t pitchValue) {
    // 14-bit pitch fader (0..16383). Send High-Res MIDI CC
    uint8_t msb = (pitchValue >> 7) & 0x7F; // 7-bit MSB
    uint8_t lsb = pitchValue & 0x7F;        // 7-bit LSB

    sendMidiCC(MIDI_CC_PITCH_MSB, msb);
    sendMidiCC(MIDI_CC_PITCH_LSB, lsb);
}

// -----------------------------------------------------------------------------
// MIDI INPUT FROM REKORDBOX / TRAKTOR (Controls CDJ Hardware LEDs)
// -----------------------------------------------------------------------------

void processIncomingMidi() {
#if defined(USE_TEENSY_MIDI)
    while (usbMIDI.read()) {
        uint8_t type    = usbMIDI.getType();
        uint8_t channel = usbMIDI.getChannel();
        uint8_t data1   = usbMIDI.getData1();
        uint8_t data2   = usbMIDI.getData2();

        if (channel == midiChannel) {
            bool on = (type == usbMIDI.NoteOn && data2 > 0);
            if (data1 == MIDI_NOTE_PLAY_PAUSE) cdj.setPlayLed(on);
            if (data1 == MIDI_NOTE_CUE)        cdj.setCueLed(on);
            if (data1 == MIDI_NOTE_LOOP_IN)    cdj.setLoopInLed(on);
            if (data1 == MIDI_NOTE_LOOP_OUT)   cdj.setLoopOutLed(on);
            if (data1 == MIDI_NOTE_HOTCUE_A)   cdj.setHotCue(0, on ? HOTCUE_GREEN : HOTCUE_OFF);
            if (data1 == MIDI_NOTE_HOTCUE_B)   cdj.setHotCue(1, on ? HOTCUE_GREEN : HOTCUE_OFF);
            if (data1 == MIDI_NOTE_HOTCUE_C)   cdj.setHotCue(2, on ? HOTCUE_GREEN : HOTCUE_OFF);
            cdj.commitLeds();
        }
    }
#elif defined(USE_MIDIUSB)
    midiEventPacket_t rx = MidiUSB.read();
    if (rx.header != 0) {
        uint8_t type    = rx.byte1 & 0xF0;
        uint8_t channel = (rx.byte1 & 0x0F) + 1;
        uint8_t data1   = rx.byte2;
        uint8_t data2   = rx.byte3;

        if (channel == midiChannel) {
            bool on = (type == 0x90 && data2 > 0);
            if (data1 == MIDI_NOTE_PLAY_PAUSE) cdj.setPlayLed(on);
            if (data1 == MIDI_NOTE_CUE)        cdj.setCueLed(on);
            if (data1 == MIDI_NOTE_LOOP_IN)    cdj.setLoopInLed(on);
            if (data1 == MIDI_NOTE_LOOP_OUT)   cdj.setLoopOutLed(on);
            if (data1 == MIDI_NOTE_HOTCUE_A)   cdj.setHotCue(0, on ? HOTCUE_GREEN : HOTCUE_OFF);
            if (data1 == MIDI_NOTE_HOTCUE_B)   cdj.setHotCue(1, on ? HOTCUE_GREEN : HOTCUE_OFF);
            if (data1 == MIDI_NOTE_HOTCUE_C)   cdj.setHotCue(2, on ? HOTCUE_GREEN : HOTCUE_OFF);
            cdj.commitLeds();
        }
    }
#endif
}

// -----------------------------------------------------------------------------
// ARDUINO SETUP & MAIN LOOP
// -----------------------------------------------------------------------------

void setup() {
    // Initialize CDJ Panel in SPI Slave Mode using pins from CDJ1000MK3_Pins.h
    cdj.begin(CDJ_ROLE_SLAVE, CDJ_PIN_CS);

    // Register hardware event handlers
    cdj.onButton(onHardwareButton);
    cdj.onJog(onHardwareJog);
    cdj.onPitch(onHardwarePitch);

    // Default LED States
    cdj.setVinylLed(true);
    cdj.setCueLed(true);
    cdj.setJogPosition(1);
    cdj.commitLeds();
}

void loop() {
    // Update CDJ-1000 MK3 Panel communication
    cdj.update();

    // Process incoming feedback from Rekordbox / Traktor
    processIncomingMidi();
}

