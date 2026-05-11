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
#include "AD8232Sensor.h"
#include "BPFeatureExtractor.h"
#include "MAX30102Sensor.h"
#include "PPGWindowBuilder.h"
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
AD8232Sensor ecgSensor;
BPFeatureExtractor bpFeatureExtractor;
MAX30102Sensor heartRateSensor;
PPGWindowBuilder ppgWindowBuilder;
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
#define PATIENT_EMAIL "patientMail@main.com"     // TODO: Change to your patient email
#define PATIENT_PASSWORD "password"             // TODO: Change to your patient password

// Vitals sending interval (send every 60 seconds)
#define VITALS_SEND_INTERVAL 60000
static unsigned long lastVitalsSentTime = 0;

// ============= CONFIGURATION =============
#define ECG_OFFLINE_MODE false
#define TIMETOBOOT 3000    // Wait for this time (msec) to output SpO2
#define SAMPLING   200     // Display/TFT refresh divider for MAX30102 samples
#define USEFIFO            // Use FIFO mode for MAX30102
#define UPDATE_INTERVAL 200 // Update interval in milliseconds
#define ENABLE_VERBOSE_SENSOR_LOG false
#define ENABLE_BACKGROUND_WIFI_SCAN false
#define ENABLE_PPG_WINDOW_UPLOAD true
#define ML_USE_ECG_WINDOW false
#define PPG_UPLOAD_RETRY_INTERVAL 5000

// AD8232 wiring for ESP32
//   AD8232 OUT  -> GPIO34 (ADC1, input-only pin)
//   AD8232 LO+  -> GPIO32
//   AD8232 LO-  -> GPIO33
#define ECG_SIGNAL_PIN 34
#define ECG_LO_PLUS_PIN 32
#define ECG_LO_MINUS_PIN 33
#define ECG_SAMPLE_RATE_HZ 250
#define ECG_ENABLE_RAW_STREAM false
#define ECG_RAW_STREAM_DIVIDER 5

// ============= GLOBAL VARIABLES =============
static int sampleCounter = 0;
static int displayCounter = 0;
static unsigned long lastUpdateTime = 0;
static unsigned long lastWiFiScanTime = 0;
static unsigned long lastTemperatureUpdateTime = 0;
static unsigned long lastPPGUploadAttemptTime = 0;
static float currentDisplayTemperature = 36.6;
static uint16_t ecgRawSampleCounter = 0;
static bool mlWindowPending = false;
String closestKnownAPName = "";

// Forward declarations for offline ECG helpers
void processECGSample();
void displayECGData();
void displayBPFeatureData();
void printBPFeatureCSV();
void sendPPGWindowWithVitals();
void printMLWindowPreview();

// ============= WiFi KNOWN APs =============
struct KnownAP {
  const char* name;
  const char* macAddress;
  const char* password;  // Add password field
};

const KnownAP knownAPs[] = {
  {"AP1", "BBSID1", "password1"},     // ← UPDATE WITH REAL PASSWORD
  {"AP2", "BBSID2", "password2"}, // ← UPDATE WITH REAL PASSWORD
  {"AP3", "BBSID3", "password3"}      // ← UPDATE WITH REAL PASSWORD
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

  if (!ECG_OFFLINE_MODE) {
    // Test preferences functionality
    testPreferences();
    
    // Debug AuthenticationManager preferences state
    authManager.debugPreferencesState();
  }

  Serial.println("Initializing AD8232 ECG Sensor...");
  if (!ecgSensor.begin(ECG_SIGNAL_PIN, ECG_LO_PLUS_PIN, ECG_LO_MINUS_PIN, ECG_SAMPLE_RATE_HZ)) {
    Serial.println("ERROR: AD8232 failed to initialize!");
  } else {
    Serial.println("✓ AD8232 initialized successfully");
    Serial.print("  ECG OUT -> GPIO ");
    Serial.println(ECG_SIGNAL_PIN);
    Serial.print("  LO+ -> GPIO ");
    Serial.println(ECG_LO_PLUS_PIN);
    Serial.print("  LO- -> GPIO ");
    Serial.println(ECG_LO_MINUS_PIN);
  }
  delay(250);

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
    Serial.println("  MAX30102 PPG capture profile:");
    Serial.println("    Sample rate: 200 Hz");
    Serial.println("    FIFO averaging: 1 (no decimation)");
    Serial.print("    Raw capture window: ");
    Serial.print(PPGWindowBuilder::RAW_WINDOW_SAMPLES);
    Serial.println(" IR samples (~3.00 s)");
    Serial.print("    Upload window: ");
    Serial.print(PPGWindowBuilder::MODEL_WINDOW_SAMPLES);
    Serial.println(" normalized PPG floats");
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

  if (ECG_OFFLINE_MODE) {
    Serial.println("\n=== Offline ECG/BP Feature Mode ===");
    Serial.println("WiFi scan, authentication, NTP sync, and backend sends are skipped.");
    Serial.println("Use Serial Monitor for ECG summaries and BP-estimation features.");
    Serial.println("Optional raw ECG stream can be enabled with ECG_ENABLE_RAW_STREAM.\n");
  } else {
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

  Serial.println("Vitals payload mode enabled.");
  Serial.println("The sketch will add one cleaned 3-second normalized PPG window to the old JSON.");
  Serial.println("If the PPG window is not ready yet, it will not send.");
  }
}

// ============= MAIN LOOP =============
void loop()
{
  if (ecgSensor.update()) {
    processECGSample();
  }

  // Handle MAX30102 data (continuous)
  #ifdef USEFIFO
  heartRateSensor.check();

  while (heartRateSensor.hasData()) {
    if (ecgSensor.update()) {
      processECGSample();
    }

    uint32_t red, ir;
    heartRateSensor.readSensor(red, ir);
    uint32_t ppgSampleTimeMicros = micros();
    
    sampleCounter++;
    displayCounter++;
    
    // Update sensor data and SpO2 calculation
    heartRateSensor.updateSpO2(red, ir);
    if (!mlWindowPending && ENABLE_PPG_WINDOW_UPLOAD) {
      if (ppgWindowBuilder.addSample((float)ir, heartRateSensor.isFingerDetected())) {
        mlWindowPending = true;
        printMLWindowPreview();
      } else if (ppgWindowBuilder.consumeRejectedWindowFlag()) {
        Serial.println("[PPG WINDOW] Rejected noisy PPG capture; collecting a fresh window.");
        Serial.print("  Raw min/max source range: ");
        Serial.print(ppgWindowBuilder.getLastWindowMin(), 0);
        Serial.print(" -> ");
        Serial.println(ppgWindowBuilder.getLastWindowMax(), 0);
        Serial.print("  Detected peaks: ");
        Serial.println(ppgWindowBuilder.getLastWindowPeakCount());
      }
    }
    
    // Heart rate detection
    bool ppgBeatDetected = hrMonitor.detectBeat(ir, ppgSampleTimeMicros);
    if (ppgBeatDetected) {
      bpFeatureExtractor.registerPPGBeat(ppgSampleTimeMicros, heartRateSensor.isFingerDetected());
      if (bpFeatureExtractor.hasNewPAT()) {
        printBPFeatureCSV();
      }
    }
    
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

  if (mlWindowPending &&
      !ECG_OFFLINE_MODE &&
      millis() - lastPPGUploadAttemptTime >= PPG_UPLOAD_RETRY_INTERVAL) {
    sendPPGWindowWithVitals();
    lastPPGUploadAttemptTime = millis();
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
      if (ENABLE_VERBOSE_SENSOR_LOG) {
        displayCombinedData();
      }
      updateTFTDisplay();  // Update TFT display
    }
    displayCounter = 0;
  }

  // Perform WiFi scan at specified interval
  if (!ECG_OFFLINE_MODE &&
      ENABLE_BACKGROUND_WIFI_SCAN &&
      millis() - lastWiFiScanTime >= WiFi_SCAN_INTERVAL) {
    findClosestKnownAP();
    lastWiFiScanTime = millis();
  }

  // Legacy time-based vitals upload is intentionally disabled here.
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

  Serial.println("---");

  // Display AD8232 ECG data and BP-oriented timing features
  displayECGData();

  Serial.println("---");

  displayBPFeatureData();
  
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

void processECGSample()
{
  if (ecgSensor.consumeBeatDetected()) {
    bpFeatureExtractor.registerECGPeak(ecgSensor.getLastRPeakTimeMicros());
  }

  if (!ECG_ENABLE_RAW_STREAM || !ecgSensor.hasNewSample()) {
    return;
  }

  ecgRawSampleCounter++;
  if (ecgRawSampleCounter < ECG_RAW_STREAM_DIVIDER) {
    return;
  }

  ecgRawSampleCounter = 0;

  AD8232Sensor::Sample sample = ecgSensor.getLatestSample();
  Serial.print("ECG_RAW,");
  Serial.print(sample.timestampMicros);
  Serial.print(",");
  Serial.print(sample.raw);
  Serial.print(",");
  Serial.print(sample.filtered, 2);
  Serial.print(",");
  Serial.println(sample.leadOff ? 1 : 0);
}

void displayECGData()
{
  AD8232Sensor::Sample sample = ecgSensor.getLatestSample();

  Serial.println("[AD8232 ECG]");
  Serial.print("  Lead Status: ");
  Serial.println(sample.leadOff ? "OFF / CHECK ELECTRODES" : "Connected");

  if (sample.timestampMicros == 0) {
    Serial.println("  ECG Status: Waiting for first samples...");
    return;
  }

  Serial.print("  Raw ADC: ");
  Serial.println(sample.raw);

  Serial.print("  Filtered ECG: ");
  Serial.println(sample.filtered, 2);

  Serial.print("  Signal Quality: ");
  Serial.print(ecgSensor.getSignalQuality() * 100.0f, 0);
  Serial.println("%");

  if (!ecgSensor.hasValidSignal()) {
    Serial.println("  ECG Status: Stabilizing / noisy");
    return;
  }

  Serial.print("  ECG HR: ");
  if (ecgSensor.getHeartRateBpm() > 0.0f) {
    Serial.print(ecgSensor.getHeartRateBpm(), 1);
    Serial.println(" bpm");
  } else {
    Serial.println("Calculating...");
  }

  Serial.print("  RR Interval: ");
  if (ecgSensor.getLastRRIntervalMs() > 0) {
    Serial.print(ecgSensor.getLastRRIntervalMs());
    Serial.println(" ms");
  } else {
    Serial.println("--");
  }

  Serial.print("  QRS Peak Amplitude: ");
  Serial.println(ecgSensor.getLastQRSAmplitude(), 2);
}

void displayBPFeatureData()
{
  Serial.println("[BP ESTIMATION FEATURES]");

  Serial.print("  PPG Finger State: ");
  Serial.println(heartRateSensor.isFingerDetected() ? "Detected" : "Not detected");

  Serial.print("  Latest ECG R-Peak Time: ");
  if (bpFeatureExtractor.getLastECGPeakTimeMicros() > 0) {
    Serial.print(bpFeatureExtractor.getLastECGPeakTimeMicros());
    Serial.println(" us");
  } else {
    Serial.println("--");
  }

  Serial.print("  Latest PPG Beat Time: ");
  if (bpFeatureExtractor.getLastPPGBeatTimeMicros() > 0) {
    Serial.print(bpFeatureExtractor.getLastPPGBeatTimeMicros());
    Serial.println(" us");
  } else {
    Serial.println("--");
  }

  Serial.print("  Latest PAT: ");
  if (bpFeatureExtractor.getLatestPATms() > 0.0f) {
    Serial.print(bpFeatureExtractor.getLatestPATms(), 1);
    Serial.println(" ms");
  } else {
    Serial.println("Waiting for ECG + PPG beats...");
  }

  Serial.print("  Average PAT: ");
  if (bpFeatureExtractor.getAveragePATms() > 0.0f) {
    Serial.print(bpFeatureExtractor.getAveragePATms(), 1);
    Serial.println(" ms");
  } else {
    Serial.println("--");
  }

  Serial.print("  PPG HR: ");
  if (hrMonitor.isValidHR()) {
    Serial.print(hrMonitor.getAverageHR());
    Serial.println(" bpm");
  } else {
    Serial.println("--");
  }

  Serial.print("  PPG DC IR: ");
  Serial.println(heartRateSensor.getAveragedIR(), 0);

  Serial.print("  PPG DC Red: ");
  Serial.println(heartRateSensor.getAveragedRed(), 0);
}

void printBPFeatureCSV()
{
  Serial.print("BP_FEATURE,");
  Serial.print(bpFeatureExtractor.getLastECGPeakTimeMicros());
  Serial.print(",");
  Serial.print(bpFeatureExtractor.getLastPPGBeatTimeMicros());
  Serial.print(",");
  Serial.print(bpFeatureExtractor.getLatestPATms(), 1);
  Serial.print(",");
  Serial.print(bpFeatureExtractor.getAveragePATms(), 1);
  Serial.print(",");
  Serial.print(ecgSensor.getHeartRateBpm(), 1);
  Serial.print(",");
  Serial.print(ecgSensor.getLastRRIntervalMs());
  Serial.print(",");
  Serial.print(ecgSensor.getSignalQuality(), 3);
  Serial.print(",");
  Serial.print(hrMonitor.getBeatsPerMinute(), 1);
  Serial.print(",");
  Serial.print(heartRateSensor.getAveragedIR(), 0);
  Serial.print(",");
  Serial.println(heartRateSensor.getAveragedRed(), 0);

  bpFeatureExtractor.clearNewPAT();
}

void printMLWindowPreview()
{
  if (!ppgWindowBuilder.isWindowReady()) {
    return;
  }

  const float* ppgWindow = ppgWindowBuilder.getWindow();

  Serial.println("\n[PPG WINDOW]");
  Serial.println("  One cleaned 3-second PPG upload window is ready.");
  Serial.print("  Raw min/max source range: ");
  Serial.print(ppgWindowBuilder.getLastWindowMin(), 0);
  Serial.print(" -> ");
  Serial.println(ppgWindowBuilder.getLastWindowMax(), 0);
  Serial.print("  Detected peaks in source window: ");
  Serial.println(ppgWindowBuilder.getLastWindowPeakCount());
  Serial.print("  Preview: ");
  for (int i = 0; i < 5; i++) {
    if (i > 0) {
      Serial.print(", ");
    }
    Serial.print(ppgWindow[i], 4);
  }
  Serial.println();
}

void sendPPGWindowWithVitals()
{
  if (!mlWindowPending || !ppgWindowBuilder.isWindowReady()) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[PPG] WiFi disconnected, reconnecting before upload...");
    connectToWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[PPG] WiFi still unavailable, keeping current PPG window for retry.");
      return;
    }
  }

  if (!authManager.isAuthenticated()) {
    Serial.println("[PPG] Not authenticated, attempting login before upload...");
    attemptAutoLogin();
    if (!authManager.isAuthenticated()) {
      Serial.println("[PPG] Login failed, keeping current PPG window for retry.");
      return;
    }
  }

  const float* ppgWindow = ppgWindowBuilder.getWindow();
  int ppgCount = ppgWindowBuilder.getWindowSize();
  int hr = hrMonitor.isValidHR() ? hrMonitor.getAverageHR() : 0;
  float spo2 = heartRateSensor.getFilteredSpO2();
  int steps = stepDetector.getStepCount();

  float temperature = currentDisplayTemperature;
  if (temperature == DEVICE_DISCONNECTED_C || temperature < 20.0f || temperature > 45.0f) {
    temperature = 36.6f;
  }

  if (authManager.sendVitals(BACKEND_URL, hr, spo2, steps, temperature, ppgWindow, ppgCount)) {
    String mlWarning = authManager.getLastMLWarning();
    if (mlWarning.isEmpty()) {
      Serial.println("[PPG] Vitals + PPG window uploaded successfully.");
    } else {
      Serial.print("[PPG] Vitals saved, but ML still rejected this window: ");
      Serial.println(mlWarning);
      Serial.println("[PPG] Dropping this saved window and collecting a cleaner one.");
    }
    ppgWindowBuilder.consumeWindow();
    mlWindowPending = false;
  } else {
    Serial.print("[PPG] Failed to upload PPG window: ");
    Serial.println(authManager.getLastError());
  }
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
 * Test function to preview the vitals JSON once a PPG window is ready
 */
void testSensorsAndPayload() {
  Serial.println("\n========== PPG WINDOW TEST ==========");
  
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
  String samplePayload = ppgWindowBuilder.isWindowReady()
      ? authManager.buildVitalsJSON(
            hr,
            spo2,
            steps,
            temperature,
            ppgWindowBuilder.getWindow(),
            ppgWindowBuilder.getWindowSize())
      : String("{\"status\":\"ppg_not_ready\"}");
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

