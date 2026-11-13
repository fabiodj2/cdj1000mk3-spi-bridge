/**
 * @file CDJ1000MK3_Panel.cpp
 * @brief Implementation of High-Level CDJ-1000 MK3 Panel Controller
 */

#include "CDJ1000MK3_Panel.h"
#include <stdlib.h>

CDJ1000MK3_Panel::CDJ1000MK3_Panel()
    : _isFirstFrame(true), _txDirty(false), _buttonCb(nullptr), _jogCb(nullptr), _pitchCb(nullptr), _lastDispatchedPitch(8192) {
    memset(&_state, 0, sizeof(_state));
    memset(&_prevState, 0, sizeof(_prevState));
    memset(&_debouncedState, 0, sizeof(_debouncedState));
    memset(_rxRaw, 0, CDJ_FRAME_SIZE);
    CDJ_InitTxPackage7(_txRaw);
}

bool CDJ1000MK3_Panel::begin(CDJ_SpiRole_t role, uint8_t csPin) {
    _isFirstFrame = true;
    CDJ_InitTxPackage7(_txRaw);
    _spi.setTxFrame(_txRaw);
    return _spi.begin(role, csPin);
}

bool CDJ1000MK3_Panel::update() {
    if (!_spi.isFrameAvailable()) {
        return false;
    }

    _spi.getRxFrame(_rxRaw);

    // Parse packet - MUST match 0x01 0x10 header!
    if (!CDJ_ParseRxFrame(_rxRaw, &_state, false)) {
        return false;
    }

    // First frame initialization (prevent initial jog delta jump)
    if (_isFirstFrame) {
        _isFirstFrame = false;
        _lastDispatchedPitch = _state.pitchFader;
        _prevState = _state;
        _debouncedState = _state;
        return true;
    }

    // Debounced button dispatch: state must be stable for 2 frames
    _dispatchButtonEvents();

    // Check Jog rotation OR touch sensor state change
    bool jogMoved = (_state.jogPulseDelta != 0);
    bool touchChanged = (_state.jogTouched != _prevState.jogTouched);
    if ((jogMoved || touchChanged) && _jogCb) {
        _jogCb(_state.jogPulseDelta, _state.jogTouched, _state.jogSpeed);
    }

    // Check Pitch Fader change (with deadband filtering against last dispatched value)
    if (abs((int32_t)_state.pitchFader - (int32_t)_lastDispatchedPitch) >= 48 && _pitchCb) {
        _lastDispatchedPitch = _state.pitchFader;
        _pitchCb(_state.pitchFader);
    }

    _prevState = _state;

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

static void setButtonStateInStruct(CDJ_PanelState_t* st, CDJ_ButtonId_t btn, bool val) {
    if (!st) return;
    switch (btn) {
        case CDJ_ID_PLAY:           st->play = val; break;
        case CDJ_ID_CUE:            st->cue = val; break;
        case CDJ_ID_LOOP_IN:        st->loopIn = val; break;
        case CDJ_ID_LOOP_OUT:       st->loopOut = val; break;
        case CDJ_ID_RELOOP_EXIT:    st->reloopExit = val; break;
        case CDJ_ID_HOTCUE_A:       st->hotCueA = val; break;
        case CDJ_ID_HOTCUE_B:       st->hotCueB = val; break;
        case CDJ_ID_HOTCUE_C:       st->hotCueC = val; break;
        case CDJ_ID_REC_MODE:       st->recMode = val; break;
        case CDJ_ID_TRACK_REV:      st->trackRev = val; break;
        case CDJ_ID_TRACK_FWD:      st->trackFwd = val; break;
        case CDJ_ID_SEARCH_REV:     st->searchRev = val; break;
        case CDJ_ID_SEARCH_FWD:     st->searchFwd = val; break;
        case CDJ_ID_FOLDER_REV:     st->folderRev = val; break;
        case CDJ_ID_FOLDER_FWD:     st->folderFwd = val; break;
        case CDJ_ID_JOG_MODE:       st->jogMode = val; break;
        case CDJ_ID_TEMPO_RANGE:    st->tempoRange = val; break;
        case CDJ_ID_MASTER_TEMPO:   st->masterTempo = val; break;
        case CDJ_ID_TEMPO_RESET:    st->tempoReset = val; break;
        case CDJ_ID_CALL_FWD:       st->callFwd = val; break;
        case CDJ_ID_CALL_REV:       st->callRev = val; break;
        case CDJ_ID_MEMORY:         st->memory = val; break;
        case CDJ_ID_DELETE:         st->del = val; break;
        case CDJ_ID_EJECT:          st->eject = val; break;
        case CDJ_ID_TIME_MODE:      st->timeMode = val; break;
        case CDJ_ID_TEXT_MODE:      st->textMode = val; break;
        case CDJ_ID_JOG_TOUCH:      st->jogTouched = val; break;
        case CDJ_ID_REVERSE_SWITCH: st->reverseSwitch = !val; break;
        default: break;
    }
}

void CDJ1000MK3_Panel::_dispatchButtonEvents() {
    if (!_buttonCb) return;

    for (int i = 1; i < (int)CDJ_ID_MAX; i++) {
        CDJ_ButtonId_t btn = (CDJ_ButtonId_t)i;
        bool curr = getButtonStateFromStruct(_state, btn);
        bool prev = getButtonStateFromStruct(_prevState, btn);
        bool debounced = getButtonStateFromStruct(_debouncedState, btn);

        // Signal must be stable for 2 frames before accepting transition
        if (curr == prev && curr != debounced) {
            setButtonStateInStruct(&_debouncedState, btn, curr);
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
    _spi.updateAllPackagesJogPosition(pos);
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

void CDJ1000MK3_Panel::setTrackDisplay(uint8_t trackNumber) {
    CDJ_SetTxTrackDisplay(_txRaw, trackNumber);
    _txDirty = true;
}

void CDJ1000MK3_Panel::enableMultiPackageMode(bool enable) {
    _spi.setNumTxPackages(enable ? 8 : 1);
}

void CDJ1000MK3_Panel::setCustomText(const char* text) {
    if (!text) return;
    uint8_t pkg6[CDJ_FRAME_SIZE];
    memset(pkg6, 0, CDJ_FRAME_SIZE);
    pkg6[0] = 144; // Package 6 Header (0x90)
    uint8_t len = strlen(text);
    for (uint8_t i = 0; i < 16; i++) {
        pkg6[1 + i] = (i < len) ? (uint8_t)text[i] : ' ';
    }
    pkg6[25] = _txRaw[25];
    pkg6[26] = CDJ_CalculateTxChecksum(pkg6);
    _spi.setTxPackage(6, pkg6);
}

void CDJ1000MK3_Panel::setWaveform(const uint8_t* bars100) {
    if (!bars100) return;
    uint8_t pkg[CDJ_FRAME_SIZE];

    // Package 0: Bars 1..24
    memset(pkg, 0, CDJ_FRAME_SIZE);
    pkg[0] = 0;
    memcpy(&pkg[1], &bars100[0], 24);
    pkg[25] = _txRaw[25];
    pkg[26] = CDJ_CalculateTxChecksum(pkg);
    _spi.setTxPackage(0, pkg);

    // Package 1: Bars 25..48
    memset(pkg, 0, CDJ_FRAME_SIZE);
    pkg[0] = 24;
    memcpy(&pkg[1], &bars100[24], 24);
    pkg[25] = _txRaw[25];
    pkg[26] = CDJ_CalculateTxChecksum(pkg);
    _spi.setTxPackage(1, pkg);

    // Package 2: Bars 49..72
    memset(pkg, 0, CDJ_FRAME_SIZE);
    pkg[0] = 48;
    memcpy(&pkg[1], &bars100[48], 24);
    pkg[25] = _txRaw[25];
    pkg[26] = CDJ_CalculateTxChecksum(pkg);
    _spi.setTxPackage(2, pkg);

    // Package 3: Bars 73..96
    memset(pkg, 0, CDJ_FRAME_SIZE);
    pkg[0] = 72;
    memcpy(&pkg[1], &bars100[72], 24);
    pkg[25] = _txRaw[25];
    pkg[26] = CDJ_CalculateTxChecksum(pkg);
    _spi.setTxPackage(3, pkg);

    // Package 4: Bars 97..100
    memset(pkg, 0, CDJ_FRAME_SIZE);
    pkg[0] = 96;
    memcpy(&pkg[1], &bars100[96], 4);
    pkg[25] = _txRaw[25];
    pkg[26] = CDJ_CalculateTxChecksum(pkg);
    _spi.setTxPackage(4, pkg);
}

void CDJ1000MK3_Panel::setRandomWaveform() {
    uint8_t wfm[100];
    uint8_t h = 2;
    for (int i = 0; i < 100; i++) {
        int r = random(-1, 2);
        int nh = (int)h + r;
        if (nh < 1) nh = 1;
        if (nh > 5) nh = 5;
        h = nh;
        if (i % 8 == 0) h = 5; // Beat peak
        switch (h) {
            case 1: wfm[i] = 0x01; break;
            case 2: wfm[i] = 0x03; break;
            case 3: wfm[i] = 0x07; break;
            case 4: wfm[i] = 0x0F; break;
            default: wfm[i] = 0x1F; break;
        }
    }
    setWaveform(wfm);
}

void CDJ1000MK3_Panel::setTimeDisplay(uint8_t minutes, uint8_t seconds, uint8_t frames) {
    if (minutes > 99) minutes = 99;
    if (seconds > 59) seconds = 59;
    if (frames > 74) frames = 74;

    _txRaw[4] = CDJ_DigitTo7Segment('0' + (minutes / 10));
    _txRaw[5] = CDJ_DigitTo7Segment('0' + (minutes % 10));
    _txRaw[7] = CDJ_DigitTo7Segment('0' + (seconds / 10));
    _txRaw[8] = CDJ_DigitTo7Segment('0' + (seconds % 10));
    _txRaw[10] = CDJ_DigitTo7Segment('0' + (frames / 10));
    _txRaw[11] = CDJ_DigitTo7Segment('0' + (frames % 10));
    _txRaw[26] = CDJ_CalculateTxChecksum(_txRaw);
    _txDirty = true;
}

void CDJ1000MK3_Panel::setTempoDisplay(float pitchPercent) {
    float absVal = fabs(pitchPercent);
    if (absVal > 99.99f) absVal = 99.99f;
    uint16_t intVal = (uint16_t)(absVal * 100.0f + 0.5f);
    uint8_t d1 = (intVal / 100) % 10;
    uint8_t d2 = (intVal / 10) % 10;
    uint8_t d3 = intVal % 10;

    _txRaw[13] = CDJ_DigitTo7Segment('0' + d1);
    _txRaw[14] = CDJ_DigitTo7Segment('0' + d2);
    _txRaw[15] = CDJ_DigitTo7Segment('0' + d3);
    _txRaw[26] = CDJ_CalculateTxChecksum(_txRaw);
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

