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
#include "MAX30102Sensor.h"
#include "HeartRateMonitor.h"
#include "StepDetector.h"

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
