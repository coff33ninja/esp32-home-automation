// Hardware-backed wrappers for module system
#include "module_system.h"
#include "hardware_detection.h"
#include "config.h"
#include <Wire.h>
#include <Arduino.h>
#include <FastLED.h>

// --- Light sensor (BH1750) ---
// Conservative BH1750 read using Wire; if device not present returns 0
int readLightLevel() {
  // Simple BH1750 one-shot read (no dependency on external lib)
  const uint8_t addr = I2C_ADDR_LIGHT_SENSOR;
  Wire.beginTransmission(addr);
  // BH1750 one-time high-res mode = 0x20
  Wire.write(0x20);
  if (Wire.endTransmission() != 0) return 0;
  delay(180);
  Wire.requestFrom((int)addr, 2);
  if (Wire.available() < 2) return 0;
  uint16_t raw = Wire.read() << 8;
  raw |= Wire.read();
  // BH1750 returns lux in raw/1.2
  int lux = (int)(raw / 1.2);
  return lux;
}

bool initLightSensor() {
  // Nothing special; hardware_detection will perform init
  return true;
}

void updateLightSensor() {
  // Optional periodic sampling could be done here
}

void shutdownLightSensor() {
  // No action required
}

// --- Motion sensor (PIR) ---
bool readMotionState() {
  // PIR pin is defined in hardware detection as example; use config PIN 39
  int motionPin = 39;
  pinMode(motionPin, INPUT);
  return digitalRead(motionPin) == HIGH;
}

bool initMotionSensor() {
  pinMode(39, INPUT);
  return true;
}

void updateMotionSensor() {
  // Could debounce or generate events here
}

void shutdownMotionSensor() {
  // No-op
}

// --- Buzzer (simple tone using ledc) ---
void playTone(int frequency, int duration) {
  // Use MOTOR_PWM_CHANNEL as channel 0 if available; otherwise no-op
  const int buzzerPin = 17; // as used in hardware_detection
  const int channel = 2; // choose a spare channel
  const int freq = frequency > 0 ? frequency : 1000;
  const int duty = 127; // 50% on 8-bit
  ledcSetup(channel, freq, 8);
  ledcAttachPin(buzzerPin, channel);
  ledcWrite(channel, duty);
  delay(duration);
  ledcWrite(channel, 0);
}

bool initBuzzer() {
  pinMode(17, OUTPUT);
  digitalWrite(17, LOW);
  return true;
}

void updateBuzzer() {
  // no-op
}

void shutdownBuzzer() {
  digitalWrite(17, LOW);
}

// --- Relay expansion helpers are implemented in module_system.cpp ---

// --- Basic validation implementation (same as stub) ---
bool validateModuleConfig(const PluginModuleConfig& config) {
  if (config.moduleId == 0) return false;
  if (strlen(config.name) == 0) return false;
  if (config.capabilities == 0) return false;
  return true;
}
