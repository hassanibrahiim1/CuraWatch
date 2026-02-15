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
