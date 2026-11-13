/**
 * @file CDJ1000MK3_SPI.h
 * @brief Hardware SPI Abstraction Layer for Pioneer CDJ-1000 MK3
 * 
 * Supports:
 * 1. SPI Slave Mode: Arduino connects to physical CDJ Display Board (Display Board is Master).
 *    - Native AVR hardware SPI Slave with SS frame-sync interrupt (Uno/Nano/Mega/Pro Micro).
 *    - ESP32 hardware SPI slave driver.
 * 2. SPI Master Mode: Arduino acts as CDJ Main Board (driving physical or simulated panel).
 */

#ifndef CDJ1000MK3_SPI_H
#define CDJ1000MK3_SPI_H

#include <Arduino.h>
#include <SPI.h>
#include "CDJ1000MK3_Pins.h"
#include "CDJ1000MK3_Protocol.h"

typedef enum {
    CDJ_ROLE_SLAVE = 0,  // Connected to Display Board (Display Board generates Clock)
    CDJ_ROLE_MASTER      // Arduino generates Clock & CS (Acts as CDJ Main Assy)
} CDJ_SpiRole_t;

class CDJ1000MK3_SPI {
public:
    CDJ1000MK3_SPI();

    /**
     * @brief Initialize SPI interface
     * @param role CDJ_ROLE_SLAVE (default for panel reading) or CDJ_ROLE_MASTER
     * @param csPin Chip Select pin (SS). Defaults to CDJ_PIN_CS from CDJ1000MK3_Pins.h
     * @param clockSpeed Clock speed in Hz for Master mode (e.g. 1000000 for 1MHz)
     */
    bool begin(CDJ_SpiRole_t role = CDJ_ROLE_SLAVE, uint8_t csPin = CDJ_PIN_CS, uint32_t clockSpeed = CDJ_SPI_CLOCK_HZ);

    /**
     * @brief Check if a complete 27-byte frame has been received from the CDJ
     */
    bool isFrameAvailable();

    /**
     * @brief Retrieve the latest received 27-byte frame (copies from internal buffer)
     * @param dest 27-byte destination buffer
     */
    void getRxFrame(uint8_t *dest);

    /**
     * @brief Update the 27-byte transmit frame sent to the CDJ on the next SPI transfer
     * @param src 27-byte source buffer
     */
    void setTxFrame(const uint8_t *src);

    /**
     * @brief Update a specific package in the multi-package transmission queue (0..7)
     * @param pkgIndex Package index (0=WFM1, 1=WFM2, 2=WFM3, 3=WFM4, 4=WFM5, 5=TB/CB, 6=Text, 7=Status/LEDs)
     * @param src 27-byte source buffer
     */
    void setTxPackage(uint8_t pkgIndex, const uint8_t *src);

    /**
     * @brief Set number of packages to cycle through (1 = only package 7; 8 = full screen cycle)
     */
    void setNumTxPackages(uint8_t numPkgs);

    /**
     * @brief Update jog ring cursor byte across all active packages
     */
    void updateAllPackagesJogPosition(uint8_t pos);

    /**
     * @brief For Master Mode: Perform synchronous 27-byte full-duplex transfer
     */
    bool transferMaster(const uint8_t *tx, uint8_t *rx);

    // Internal ISR helpers (called from hardware interrupt handlers)
    static void isrOnByteReceived(uint8_t byteIn);
    static void isrOnChipSelectChange();

private:
    CDJ_SpiRole_t _role;
    uint8_t _csPin;
    uint32_t _clockSpeed;
    bool _initialized;
};

// Global singleton pointer for ISR routing
extern CDJ1000MK3_SPI* g_pCDJ_SPI;

#endif // CDJ1000MK3_SPI_H

