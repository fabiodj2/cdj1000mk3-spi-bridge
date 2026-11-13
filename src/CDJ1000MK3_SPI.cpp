/**
 * @file CDJ1000MK3_SPI.cpp
 * @brief Implementation of Hardware SPI Layer for CDJ-1000 MK3
 */

#include "CDJ1000MK3_SPI.h"

CDJ1000MK3_SPI* g_pCDJ_SPI = nullptr;

// Internal ring/ping-pong buffers for interrupt-safe data exchange
static volatile uint8_t s_rxBuf[CDJ_FRAME_SIZE];
static volatile uint8_t s_rxCompleted[CDJ_FRAME_SIZE];
static volatile uint8_t s_txPackages[8][CDJ_FRAME_SIZE];
static volatile uint8_t s_numTxPackages = 1;
static volatile uint8_t s_currentPkgIdx = 0;
static volatile uint8_t s_byteIdx = 0;
static volatile bool    s_frameReady = false;
static uint8_t          s_csPinInternal = CDJ_PIN_CS;

#if defined(ESP32)
#include "driver/spi_slave.h"
static spi_slave_transaction_t s_esp32_txrx;
static WORD_ALIGNED_ATTR uint8_t s_esp32_tx[CDJ_FRAME_SIZE];
static WORD_ALIGNED_ATTR uint8_t s_esp32_rx[CDJ_FRAME_SIZE];
#endif

// Forward interrupt handlers
#if defined(__AVR__)
static inline void handleCsTransition(uint8_t csState) {
    if (csState == LOW) {
        // Frame start: Reset byte counter and preload first transmit byte
        s_byteIdx = 0;
        SPDR = s_txPackages[s_currentPkgIdx][0];
    } else {
        // Frame end: Rising edge indicates 27-byte packet transmission complete
        if (s_byteIdx >= 25) { // Received at least 25-27 bytes
            for (uint8_t i = 0; i < CDJ_FRAME_SIZE; i++) {
                s_rxCompleted[i] = s_rxBuf[i];
            }
            s_frameReady = true;
        }
        s_byteIdx = 0;

        // Advance to next package in cycle (0..s_numTxPackages - 1)
        if (s_numTxPackages > 1) {
            s_currentPkgIdx = (s_currentPkgIdx + 1) % s_numTxPackages;
        } else {
            s_currentPkgIdx = 0;
        }

        SPDR = s_txPackages[s_currentPkgIdx][0]; // Preload for start of next frame
    }
}

static void onCsPinInterrupt() {
    handleCsTransition(digitalRead(s_csPinInternal));
}

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
// Pin Change Interrupt for Pin 10 (PB2 / PCINT2 on PORTB)
ISR(PCINT0_vect) {
    static uint8_t s_lastCsPinState = HIGH;
    uint8_t currentCsState = (PINB & _BV(PINB2)) ? HIGH : LOW;
    if (currentCsState != s_lastCsPinState) {
        s_lastCsPinState = currentCsState;
        handleCsTransition(currentCsState);
    }
}
#endif

// AVR Hardware SPI Slave Complete Interrupt
ISR(SPI_STC_vect) {
    uint8_t inByte = SPDR;

    if (s_byteIdx < CDJ_FRAME_SIZE) {
        s_rxBuf[s_byteIdx] = inByte;
        s_byteIdx++;
        if (s_byteIdx < CDJ_FRAME_SIZE) {
            SPDR = s_txPackages[s_currentPkgIdx][s_byteIdx];
        } else {
            // Frame complete! All 27 bytes received
            for (uint8_t i = 0; i < CDJ_FRAME_SIZE; i++) {
                s_rxCompleted[i] = s_rxBuf[i];
            }
            s_frameReady = true;
            SPDR = s_txPackages[s_currentPkgIdx][0]; // Reset preload for next packet
        }
    } else {
        SPDR = s_txPackages[s_currentPkgIdx][0];
    }
}
#else
static void onCsPinInterrupt() {
    uint8_t csState = digitalRead(s_csPinInternal);
    if (csState == LOW) {
        s_byteIdx = 0;
    } else {
        if (s_byteIdx >= 25) {
            for (uint8_t i = 0; i < CDJ_FRAME_SIZE; i++) {
                s_rxCompleted[i] = s_rxBuf[i];
            }
            s_frameReady = true;
        }
        s_byteIdx = 0;
    }
}
#endif

CDJ1000MK3_SPI::CDJ1000MK3_SPI()
    : _role(CDJ_ROLE_SLAVE), _csPin(CDJ_PIN_CS), _clockSpeed(CDJ_SPI_CLOCK_HZ), _initialized(false) {
    g_pCDJ_SPI = this;
    memset((void*)s_rxBuf, 0, CDJ_FRAME_SIZE);
    memset((void*)s_rxCompleted, 0, CDJ_FRAME_SIZE);

    // Initialize all 8 packages with standard Pioneer headers (0, 24, 48, 72, 96, 120, 144, 168)
    const uint8_t pkgHeaders[8] = { 0, 24, 48, 72, 96, 120, 144, 168 };
    for (uint8_t p = 0; p < 8; p++) {
        memset((void*)s_txPackages[p], 0, CDJ_FRAME_SIZE);
        s_txPackages[p][0] = pkgHeaders[p];
        s_txPackages[p][25] = 1; // Default jog ring position
        s_txPackages[p][26] = CDJ_CalculateTxChecksum((const uint8_t*)s_txPackages[p]);
    }

    // Package 7 default template
    CDJ_InitTxPackage7((uint8_t*)s_txPackages[7]);
    // Also copy to package 0 as single-package fallback
    CDJ_InitTxPackage7((uint8_t*)s_txPackages[0]);
    s_numTxPackages = 1;
    s_currentPkgIdx = 0;
}

bool CDJ1000MK3_SPI::begin(CDJ_SpiRole_t role, uint8_t csPin, uint32_t clockSpeed) {
    _role = role;
    _csPin = csPin;
    _clockSpeed = clockSpeed;
    s_csPinInternal = csPin;

    if (_role == CDJ_ROLE_SLAVE) {
        // ---------------------------------------------------------------------
        // SLAVE MODE (Connected to CDJ-1000 MK3 Display Board)
        // ---------------------------------------------------------------------
#if defined(__AVR__)
        // AVR Hardware SPI Slave configuration
        pinMode(MISO, OUTPUT);
        pinMode(MOSI, INPUT);
        pinMode(SCK, INPUT);
        pinMode(_csPin, INPUT_PULLUP);

        // Preload first byte into SPDR
        SPDR = s_txPackages[0][0];

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
        if (_csPin == 10) {
            // Pin 10 is PB2 (PCINT2). Enable Pin Change Interrupt on PORTB
            PCMSK0 |= _BV(PCINT2);
            PCIFR  |= _BV(PCIF0);
            PCICR  |= _BV(PCIE0);
        } else if (digitalPinToInterrupt(_csPin) != NOT_AN_INTERRUPT) {
            attachInterrupt(digitalPinToInterrupt(_csPin), onCsPinInterrupt, CHANGE);
        }
#else
        if (digitalPinToInterrupt(_csPin) != NOT_AN_INTERRUPT) {
            attachInterrupt(digitalPinToInterrupt(_csPin), onCsPinInterrupt, CHANGE);
        }
#endif

        // Set SPI Control Register (SPCR):
        // SPE=1 (SPI Enable), SPIE=1 (Interrupt Enable), CPOL=1, CPHA=1 (SPI Mode 3), MSTR=0 (Slave)
        SPCR = _BV(SPE) | _BV(SPIE) | _BV(CPOL) | _BV(CPHA);

        _initialized = true;
        return true;

#elif defined(ESP32)
        // ESP32 Hardware SPI Slave Configuration using CDJ1000MK3_Pins.h
        spi_bus_config_t buscfg = {
            .mosi_io_num = CDJ_PIN_MOSI,
            .miso_io_num = CDJ_PIN_MISO,
            .sclk_io_num = CDJ_PIN_SCLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 32
        };
        spi_slave_interface_config_t slvcfg = {
            .spics_io_num = _csPin,
            .flags = 0,
            .queue_size = 3,
            .mode = 3, // SPI Mode 3
            .post_setup_cb = NULL,
            .post_trans_cb = NULL
        };
        esp_err_t ret = spi_slave_initialize(VSPI_HOST, &buscfg, &slvcfg, 1);
        if (ret == ESP_OK) {
            _initialized = true;
            return true;
        }
        return false;
#else
        // Generic fallback: Attach CS interrupt
        pinMode(_csPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(_csPin), onCsPinInterrupt, CHANGE);
        _initialized = true;
        return true;
#endif

    } else {
        // ---------------------------------------------------------------------
        // MASTER MODE (Arduino acts as CDJ Main Assy)
        // ---------------------------------------------------------------------
        pinMode(_csPin, OUTPUT);
        digitalWrite(_csPin, HIGH);
        SPI.begin();
        _initialized = true;
        return true;
    }
}

bool CDJ1000MK3_SPI::isFrameAvailable() {
#if defined(ESP32)
    if (_role == CDJ_ROLE_SLAVE) {
        memset(&s_esp32_txrx, 0, sizeof(s_esp32_txrx));
        s_esp32_txrx.length = CDJ_FRAME_SIZE * 8;
        s_esp32_txrx.tx_buffer = s_esp32_tx;
        s_esp32_txrx.rx_buffer = s_esp32_rx;
        esp_err_t ret = spi_slave_transmit(VSPI_HOST, &s_esp32_txrx, 0); // Non-blocking
        if (ret == ESP_OK) {
            memcpy((void*)s_rxCompleted, s_esp32_rx, CDJ_FRAME_SIZE);
            return true;
        }
        return false;
    }
#endif
    return s_frameReady;
}

void CDJ1000MK3_SPI::getRxFrame(uint8_t *dest) {
    if (!dest) return;
#if defined(__AVR__)
    uint8_t oldSreg = SREG;
    cli();
#endif
    for (uint8_t i = 0; i < CDJ_FRAME_SIZE; i++) {
        dest[i] = s_rxCompleted[i];
    }
    s_frameReady = false;
#if defined(__AVR__)
    SREG = oldSreg;
#endif
}

void CDJ1000MK3_SPI::setTxFrame(const uint8_t *src) {
    if (!src) return;
    setTxPackage(s_numTxPackages > 1 ? 7 : 0, src);
}

void CDJ1000MK3_SPI::setTxPackage(uint8_t pkgIndex, const uint8_t *src) {
    if (!src || pkgIndex >= 8) return;
#if defined(__AVR__)
    uint8_t oldSreg = SREG;
    cli();
#endif
    for (uint8_t i = 0; i < CDJ_FRAME_SIZE; i++) {
        s_txPackages[pkgIndex][i] = src[i];
    }
#if defined(ESP32)
    memcpy(s_esp32_tx, src, CDJ_FRAME_SIZE);
#endif
#if defined(__AVR__)
    SREG = oldSreg;
#endif
}

void CDJ1000MK3_SPI::setNumTxPackages(uint8_t numPkgs) {
    if (numPkgs < 1) numPkgs = 1;
    if (numPkgs > 8) numPkgs = 8;
    s_numTxPackages = numPkgs;
}

void CDJ1000MK3_SPI::updateAllPackagesJogPosition(uint8_t pos) {
#if defined(__AVR__)
    uint8_t oldSreg = SREG;
    cli();
#endif
    for (uint8_t p = 0; p < 8; p++) {
        s_txPackages[p][25] = pos;
        s_txPackages[p][26] = CDJ_CalculateTxChecksum((const uint8_t*)s_txPackages[p]);
    }
#if defined(__AVR__)
    SREG = oldSreg;
#endif
}

bool CDJ1000MK3_SPI::transferMaster(const uint8_t *tx, uint8_t *rx) {
    if (_role != CDJ_ROLE_MASTER || !tx || !rx) return false;

    SPI.beginTransaction(SPISettings(_clockSpeed, MSBFIRST, SPI_MODE3));
    digitalWrite(_csPin, LOW);
    delayMicroseconds(2);

    for (uint8_t i = 0; i < CDJ_FRAME_SIZE; i++) {
        rx[i] = SPI.transfer(tx[i]);
    }

    delayMicroseconds(2);
    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();

    return true;
}

