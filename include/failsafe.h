/**
 * Fail-Safe Module
 * 
 * Implements critical fail-safe logic to ensure the system boots to a safe state.
 * Prevents unexpected loud audio or bright lights on power-up.
 * 
 * Safety Features:
 * - Muted volume on boot
 * - All lights off on boot
 * - LED matrix off on boot
 * - Watchdog timer monitoring
 * - Safe shutdown on errors
 */

#ifndef FAILSAFE_H
#define FAILSAFE_H

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"
#include "motor_control.h"
#include "relay_control.h"
#include <WiFi.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // Default for ESP32 Dev board
#endif

// Fail-safe states
struct FailsafeState {
    bool initialized;
    unsigned long bootTime;
    unsigned long lastHeartbeat;
    bool criticalError;
    char errorMessage[100];
};

// External references
extern CRGB leds[NUM_LEDS];

// Global variables
extern FailsafeState failsafeState;

// Function prototypes
void initFailsafe();
void applyFailsafeState();
void checkFailsafeConditions();
void reportCriticalError(const char* error);
bool isSystemHealthy();
void emergencyShutdown();
void recordHeartbeat();

// Note: implementation and global definitions moved to src/failsafe.cpp

#endif // FAILSAFE_H
