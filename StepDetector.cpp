#include "StepDetector.h"

// Define static threshold constants
const float StepDetector::ACCEL_THRESHOLD_HIGH = 12.0;
const float StepDetector::ACCEL_THRESHOLD_LOW = 10.0;

StepDetector::StepDetector()
  : stepCount(0), stepDetected(false),
    accelMagnitude(0), pitch(0), roll(0), gyroMagnitude(0) {
}

bool StepDetector::initialize() {
  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found.");
    return false;
  }

  // Configure sensors
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 initialized successfully!");
  return true;
}

void StepDetector::update() {
  // Read sensor data
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Calculate all values
  calculateAccelerometerMagnitude(a);
  calculateOrientation(a);
  calculateGyroMagnitude(g);
  detectSteps();
}

void StepDetector::calculateAccelerometerMagnitude(sensors_event_t& a) {
  accelMagnitude = sqrt(
      a.acceleration.x * a.acceleration.x +
      a.acceleration.y * a.acceleration.y +
      a.acceleration.z * a.acceleration.z);
}

void StepDetector::calculateOrientation(sensors_event_t& a) {
  pitch = atan2(
      a.acceleration.x,
      sqrt(a.acceleration.y * a.acceleration.y +
           a.acceleration.z * a.acceleration.z)) * 180.0 / PI;

  roll = atan2(
      a.acceleration.y,
      a.acceleration.z) * 180.0 / PI;
}

void StepDetector::calculateGyroMagnitude(sensors_event_t& g) {
  gyroMagnitude = sqrt(
      g.gyro.x * g.gyro.x +
      g.gyro.y * g.gyro.y +
      g.gyro.z * g.gyro.z);
}

void StepDetector::detectSteps() {
  if (accelMagnitude > ACCEL_THRESHOLD_HIGH && !stepDetected) {
    stepDetected = true;
    stepCount++;
  }

  if (accelMagnitude < ACCEL_THRESHOLD_LOW) {
    stepDetected = false;
  }
}

int StepDetector::getStepCount() const {
  return stepCount;
}

float StepDetector::getAccelMagnitude() const {
  return accelMagnitude;
}

float StepDetector::getPitch() const {
  return pitch;
}

float StepDetector::getRoll() const {
  return roll;
}

float StepDetector::getGyroMagnitude() const {
  return gyroMagnitude;
}
