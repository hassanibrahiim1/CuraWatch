#ifndef MAX30102SENSOR_H
#define MAX30102SENSOR_H

#include <Wire.h>
#include "MAX30105.h"

class MAX30102Sensor {
public:
  MAX30102Sensor();
  
  // Initialization
  bool begin();
  
  // Data collection and processing
  void check();
  bool hasData();
  void readSensor(uint32_t &red, uint32_t &ir);
  
  // SpO2 calculation
  void updateSpO2(double red, double ir);
  float getSpO2() const;
  float getFilteredSpO2() const;
  float getTemperature();
  
  // Finger detection
  bool isFingerDetected() const;
  
  // Getters for averaging values
  double getAveragedRed() const;
  double getAveragedIR() const;

private:
  MAX30105 particleSensor;
  
  // SpO2 calculation variables
  double avered;
  double aveir;
  double sumirrms;
  double sumredrms;
  int sampleCount;
  
  // SpO2 values
  float ESpO2;           // estimated SpO2
  double FSpO2;          // filter factor
  double frate;          // low pass filter factor
  double lastSpO2;
  
  // Constants
  static const int NUM_SAMPLES = 100;
  static const float SCALE;
  static const int FINGER_ON_THRESHOLD;
  
  // Internal calculations
  void updateAverages(double red, double ir);
  float calculateSpO2(double redRMS, double irRMS);
};

#endif
