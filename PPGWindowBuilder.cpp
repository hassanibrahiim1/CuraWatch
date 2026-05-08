#include "PPGWindowBuilder.h"

PPGWindowBuilder::PPGWindowBuilder()
  : rawIndex(0), windowReady(false), lastWindowMin(0.0f), lastWindowMax(0.0f) {
  reset();
}

void PPGWindowBuilder::reset() {
  rawIndex = 0;
  windowReady = false;
  lastWindowMin = 0.0f;
  lastWindowMax = 0.0f;

  for (int i = 0; i < RAW_WINDOW_SAMPLES; i++) {
    rawSamples[i] = 0.0f;
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

  buildWindow();
  windowReady = true;
  rawIndex = 0;
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

void PPGWindowBuilder::consumeWindow() {
  windowReady = false;
}

void PPGWindowBuilder::buildWindow() {
  float smoothed[RAW_WINDOW_SAMPLES];
  smoothRawSamples(smoothed);
  resampleLinear(smoothed, RAW_WINDOW_SAMPLES, modelWindow, MODEL_WINDOW_SAMPLES);
  normalizeWindow(modelWindow, MODEL_WINDOW_SAMPLES);
}

void PPGWindowBuilder::smoothRawSamples(float* smoothed) const {
  for (int i = 0; i < RAW_WINDOW_SAMPLES; i++) {
    float previous = rawSamples[(i == 0) ? 0 : i - 1];
    float current = rawSamples[i];
    float next = rawSamples[(i == RAW_WINDOW_SAMPLES - 1) ? RAW_WINDOW_SAMPLES - 1 : i + 1];
    smoothed[i] = (previous + current + next) / 3.0f;
  }
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

  lastWindowMin = minValue;
  lastWindowMax = maxValue;

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
