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
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>
#include "MAX30102Sensor.h"
#include "HeartRateMonitor.h"
#include "StepDetector.h"
#include "AuthenticationManager.h"

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

// ============= DS18B20 TEMPERATURE SENSOR =============
#define ONE_WIRE_BUS 5  // Connect DS18B20 data pin to GPIO 5 (D5) - FIXED PIN CONFLICT
#define DEVICE_DISCONNECTED_C -127  // DS18B20 disconnected value
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

// ============= AUTHENTICATION & BACKEND =============
AuthenticationManager authManager;

// Test preferences persistence
void testPreferences() {
  Serial.println("\n========== PREFERENCES TEST ==========");
  
  Preferences testPrefs;
  if (!testPrefs.begin("test_namespace", false)) {
    Serial.println("✗ Failed to initialize test preferences");
    return;
  }
  
  // Check for persistent value from previous run
  String persistentValue = testPrefs.getString("persistent_test", "");
  if (!persistentValue.isEmpty()) {
    Serial.print("✓ Found persistent value from previous run: ");
    Serial.println(persistentValue);
  } else {
    Serial.println("No persistent value found (first run or cleared)");
  }
  
  // Save a new persistent value with timestamp
  String newValue = "run_at_" + String(millis());
  testPrefs.putString("persistent_test", newValue);
  Serial.print("✓ Saved new persistent value: ");
  Serial.println(newValue);
  
  // Test immediate read-back
  String testValue = testPrefs.getString("persistent_test", "");
  if (testValue == newValue) {
    Serial.println("✓ Immediate read-back successful");
  } else {
    Serial.println("✗ Immediate read-back failed!");
  }
  
  testPrefs.end();
  Serial.println("========== END PREFERENCES TEST ==========\n");
}

// Backend configuration
#define BACKEND_URL "https://cura-watch.onrender.com"  // TODO: Change to your backend server IP/address
#define PATIENT_EMAIL "mustafa.ahmed.00878@gmail.com"     // TODO: Change to your patient email
#define PATIENT_PASSWORD "patientpassword123"             // TODO: Change to your patient password

// Vitals sending interval (send every 60 seconds)
#define VITALS_SEND_INTERVAL 60000
static unsigned long lastVitalsSentTime = 0;

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
static unsigned long lastTemperatureUpdateTime = 0;
static float currentDisplayTemperature = 36.6;
String closestKnownAPName = "";

// ============= WiFi KNOWN APs =============
struct KnownAP {
  const char* name;
  const char* macAddress;
  const char* password;  // Add password field
};

const KnownAP knownAPs[] = {
  {"Hassan", "6a:bb:30:8c:73:4f", "12345678"},     // ← UPDATE WITH REAL PASSWORD
  {"Mohamed", "da:6c:44:b6:ee:05", "12345678"}, // ← UPDATE WITH REAL PASSWORD
  {"MyHome", "58:d7:59:6c:fa:d0", "ibrahassan376236*"}      // ← UPDATE WITH REAL PASSWORD
};

const int NUM_KNOWN_APS = sizeof(knownAPs) / sizeof(knownAPs[0]);

#define WiFi_SCAN_INTERVAL 10000  // Scan WiFi every 5 seconds
#define TEMPERATURE_UPDATE_INTERVAL 1000  // Update DS18B20 every 1 second

// ============= SETUP =============
void setup()
{
  // Initialize Serial
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(1000);
  Serial.println("\n\n=== CuraWatch Combined System Starting ===\n");

  // Test preferences functionality
  testPreferences();
  
  // Debug AuthenticationManager preferences state
  authManager.debugPreferencesState();

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

  // Initialize DS18B20 Temperature Sensor
  Serial.println("Initializing DS18B20 Temperature Sensor...");
  Serial.print("Using GPIO pin: ");
  Serial.println(ONE_WIRE_BUS);
  
  // Check if any devices are found on the OneWire bus
  Serial.print("Searching for OneWire devices...");
  int deviceCount = tempSensor.getDeviceCount();
  Serial.print("Found ");
  Serial.print(deviceCount);
  Serial.println(" devices");
  
  if (deviceCount == 0) {
    Serial.println("⚠ No OneWire devices found!");
    Serial.println("Check wiring: DS18B20 data pin should be connected to GPIO 5");
    Serial.println("Ensure 4.7kΩ pull-up resistor between data and 3.3V");
    Serial.println("Power connections: VCC to 3.3V, GND to GND");
    
    // Try to search for devices manually
    Serial.println("Attempting manual device search...");
    byte addr[8];
    if (oneWire.search(addr)) {
      Serial.print("Found device with address: ");
      for (int i = 0; i < 8; i++) {
        if (addr[i] < 16) Serial.print("0");
        Serial.print(addr[i], HEX);
        if (i < 7) Serial.print(":");
      }
      Serial.println();
      
      // Check if it's a DS18B20
      if (addr[0] == 0x28) {
        Serial.println("✓ Device is a DS18B20!");
        Serial.println("Sensor is responding but DallasTemperature library may have issues");
        Serial.println("Will continue with temperature readings despite device count = 0");
      } else {
        Serial.print("⚠ Device found but not DS18B20 (family code: 0x");
        Serial.print(addr[0], HEX);
        Serial.println(")");
      }
    } else {
      Serial.println("No devices found on OneWire bus");
      oneWire.reset_search();
    }
  } else {
    Serial.println("✓ OneWire devices found successfully");
  }
  
  tempSensor.begin();
  tempSensor.setResolution(12);  // Set resolution to 12-bit (0.0625°C precision)
  tempSensor.setWaitForConversion(false);  // Non-blocking mode
  
  // Request first temperature reading
  Serial.println("Requesting temperature reading...");
  tempSensor.requestTemperatures();
  delay(100);
  
  float initialTemp = tempSensor.getTempCByIndex(0);
  Serial.print("Raw temperature reading: ");
  Serial.println(initialTemp);
  
  if (initialTemp != DEVICE_DISCONNECTED_C) {
    Serial.print("✓ DS18B20 initialized successfully - Initial temp: ");
    Serial.print(initialTemp);
    Serial.println("°C");
  } else {
    Serial.println("ERROR: DS18B20 not found!");
    Serial.println("Possible issues:");
    Serial.println("  - Sensor not connected or faulty");
    Serial.println("  - Wrong GPIO pin (currently GPIO 5)");
    Serial.println("  - Missing pull-up resistor (4.7kΩ between data and 3.3V)");
    Serial.println("  - Power supply issue");
    Serial.println("Will continue without temperature sensor...");
  }

  delay(500);

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

  // ============= AUTHENTICATION SETUP =============
  Serial.println("\n=== Authentication Setup ===");
  
  // FIRST: Save credentials if this is the first run (uncomment the line below)
  // Option 1: Save credentials automatically
  // authManager.saveCredentials(PATIENT_EMAIL, PATIENT_PASSWORD);
  // Serial.println("✓ New credentials saved to ESP32 storage\n");
  
  // Option 2: Save credentials manually (call this function once)
  saveNewCredentials(PATIENT_EMAIL, PATIENT_PASSWORD);  // Remove this line after first run!
  
  // Check for stored credentials
  String storedEmail, storedPassword;
  if (authManager.loadCredentials(storedEmail, storedPassword)) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(20, 100);
    tft.println("Found stored");
    tft.setCursor(20, 115);
    tft.println("credentials");
    Serial.println("✓ Found stored credentials");
  } else {
    Serial.println("[AUTH] No credentials found in storage");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(20, 100);
    tft.println("No credentials");
    tft.setCursor(20, 115);
    tft.println("found - check code");
  }
  
  delay(2000); // Show message for 2 seconds
  
  // SECOND: Connect to WiFi first
  Serial.println("\nConnecting to WiFi for NTP sync...");
  Serial.println("⚠ IMPORTANT: Update WiFi passwords in knownAPs array above!");
  connectToWiFi();
  
  // THIRD: Setup NTP time synchronization (now that WiFi is connected)
  Serial.println("Setting up NTP time synchronization...");
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");  // Add timezone offset and more servers
  delay(5000);  // Give more time for NTP sync
  
  // Verify time is set
  time_t now = time(nullptr);
  if (now > 1609459200) {  // Check if time is reasonable (after 2021)
    Serial.println("✓ NTP time synchronized successfully");
    Serial.print("Current time: ");
    Serial.println(ctime(&now));
  } else {
    Serial.println("⚠ NTP sync may have failed - timestamps may be incorrect");
    Serial.println("This could be due to network issues or firewall blocking NTP");
    Serial.println("Will try to sync again in 30 seconds...");
    
    // Try alternative NTP servers
    delay(30000);
    configTime(3 * 3600, 0, "0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org");
    delay(5000);
    
    now = time(nullptr);
    if (now > 1609459200) {
      Serial.println("✓ NTP sync successful on retry!");
      Serial.print("Current time: ");
      Serial.println(ctime(&now));
    } else {
      Serial.println("✗ NTP sync still failing - using device uptime for timestamps");
    }
  }

  // Test sensors and show sample payload
  testSensorsAndPayload();
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
  
  // Refresh cached DS18B20 temperature reading
  if (currentTime - lastTemperatureUpdateTime >= TEMPERATURE_UPDATE_INTERVAL) {
    float ds18b20Temperature = tempSensor.getTempCByIndex(0);
    if (ds18b20Temperature != DEVICE_DISCONNECTED_C &&
        ds18b20Temperature >= 20.0 &&
        ds18b20Temperature <= 45.0) {
      currentDisplayTemperature = ds18b20Temperature;
    }
    tempSensor.requestTemperatures();
    lastTemperatureUpdateTime = currentTime;
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

  // Send vitals periodically to backend (if authenticated)
  if (currentTime - lastVitalsSentTime >= VITALS_SEND_INTERVAL) {
    if (authManager.isAuthenticated()) {
      sendVitalsToBackend();
    } else {
      // Try to auto-login if not authenticated
      attemptAutoLogin();
    }
    lastVitalsSentTime = currentTime;
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
 * Update the TFT display with current HR, SpO2, Steps, and temperature
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
  float temperature = currentDisplayTemperature;
  
  // Define colors
  uint16_t hrColor = getHRColor(hr);
  uint16_t spo2Color = getSPO2Color(spo2);
  uint16_t stepColor = TFT_CYAN;
  uint16_t temperatureColor = TFT_ORANGE;
  
  // Draw four metric boxes in a 2x2 arrangement
  drawMetricBox(70, 30, "HR", String(hr), "bpm", hrColor, 35);
  
  drawMetricBox(170, 30, "SpO2", String((int)spo2), "%", spo2Color, 30);
  
  drawMetricBox(70, 120, "STEPS", String(steps), "", stepColor, 30);
  drawMetricBox(170, 120, "TEMP", String(temperature, 1), "C", temperatureColor, 30);
  
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
    closestKnownAPName = "";
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
    closestKnownAPName = closestName;
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
    closestKnownAPName = "";
    Serial.println("No known access points detected in scan.");
  }
  
  Serial.println("================================\n");
  
  // Clean up scan results memory
  WiFi.scanDelete();
}

// ============= AUTHENTICATION & BACKEND FUNCTIONS =============

/**
 * Connect to WiFi network using strongest known AP or SSID
 */
void connectToWiFi() {
  Serial.println("\n========== WiFi CONNECTION ==========");
  
  // Try to find and connect to closest known AP
  Serial.println("Scanning for known networks...");
  int numNetworks = WiFi.scanNetworks();
  
  if (numNetworks == 0) {
    Serial.println("No networks found.");
    return;
  }
  
  int closestRSSI = -200;
  String closestSSID = "";
  String closestPassword = "";
  
  for (int i = 0; i < numNetworks; i++) {
    String ssid = WiFi.SSID(i);
    int32_t rssi = WiFi.RSSI(i);
    uint8_t* bssid = WiFi.BSSID(i);
    String macAddress = macToString(bssid);
    
    String knownAPName;
    if (isKnownAP(macAddress, knownAPName)) {
      // Find the password for this known AP
      for (int j = 0; j < NUM_KNOWN_APS; j++) {
        if (macAddress.equalsIgnoreCase(String(knownAPs[j].macAddress))) {
          if (rssi > closestRSSI) {
            closestRSSI = rssi;
            closestSSID = ssid;
            closestPassword = String(knownAPs[j].password);
          }
          break;
        }
      }
    }
  }
  
  WiFi.scanDelete();
  
  if (closestSSID.isEmpty()) {
    Serial.println("ERROR: No known WiFi networks available");
    return;
  }
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(closestSSID);
  
  WiFi.begin(closestSSID.c_str(), closestPassword.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Show WiFi connection on TFT
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(20, 100);
    tft.println("WiFi Connected!");
    tft.setCursor(20, 115);
    tft.print("IP: ");
    tft.println(WiFi.localIP());
    
    Serial.println("================================\n");
  } else {
    Serial.println("\nERROR: Failed to connect to WiFi");
    Serial.println("Check WiFi passwords in knownAPs array");
    
    // Show WiFi error on TFT
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(20, 100);
    tft.println("WiFi Failed!");
    tft.setCursor(20, 115);
    tft.println("Check passwords");
    
    Serial.println("================================\n");
  }
}

/**
 * Attempt to automatically login using stored credentials
 */
void attemptAutoLogin() {
  Serial.println("\n========== AUTO-LOGIN ATTEMPT ==========");
  
  // Ensure WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi, cannot login");
    connectToWiFi();
    return;
  }
  
  // Attempt auto-login
  if (authManager.autoLogin(BACKEND_URL)) {
    Serial.println("✓ Auto-login successful!");
    displayAuthStatus(true);
  } else {
    Serial.print("✗ Auto-login failed: ");
    Serial.println(authManager.getLastError());
    
    // Try manual login with configured credentials
    Serial.println("\nAttempting manual login...");
    if (authManager.login(PATIENT_EMAIL, PATIENT_PASSWORD, BACKEND_URL)) {
      Serial.println("✓ Manual login successful!");
      displayAuthStatus(true);
    } else {
      Serial.print("✗ Manual login failed: ");
      Serial.println(authManager.getLastError());
      displayAuthStatus(false);
    }
  }
  Serial.println("================================\n");
}

/**
 * Send current vitals to backend
 */
void sendVitalsToBackend() {
  Serial.println("\n========== SENDING VITALS ==========");
  
  // Get current sensor values
  int hr = hrMonitor.isValidHR() ? hrMonitor.getAverageHR() : 0;
  float spo2 = heartRateSensor.getFilteredSpO2();
  int steps = stepDetector.getStepCount();
  
  // Get temperature from DS18B20
  tempSensor.requestTemperatures();
  delay(100);  // Wait for conversion
  float temperature = tempSensor.getTempCByIndex(0);
  
  // Validate temperature reading
  if (temperature == DEVICE_DISCONNECTED_C) {
    Serial.println("⚠ Temperature sensor disconnected, using default value");
    temperature = 36.6;  // Normal body temperature
  } else if (temperature < 20 || temperature > 45) {
    Serial.print("⚠ Invalid temperature reading: ");
    Serial.print(temperature);
    Serial.println("°C, using default value");
    temperature = 36.6;
  }
  
  // Convert to Fahrenheit for display (optional)
  float tempF = (temperature * 9.0/5.0) + 32.0;
  
  Serial.print("HR: ");
  Serial.print(hr);
  Serial.print(" | SpO2: ");
  Serial.print(spo2);
  Serial.print("% | Steps: ");
  Serial.print(steps);
  Serial.print(" | Temp: ");
  Serial.print(temperature);
  Serial.print("°C (");
  Serial.print(tempF);
  Serial.println("°F)");
  
  // Ensure WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi, skipping vitals send");
    connectToWiFi();
    Serial.println("================================\n");
    return;
  }
  
  // Send vitals (now includes temperature)
  if (authManager.sendVitals(BACKEND_URL, hr, spo2, steps, temperature)) {
    Serial.println("✓ Vitals sent successfully!");
  } else {
    Serial.print("✗ Failed to send vitals: ");
    Serial.println(authManager.getLastError());
    
    // If authentication error detected, attempt to re-login
    if (authManager.getLastError().indexOf("401") >= 0 || 
        authManager.getLastError().indexOf("Not authenticated") >= 0) {
      Serial.println("\nAuthentication error detected, attempting re-login...");
      attemptAutoLogin();
    }
  }
  
  Serial.println("================================\n");
}

/**
 * Display authentication status on TFT
 */
void displayAuthStatus(bool authenticated) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  
  if (authenticated) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(30, 100);
    tft.println("Authenticated");
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(40, 130);
    tft.println("Ready to send vitals");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(45, 100);
    tft.println("Auth Failed");
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(30, 130);
    tft.println("Check WiFi & Credentials");
  }
  
  delay(2000);
}

/**
 * Test function to verify DS18B20 sensor and show sample JSON payload
 */
void testSensorsAndPayload() {
  Serial.println("\n========== SENSOR TEST & SAMPLE PAYLOAD ==========");
  
  // Get sensor readings
  int hr = hrMonitor.isValidHR() ? hrMonitor.getAverageHR() : 72;  // Sample value
  float spo2 = heartRateSensor.getFilteredSpO2();
  int steps = stepDetector.getStepCount();
  
  // Get temperature
  tempSensor.requestTemperatures();
  delay(100);
  float temperature = tempSensor.getTempCByIndex(0);
  if (temperature == DEVICE_DISCONNECTED_C) {
    temperature = 36.6;  // Sample value if sensor fails
  }
  
  Serial.println("Current Sensor Readings:");
  Serial.print("  Heart Rate: ");
  Serial.print(hr);
  Serial.println(" bpm");
  Serial.print("  Oxygen (SpO2): ");
  Serial.print(spo2);
  Serial.println("%");
  Serial.print("  Steps: ");
  Serial.println(steps);
  Serial.print("  Temperature: ");
  Serial.print(temperature);
  Serial.println("°C");
  
  // Show what will be sent to backend
  Serial.println("\nJSON Payload that will be sent:");
  String samplePayload = authManager.buildVitalsJSON(hr, spo2, steps, temperature);
  Serial.println(samplePayload);
  
  Serial.println("===========================================\n");
}

/**
 * Manually save credentials (call this once to store credentials)
 * Example: saveNewCredentials("user@example.com", "password123");
 */
void saveNewCredentials(const char* email, const char* password) {
  Serial.print("\nManually saving credentials for: ");
  Serial.println(email);
  authManager.saveCredentials(email, password);
  Serial.println("✓ Credentials saved\n");
}

/**
 * Get current stored email (if any)
 */
String getStoredEmail() {
  String email, password;
  if (authManager.loadCredentials(email, password)) {
    return email;
  }
  return "Not stored";
}

/**
 * Display current authentication state
 */
void printAuthStatus() {
  Serial.println("\n========== AUTHENTICATION STATUS ==========");
  
  if (authManager.isAuthenticated()) {
    Serial.println("Status: AUTHENTICATED ✓");
    Serial.print("Token: ");
    Serial.println(authManager.getToken().substring(0, 30) + "...");
    
    if (authManager.tokenExpired()) {
      Serial.println("WARNING: Token may be expired!");
    } else {
      Serial.println("Token status: Valid");
    }
  } else {
    Serial.println("Status: NOT AUTHENTICATED");
    Serial.println("Next: Call attemptAutoLogin() to login");
  }
  
  Serial.println("==========================================\n");
}

