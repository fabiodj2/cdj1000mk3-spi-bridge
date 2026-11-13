/**
 * @file CDJ1000MK3_Pins.h
 * @brief Dedicated Hardware Pinout Configuration for Pioneer CDJ-1000 MK3 Emulator
 * ==============================================================================
 * File ini memisahkan seluruh konfigurasi pinout hardware mikrokontroler & CDJ.
 * 
 * Anda dapat:
 * 1. Menggunakan Auto-detection (otomatis mendeteksi Uno, Nano, Pro Micro, Mega, ESP32, Teensy, STM32, RP2040).
 * 2. Meng-uncomment salah satu preset di bawah untuk memaksakan tipe board.
 * 3. Memilih CDJ_BOARD_CUSTOM untuk menentukan pin GPIO kustom sesuai wiring Anda.
 * ==============================================================================
 */

#ifndef CDJ1000MK3_PINS_H
#define CDJ1000MK3_PINS_H

#ifdef ARDUINO
    #include <Arduino.h>
#else
    #include <stdint.h>
    #include <stdbool.h>
#endif

// =============================================================================
// 1. PRESET SELECTION (Uncomment salah satu jika ingin override manual)
// =============================================================================
// #define CDJ_BOARD_UNO_NANO          // Arduino Uno, Nano, Pro Mini (ATmega328P)
// #define CDJ_BOARD_PRO_MICRO         // Arduino Pro Micro / Leonardo (ATmega32U4)
// #define CDJ_BOARD_MEGA2560          // Arduino Mega 2560
// #define CDJ_BOARD_ESP32             // ESP32 Standard (VSPI)
// #define CDJ_BOARD_TEENSY            // Teensy 3.2 / 4.0 / 4.1
// #define CDJ_BOARD_STM32_BLUEPILL    // STM32F103C8T6 (Blue Pill SPI1)
// #define CDJ_BOARD_RP2040            // Raspberry Pi Pico (RP2040)
// #define CDJ_BOARD_CUSTOM            // Konfigurasi pin kustom Anda sendiri

// =============================================================================
// 2. AUTO-DETECTION BOARD (Jika tidak ada preset manual yang dipilih)
// =============================================================================
#if !defined(CDJ_BOARD_UNO_NANO) && !defined(CDJ_BOARD_PRO_MICRO) && \
    !defined(CDJ_BOARD_MEGA2560) && !defined(CDJ_BOARD_ESP32) &&     \
    !defined(CDJ_BOARD_TEENSY)   && !defined(CDJ_BOARD_STM32_BLUEPILL) && \
    !defined(CDJ_BOARD_RP2040)   && !defined(CDJ_BOARD_CUSTOM)

    #if defined(__AVR_ATmega2560__)
        #define CDJ_BOARD_MEGA2560
    #elif defined(__AVR_ATmega32U4__)
        #define CDJ_BOARD_PRO_MICRO
    #elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
        #define CDJ_BOARD_UNO_NANO
    #elif defined(ESP32)
        #define CDJ_BOARD_ESP32
    #elif defined(CORE_TEENSY)
        #define CDJ_BOARD_TEENSY
    #elif defined(ARDUINO_ARCH_STM32)
        #define CDJ_BOARD_STM32_BLUEPILL
    #elif defined(ARDUINO_ARCH_RP2040)
        #define CDJ_BOARD_RP2040
    #else
        #define CDJ_BOARD_UNO_NANO // Default fallback
    #endif

#endif

// =============================================================================
// 3. DEFINISI PINOUT SETIAP BOARD
// =============================================================================

#if defined(CDJ_BOARD_CUSTOM)
    // -------------------------------------------------------------------------
    // CUSTOM PINOUT: Atur pin GPIO sesuai kebutuhan rangkaian Anda
    // -------------------------------------------------------------------------
    #ifndef CDJ_PIN_SCLK
        #define CDJ_PIN_SCLK        13   // Hubungkan ke CDJ Pin 13 (SCLK)
    #endif
    #ifndef CDJ_PIN_MISO
        #define CDJ_PIN_MISO        12   // Hubungkan ke CDJ Pin 14 (SOUT)
    #endif
    #ifndef CDJ_PIN_MOSI
        #define CDJ_PIN_MOSI        11   // Hubungkan ke CDJ Pin 15 (SIN)
    #endif
    #ifndef CDJ_PIN_CS
        #define CDJ_PIN_CS          10   // Hubungkan ke CDJ Pin 16 (XCS)
    #endif
    #ifndef CDJ_BOARD_NAME
        #define CDJ_BOARD_NAME      "Custom Pinout Configuration"
    #endif

#elif defined(CDJ_BOARD_UNO_NANO)
    // -------------------------------------------------------------------------
    // ARDUINO UNO / NANO / PRO MINI (ATmega328P - 5V)
    // -------------------------------------------------------------------------
    #define CDJ_PIN_SCLK            13   // SCK (Input Clock dari CDJ Pin 13)
    #define CDJ_PIN_MISO            12   // MISO (Output Slave ke SIN CDJ Pin 15)
    #define CDJ_PIN_MOSI            11   // MOSI (Input Slave dari SOUT CDJ Pin 14)
    #define CDJ_PIN_CS              10   // SS / XCS (Frame Sync dari CDJ Pin 16)
    #define CDJ_BOARD_NAME          "Arduino Uno / Nano (ATmega328P)"

#elif defined(CDJ_BOARD_PRO_MICRO)
    // -------------------------------------------------------------------------
    // ARDUINO PRO MICRO / LEONARDO (ATmega32U4 - 5V Native USB)
    // -------------------------------------------------------------------------
    #define CDJ_PIN_SCLK            15   // Hardware SCK
    #define CDJ_PIN_MISO            14   // Hardware MISO (SOUT dari CDJ)
    #define CDJ_PIN_MOSI            16   // Hardware MOSI (SIN ke CDJ)
    #define CDJ_PIN_CS              10   // Pin 10 (SS / XCS)
    #define CDJ_BOARD_NAME          "Arduino Pro Micro / Leonardo (ATmega32U4)"

#elif defined(CDJ_BOARD_MEGA2560)
    // -------------------------------------------------------------------------
    // ARDUINO MEGA 2560 (ATmega2560 - 5V)
    // -------------------------------------------------------------------------
    #define CDJ_PIN_SCLK            52   // Hardware SCK
    #define CDJ_PIN_MISO            50   // Hardware MISO (SOUT dari CDJ)
    #define CDJ_PIN_MOSI            51   // Hardware MOSI (SIN ke CDJ)
    #define CDJ_PIN_CS              53   // Hardware SS (XCS)
    #define CDJ_BOARD_NAME          "Arduino Mega 2560"

#elif defined(CDJ_BOARD_ESP32)
    // -------------------------------------------------------------------------
    // ESP32 STANDARD (VSPI Default - 3.3V, Wajib Level Shifter ke CDJ 5V)
    // -------------------------------------------------------------------------
    #define CDJ_PIN_SCLK            18   // VSPI CLK
    #define CDJ_PIN_MISO            19   // VSPI MISO (SOUT dari CDJ)
    #define CDJ_PIN_MOSI            23   // VSPI MOSI (SIN ke CDJ)
    #define CDJ_PIN_CS              5    // VSPI CS (XCS)
    #define CDJ_BOARD_NAME          "ESP32 (VSPI)"

#elif defined(CDJ_BOARD_TEENSY)
    // -------------------------------------------------------------------------
    // TEENSY 3.2 / 4.0 / 4.1 (3.3V, Wajib Level Shifter ke CDJ 5V)
    // -------------------------------------------------------------------------
    #define CDJ_PIN_SCLK            13   // Hardware SCK
    #define CDJ_PIN_MISO            12   // Hardware MISO (SOUT dari CDJ)
    #define CDJ_PIN_MOSI            11   // Hardware MOSI (SIN ke CDJ)
    #define CDJ_PIN_CS              10   // Hardware CS (XCS)
    #define CDJ_BOARD_NAME          "Teensy 3.x / 4.x"

#elif defined(CDJ_BOARD_STM32_BLUEPILL)
    // -------------------------------------------------------------------------
    // STM32 BLUE PILL (STM32F103C8T6 - SPI1)
    // -------------------------------------------------------------------------
    #define CDJ_PIN_SCLK            PA5  // SPI1 SCK
    #define CDJ_PIN_MISO            PA6  // SPI1 MISO (SOUT dari CDJ)
    #define CDJ_PIN_MOSI            PA7  // SPI1 MOSI (SIN ke CDJ)
    #define CDJ_PIN_CS              PA4  // SPI1 NSS (XCS)
    #define CDJ_BOARD_NAME          "STM32 Blue Pill (SPI1)"

#elif defined(CDJ_BOARD_RP2040)
    // -------------------------------------------------------------------------
    // RASPBERRY PI PICO / RP2040 (SPI0)
    // -------------------------------------------------------------------------
    #define CDJ_PIN_SCLK            18   // SPI0 SCK
    #define CDJ_PIN_MISO            16   // SPI0 RX / MISO (SOUT dari CDJ)
    #define CDJ_PIN_MOSI            19   // SPI0 TX / MOSI (SIN ke CDJ)
    #define CDJ_PIN_CS              17   // SPI0 CS (XCS)
    #define CDJ_BOARD_NAME          "Raspberry Pi Pico (RP2040)"

#endif

// =============================================================================
// 4. TABEL KONEKSI FISIK KONEKTOR CDJ-1000 MK3 (CN2002 / TOP DISPLAY BOARD)
// =============================================================================
/*
 * +-------------+-----------------------+-------------------------------------+
 * | Pin Fisik   | Sinyal Asli CDJ       | Terhubung ke Pin Mikrokontroler     |
 * +-------------+-----------------------+-------------------------------------+
 * | Pin 13      | SCLK                  | CDJ_PIN_SCLK                        |
 * | Pin 14      | SOUT (Data dari CDJ)  | CDJ_PIN_MISO                        |
 * | Pin 15      | SIN  (Data ke CDJ)    | CDJ_PIN_MOSI                        |
 * | Pin 16      | XCS  (Chip Select)    | CDJ_PIN_CS                          |
 * | GND         | Ground Referensi      | GND (Ground bersama Arduino)        |
 * | +5V         | Daya Logika (PSU CDJ) | +5V                                 |
 * +-------------+-----------------------+-------------------------------------+
 */

// Helper Struct untuk konfigurasi pin dinamis (jika diubah saat runtime)
struct CDJ_PinMapping {
    uint8_t sclk;
    uint8_t miso;
    uint8_t mosi;
    uint8_t cs;
};

static inline CDJ_PinMapping CDJ_GetDefaultPins() {
    CDJ_PinMapping p;
    p.sclk = CDJ_PIN_SCLK;
    p.miso = CDJ_PIN_MISO;
    p.mosi = CDJ_PIN_MOSI;
    p.cs   = CDJ_PIN_CS;
    return p;
}

#endif // CDJ1000MK3_PINS_H
