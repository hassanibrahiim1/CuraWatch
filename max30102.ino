// Tinker Foundry
// Accompanying video: https://youtu.be/xjwSKy6jzTI
// Code for ESP32 to interface with MAX30102 breakout board and report blood oxygen level and heart rate
// Refer to https://github.com/DKARDU/bloodoxygen - below code is simplified version
// https://www.analog.com/media/en/technical-documentation/data-sheets/MAX30102.pdf
// Connections from WEMOS D1 R32 board to MAX30102 Breakout Board as follows:
//  SCL (ESP32) to SCL (breakout board)
//  SDA (ESP32) to SDA (breakout board)
//  3V3 (ESP32) to VIN (breakout board)
//  GND (ESP32) to GND (breakout board)

// This file contains helper functions for MAX30102 sensor
// Main setup() and loop() are in CuraWatch_Combined.ino

#include <Wire.h>
#include "MAX30102Sensor.h"
#include "HeartRateMonitor.h"

// Helper functions can be added here if needed
// Note: Global variables and main setup()/loop() are defined in CuraWatch_Combined.ino