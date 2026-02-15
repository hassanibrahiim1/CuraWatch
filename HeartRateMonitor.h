#ifndef HEARTRATEMONITOR_H
#define HEARTRATEMONITOR_H

#include "heartRate.h"

class HeartRateMonitor {
public:
  HeartRateMonitor();
  
  // Heart rate detection and processing
  void detectBeat(uint32_t ir);
  bool isValidHR() const;
  
  // Getters
  float getBeatsPerMinute() const;
  int getAverageHR() const;
  
  // Reset when finger is removed
  void reset();
  
private:
  // Heart rate tracking
  static const byte RATE_SIZE = 8;
  byte rates[RATE_SIZE];
  byte rateSpot;
  long lastBeat;
  
  // HR values
  float beatsPerMinute;
  int beatAvg;
  bool validHR;
  
  // Validation thresholds
  static const int MIN_HR = 40;
  static const int MAX_HR = 180;
  static const int MIN_DELTA = 300;  // Minimum ms between beats (200 bpm max)
  static const int FINGER_ON = 30000;
  static const int MIN_READINGS = 3; // Need at least 3 readings for valid average
  
  // Internal calculations
  void calculateAverage();
};

#endif
