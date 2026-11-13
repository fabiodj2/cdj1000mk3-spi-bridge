/**
 * @file CDJ1000MK3_Panel.cpp
 * @brief Implementation of High-Level CDJ-1000 MK3 Panel Controller
 */

#include "CDJ1000MK3_Panel.h"
#include <stdlib.h>

CDJ1000MK3_Panel::CDJ1000MK3_Panel()
    : _txDirty(false), _buttonCb(nullptr), _jogCb(nullptr), _pitchCb(nullptr) {
    memset(&_state, 0, sizeof(_state));
    memset(&_prevState, 0, sizeof(_prevState));
    memset(_rxRaw, 0, CDJ_FRAME_SIZE);
    CDJ_InitTxPackage7(_txRaw);
}

bool CDJ1000MK3_Panel::begin(CDJ_SpiRole_t role, uint8_t csPin) {
    CDJ_InitTxPackage7(_txRaw);
    _spi.setTxFrame(_txRaw);
    return _spi.begin(role, csPin);
}

bool CDJ1000MK3_Panel::update() {
    if (!_spi.isFrameAvailable()) {
        return false;
    }

    _spi.getRxFrame(_rxRaw);

    _prevState = _state;

    // Parse packet
    if (!CDJ_ParseRxFrame(_rxRaw, &_state, false)) {
        return false;
    }

    // Check button transition events
    _dispatchButtonEvents();

    // Check Jog rotation
    if (_state.jogPulseDelta != 0 && _jogCb) {
        _jogCb(_state.jogPulseDelta, _state.jogTouched, _state.jogSpeed);
    }

    // Check Pitch Fader change (with deadband filtering)
    if (abs((int32_t)_state.pitchFader - (int32_t)_prevState.pitchFader) >= 4 && _pitchCb) {
        _pitchCb(_state.pitchFader);
    }

    // If LEDs or displays were modified, update transmit buffer
    if (_txDirty) {
        _spi.setTxFrame(_txRaw);
        _txDirty = false;
    }

    return true;
}

static bool getButtonStateFromStruct(const CDJ_PanelState_t& st, CDJ_ButtonId_t btn) {
    switch (btn) {
        case CDJ_ID_PLAY:           return st.play;
        case CDJ_ID_CUE:            return st.cue;
        case CDJ_ID_LOOP_IN:        return st.loopIn;
        case CDJ_ID_LOOP_OUT:       return st.loopOut;
        case CDJ_ID_RELOOP_EXIT:    return st.reloopExit;
        case CDJ_ID_HOTCUE_A:       return st.hotCueA;
        case CDJ_ID_HOTCUE_B:       return st.hotCueB;
        case CDJ_ID_HOTCUE_C:       return st.hotCueC;
        case CDJ_ID_REC_MODE:       return st.recMode;
        case CDJ_ID_TRACK_REV:      return st.trackRev;
        case CDJ_ID_TRACK_FWD:      return st.trackFwd;
        case CDJ_ID_SEARCH_REV:     return st.searchRev;
        case CDJ_ID_SEARCH_FWD:     return st.searchFwd;
        case CDJ_ID_FOLDER_REV:     return st.folderRev;
        case CDJ_ID_FOLDER_FWD:     return st.folderFwd;
        case CDJ_ID_JOG_MODE:       return st.jogMode;
        case CDJ_ID_TEMPO_RANGE:    return st.tempoRange;
        case CDJ_ID_MASTER_TEMPO:   return st.masterTempo;
        case CDJ_ID_TEMPO_RESET:    return st.tempoReset;
        case CDJ_ID_CALL_FWD:       return st.callFwd;
        case CDJ_ID_CALL_REV:       return st.callRev;
        case CDJ_ID_MEMORY:         return st.memory;
        case CDJ_ID_DELETE:         return st.del;
        case CDJ_ID_EJECT:          return st.eject;
        case CDJ_ID_TIME_MODE:      return st.timeMode;
        case CDJ_ID_TEXT_MODE:      return st.textMode;
        case CDJ_ID_JOG_TOUCH:      return st.jogTouched;
        case CDJ_ID_REVERSE_SWITCH: return !st.reverseSwitch; // Active reverse
        default:                    return false;
    }
}

bool CDJ1000MK3_Panel::isPressed(CDJ_ButtonId_t btn) const {
    return getButtonStateFromStruct(_state, btn);
}

bool CDJ1000MK3_Panel::justPressed(CDJ_ButtonId_t btn) const {
    return getButtonStateFromStruct(_state, btn) && !getButtonStateFromStruct(_prevState, btn);
}

bool CDJ1000MK3_Panel::justReleased(CDJ_ButtonId_t btn) const {
    return !getButtonStateFromStruct(_state, btn) && getButtonStateFromStruct(_prevState, btn);
}

void CDJ1000MK3_Panel::_dispatchButtonEvents() {
    if (!_buttonCb) return;

    for (int i = 1; i < (int)CDJ_ID_MAX; i++) {
        CDJ_ButtonId_t btn = (CDJ_ButtonId_t)i;
        bool curr = getButtonStateFromStruct(_state, btn);
        bool prev = getButtonStateFromStruct(_prevState, btn);
        if (curr != prev) {
            _buttonCb(btn, curr);
        }
    }
}

// -----------------------------------------------------------------------------
// LED & FEEDBACK CONTROLS
// -----------------------------------------------------------------------------

void CDJ1000MK3_Panel::setPlayLed(bool on) {
    CDJ_SetTxLed(_txRaw, 17, CDJ_LED_PLAY, on);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setCueLed(bool on) {
    CDJ_SetTxLed(_txRaw, 17, CDJ_LED_CUE, on);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setLoopInLed(bool on) {
    CDJ_SetTxLed(_txRaw, 17, CDJ_LED_LOOP_IN, on);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setLoopOutLed(bool on) {
    CDJ_SetTxLed(_txRaw, 17, CDJ_LED_LOOP_OUT, on);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setReloopExitLed(bool on) {
    CDJ_SetTxLed(_txRaw, 17, CDJ_LED_RELOOP_EXIT, on);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setReverseLed(bool on) {
    CDJ_SetTxLed(_txRaw, 17, CDJ_LED_REVERSE, on);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setHotCue(uint8_t cueIndex, CDJ_HotCueColor_t color) {
    CDJ_SetTxHotCue(_txRaw, cueIndex, color);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setMasterTempoLed(bool on) {
    CDJ_SetTxLed(_txRaw, 19, CDJ_LED_MASTER_TEMPO, on);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setTempoResetLed(bool on) {
    CDJ_SetTxLed(_txRaw, 19, CDJ_LED_TEMPO_RESET, on);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setVinylLed(bool on) {
    CDJ_SetTxLed(_txRaw, 19, CDJ_LED_VINYL, on);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setCdjLed(bool on) {
    CDJ_SetTxLed(_txRaw, 19, CDJ_LED_CDJ, on);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setSdCardLed(bool on) {
    CDJ_SetTxLed(_txRaw, 19, CDJ_LED_SD_CARD, on);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setJogPosition(uint8_t pos) {
    CDJ_SetTxJogRing(_txRaw, pos, _txRaw[21]);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setJogCuePosition(uint8_t cuePos) {
    CDJ_SetTxJogRing(_txRaw, _txRaw[25], cuePos);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setTouchRing(bool on) {
    CDJ_SetTxTouchRingVFD(_txRaw, on);
    _txDirty = true;
}

void CDJ1000MK3_Panel::commitLeds() {
    if (_txDirty) {
        _spi.setTxFrame(_txRaw);
        _txDirty = false;
    }
}

const char* CDJ1000MK3_Panel::getButtonName(CDJ_ButtonId_t btn) {
    switch (btn) {
        case CDJ_ID_PLAY:           return "PLAY/PAUSE";
        case CDJ_ID_CUE:            return "CUE";
        case CDJ_ID_LOOP_IN:        return "LOOP IN";
        case CDJ_ID_LOOP_OUT:       return "LOOP OUT";
        case CDJ_ID_RELOOP_EXIT:    return "RELOOP/EXIT";
        case CDJ_ID_HOTCUE_A:       return "HOT CUE A";
        case CDJ_ID_HOTCUE_B:       return "HOT CUE B";
        case CDJ_ID_HOTCUE_C:       return "HOT CUE C";
        case CDJ_ID_REC_MODE:       return "REC MODE";
        case CDJ_ID_TRACK_REV:      return "TRACK SEARCH <<";
        case CDJ_ID_TRACK_FWD:      return "TRACK SEARCH >>";
        case CDJ_ID_SEARCH_REV:     return "SEARCH <<";
        case CDJ_ID_SEARCH_FWD:     return "SEARCH >>";
        case CDJ_ID_FOLDER_REV:     return "MP3 FOLDER <";
        case CDJ_ID_FOLDER_FWD:     return "MP3 FOLDER >";
        case CDJ_ID_JOG_MODE:       return "JOG MODE (VINYL/CDJ)";
        case CDJ_ID_TEMPO_RANGE:    return "TEMPO RANGE";
        case CDJ_ID_MASTER_TEMPO:   return "MASTER TEMPO";
        case CDJ_ID_TEMPO_RESET:    return "TEMPO RESET";
        case CDJ_ID_CALL_FWD:       return "CALL >";
        case CDJ_ID_CALL_REV:       return "< CALL";
        case CDJ_ID_MEMORY:         return "MEMORY";
        case CDJ_ID_DELETE:         return "DELETE";
        case CDJ_ID_EJECT:          return "EJECT";
        case CDJ_ID_TIME_MODE:      return "TIME MODE/AUTO CUE";
        case CDJ_ID_TEXT_MODE:      return "TEXT MODE";
        case CDJ_ID_JOG_TOUCH:      return "JOG PLATTER TOUCH";
        case CDJ_ID_REVERSE_SWITCH: return "REVERSE SWITCH";
        default:                    return "UNKNOWN";
    }
}

