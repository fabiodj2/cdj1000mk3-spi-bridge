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
     * @param role CDJ_ROLE_SLAVE (for reading panel when motherboard provides clock) or CDJ_ROLE_MASTER (standalone/drives panel)
     * @param csPin Chip Select pin (SS). Defaults to CDJ_PIN_CS from CDJ1000MK3_Pins.h
     * @param clockSpeed Clock speed in Hz for Master mode (default: CDJ_SPI_CLOCK_HZ)
     */
    bool begin(CDJ_SpiRole_t role = CDJ_ROLE_SLAVE, uint8_t csPin = CDJ_PIN_CS, uint32_t clockSpeed = CDJ_SPI_CLOCK_HZ);

    /**
     * @brief Switch operating SPI role at runtime (reconfigures hardware registers & pins)
     * @param role CDJ_ROLE_MASTER or CDJ_ROLE_SLAVE
     * @param clockSpeed Clock speed in Hz for Master mode
     */
    bool setRole(CDJ_SpiRole_t role, uint32_t clockSpeed = CDJ_SPI_CLOCK_HZ);

    /**
     * @brief Deinitialize and release SPI hardware registers & interrupts
     */
    void end();

    /**
     * @brief Get currently active SPI role
     */
    CDJ_SpiRole_t getRole() const { return _role; }

    /**
     * @brief Check if currently operating in Master mode
     */
    bool isMaster() const { return _role == CDJ_ROLE_MASTER; }

    /**
     * @brief Check if currently operating in Slave mode
     */
    bool isSlave() const { return _role == CDJ_ROLE_SLAVE; }

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
     * @brief Get current number of packages in cycle
     */
    uint8_t getNumTxPackages() const;

    /**
     * @brief Copy current transmit package in queue (0..7) to destination buffer
     */
    void getCurrentTxPackage(uint8_t *dest);

    /**
     * @brief Advance to next package in cycle (0..s_numTxPackages - 1)
     */
    void advanceTxPackage();

    /**
     * @brief Get currently active package index (0..7)
     */
    uint8_t getCurrentPackageIndex() const;

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

