#ifndef BPFEATUREEXTRACTOR_H
#define BPFEATUREEXTRACTOR_H

#include <Arduino.h>

class BPFeatureExtractor {
public:
  BPFeatureExtractor();

  void registerECGPeak(uint32_t timestampMicros);
  void registerPPGBeat(uint32_t timestampMicros, bool fingerDetected);

  bool hasNewPAT() const;
  void clearNewPAT();

  float getLatestPATms() const;
  float getAveragePATms() const;
  uint32_t getLastECGPeakTimeMicros() const;
  uint32_t getLastPPGBeatTimeMicros() const;

private:
  static const int PAT_HISTORY_SIZE = 6;
  static const uint32_t MIN_PAT_US = 80000;
  static const uint32_t MAX_PAT_US = 450000;

  uint32_t lastECGPeakMicros;
  uint32_t lastPPGBeatMicros;
  float latestPATms;
  float averagePATms;
  float patHistory[PAT_HISTORY_SIZE];
  uint8_t patSpot;
  uint8_t patCount;
  bool newPATAvailable;

  void updateAverage();
};

#endif
