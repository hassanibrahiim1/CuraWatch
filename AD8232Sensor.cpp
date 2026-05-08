#include "AD8232Sensor.h"
#include <math.h>

const float AD8232Sensor::BASELINE_ALPHA = 0.995f;
const float AD8232Sensor::SMOOTHING_ALPHA = 0.18f;
const float AD8232Sensor::ENVELOPE_ALPHA = 0.96f;
const float AD8232Sensor::NOISE_ALPHA = 0.985f;

AD8232Sensor::AD8232Sensor()
  : signalPin(34), leadOffPlusPin(32), leadOffMinusPin(33),
    sampleIntervalMicros(4000), lastSampleTimeMicros(0),
    newSampleAvailable(false), beatDetected(false), validSignal(false),
    baseline(0.0f), smoothed(0.0f), previousSmoothed(0.0f), previousSlope(0.0f),
    envelope(0.0f), noiseFloor(1.0f), adaptiveThreshold(40.0f), signalQuality(0.0f),
    heartRateBpm(0.0f), lastQRSAmplitude(0.0f), lastBeatTimeMicros(0),
    lastRRIntervalMicros(0) {
  latestSample.raw = 0;
  latestSample.filtered = 0.0f;
  latestSample.timestampMicros = 0;
  latestSample.leadOff = true;
}

bool AD8232Sensor::begin(uint8_t signalPin,
                         uint8_t leadOffPlusPin,
                         uint8_t leadOffMinusPin,
                         uint16_t sampleRateHz) {
  if (sampleRateHz == 0) {
    return false;
  }

  this->signalPin = signalPin;
  this->leadOffPlusPin = leadOffPlusPin;
  this->leadOffMinusPin = leadOffMinusPin;
  sampleIntervalMicros = 1000000UL / sampleRateHz;

  pinMode(this->signalPin, INPUT);
  pinMode(this->leadOffPlusPin, INPUT);
  pinMode(this->leadOffMinusPin, INPUT);

#if defined(ESP32)
  analogReadResolution(12);
  analogSetPinAttenuation(this->signalPin, ADC_11db);
#endif

  lastSampleTimeMicros = micros();
  return true;
}

bool AD8232Sensor::update() {
  newSampleAvailable = false;
  beatDetected = false;

  uint32_t now = micros();
  if ((uint32_t)(now - lastSampleTimeMicros) < sampleIntervalMicros) {
    return false;
  }

  lastSampleTimeMicros = now;

  bool leadOffState = digitalRead(leadOffPlusPin) == HIGH ||
                      digitalRead(leadOffMinusPin) == HIGH;
  uint16_t rawSample = analogRead(signalPin);

  processSample(rawSample, now, leadOffState);
  return true;
}

bool AD8232Sensor::hasNewSample() const {
  return newSampleAvailable;
}

AD8232Sensor::Sample AD8232Sensor::getLatestSample() const {
  return latestSample;
}

bool AD8232Sensor::isLeadOff() const {
  return latestSample.leadOff;
}

bool AD8232Sensor::hasValidSignal() const {
  return validSignal;
}

float AD8232Sensor::getSignalQuality() const {
  return signalQuality;
}

float AD8232Sensor::getHeartRateBpm() const {
  return heartRateBpm;
}

uint32_t AD8232Sensor::getLastRRIntervalMs() const {
  return lastRRIntervalMicros / 1000UL;
}

uint32_t AD8232Sensor::getLastRPeakTimeMicros() const {
  return lastBeatTimeMicros;
}

float AD8232Sensor::getLastQRSAmplitude() const {
  return lastQRSAmplitude;
}

bool AD8232Sensor::consumeBeatDetected() {
  bool detected = beatDetected;
  beatDetected = false;
  return detected;
}

void AD8232Sensor::processSample(uint16_t rawSample,
                                 uint32_t timestampMicros,
                                 bool leadOffState) {
  float raw = (float)rawSample;

  if (baseline == 0.0f) {
    baseline = raw;
  }

  baseline = BASELINE_ALPHA * baseline + (1.0f - BASELINE_ALPHA) * raw;

  float highPassed = raw - baseline;
  smoothed = (1.0f - SMOOTHING_ALPHA) * smoothed + SMOOTHING_ALPHA * highPassed;
  float slope = smoothed - previousSmoothed;
  float absFiltered = fabs(smoothed);
  float absSlope = fabs(slope);

  updateSignalQuality(absFiltered, absSlope, leadOffState);

  latestSample.raw = rawSample;
  latestSample.filtered = smoothed;
  latestSample.timestampMicros = timestampMicros;
  latestSample.leadOff = leadOffState;
  newSampleAvailable = true;

  if (leadOffState) {
    validSignal = false;
    signalQuality = 0.0f;
    previousSlope = slope;
    previousSmoothed = smoothed;
    return;
  }

  bool isLocalPeak = previousSlope > 0.0f && slope <= 0.0f;
  bool pastRefractory = lastBeatTimeMicros == 0 ||
                        (timestampMicros - lastBeatTimeMicros) > MIN_RR_INTERVAL_US;

  if (validSignal && isLocalPeak && pastRefractory && previousSmoothed > adaptiveThreshold) {
    beatDetected = true;
    lastQRSAmplitude = previousSmoothed;

    if (lastBeatTimeMicros != 0) {
      lastRRIntervalMicros = timestampMicros - lastBeatTimeMicros;
      if (lastRRIntervalMicros >= MIN_RR_INTERVAL_US &&
          lastRRIntervalMicros <= MAX_RR_INTERVAL_US) {
        heartRateBpm = 60000000.0f / (float)lastRRIntervalMicros;
      }
    }

    lastBeatTimeMicros = timestampMicros;
  }

  previousSlope = slope;
  previousSmoothed = smoothed;
}

void AD8232Sensor::updateSignalQuality(float absFiltered,
                                       float absSlope,
                                       bool leadOffState) {
  envelope = ENVELOPE_ALPHA * envelope + (1.0f - ENVELOPE_ALPHA) * absFiltered;
  noiseFloor = NOISE_ALPHA * noiseFloor + (1.0f - NOISE_ALPHA) * absSlope;

  float thresholdFromNoise = noiseFloor * 6.0f;
  adaptiveThreshold = max(35.0f, min(400.0f, thresholdFromNoise));

  float qualityRatio = envelope / max(noiseFloor * 8.0f, 1.0f);
  signalQuality = constrain((qualityRatio - 0.4f) / 2.0f, 0.0f, 1.0f);
  validSignal = !leadOffState && envelope > 20.0f && signalQuality > 0.15f;
}
