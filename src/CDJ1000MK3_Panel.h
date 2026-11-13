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
typedef void (*CDJ_KnobsCallback_t)(uint8_t touchBrake, uint8_t releaseStart);

class CDJ1000MK3_Panel {
public:
    CDJ1000MK3_Panel();

    /**
     * @brief Initialize CDJ-1000 MK3 Panel Controller
     * @param role CDJ_ROLE_SLAVE (reading panel when motherboard provides clock) or CDJ_ROLE_MASTER (standalone/direct to panel)
     * @param csPin Chip Select pin (default: CDJ_PIN_CS from CDJ1000MK3_Pins.h)
     * @param clockSpeed Clock speed in Hz for Master mode (default: CDJ_SPI_CLOCK_HZ)
     */
    bool begin(CDJ_SpiRole_t role = CDJ_ROLE_SLAVE, uint8_t csPin = CDJ_PIN_CS, uint32_t clockSpeed = CDJ_SPI_CLOCK_HZ);

    /**
     * @brief Switch operating SPI role at runtime (reconfigures hardware SPI registers & pin directions)
     * @param role CDJ_ROLE_MASTER or CDJ_ROLE_SLAVE
     * @param clockSpeed Clock speed in Hz for Master mode
     */
    bool setRole(CDJ_SpiRole_t role, uint32_t clockSpeed = CDJ_SPI_CLOCK_HZ);

    /**
     * @brief Get currently active SPI role
     */
    CDJ_SpiRole_t getRole() const { return _role; }

    /**
     * @brief Check if operating in Master mode
     */
    bool isMaster() const { return _role == CDJ_ROLE_MASTER; }

    /**
     * @brief Check if operating in Slave mode
     */
    bool isSlave() const { return _role == CDJ_ROLE_SLAVE; }

    /**
     * @brief Set Master mode polling interval in milliseconds (default: 15 ms ~ 66 Hz)
     */
    void setMasterPollInterval(uint32_t intervalMs) { _masterPollIntervalMs = intervalMs; }

    /**
     * @brief Get Master mode polling interval in milliseconds
     */
    uint32_t getMasterPollInterval() const { return _masterPollIntervalMs; }

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
    void onKnobs(CDJ_KnobsCallback_t cb)   { _knobCb = cb; }

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
    CDJ_SpiRole_t     _role;
    uint8_t           _csPin;
    uint32_t          _clockSpeed;
    uint32_t          _masterPollIntervalMs;
    uint32_t          _lastMasterPollTime;

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
    CDJ_KnobsCallback_t  _knobCb;
    uint16_t             _lastDispatchedPitch;
    uint8_t              _lastDispatchedTouchBrake;
    uint8_t              _lastDispatchedReleaseStart;

    void _dispatchButtonEvents();
};

#endif // CDJ1000MK3_PANEL_H

