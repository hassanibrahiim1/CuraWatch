# CuraWatch - TFT Display Quick Reference

## Pin Connection Summary
```
ESP32       LCD Display
GPIO15 ───→ CS
GPIO2  ───→ RES
GPIO4  ───→ A0/DC
GPIO23 ───→ SDA/MOSI
GPIO18 ───→ SCK
3.3V   ───→ VCC
GND    ───→ GND
```

## What You Need to Do Before Uploading

1. **Install TFT_eSPI Library**
   - Arduino IDE → Sketch → Include Library → Manage Libraries
   - Search "TFT_eSPI" by Bodmer
   - Click Install

2. **Copy User_Setup.h**
   - The file `User_Setup.h` is already in your project folder
   - It has the correct configuration for your HW setup

3. **Upload the Sketch**
   - Your modified `CuraWatch_Combined.ino` now includes TFT support
   - All display functions are already integrated

## Display Layout (240x240 Round LCD)

```
        ┌─────────────────┐
        │    HR: 75 bpm   │  ← Top Center (Heart Rate)
        │   (Green box)   │
        └─────────────────┘
              /         \
             /           \
    ┌──────────┐    ┌──────────┐
    │ SPO2: 97%│    │ STEPS: 2847
    │(Green box)    │(Cyan box)│
    └──────────┘    └──────────┘
        
    ─────────────────────────────
    Sensor: OK
```

## Display Data Updates

- **Heart Rate (HR)**
  - Range: 0-180+ bpm
  - Updates when finger detected
  - Color changes based on safety level

- **Blood Oxygen (SpO2)**
  - Range: 0-100%
  - Updates every sample
  - Color indicates saturation level

- **Steps**
  - Counts from 0 upward
  - Updates from accelerometer
  - Cyan color

## Color Meanings

### Heart Rate Colors
- 🔵 Blue: Below 60 bpm (Low)
- 🟢 Green: 60-100 bpm (Normal)
- 🟡 Yellow: 100-120 bpm (Elevated)
- 🔴 Red: Above 120 bpm (High)

### SpO2 Colors
- 🟢 Green: 95%+ (Normal)
- 🟡 Yellow: 90-95% (Acceptable)
- 🟠 Orange: 85-90% (Low)
- 🔴 Red: Below 85% (Critical)

### Error States
- 🔴 "No Finger" - Finger not on MAX30102 sensor
- ⚫ Gray - No valid sensor data

## If Display Doesn't Work

### Black screen or nothing appears:
1. Check power connections (3.3V and GND)
2. Check pin connections with above table
3. Look at Serial Monitor - it will show initialization messages
4. Press RST button on ESP32

### Display is very dim or flickering:
1. Check ground connection
2. Add bigger capacitor (10µF) on power line
3. Check wire quality

### Garbled text:
1. Lower SPI frequency in User_Setup.h to 20MHz
2. Check SCK and MOSI pin connections

### Partial display or wrong orientation:
1. Check TFT_ROTATION in User_Setup.h (should be 0)
2. Verify RST pin connection

## Serial Monitor Messages

When you upload, you should see:
```
=== CuraWatch Combined System Starting ===

Initializing MAX30102 Heart Rate Sensor...
✓ MAX30102 initialized successfully

Initializing MPU6050 Accelerometer/Gyroscope...
✓ MPU6050 initialized successfully

=== All Sensors Ready ===

Initializing TFT Display...
✓ TFT Display initialized successfully
```

If you see errors, check:
- Sensor I2C connections
- TFT power and pin connections
- Library installations

## Adjusting Display Refresh Rate

In CuraWatch_Combined.ino, line ~43:
```cpp
#define SAMPLING 100  // Higher = slower updates
```

- Value of 50 = faster updates (more power use)
- Value of 100 = medium (default, balanced)
- Value of 200 = slower updates (less power use)

## Next Steps

1. ✓ Install TFT_eSPI library
2. ✓ Verify hardware connections
3. Upload the sketch
4. Check Serial Monitor for success messages
5. Verify data displays on LCD

---
See `TFT_DISPLAY_SETUP.md` for detailed troubleshooting and configuration options.
