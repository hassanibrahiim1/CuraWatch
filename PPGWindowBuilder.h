#ifndef PPGWINDOWBUILDER_H
#define PPGWINDOWBUILDER_H

#include <Arduino.h>

class PPGWindowBuilder {
public:
  static const int RAW_WINDOW_SAMPLES = 600;
  static const int MODEL_WINDOW_SAMPLES = 375;

  PPGWindowBuilder();

  void reset();
  bool addSample(float rawSample, bool fingerDetected);

  bool isWindowReady() const;
  const float* getWindow() const;
  int getWindowSize() const;
  float getLastWindowMin() const;
  float getLastWindowMax() const;
  int getLastWindowPeakCount() const;
  bool consumeRejectedWindowFlag();
  void consumeWindow();

private:
  float rawSamples[RAW_WINDOW_SAMPLES];
  float smoothedSamples[RAW_WINDOW_SAMPLES];
  float filteredSamples[RAW_WINDOW_SAMPLES];
  float modelWindow[MODEL_WINDOW_SAMPLES];
  int rawIndex;
  bool windowReady;
  bool lastRejectedWindow;
  float lastWindowMin;
  float lastWindowMax;
  int lastWindowPeakCount;

  static const int BASELINE_RADIUS_SAMPLES = 100;
  static const int MIN_PEAK_DISTANCE_SAMPLES = 70;
  static const float MIN_SIGNAL_RANGE;

  bool buildWindow();
  void smoothRawSamples(float* smoothed) const;
  void removeBaselineDrift(const float* input, float* output, int count) const;
  int countPulsePeaks(const float* samples, int count) const;
  void resampleLinear(const float* input, int inputCount, float* output, int outputCount) const;
  void normalizeWindow(float* samples, int count);
};

#endif
