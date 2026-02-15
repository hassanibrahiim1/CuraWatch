#include "MAX30102Sensor.h"

const float MAX30102Sensor::SCALE = 88.0;
const int MAX30102Sensor::FINGER_ON_THRESHOLD = 30000;

MAX30102Sensor::MAX30102Sensor() 
  : avered(0), aveir(0), sumirrms(0), sumredrms(0), sampleCount(0),
    ESpO2(0), FSpO2(0.7), frate(0.95) {
}

bool MAX30102Sensor::begin() {
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    return false;
  }
  
  // Configure sensor
  byte ledBrightness = 0x7F;   // 0=Off to 255=50mA
  byte sampleAverage = 4;       // Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2;             // 2 = Red + IR
  int sampleRate = 200;         // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411;         // Options: 69, 118, 215, 411
  int adcRange = 16384;         // Options: 2048, 4096, 8192, 16384
  
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor.enableDIETEMPRDY();
  
  return true;
}

void MAX30102Sensor::check() {
  particleSensor.check();
}

bool MAX30102Sensor::hasData() {
  return particleSensor.available();
}

void MAX30102Sensor::readSensor(uint32_t &red, uint32_t &ir) {
  // Note: MH-ET LIVE MAX30102 board has swapped outputs
  red = particleSensor.getFIFOIR();
  ir = particleSensor.getFIFORed();
  particleSensor.nextSample();
}

void MAX30102Sensor::updateAverages(double red, double ir) {
  avered = avered * frate + red * (1.0 - frate);
  aveir = aveir * frate + ir * (1.0 - frate);
  sumredrms += (red - avered) * (red - avered);
  sumirrms += (ir - aveir) * (ir - aveir);
  sampleCount++;
}

void MAX30102Sensor::updateSpO2(double red, double ir) {
  double fred = (double)red;
  double fir = (double)ir;
  
  updateAverages(fred, fir);
  
  if (sampleCount >= NUM_SAMPLES) {
    double R = (sqrt(sumredrms) / avered) / (sqrt(sumirrms) / aveir);
    lastSpO2 = calculateSpO2(sqrt(sumredrms) / avered, sqrt(sumirrms) / aveir);
    ESpO2 = FSpO2 * ESpO2 + (1.0 - FSpO2) * lastSpO2;
    
    sumredrms = 0.0;
    sumirrms = 0.0;
    sampleCount = 0;
  }
}

float MAX30102Sensor::calculateSpO2(double redRMS, double irRMS) {
  double R = (redRMS / avered) / (irRMS / aveir);
  return -23.3 * (R - 0.4) + 100;
}

float MAX30102Sensor::getSpO2() const {
  return lastSpO2;
}

float MAX30102Sensor::getFilteredSpO2() const {
  return ESpO2;
}

float MAX30102Sensor::getTemperature() {
  return particleSensor.readTemperatureF();
}

bool MAX30102Sensor::isFingerDetected() const {
  return aveir > FINGER_ON_THRESHOLD;
}

double MAX30102Sensor::getAveragedRed() const {
  return avered;
}

double MAX30102Sensor::getAveragedIR() const {
  return aveir;
}
