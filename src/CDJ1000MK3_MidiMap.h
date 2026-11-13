/**
 * @file CDJ1000MK3_MidiMap.h
 * @brief Standard MIDI CC and Note Definitions for Pioneer DJ Softwares
 * Compatible with Rekordbox, Traktor Pro, VirtualDJ, and Serato.
 */

#ifndef CDJ1000MK3_MIDIMAP_H
#define CDJ1000MK3_MIDIMAP_H

#include <stdint.h>

// Default MIDI Notes (Deck 1 = Ch 1 / 0x90, Deck 2 = Ch 2 / 0x91)
#define MIDI_NOTE_PLAY_PAUSE        0x0B  // 11
#define MIDI_NOTE_CUE               0x0C  // 12
#define MIDI_NOTE_LOOP_IN           0x10  // 16
#define MIDI_NOTE_LOOP_OUT          0x11  // 17
#define MIDI_NOTE_RELOOP_EXIT       0x12  // 18
#define MIDI_NOTE_HOTCUE_A          0x1B  // 27
#define MIDI_NOTE_HOTCUE_B          0x1C  // 28
#define MIDI_NOTE_HOTCUE_C          0x1D  // 29

#define MIDI_NOTE_TRACK_REV         0x15  // 21
#define MIDI_NOTE_TRACK_FWD         0x16  // 22
#define MIDI_NOTE_SEARCH_REV        0x17  // 23
#define MIDI_NOTE_SEARCH_FWD        0x18  // 24
#define MIDI_NOTE_FOLDER_REV        0x19  // 25
#define MIDI_NOTE_FOLDER_FWD        0x1A  // 26

#define MIDI_NOTE_MASTER_TEMPO      0x1E  // 30
#define MIDI_NOTE_TEMPO_RANGE       0x1F  // 31
#define MIDI_NOTE_REVERSE           0x20  // 32
#define MIDI_NOTE_JOG_TOUCH         0x36  // 54 (Platter touch / scratch enable)
#define MIDI_NOTE_SLIP_MODE         0x3F  // 63

// Continuous Controllers (CC)
#define MIDI_CC_PITCH_MSB           0x00  // High-res Pitch Fader MSB
#define MIDI_CC_PITCH_LSB           0x20  // High-res Pitch Fader LSB
#define MIDI_CC_JOG_SCRATCH         0x21  // Jog rotation (relative: 64 + delta)
#define MIDI_CC_JOG_PITCH_BEND      0x22  // Jog outer ring pitch bend
#define MIDI_CC_TOUCH_BRAKE         0x08  // Brake adjustment knob
#define MIDI_CC_RELEASE_START       0x09  // Release start adjustment knob

#endif // CDJ1000MK3_MIDIMAP_H

