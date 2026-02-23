// CuraWatch - Combined Project
// Combines MAX30102 (Heart Rate & SpO2) and MPU6050 (Step Detection)
// Both sensors communicate via I2C bus
//
// I2C Connections (shared bus):
//  SCL (ESP32/Arduino) to SCL (both sensors)
//  SDA (ESP32/Arduino) to SDA (both sensors)
//  3V3 to VIN (both sensors)
//  GND to GND (both sensors)
//
// I2C Addresses:
//  MAX30102: 0x57 (default)
//  MPU6050: 0x68 (default)

#include <Wire.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include "MAX30102Sensor.h"
#include "HeartRateMonitor.h"
#include "StepDetector.h"

// ============= TFT DISPLAY SETUP =============
TFT_eSPI tft = TFT_eSPI();

// Pin definitions for GC9A01 (1.28" Round LCD)
#define TFT_CS    15   // Chip Select (D15)
#define TFT_RST    2   // Reset (D2)
#define TFT_DC     4   // Data/Command (A0 - D4)
#define TFT_MOSI  23   // SDA (D23)
#define TFT_CLK   18   // SCK (D18)

// Display dimensions
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 240

// ============= SENSOR INSTANCES =============
MAX30102Sensor heartRateSensor;
HeartRateMonitor hrMonitor;
StepDetector stepDetector;

// ============= CONFIGURATION =============
#define TIMETOBOOT 3000    // Wait for this time (msec) to output SpO2
#define SAMPLING   100     // Display rate - higher = less frequent
#define USEFIFO            // Use FIFO mode for MAX30102
#define UPDATE_INTERVAL 200 // Update interval in milliseconds

// ============= GLOBAL VARIABLES =============
static int sampleCounter = 0;
static int displayCounter = 0;
static unsigned long lastUpdateTime = 0;
static unsigned long lastWiFiScanTime = 0;

// ============= WiFi KNOWN APs =============
struct KnownAP {
  const char* name;
  const char* macAddress;
};

const KnownAP knownAPs[] = {
  {"Hassan", "6a:bb:30:8c:73:4f"},
  {"Fatma", "ee:27:8a:bf:0a:65"},
  {"Router", "58:d7:59:6c:fa:d0"}
};

const int NUM_KNOWN_APS = sizeof(knownAPs) / sizeof(knownAPs[0]);

#define WiFi_SCAN_INTERVAL 5000  // Scan WiFi every 5 seconds

// ============= SETUP =============
void setup()
{
  // Initialize Serial
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(1000);
  Serial.println("\n\n=== CuraWatch Combined System Starting ===\n");

  // Initialize I2C (call only once for both sensors!)
  Wire.begin();
  delay(500);

  // Initialize MAX30102 Heart Rate Sensor
  Serial.println("Initializing MAX30102 Heart Rate Sensor...");
  int maxAttempts = 5;
  while (!heartRateSensor.begin() && maxAttempts-- > 0) {
    Serial.println("  MAX30102 not found. Retrying...");
    delay(1000);
  }
  
  if (maxAttempts <= 0) {
    Serial.println("ERROR: MAX30102 failed to initialize!");
  } else {
    Serial.println("✓ MAX30102 initialized successfully");
  }

  delay(500);

  // Initialize MPU6050 Step Detector
  Serial.println("Initializing MPU6050 Accelerometer/Gyroscope...");
  if (!stepDetector.initialize()) {
    Serial.println("ERROR: MPU6050 failed to initialize!");
  } else {
    Serial.println("✓ MPU6050 initialized successfully");
  }

  delay(500);
  Serial.println("\n=== All Sensors Ready ===\n");
  delay(2500);

  // Initialize TFT Display
  Serial.println("Initializing TFT Display...");
  initializeDisplay();
  Serial.println("✓ TFT Display initialized successfully\n");

  // Initialize WiFi in station mode (scan only, no connection)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  // Turn off auto-connect
  delay(100);
  Serial.println("\n=== WiFi Scanner Ready ===");
  Serial.println("Known Access Points (Whitelist):");
  for (int i = 0; i < NUM_KNOWN_APS; i++) {
    Serial.print("  ");
    Serial.print(knownAPs[i].name);
    Serial.print(": ");
    Serial.println(knownAPs[i].macAddress);
  }
  Serial.println();
}

// ============= MAIN LOOP =============
void loop()
{
  // Handle MAX30102 data (continuous)
  #ifdef USEFIFO
  heartRateSensor.check();

  while (heartRateSensor.hasData()) {
    uint32_t red, ir;
    heartRateSensor.readSensor(red, ir);
    
    sampleCounter++;
    displayCounter++;
    
    // Update sensor data and SpO2 calculation
    heartRateSensor.updateSpO2(red, ir);
    
    // Heart rate detection
    hrMonitor.detectBeat(ir);
    
    if (sampleCounter >= 100) {
      sampleCounter = 0;
    }
  }
  #endif

  // Handle MPU6050 data at specified interval
  unsigned long currentTime = millis();
  if (currentTime - lastUpdateTime >= UPDATE_INTERVAL) {
    stepDetector.update();
    lastUpdateTime = currentTime;
  }

  // Display combined sensor data at reduced rate
  if ((displayCounter % SAMPLING) == 0) {
    if (millis() > TIMETOBOOT) {
      displayCombinedData();
      updateTFTDisplay();  // Update TFT display
    }
    displayCounter = 0;
  }

  // Perform WiFi scan at specified interval
  if (millis() - lastWiFiScanTime >= WiFi_SCAN_INTERVAL) {
    findClosestKnownAP();
    lastWiFiScanTime = millis();
  }
}

// ============= DISPLAY FUNCTIONS =============
/**
 * Display combined data from both sensors
 */
void displayCombinedData()
{
  Serial.println("\n========== SENSOR DATA ==========");
  
  // Display Heart Rate & SpO2 Data
  displayHeartRateData();
  
  Serial.println("---");
  
  // Display Step Counter & Motion Data
  displayStepData();
  
  Serial.println("================================\n");
}

/**
 * Display Heart Rate and SpO2 data from MAX30102
 */
void displayHeartRateData()
{
  Serial.println("[HEART RATE & OXYGEN SENSOR]");
  
  if (!heartRateSensor.isFingerDetected()) {
    Serial.println("  Status: No finger detected");
    hrMonitor.reset();
    return;
  }
  
  Serial.print("  SpO2: ");
  Serial.print(heartRateSensor.getFilteredSpO2());
  Serial.println("%");
  
  if (hrMonitor.isValidHR()) {
    Serial.print("  Current HR: ");
    Serial.print(hrMonitor.getBeatsPerMinute(), 1);
    Serial.println(" bpm");
    
    Serial.print("  Average HR: ");
    Serial.print(hrMonitor.getAverageHR());
    Serial.println(" bpm");
  } else {
    Serial.println("  HR Status: Calculating...");
  }
  
  // Display temperature
  Serial.print("  Sensor Temp: ");
  Serial.print(heartRateSensor.getTemperature());
  Serial.println("°F");
}

/**
 * Display step count and motion data from MPU6050
 */
void displayStepData()
{
  Serial.println("[MOTION & STEP COUNTER]");
  
  Serial.print("  Step Count: ");
  Serial.println(stepDetector.getStepCount());
  
  Serial.print("  Acceleration Magnitude: ");
  Serial.print(stepDetector.getAccelMagnitude());
  Serial.println(" m/s²");
  
  Serial.print("  Pitch: ");
  Serial.print(stepDetector.getPitch());
  Serial.println("°");
  
  Serial.print("  Roll: ");
  Serial.print(stepDetector.getRoll());
  Serial.println("°");
  
  Serial.print("  Gyro Magnitude: ");
  Serial.print(stepDetector.getGyroMagnitude());
  Serial.println(" rad/s");
}

/**
 * Alternative simplified display (optional - single line output)
 */
void displaySimplifiedData()
{
  Serial.print("HR: ");
  if (hrMonitor.isValidHR()) {
    Serial.print(hrMonitor.getAverageHR());
    Serial.print(" bpm");
  } else {
    Serial.print("--");
  }
  
  Serial.print(" | SpO2: ");
  Serial.print(heartRateSensor.getFilteredSpO2());
  Serial.print("% | Steps: ");
  Serial.print(stepDetector.getStepCount());
  Serial.print(" | Accel: ");
  Serial.print(stepDetector.getAccelMagnitude());
  Serial.println(" m/s²");
}

// ============= TFT DISPLAY FUNCTIONS =============
/**
 * Initialize the TFT display (GC9A01 1.28" Round LCD)
 */
void initializeDisplay()
{
  // Configure SPI pins for ESP32
  SPI.begin(TFT_CLK, -1, TFT_MOSI, TFT_CS);  // SCK, MISO (unused), MOSI, CS
  
  // Initialize TFT_eSPI with custom pin configuration
  tft.init();
  
  // Set rotation (0=normal, 1=90°, 2=180°, 3=270°)
  tft.setRotation(0);
  
  // Clear screen with black background
  tft.fillScreen(TFT_BLACK);
  
  // Display welcome screen
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(55, 100);
  tft.println("CuraWatch");
  
  tft.setTextSize(1);
  tft.setCursor(55, 125);
  tft.println("Initializing...");
  
  delay(1500);
  tft.fillScreen(TFT_BLACK);
}

/**
 * Update the TFT display with current HR, SpO2, and Steps
 */
void updateTFTDisplay()
{
  // Only update if finger is detected
  if (!heartRateSensor.isFingerDetected()) {
    displayNoFingerScreen();
    return;
  }

  // Clear screen
  tft.fillScreen(TFT_BLACK);
  
  // Get current sensor values
  int hr = hrMonitor.isValidHR() ? hrMonitor.getAverageHR() : 0;
  float spo2 = heartRateSensor.getFilteredSpO2();
  int steps = stepDetector.getStepCount();
  
  // Define colors
  uint16_t hrColor = getHRColor(hr);
  uint16_t spo2Color = getSPO2Color(spo2);
  uint16_t stepColor = TFT_CYAN;
  
  // Draw three metric boxes in a triangular arrangement
  // Top center: Heart Rate
  drawMetricBox(120, 20, "HR", String(hr), "bpm", hrColor, 35);
  
  // Bottom left: SpO2
  drawMetricBox(40, 140, "SpO2", String((int)spo2), "%", spo2Color, 30);
  
  // Bottom right: Steps
  drawMetricBox(200, 140, "STEPS", String(steps), "", stepColor, 30);
  
  // Draw status indicators at bottom
  drawStatusBar();
}

/**
 * Draw a metric box on the display
 * x, y: position (top-left corner)
 * label: metric name
 * value: metric value
 * unit: unit of measurement
 * color: color for the box
 * fontSize: font size multiplier
 */
void drawMetricBox(int x, int y, const char* label, String value, const char* unit, uint16_t color, int fontSize)
{
  int boxWidth = 60;
  int boxHeight = 50;
  
  // Draw rounded rectangle border
  tft.drawRoundRect(x - boxWidth/2, y, boxWidth, boxHeight, 5, color);
  
  // Draw filled rectangle for label background
  tft.fillRect(x - boxWidth/2 + 2, y + 2, boxWidth - 4, 15, color);
  
  // Draw label
  tft.setTextColor(TFT_BLACK, color);
  tft.setTextSize(1);
  int labelWidth = strlen(label) * 6;
  tft.setCursor(x - labelWidth/2, y + 5);
  tft.print(label);
  
  // Draw value
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(2);
  int valueWidth = value.length() * 12;
  tft.setCursor(x - valueWidth/2, y + 20);
  tft.print(value);
  
  // Draw unit
  if (strlen(unit) > 0) {
    tft.setTextSize(1);
    int unitWidth = strlen(unit) * 6;
    tft.setCursor(x - unitWidth/2, y + 35);
    tft.print(unit);
  }
}

/**
 * Get color for heart rate based on value
 */
uint16_t getHRColor(int hr)
{
  if (hr == 0) return TFT_DARKGREY;     // No signal
  if (hr < 60) return TFT_BLUE;         // Low (bradycardia)
  if (hr <= 100) return TFT_GREEN;      // Normal
  if (hr <= 120) return TFT_YELLOW;     // Elevated
  return TFT_RED;                       // High (tachycardia)
}

/**
 * Get color for SpO2 based on value
 */
uint16_t getSPO2Color(float spo2)
{
  if (spo2 == 0) return TFT_DARKGREY;   // No signal
  if (spo2 >= 95) return TFT_GREEN;     // Normal
  if (spo2 >= 90) return TFT_YELLOW;    // Acceptable
  if (spo2 >= 85) return TFT_ORANGE;    // Low
  return TFT_RED;                       // Critical
}

/**
 * Display "No Finger Detected" screen
 */
void displayNoFingerScreen()
{
  static unsigned long lastNoFingerTime = 0;
  unsigned long currentTime = millis();
  
  // Update screen only if it hasn't been updated recently
  if (currentTime - lastNoFingerTime > 500) {
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    
    int textWidth = 8 * 12;  // Approximate width of "No Finger"
    tft.setCursor(120 - textWidth/2, 105);
    tft.print("No Finger");
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    textWidth = 13 * 6;  // Approximate width of "Place on sensor"
    tft.setCursor(120 - textWidth/2, 125);
    tft.print("Place on sensor");
    
    lastNoFingerTime = currentTime;
  }
}

/**
 * Draw status bar at the bottom
 */
void drawStatusBar()
{
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  
  // Draw a separator line
  tft.drawLine(10, 215, 230, 215, TFT_DARKGREY);
  
  // Display sensor status
  String statusText = "Sensor: OK";
  tft.setCursor(15, 220);
  tft.print(statusText);
}

// ============= WiFi SCANNING FUNCTIONS =============
/**
 * Convert MAC address bytes to string format (xx:xx:xx:xx:xx:xx)
 */
String macToString(const uint8_t* macArray) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           macArray[0], macArray[1], macArray[2], macArray[3], macArray[4], macArray[5]);
  return String(macStr);
}

/**
 * Check if a given MAC address matches any known AP in our whitelist
 */
bool isKnownAP(String macAddress, String& knownAPName) {
  for (int i = 0; i < NUM_KNOWN_APS; i++) {
    if (macAddress.equalsIgnoreCase(String(knownAPs[i].macAddress))) {
      knownAPName = String(knownAPs[i].name);
      return true;
    }
  }
  return false;
}

/**
 * Scan for WiFi networks and find the closest known AP
 * Filters results by MAC address whitelist and returns the strongest signal
 */
void findClosestKnownAP() {
  Serial.println("\n========== WiFi SCAN ==========");
  Serial.println("Scanning for available networks...");
  
  // Start WiFi scan (non-blocking)
  int numNetworks = WiFi.scanNetworks();
  
  if (numNetworks == 0) {
    Serial.println("No networks found.");
    Serial.println("================================\n");
    return;
  }
  
  Serial.print("Found ");
  Serial.print(numNetworks);
  Serial.println(" networks total.");
  
  // Variables to track the closest known AP
  int closestRSSI = -200;  // Start with worst possible signal
  String closestMAC = "";
  String closestSSID = "";
  String closestName = "";
  bool foundKnownAP = false;
  
  // Loop through scan results
  Serial.println("\nProcessing scan results:");
  for (int i = 0; i < numNetworks; i++) {
    // Get network info
    String ssid = WiFi.SSID(i);
    int32_t rssi = WiFi.RSSI(i);
    uint8_t* bssid = WiFi.BSSID(i);
    String macAddress = macToString(bssid);
    
    // Check if this MAC is in our whitelist
    String knownAPName;
    if (isKnownAP(macAddress, knownAPName)) {
      foundKnownAP = true;
      Serial.print("  ✓ KNOWN: ");
      Serial.print(knownAPName);
      Serial.print(" (");
      Serial.print(ssid);
      Serial.print(") - MAC: ");
      Serial.print(macAddress);
      Serial.print(" - RSSI: ");
      Serial.print(rssi);
      Serial.println(" dBm");
      
      // Check if this is the strongest signal found so far
      if (rssi > closestRSSI) {
        closestRSSI = rssi;
        closestMAC = macAddress;
        closestSSID = ssid;
        closestName = knownAPName;
      }
    } else {
      // Unknown AP - ignore
      Serial.print("  ✗ UNKNOWN: ");
      Serial.print(ssid);
      Serial.print(" (");
      Serial.print(macAddress);
      Serial.println(") - IGNORED");
    }
  }
  
  // Display results
  Serial.println("\n--- SCAN RESULTS ---");
  if (foundKnownAP) {
    Serial.println("Closest Known Access Point:");
    Serial.print("  Name: ");
    Serial.println(closestName);
    Serial.print("  SSID: ");
    Serial.println(closestSSID);
    Serial.print("  MAC Address: ");
    Serial.println(closestMAC);
    Serial.print("  Signal Strength (RSSI): ");
    Serial.print(closestRSSI);
    Serial.println(" dBm");
  } else {
    Serial.println("No known access points detected in scan.");
  }
  
  Serial.println("================================\n");
  
  // Clean up scan results memory
  WiFi.scanDelete();
}
