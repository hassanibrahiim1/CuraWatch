#include "PPGWindowBuilder.h"

const float PPGWindowBuilder::MIN_SIGNAL_RANGE = 350.0f;

PPGWindowBuilder::PPGWindowBuilder()
  : rawIndex(0),
    windowReady(false),
    lastRejectedWindow(false),
    lastWindowMin(0.0f),
    lastWindowMax(0.0f),
    lastWindowPeakCount(0) {
  reset();
}

void PPGWindowBuilder::reset() {
  rawIndex = 0;
  windowReady = false;
  lastRejectedWindow = false;
  lastWindowMin = 0.0f;
  lastWindowMax = 0.0f;
  lastWindowPeakCount = 0;

  for (int i = 0; i < RAW_WINDOW_SAMPLES; i++) {
    rawSamples[i] = 0.0f;
    smoothedSamples[i] = 0.0f;
    filteredSamples[i] = 0.0f;
  }

  for (int i = 0; i < MODEL_WINDOW_SAMPLES; i++) {
    modelWindow[i] = 0.0f;
  }
}

bool PPGWindowBuilder::addSample(float rawSample, bool fingerDetected) {
  if (windowReady) {
    return true;
  }

  if (!fingerDetected) {
    reset();
    return false;
  }

  rawSamples[rawIndex++] = rawSample;

  if (rawIndex < RAW_WINDOW_SAMPLES) {
    return false;
  }

  rawIndex = 0;
  if (!buildWindow()) {
    lastRejectedWindow = true;
    return false;
  }

  windowReady = true;
  lastRejectedWindow = false;
  return true;
}

bool PPGWindowBuilder::isWindowReady() const {
  return windowReady;
}

const float* PPGWindowBuilder::getWindow() const {
  return modelWindow;
}

int PPGWindowBuilder::getWindowSize() const {
  return MODEL_WINDOW_SAMPLES;
}

float PPGWindowBuilder::getLastWindowMin() const {
  return lastWindowMin;
}

float PPGWindowBuilder::getLastWindowMax() const {
  return lastWindowMax;
}

int PPGWindowBuilder::getLastWindowPeakCount() const {
  return lastWindowPeakCount;
}

bool PPGWindowBuilder::consumeRejectedWindowFlag() {
  bool rejected = lastRejectedWindow;
  lastRejectedWindow = false;
  return rejected;
}

void PPGWindowBuilder::consumeWindow() {
  windowReady = false;
}

bool PPGWindowBuilder::buildWindow() {
  smoothRawSamples(smoothedSamples);

  float rawMin = smoothedSamples[0];
  float rawMax = smoothedSamples[0];
  for (int i = 1; i < RAW_WINDOW_SAMPLES; i++) {
    rawMin = min(rawMin, smoothedSamples[i]);
    rawMax = max(rawMax, smoothedSamples[i]);
  }

  lastWindowMin = rawMin;
  lastWindowMax = rawMax;
  if ((rawMax - rawMin) < MIN_SIGNAL_RANGE) {
    lastWindowPeakCount = 0;
    return false;
  }

  removeBaselineDrift(smoothedSamples, filteredSamples, RAW_WINDOW_SAMPLES);
  lastWindowPeakCount = countPulsePeaks(filteredSamples, RAW_WINDOW_SAMPLES);
  if (lastWindowPeakCount < 2) {
    return false;
  }

  resampleLinear(filteredSamples, RAW_WINDOW_SAMPLES, modelWindow, MODEL_WINDOW_SAMPLES);
  normalizeWindow(modelWindow, MODEL_WINDOW_SAMPLES);
  return true;
}

void PPGWindowBuilder::smoothRawSamples(float* smoothed) const {
  for (int i = 0; i < RAW_WINDOW_SAMPLES; i++) {
    float previous = rawSamples[(i == 0) ? 0 : i - 1];
    float current = rawSamples[i];
    float next = rawSamples[(i == RAW_WINDOW_SAMPLES - 1) ? RAW_WINDOW_SAMPLES - 1 : i + 1];
    smoothed[i] = (previous + current + next) / 3.0f;
  }
}

void PPGWindowBuilder::removeBaselineDrift(const float* input,
                                           float* output,
                                           int count) const {
  if (count <= 0) {
    return;
  }

  for (int i = 0; i < count; i++) {
    int left = max(0, i - BASELINE_RADIUS_SAMPLES);
    int right = min(count - 1, i + BASELINE_RADIUS_SAMPLES);
    float sum = 0.0f;

    for (int j = left; j <= right; j++) {
      sum += input[j];
    }

    float baseline = sum / (float)(right - left + 1);
    output[i] = input[i] - baseline;
  }
}

int PPGWindowBuilder::countPulsePeaks(const float* samples, int count) const {
  if (count < 3) {
    return 0;
  }

  float minValue = samples[0];
  float maxValue = samples[0];
  for (int i = 1; i < count; i++) {
    minValue = min(minValue, samples[i]);
    maxValue = max(maxValue, samples[i]);
  }

  float range = maxValue - minValue;
  if (range < 1e-6f) {
    return 0;
  }

  float peakThreshold = minValue + (range * 0.55f);
  int peakCount = 0;
  int lastPeakIndex = -MIN_PEAK_DISTANCE_SAMPLES;
  float lastPeakValue = minValue;

  for (int i = 1; i < count - 1; i++) {
    bool isLocalPeak = samples[i] > peakThreshold &&
                       samples[i] > samples[i - 1] &&
                       samples[i] >= samples[i + 1];
    if (!isLocalPeak) {
      continue;
    }

    if (i - lastPeakIndex >= MIN_PEAK_DISTANCE_SAMPLES) {
      peakCount++;
      lastPeakIndex = i;
      lastPeakValue = samples[i];
    } else if (samples[i] > lastPeakValue) {
      lastPeakIndex = i;
      lastPeakValue = samples[i];
    }
  }

  return peakCount;
}

void PPGWindowBuilder::resampleLinear(const float* input,
                                      int inputCount,
                                      float* output,
                                      int outputCount) const {
  if (inputCount <= 1 || outputCount <= 1) {
    return;
  }

  for (int i = 0; i < outputCount; i++) {
    float position = ((float)i * (inputCount - 1)) / (outputCount - 1);
    int leftIndex = (int)position;
    int rightIndex = min(leftIndex + 1, inputCount - 1);
    float fraction = position - leftIndex;

    output[i] = input[leftIndex] +
                (input[rightIndex] - input[leftIndex]) * fraction;
  }
}

void PPGWindowBuilder::normalizeWindow(float* samples, int count) {
  if (count <= 0) {
    return;
  }

  float minValue = samples[0];
  float maxValue = samples[0];

  for (int i = 1; i < count; i++) {
    minValue = min(minValue, samples[i]);
    maxValue = max(maxValue, samples[i]);
  }

  float range = maxValue - minValue;
  if (range < 1e-6f) {
    for (int i = 0; i < count; i++) {
      samples[i] = 0.0f;
    }
    return;
  }

  for (int i = 0; i < count; i++) {
    samples[i] = (samples[i] - minValue) / range;
  }
}
