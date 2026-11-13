/**
 * @file CDJ1000MK3_Panel.h
 * @brief High-Level Panel Controller & State Engine for Pioneer CDJ-1000 MK3
 */

#ifndef CDJ1000MK3_PANEL_H
#define CDJ1000MK3_PANEL_H

#include <Arduino.h>
#include "CDJ1000MK3_Pins.h"
#include "CDJ1000MK3_Protocol.h"
#include "CDJ1000MK3_SPI.h"

// Callback function signatures
typedef void (*CDJ_ButtonCallback_t)(CDJ_ButtonId_t btn, bool pressed);
typedef void (*CDJ_JogCallback_t)(int16_t deltaPulses, bool touched, uint16_t speed);
typedef void (*CDJ_PitchCallback_t)(uint16_t pitchValue);

class CDJ1000MK3_Panel {
public:
    CDJ1000MK3_Panel();

    /**
     * @brief Initialize CDJ-1000 MK3 Panel Controller
     * @param role CDJ_ROLE_SLAVE (default: Arduino reads physical CDJ top panel)
     * @param csPin Chip Select pin (default: CDJ_PIN_CS from CDJ1000MK3_Pins.h)
     */
    bool begin(CDJ_SpiRole_t role = CDJ_ROLE_SLAVE, uint8_t csPin = CDJ_PIN_CS);

    /**
     * @brief Periodic update loop. Call this in Arduino loop().
     * @return true if a new frame was processed, false otherwise.
     */
    bool update();

    // -------------------------------------------------------------------------
    // EVENT CALLBACK REGISTRATION
    // -------------------------------------------------------------------------
    void onButton(CDJ_ButtonCallback_t cb) { _buttonCb = cb; }
    void onJog(CDJ_JogCallback_t cb)       { _jogCb = cb; }
    void onPitch(CDJ_PitchCallback_t cb)   { _pitchCb = cb; }

    // -------------------------------------------------------------------------
    // INPUT STATE GETTERS
    // -------------------------------------------------------------------------
    bool isPressed(CDJ_ButtonId_t btn) const;
    bool justPressed(CDJ_ButtonId_t btn) const;
    bool justReleased(CDJ_ButtonId_t btn) const;

    bool isJogTouched() const           { return _state.jogTouched; }
    bool isJogRotating() const          { return _state.jogRotating; }
    bool isJogForward() const           { return _state.jogDirectionFwd; }
    int16_t getJogDelta() const         { return _state.jogPulseDelta; }
    uint16_t getJogPulses() const       { return _state.jogPulseCounter; }
    uint16_t getJogSpeed() const        { return _state.jogSpeed; }

    uint16_t getPitch() const           { return _state.pitchFader; }       // 14-bit (0..16383)
    uint16_t getPitchCenterPot() const  { return _state.pitchCenterPot; }  // 14-bit
    uint8_t getTouchBrake() const       { return _state.touchBrake; }      // 8-bit (0..255)
    uint8_t getReleaseStart() const     { return _state.releaseStart; }    // 8-bit (0..255)

    bool getReverseSwitch() const       { return _state.reverseSwitch; }
    bool getEjectLock() const           { return _state.ejectLocked; }

    const CDJ_PanelState_t& getState() const { return _state; }
    const uint8_t* getRawRx() const { return _rxRaw; }

    // -------------------------------------------------------------------------
    // LED & FEEDBACK CONTROLS (Main Assy => Display Board)
    // -------------------------------------------------------------------------
    void setPlayLed(bool on);
    void setCueLed(bool on);
    void setLoopInLed(bool on);
    void setLoopOutLed(bool on);
    void setReloopExitLed(bool on);
    void setReverseLed(bool on);
    void setHotCue(uint8_t cueIndex, CDJ_HotCueColor_t color); // cueIndex 0=A, 1=B, 2=C
    void setMasterTempoLed(bool on);
    void setTempoResetLed(bool on);
    void setVinylLed(bool on);
    void setCdjLed(bool on);
    void setSdCardLed(bool on);

    // Jog Platter Center Display Controls
    void setJogPosition(uint8_t pos);         // 1..135 ring position, or 136-138 animations
    void setJogCuePosition(uint8_t cuePos);   // 0=off, 1..85 position
    void setTouchRing(bool on);               // Center VFD circle on touch
    void setTrackDisplay(uint8_t trackNumber); // 1..99 track number on 7-segment display

    // Display & Multi-Package Controls
    void enableMultiPackageMode(bool enable); // 8-package full screen refresh (Waveform, Text, Time, LEDs)
    void setCustomText(const char* text);    // Display up to 16 ASCII characters on VFD Dot-Matrix (Package 6)
    void setWaveform(const uint8_t* bars100); // 100 bars across the display (Package 0..4)
    void setRandomWaveform();                // Generate dynamic random music wave
    void setTimeDisplay(uint8_t minutes, uint8_t seconds, uint8_t frames);
    void setTempoDisplay(float pitchPercent);

    // Push pending LED/display changes immediately to SPI transmit frame
    void commitLeds();

    // Utility: Return button name string
    static const char* getButtonName(CDJ_ButtonId_t btn);

private:
    CDJ1000MK3_SPI    _spi;
    CDJ_PanelState_t  _state;
    CDJ_PanelState_t  _prevState;
    CDJ_PanelState_t  _debouncedState;
    bool              _isFirstFrame;

    uint8_t           _txRaw[CDJ_FRAME_SIZE];
    uint8_t           _rxRaw[CDJ_FRAME_SIZE];
    bool              _txDirty;

    CDJ_ButtonCallback_t _buttonCb;
    CDJ_JogCallback_t    _jogCb;
    CDJ_PitchCallback_t  _pitchCb;
    uint16_t             _lastDispatchedPitch;

    void _dispatchButtonEvents();
};

#endif // CDJ1000MK3_PANEL_H

