# TFT Display Integration Guide for CuraWatch

## Hardware Setup - GC9A01 1.28" Round LCD Display

### Pin Connections (ESP32 ↔ LCD Display)
```
ESP32 Pin    →    LCD Pin
GPIO15 (D15) →    CS  (Chip Select)
GPIO2  (D2)  →    RES (Reset)
GPIO4  (D4)  →    A0  (Data/Command)
GPIO23 (D23) →    SDA (MOSI/Data)
GPIO18 (D18) →    SCK (Clock)
3.3V         →    VCC (Power)
GND          →    GND (Ground)
```

### Important Notes:
- Make sure to use 3.3V from ESP32, NOT 5V
- Add a 0.1µF capacitor between VCC and GND for power stabilization
- Add 10k pull-up resistors on CS and DC pins (optional but recommended)
- The LCD is write-only, so MISO is not connected

## Software Setup

### 1. Install TFT_eSPI Library
The sketch now includes support for the GC9A01 display using the TFT_eSPI library.

**Method 1: Using Arduino IDE Library Manager (Recommended)**
1. Open Arduino IDE
2. Go to **Sketch** → **Include Library** → **Manage Libraries**
3. Search for "TFT_eSPI" by Bodmer
4. Click **Install** (install the latest version)

**Method 2: Manual Installation**
1. Download the latest TFT_eSPI from: https://github.com/Bodmer/TFT_eSPI
2. Unzip and copy the folder to your Arduino libraries folder
3. Restart Arduino IDE

### 2. Configure TFT_eSPI for GC9A01
Two methods are available:

**Method A: Using User_Setup.h (Automatic)**
A `User_Setup.h` file has been provided in this project directory. When you compile:
- The Arduino IDE will use this configuration file
- All pin definitions are already set correctly

**Method B: Manual Configuration (If Method A doesn't work)**
1. Navigate to your TFT_eSPI library folder:
   - Windows: `Documents\Arduino\libraries\TFT_eSPI\`
   - Mac: `~/Documents/Arduino/libraries/TFT_eSPI/`
   - Linux: `~/Arduino/libraries/TFT_eSPI/`

2. Open `User_Setup.h` in a text editor

3. Find and modify these lines:
```cpp
// Uncomment driver
#define GC9A01_DRIVER

// Set dimensions
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// Set pin definitions
#define TFT_CS    15      // GPIO15
#define TFT_DC     4      // GPIO4 (A0)
#define TFT_RST    2      // GPIO2
#define TFT_MOSI  23      // GPIO23 (SDA)
#define TFT_SCLK  18      // GPIO18 (SCK)

// Set SPI frequency
#define SPI_FREQUENCY  40000000

// Set rotation
#define TFT_ROTATION  0
```

### 3. Install Required Dependencies
The following libraries should already be installed:
- **Wire.h** - Built-in I2C library (for MAX30102 & MPU6050)
- **WiFi.h** - Built-in WiFi library (for ESP32)
- **TFT_eSPI.h** - Install from Library Manager (for LCD display)

Make sure you also have the following for sensors:
- Adafruit_MPU6050
- Adafruit_Sensor

## Display Features

### What's Displayed on the LCD:
The 240x240 round display shows three metrics in a triangular arrangement:

**Top Center: Heart Rate (HR)**
- Shows beats per minute (bpm)
- Color indicator:
  - 🔵 Blue: Low HR (< 60 bpm)
  - 🟢 Green: Normal (60-100 bpm)
  - 🟡 Yellow: Elevated (100-120 bpm)
  - 🔴 Red: High (> 120 bpm)

**Bottom Left: Blood Oxygen Saturation (SpO2)**
- Shows oxygen saturation percentage (%)
- Color indicator:
  - 🟢 Green: Normal (≥ 95%)
  - 🟡 Yellow: Acceptable (≥ 90%)
  - 🟠 Orange: Low (≥ 85%)
  - 🔴 Red: Critical (< 85%)

**Bottom Right: Step Count**
- Shows total steps counted
- 🔵 Cyan color indicator

### Status Bar:
- Bottom of display shows "Sensor: OK" status
- Will display "No Finger" message if finger is not detected on MAX30102

## Troubleshooting

### Display not showing anything:
1. **Check power**: Verify 3.3V is connected to display VCC
2. **Check pin connections**: Compare your wiring with the pin connection table above
3. **Check User_Setup.h**: Ensure the file is being used (put it in the sketch directory)
4. **Reset ESP32**: Press the RST button on the board

### Display shows only white/noisy screen:
1. Reduce SPI frequency from 40MHz to 10MHz in User_Setup.h:
   ```cpp
   #define SPI_FREQUENCY  10000000
   ```
2. Check for loose wires - especially SCK and MOSI
3. Verify capacitor on power is installed

### Serial shows data but display is blank:
1. Check TFT_RST pin connection (GPIO2)
2. Try pressing Reset button on ESP32
3. Verify rotation setting is correct (TFT_ROTATION 0)

### Display flickers:
1. Add a larger capacitor (10µF) on the VCC line
2. Ensure good ground connection
3. Keep data lines short and away from power lines

### Text appears garbled:
1. Check SPI frequency - try lowering to 20MHz
2. Verify TFT_MOSI (GPIO23) and TFT_SCLK (GPIO18) pins

## Performance Notes

- LCD updates every 5 samples (SAMPLING = 100 / 20 updates)
- Display refresh rate: ~2-5 Hz (optimized to avoid overwhelming the display)
- SPI frequency: 40MHz (safe for GC9A01)
- All sensor readings continue in the background

## Code Structure

### Display Functions in CuraWatch_Combined.ino:
- `initializeDisplay()` - Initialize TFT and show splash screen
- `updateTFTDisplay()` - Main display refresh function
- `drawMetricBox()` - Draw individual metric boxes
- `getHRColor()` - Determine HR color based on value
- `getSPO2Color()` - Determine SpO2 color based on value
- `displayNoFingerScreen()` - Show "No Finger Detected" message
- `drawStatusBar()` - Draw bottom status bar

## Next Steps

1. Verify all connections match the pin table above
2. Install TFT_eSPI library from Library Manager
3. Copy `User_Setup.h` to your project directory
4. Upload the sketch to your ESP32
5. Check Serial Monitor for initialization messages
6. Check the LCD display for sensor data

## Additional Resources

- **TFT_eSPI GitHub**: https://github.com/Bodmer/TFT_eSPI
- **GC9A01 Datasheet**: Search for "GC9A01 datasheet" online
- **ESP32 Pin Reference**: https://randomnerdtutorials.com/esp32-pinout-reference-which-gpio-pins-are-safe-to-use/

---
**Last Updated**: February 2026
**Compatible with**: Arduino IDE 2.x, ESP32 Board Package 2.0+
