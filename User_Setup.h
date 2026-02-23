// User_Setup.h - TFT_eSPI Configuration for GC9A01 1.28" Round LCD with ESP32
// This file configures TFT_eSPI library for your specific hardware

#ifndef USER_SETUP_H_
#define USER_SETUP_H_

// ============= DISPLAY DRIVER SELECTION =============
#define GC9A01_DRIVER      // GC9A01 display driver (1.28" round LCD)

// ============= DISPLAY DIMENSIONS =============
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// ============= INTERFACE TYPE =============
#define TFT_INTERFACE_SPI  // Use SPI interface

// ============= PIN DEFINITIONS FOR ESP32 =============
// ESP32 Development Board with GC9A01 1.28" Round LCD
#define TFT_CS    15      // Chip Select (D15)
#define TFT_DC     4      // Data/Command (A0 - D4)
#define TFT_RST    2      // Reset (D2)
#define TFT_MOSI  23      // SDA/MOSI (D23)
#define TFT_SCLK  18      // SCK/CLK (D18)

// SPI MISO not used for this display (write-only)
// #define TFT_MISO  -1

// ============= SPI CONFIGURATION =============
#define SPI_FREQUENCY  40000000  // 40 MHz SPI speed (GC9A01 supports up to 80MHz)
#define SPI_READ_FREQUENCY  5000000  // 5 MHz read speed

// ============= DISPLAY ROTATION =============
#define TFT_ROTATION  0  // 0=normal, 1=90°, 2=180°, 3=270°

// ============= TOUCH CONFIGURATION (if not using touch, leave as is) =============
// #define TOUCH_CS   // Not used

// ============= COLOR DEPTH =============
#define TFT_RGB565     // RGB565 16-bit color

// ============= BUFFER OPTIONS =============
#define SMOOTH_FONT   // Use smooth fonts

// ============= LIBRARY SPECIFIC OPTIONS =============
#define LOAD_GLCD      // 1. Font GLCD
#define LOAD_FONT2     // 2. Font 2
#define LOAD_FONT4     // 4. Font 4 (16 pixel height)
#define LOAD_FONT6     // 6. Font 6 (24 pixel height)
#define LOAD_GFXFF     // Allow Graphics Fonts

// ============= PERFORMANCE OPTIONS =============
#define USE_DMA_TO_TFT  // Use DMA for faster transfers (optional)

#endif // USER_SETUP_H_
