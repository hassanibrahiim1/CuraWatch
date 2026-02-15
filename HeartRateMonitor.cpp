#include "HeartRateMonitor.h"

HeartRateMonitor::HeartRateMonitor() 
  : rateSpot(0), lastBeat(0), beatsPerMinute(0), beatAvg(0), validHR(false) {
  for (byte x = 0; x < RATE_SIZE; x++) {
    rates[x] = 0;
  }
}

void HeartRateMonitor::detectBeat(uint32_t ir) {
  if (ir < FINGER_ON) {
    return; // Finger not detected
  }
  
  if (checkForBeat(ir) == true) {
    // We sensed a beat!
    long delta = millis() - lastBeat;
    
    // Validate the time between beats
    if (delta > MIN_DELTA) {
      lastBeat = millis();
      beatsPerMinute = 60 / (delta / 1000.0);
      
      // Only store valid heart rate values
      if (beatsPerMinute < MAX_HR && beatsPerMinute > MIN_HR) {
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;
        calculateAverage();
      }
    }
  }
}

void HeartRateMonitor::calculateAverage() {
  beatAvg = 0;
  byte count = 0;
  
  for (byte x = 0; x < RATE_SIZE; x++) {
    if (rates[x] != 0) {
      beatAvg += rates[x];
      count++;
    }
  }
  
  if (count > 0) {
    beatAvg /= count;
    validHR = (count >= MIN_READINGS);
  }
}

bool HeartRateMonitor::isValidHR() const {
  return validHR;
}

float HeartRateMonitor::getBeatsPerMinute() const {
  return beatsPerMinute;
}

int HeartRateMonitor::getAverageHR() const {
  return beatAvg;
}

void HeartRateMonitor::reset() {
  validHR = false;
  for (byte x = 0; x < RATE_SIZE; x++) {
    rates[x] = 0;
  }
  beatAvg = 0;
  beatsPerMinute = 0;
}
