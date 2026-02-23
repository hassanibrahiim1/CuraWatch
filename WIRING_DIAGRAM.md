# GC9A01 LCD Display - Wiring Diagram

## Simplified Wiring Diagram

```
                    ┌─────────────────────────────────┐
                    │    GC9A01 1.28" Round LCD      │
                    │      (240 x 240 Display)        │
                    │                                 │
                    │  Pin Layout (from back):        │
                    │  ┌──┬──┬──┬──┬──┬──┬──┐         │
                    │  │CS│RES│A0│SDA│SCK│VCC│GND│  │
                    │  └──┴──┴──┴──┴──┴──┴──┘         │
                    └────┬───┬───┬────┬────┬──┬───┘
                         │   │   │    │    │  │
                         │   │   │    │    │  │
        GPIO15 ───────────→  │   │    │    │  │
        (CS)                 │   │    │    │  │
                             │   │    │    │  │
        GPIO2 ───────────────→   │    │    │  │
        (RES/RESET)              │    │    │  │
                                 │    │    │  │
        GPIO4 ──────────────────→ │    │    │  │
        (A0/DC)                   │    │    │  │
                                  │    │    │  │
        GPIO23 ────────────────────→   │    │  │
        (SDA/MOSI)               │    │  │
                                 │    │  │
        GPIO18 ──────────────────────→  │  │
        (SCK/CLK)                     │  │
                                      │  │
        3.3V ────────────────────────────→  │
        (VCC)                            │
                                         │
        GND ──────────────────────────────→
        (Ground)
```

## Connection Table

```
┌────────────────────────────────────────────────┐
│         ESP32 ↔ GC9A01 Display Pins           │
├────────────────────────────────────────────────┤
│ ESP32 GPIO  │  LCD Pin  │  Function           │
├─────────────┼───────────┼─────────────────────┤
│   GPIO15    │    CS     │  Chip Select        │
│   GPIO2     │    RES    │  Reset              │
│   GPIO4     │    A0     │  Data/Command (DC)  │
│   GPIO23    │    SDA    │  MOSI/Data          │
│   GPIO18    │    SCK    │  Clock              │
│   3.3V      │    VCC    │  Power Supply       │
│   GND       │    GND    │  Ground             │
└────────────────────────────────────────────────┘
```

## Development Board Pin Reference

### ESP32 DevKit v1 Pin Layout

```
                           USB (Power & Program)
                                  ↓
         ┌───────────────────────────────────────────────┐
         │                                               │
    GND  ├─ ⚫                                      ⚫ ─┤ 3.3V
    GPIO23├─ ⚫  SDA (LCD)                        ⚫ ─┤ EN
    GPIO22├─ ⚫                                      ⚫ ─┤ SVP
    GPIO21├─ ⚫                                      ⚫ ─┤ SVN
    GPIO19├─ ⚫                                      ⚫ ─┤ GPIO34
    GPIO18├─ ⚫  SCK (LCD) ←────────────┬─────────→ ⚫ ─┤ GPIO35
    GPIO5 ├─ ⚫                         │             ⚫ ─┤ GPIO32
    GPIO17├─ ⚫                         │             ⚫ ─┤ GPIO33
    GPIO16├─ ⚫                         │             ⚫ ─┤ GPIO25
    GPIO4 ├─ ⚫  A0/DC (LCD)           │             ⚫ ─┤ GPIO26
    GPIO0 ├─ ⚫                         │             ⚫ ─┤ GPIO27
    GPIO2 ├─ ⚫  RES (LCD)            │             ⚫ ─┤ GPIO14
    GPIO15├─ ⚫  CS (LCD) ←────────────┴─────────→ ⚫ ─┤ GPIO13
    GPIO8 ├─ ⚫                                      ⚫ ─┤ GPIO12
    GPIO7 ├─ ⚫                                      ⚫ ─┤ GPIO11
    GPIO6 ├─ ⚫                                      ⚫ ─┤ GND
    GND   ├─ ⚫                                      ⚫ ─┤ 3.3V
         │                                               │
         └───────────────────────────────────────────────┘

        ⚫ = GPIO Pin Position
        LCD Pins Highlighted
```

## Sensor Connection Reminder

### MAX30102 (Heart Rate & SpO2) - I2C
```
ESP32               MAX30102
  SDA (GPIO21) ────→ SDA
  SCL (GPIO22) ────→ SCL
  3.3V         ────→ 3V3
  GND          ────→ GND
```

### MPU6050 (Accelerometer & Gyro) - I2C
```
ESP32               MPU6050
  SDA (GPIO21) ────→ SDA
  SCL (GPIO22) ────→ SCL
  3.3V         ────→ 3V3
  GND          ────→ GND
```

### GC9A01 Display - SPI
```
ESP32               GC9A01
  GPIO15 ──────────→ CS (Chip Select)
  GPIO2  ──────────→ RES (Reset)
  GPIO4  ──────────→ A0 (Data/Command)
  GPIO23 ──────────→ SDA (MOSI - Data)
  GPIO18 ──────────→ SCK (Clock)
  3.3V   ──────────→ VCC (Power)
  GND    ──────────→ GND (Ground)
```

## All Connections at a Glance

```
┌──────────────────────────────────────────────────────────────┐
│                        ESP32 DevKit                          │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  I2C Bus (GPIO21 SDA, GPIO22 SCL) for sensors:             │
│  ├─→ MAX30102 (HR & SpO2)  [Addr: 0x57]                    │
│  └─→ MPU6050 (Steps)       [Addr: 0x68]                    │
│                                                              │
│  SPI Bus (GPIO23 MOSI, GPIO18 CLK) for LCD:                │
│  └─→ GC9A01 Display        [GPIO15 CS, GPIO4 DC, GPIO2 RST]│
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

## Cable Requirements

- **7x Dupont/Jumper Wires** (for LCD display)
- **4x Dupont/Jumper Wires** (already used for MAX30102)
- **4x Dupont/Jumper Wires** (already used for MPU6050)

**Total: 15 wires** (some GND/VCC might be shared)

## Optional but Recommended

```
        VCC (3.3V) ──┬──→ LCD VCC
                     │
                   ===  0.1µF Capacitor
                     │
                    GND ──→ LCD GND
                     
    (Provides power stability to the LCD)
```

## Testing Connections

After wiring, verify with a multimeter:
- **D15 (CS):** Should have ~3.3V when inactive
- **D2 (RES):** Should have ~3.3V (pulled high)
- **D4 (DC):** Should toggle 0V-3.3V during operation
- **D23 (MOSI):** Should toggle 0V-3.3V during operation  
- **D18 (SCK):** Should have clock pulses during operation
- **VCC:** Should read 3.3V
- **GND:** Should be 0V (reference point)

## Common Temperature Readings

The display operates best at:
- **Temperature:** 0°C to 50°C
- **Humidity:** 20% to 80% (no condensation)
- **Typical Operating Current:** 80-100mA at 3.3V

## Troubleshooting Connection Issues

| Symptom | Likely Cause | Check |
|---------|-------------|-------|
| No display power | Wrong voltage | Verify 3.3V on VCC |
| Display is white | RST (D2) issue | Check GPIO2 connection |
| Display is black | Power OK but blank | Check DC (D4) connection |
| Garbled display | SPI timing issue | Check GPIO18 & 23 |
| Only partial display | Data sync issue | Verify all SPI pins |
| Screen resets constantly | Power delivery issue | Add capacitor to VCC |

---

This visual guide should help you understand the physical connections needed for your GC9A01 display with the ESP32.
