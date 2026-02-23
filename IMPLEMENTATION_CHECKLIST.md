# ✓ CuraWatch TFT Display - Implementation Checklist

## Pre-Installation Checklist

### Hardware Verification
- [ ] **GC9A01 1.28" Round LCD Display** - Confirmed you have this specific module
- [ ] **7 Jumper Wires** - For LCD connections (Female-to-Female recommended)
- [ ] **0.1µF Capacitor** (optional) - For power stability on VCC line
- [ ] **Soldering Iron** (if needed) - For securing connections
- [ ] **Multimeter** (optional) - For verifying connections

### Display Module Check
- [ ] Display powers on when 3.3V is applied (look for backlight)
- [ ] No visible damage to display or pins
- [ ] All 7 pins visible and not bent

### ESP32 Board Check
- [ ] ESP32 board is programmable (can upload existing code)
- [ ] All pins GPIO2, 4, 15, 18, 23 are accessible
- [ ] No pins are damaged or soldered previously

---

## Hardware Setup (Step by Step)

### Step 1: Disconnect Power
- [ ] Disconnect ESP32 from USB

### Step 2: Connect LCD Display Wires
Following [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md):

- [ ] **GPIO15** (D15) → LCD **CS** pin
- [ ] **GPIO2** (D2) → LCD **RES** pin
- [ ] **GPIO4** (D4) → LCD **A0** (DC) pin
- [ ] **GPIO23** (D23) → LCD **SDA** (MOSI) pin
- [ ] **GPIO18** (D18) → LCD **SCK** pin
- [ ] **3.3V** → LCD **VCC** pin
- [ ] **GND** → LCD **GND** pin

### Step 3: Double-Check All Connections
- [ ] Each wire is the right color (if using color-coded)
- [ ] No wires are touching (no short circuits)
- [ ] All connections are firm (gently tug each wire)
- [ ] LCD display is securely positioned
- [ ] Capacitor installed on VCC (if using one)

### Step 4: Power Check (Optional)
- [ ] Use multimeter to verify 3.3V between VCC and GND
- [ ] Verify no shorts between adjacent pins

---

## Software Setup (Step by Step)

### Step 1: Install TFT_eSPI Library
- [ ] Open Arduino IDE
- [ ] Go to **Sketch** → **Include Library** → **Manage Libraries**
- [ ] Search: `TFT_eSPI`
- [ ] Author: `Bodmer`
- [ ] Click **Install**
- [ ] Wait for installation to complete
- [ ] Close Library Manager

### Step 2: Verify Files in Project Directory
- [ ] [CuraWatch_Combined.ino](CuraWatch_Combined.ino) - Modified with TFT code ✓
- [ ] [User_Setup.h](User_Setup.h) - Configuration file ✓
- [ ] All other `.cpp` and `.h` files present

### Step 3: Board Selection
- [ ] Open Arduino IDE
- [ ] Go to **Tools** → **Board**
- [ ] Select **ESP32 Dev Module** (or your specific ESP32 board)
- [ ] Go to **Tools** → **Port**
- [ ] Select your COM port (where ESP32 is connected)

### Step 4: Verify Code Compiles
- [ ] Plug in ESP32 via USB
- [ ] Click **Sketch** → **Verify/Compile**
- [ ] Wait for compilation
- [ ] Should see: "Compilation complete" (no errors)
- [ ] If errors: Check TFT_eSPI is installed

### Step 5: Upload Code
- [ ] Click **Upload** button (or Sketch → Upload)
- [ ] LED on ESP32 should blink during upload
- [ ] Wait for "Upload complete" message
- [ ] If upload fails: Try again or select correct COM port

---

## First Run Checklist

### Step 1: Check Serial Monitor
- [ ] Open **Tools** → **Serial Monitor**
- [ ] Set baud rate to **115200**
- [ ] Should see initialization messages:

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

### Step 2: Check Display
- [ ] Display shows "CuraWatch Initializing..." briefly
- [ ] After ~2 seconds, switches to sensor data
- [ ] Black background with 3 colored boxes visible
- [ ] No flickering or white screen

### Step 3: Test Sensor Data
- [ ] **Heart Rate:** Show 0 bpm until finger placed on MAX30102
- [ ] **SpO2:** Should show realistic percentage (95-100% when stable)
- [ ] **Steps:** Should increment when you move/walk
- [ ] Colors change based on values

### Step 4: Test No-Finger Detection
- [ ] Remove finger from MAX30102 sensor
- [ ] Display should show "No Finger" message in red
- [ ] Place finger back on sensor
- [ ] Should return to normal display

---

## Troubleshooting Checklist

### If Display Shows Nothing (Black Screen)

**Check 1: Power**
- [ ] Verify 3.3V on LCD VCC pin with multimeter
- [ ] Verify GND connection is solid
- [ ] Try pressing ESP32 RST button

**Check 2: Reset Pin (D2)**
- [ ] Verify GPIO2 is connected to RES pin
- [ ] Try disconnecting and reconnecting this wire
- [ ] Check for loose connection

**Check 3: CS Pin (D15)**
- [ ] Verify GPIO15 is connected to CS pin
- [ ] Check for loose connection

**If Still Not Working:**
- [ ] Power down completely
- [ ] Re-check all 7 connections against [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md)
- [ ] Try different USB power source (may need more current)

### If Display Shows White/Static

**Check 1: DC Pin (D4)**
- [ ] Verify GPIO4 is connected to A0 (DC) pin
- [ ] This pin controls Data vs Command mode

**Check 2: SPI Pins (D18, D23)**
- [ ] Verify GPIO18 (SCK) is properly connected
- [ ] Verify GPIO23 (MOSI/SDA) is properly connected
- [ ] No loose wires

**It Lower SPI Speed:**
- [ ] Open [User_Setup.h](User_Setup.h)
- [ ] Find line: `#define SPI_FREQUENCY 40000000`
- [ ] Change to: `#define SPI_FREQUENCY 10000000`
- [ ] Save and re-upload

### If Display Flickers

**Check 1: Power Stability**
- [ ] Verify 0.1µF capacitor is installed on VCC-GND
- [ ] Try adding 10µF capacitor instead
- [ ] Check USB power is adequate

**Check 2: Ground Connection**
- [ ] Verify GND wire is securely connected
- [ ] Try using second GND wire for redundancy

**Check 3: Wiring Quality**
- [ ] Check all wires are high-quality (not cheap jumpers)
- [ ] Keep wires short and away from power lines
- [ ] Use shortest possible cable runs

### If Serial Shows Errors

**Error: TFT_eSPI not found**
- [ ] ReInstall TFT_eSPI library
- [ ] Restart Arduino IDE after installation
- [ ] Check spelling of #include <TFT_eSPI.h>

**Error: MAX30102 not found**
- [ ] Check I2C sensor connections (not LCD related)
- [ ] This is separate from LCD issues

**Error: Board not recognized**
- [ ] Install ESP32 board package: https://github.com/espressif/arduino-esp32
- [ ] Restart Arduino IDE

---

## Verification Tests (After Successful Upload)

### Test 1: Display Orientation
- [ ] Numbers and text are right-side up
- [ ] If upside down, change TFT_ROTATION in User_Setup.h
- [ ] Try values: 0, 1, 2, 3 (0-360°)

### Test 2: Color Verification
- [ ] HR box is GREEN when HR ~70 bpm (normal)
- [ ] SpO2 box is GREEN when SpO2 ~98% (normal)
- [ ] Steps box is always CYAN
- [ ] Status bar is WHITE text on BLACK background

### Test 3: Responsiveness
- [ ] Placing finger on MAX30102: HR appears within 5 seconds
- [ ] Removing finger: "No Finger" message within 1 second
- [ ] Walking/moving: Step count increments

### Test 4: Serial Output
- [ ] Serial Monitor shows sensor data every 2 seconds
- [ ] Data in Serial matches what's on display
- [ ] No error messages

### All Tests Passed?
- [ ] ✓ Congratulations! Your display is working perfectly
- [ ] You can now wear/use the CuraWatch with full LCD functionality

---

## Files Reference

| File | Purpose | Edit? |
|------|---------|-------|
| [CuraWatch_Combined.ino](CuraWatch_Combined.ino) | Main sketch | No (already updated) |
| [User_Setup.h](User_Setup.h) | TFT configuration | Yes (if troubleshooting) |
| [QUICK_START.md](QUICK_START.md) | Quick guide | Reference |
| [TFT_DISPLAY_SETUP.md](TFT_DISPLAY_SETUP.md) | Detailed guide | Reference |
| [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md) | Pin connections | Reference |
| [INTEGRATION_SUMMARY.md](INTEGRATION_SUMMARY.md) | Full details | Reference |

---

## Next Steps After Verification

Once display is working:

1. **Customize Display Update Rate**
   - Edit `SAMPLING` value in CuraWatch_Combined.ino (line 43)
   - Higher = slower, Lower = faster

2. **Add More Metrics** (optional)
   - Edit `updateTFTDisplay()` function
   - Add more `drawMetricBox()` calls
   - Modify metric positions

3. **Customize Colors** (optional)
   - Edit `getHRColor()` and `getSPO2Color()` functions
   - Change threshold values

4. **Test Battery Life** (optional future)
   - Reduce update frequency
   - Implement sleep modes

---

## Emergency Contacts

If stuck:
1. Check [TFT_DISPLAY_SETUP.md](TFT_DISPLAY_SETUP.md) troubleshooting section
2. Look at [WIRING_DIAGRAM.md](WIRING_DIAGRAM.md) for pin connections
3. Verify Serial Monitor output as per First Run Checklist
4. Re-read this list for your specific issue

---

**Print this page for reference!**

Last Updated: February 2026  
Compatible with: Arduino IDE 2.x, ESP32, TFT_eSPI 2.4.x+
