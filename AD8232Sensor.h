#ifndef AD8232SENSOR_H
#define AD8232SENSOR_H

#include <Arduino.h>

class AD8232Sensor {
public:
  struct Sample {
    uint16_t raw;
    float filtered;
    uint32_t timestampMicros;
    bool leadOff;
  };

  AD8232Sensor();

  bool begin(uint8_t signalPin,
             uint8_t leadOffPlusPin,
             uint8_t leadOffMinusPin,
             uint16_t sampleRateHz = 250);

  bool update();
  bool hasNewSample() const;
  Sample getLatestSample() const;

  bool isLeadOff() const;
  bool hasValidSignal() const;
  float getSignalQuality() const;
  float getHeartRateBpm() const;
  uint32_t getLastRRIntervalMs() const;
  uint32_t getLastRPeakTimeMicros() const;
  float getLastQRSAmplitude() const;

  bool consumeBeatDetected();

private:
  uint8_t signalPin;
  uint8_t leadOffPlusPin;
  uint8_t leadOffMinusPin;

  uint32_t sampleIntervalMicros;
  uint32_t lastSampleTimeMicros;

  Sample latestSample;
  bool newSampleAvailable;
  bool beatDetected;
  bool validSignal;

  float baseline;
  float smoothed;
  float previousSmoothed;
  float previousSlope;
  float envelope;
  float noiseFloor;
  float adaptiveThreshold;
  float signalQuality;
  float heartRateBpm;
  float lastQRSAmplitude;

  uint32_t lastBeatTimeMicros;
  uint32_t lastRRIntervalMicros;

  static const uint32_t MIN_RR_INTERVAL_US = 250000;
  static const uint32_t MAX_RR_INTERVAL_US = 1500000;
  static const float BASELINE_ALPHA;
  static const float SMOOTHING_ALPHA;
  static const float ENVELOPE_ALPHA;
  static const float NOISE_ALPHA;

  void processSample(uint16_t rawSample, uint32_t timestampMicros, bool leadOffState);
  void updateSignalQuality(float absFiltered, float absSlope, bool leadOffState);
};

#endif
