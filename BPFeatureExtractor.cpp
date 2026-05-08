#include "BPFeatureExtractor.h"

BPFeatureExtractor::BPFeatureExtractor()
  : lastECGPeakMicros(0), lastPPGBeatMicros(0), latestPATms(0.0f), averagePATms(0.0f),
    patSpot(0), patCount(0), newPATAvailable(false) {
  for (int i = 0; i < PAT_HISTORY_SIZE; i++) {
    patHistory[i] = 0.0f;
  }
}

void BPFeatureExtractor::registerECGPeak(uint32_t timestampMicros) {
  lastECGPeakMicros = timestampMicros;
}

void BPFeatureExtractor::registerPPGBeat(uint32_t timestampMicros, bool fingerDetected) {
  lastPPGBeatMicros = timestampMicros;

  if (!fingerDetected || lastECGPeakMicros == 0 || timestampMicros <= lastECGPeakMicros) {
    return;
  }

  uint32_t patMicros = timestampMicros - lastECGPeakMicros;
  if (patMicros < MIN_PAT_US || patMicros > MAX_PAT_US) {
    return;
  }

  latestPATms = patMicros / 1000.0f;
  patHistory[patSpot++] = latestPATms;
  patSpot %= PAT_HISTORY_SIZE;

  if (patCount < PAT_HISTORY_SIZE) {
    patCount++;
  }

  updateAverage();
  newPATAvailable = true;
}

bool BPFeatureExtractor::hasNewPAT() const {
  return newPATAvailable;
}

void BPFeatureExtractor::clearNewPAT() {
  newPATAvailable = false;
}

float BPFeatureExtractor::getLatestPATms() const {
  return latestPATms;
}

float BPFeatureExtractor::getAveragePATms() const {
  return averagePATms;
}

uint32_t BPFeatureExtractor::getLastECGPeakTimeMicros() const {
  return lastECGPeakMicros;
}

uint32_t BPFeatureExtractor::getLastPPGBeatTimeMicros() const {
  return lastPPGBeatMicros;
}

void BPFeatureExtractor::updateAverage() {
  if (patCount == 0) {
    averagePATms = 0.0f;
    return;
  }

  float total = 0.0f;
  for (uint8_t i = 0; i < patCount; i++) {
    total += patHistory[i];
  }

  averagePATms = total / patCount;
}
