#include "HeartRateMonitor.h"

HeartRateMonitor::HeartRateMonitor() 
  : rateSpot(0), lastBeatTimeMicros(0), beatsPerMinute(0), beatAvg(0), validHR(false) {
  for (byte x = 0; x < RATE_SIZE; x++) {
    rates[x] = 0;
  }
}

bool HeartRateMonitor::detectBeat(uint32_t ir, uint32_t timestampMicros) {
  if (ir < FINGER_ON) {
    return false; // Finger not detected
  }

  if (timestampMicros == 0) {
    timestampMicros = micros();
  }
  
  if (checkForBeat(ir) == true) {
    // We sensed a beat!
    uint32_t delta = timestampMicros - lastBeatTimeMicros;
    
    // Validate the time between beats
    if (lastBeatTimeMicros == 0 || delta > MIN_DELTA_US) {
      lastBeatTimeMicros = timestampMicros;
      if (delta > 0) {
        beatsPerMinute = 60000000.0f / delta;
      }
      
      // Only store valid heart rate values
      if (beatsPerMinute < MAX_HR && beatsPerMinute > MIN_HR) {
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;
        calculateAverage();
      }

      return true;
    }
  }

  return false;
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

uint32_t HeartRateMonitor::getLastBeatTimeMicros() const {
  return lastBeatTimeMicros;
}

void HeartRateMonitor::reset() {
  validHR = false;
  lastBeatTimeMicros = 0;
  for (byte x = 0; x < RATE_SIZE; x++) {
    rates[x] = 0;
  }
  beatAvg = 0;
  beatsPerMinute = 0;
}
