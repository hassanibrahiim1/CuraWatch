#ifndef STEP_DETECTOR_H
#define STEP_DETECTOR_H

#include <Wire.h>
#include "Adafruit_MPU6050.h"
#include "Adafruit_Sensor.h"
#include <math.h>

class StepDetector {
private:
  Adafruit_MPU6050 mpu;
  int stepCount;
  bool stepDetected;
  
  // Step detection thresholds
  static const float ACCEL_THRESHOLD_HIGH;
  static const float ACCEL_THRESHOLD_LOW;

public:
  StepDetector();
  
  // Initialization
  bool initialize();
  
  // Sensor data reading and processing
  void update();
  
  // Getters
  int getStepCount() const;
  float getAccelMagnitude() const;
  float getPitch() const;
  float getRoll() const;
  float getGyroMagnitude() const;
  
private:
  // Cached sensor values
  float accelMagnitude;
  float pitch;
  float roll;
  float gyroMagnitude;
  
  // Helper calculations
  void calculateAccelerometerMagnitude(sensors_event_t& a);
  void calculateOrientation(sensors_event_t& a);
  void calculateGyroMagnitude(sensors_event_t& g);
  void detectSteps();
};

#endif
