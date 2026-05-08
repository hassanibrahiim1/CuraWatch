#ifndef PPGWINDOWBUILDER_H
#define PPGWINDOWBUILDER_H

#include <Arduino.h>

class PPGWindowBuilder {
public:
  static const int RAW_WINDOW_SAMPLES = 200;
  static const int MODEL_WINDOW_SAMPLES = 125;

  PPGWindowBuilder();

  void reset();
  bool addSample(float rawSample, bool fingerDetected);

  bool isWindowReady() const;
  const float* getWindow() const;
  int getWindowSize() const;
  float getLastWindowMin() const;
  float getLastWindowMax() const;
  void consumeWindow();

private:
  float rawSamples[RAW_WINDOW_SAMPLES];
  float modelWindow[MODEL_WINDOW_SAMPLES];
  int rawIndex;
  bool windowReady;
  float lastWindowMin;
  float lastWindowMax;

  void buildWindow();
  void smoothRawSamples(float* smoothed) const;
  void resampleLinear(const float* input, int inputCount, float* output, int outputCount) const;
  void normalizeWindow(float* samples, int count);
};

#endif
