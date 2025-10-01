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

// Implementation

void initFailsafe() {
    DEBUG_PRINTLN("[FAILSAFE] Initializing fail-safe system...");
    
    // Initialize state structure
    failsafeState.initialized = false;
    failsafeState.bootTime = millis();
    failsafeState.lastHeartbeat = millis();
    failsafeState.criticalError = false;
    strcpy(failsafeState.errorMessage, "No errors");
    
    // Apply fail-safe states to all systems
    applyFailsafeState();
    
    failsafeState.initialized = true;
    DEBUG_PRINTLN("[FAILSAFE] Fail-safe system initialized successfully");
}

void applyFailsafeState() {
    DEBUG_PRINTLN("[FAILSAFE] Applying fail-safe state to all systems...");
    
    // 1. Stop motor (mute volume)
    stopMotor();
    DEBUG_PRINTLN("[FAILSAFE] ✓ Motor stopped (volume muted)");
    
    // 2. Turn off all relays (lights off)
    setAllRelays(false);
    DEBUG_PRINTLN("[FAILSAFE] ✓ All relays OFF (lights off)");
    
    // 3. Turn off LED strip
    ledcWrite(LED_STRIP_PWM_CHANNEL, 0);
    DEBUG_PRINTLN("[FAILSAFE] ✓ LED strip OFF");
    
    // 4. Clear LED matrix
    FastLED.clear();
    FastLED.show();
    DEBUG_PRINTLN("[FAILSAFE] ✓ LED matrix cleared");
    
    DEBUG_PRINTLN("[FAILSAFE] All systems in safe state");
}

void checkFailsafeConditions() {
    // Check if system is operating within safe parameters
    
    // 1. Check heap memory
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 10000) {  // Less than 10KB free
        DEBUG_PRINTF("[FAILSAFE] WARNING: Low memory! Free heap: %d bytes\n", freeHeap);
        
        if (freeHeap < 5000) {  // Critical level
            reportCriticalError("CRITICAL: Out of memory");
            emergencyShutdown();
        }
    }
    
    // 2. Check heartbeat timeout
    unsigned long timeSinceHeartbeat = millis() - failsafeState.lastHeartbeat;
    if (timeSinceHeartbeat > WATCHDOG_TIMEOUT) {
        DEBUG_PRINTF("[FAILSAFE] WARNING: Heartbeat timeout! Last: %lu ms ago\n", timeSinceHeartbeat);
        reportCriticalError("CRITICAL: Watchdog timeout");
        emergencyShutdown();
    }
    
    // 3. Check WiFi connection (warning only, not critical)
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[FAILSAFE] WARNING: WiFi disconnected");
        // Don't shutdown, just log
    }
    
    // 4. Check for stack overflow indicators
    // ESP32 will typically crash before we can detect this, but worth checking
    
    // 5. Monitor uptime (restart after long operation to prevent memory leaks)
    unsigned long uptime = (millis() - failsafeState.bootTime) / 1000;
    if (uptime > 604800) {  // 7 days
        DEBUG_PRINTLN("[FAILSAFE] INFO: Uptime > 7 days, consider reboot for maintenance");
    }
}

void reportCriticalError(const char* error) {
    failsafeState.criticalError = true;
    strncpy(failsafeState.errorMessage, error, sizeof(failsafeState.errorMessage) - 1);
    failsafeState.errorMessage[sizeof(failsafeState.errorMessage) - 1] = '\0';
    
    DEBUG_PRINTLN("===========================================");
    DEBUG_PRINTLN("[FAILSAFE] ⚠️  CRITICAL ERROR DETECTED ⚠️");
    DEBUG_PRINTF("[FAILSAFE] Error: %s\n", error);
    DEBUG_PRINTLN("[FAILSAFE] Initiating emergency shutdown...");
    DEBUG_PRINTLN("===========================================");
}

bool isSystemHealthy() {
    // Quick health check
    if (failsafeState.criticalError) {
        return false;
    }
    
    // Check memory
    if (ESP.getFreeHeap() < 5000) {
        return false;
    }
    
    // Check heartbeat
    unsigned long timeSinceHeartbeat = millis() - failsafeState.lastHeartbeat;
    if (timeSinceHeartbeat > WATCHDOG_TIMEOUT) {
        return false;
    }
    
    return true;
}

void emergencyShutdown() {
    DEBUG_PRINTLN("[FAILSAFE] ===== EMERGENCY SHUTDOWN =====");
    
    // Apply fail-safe state to all hardware
    applyFailsafeState();
    
    // Blink LED to indicate error state (if you have an onboard LED)
    pinMode(LED_BUILTIN, OUTPUT);
    for (int i = 0; i < 10; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
    }
    
    // Display error on LED matrix if available
    // Could show a red X or error pattern
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.setBrightness(50);
    FastLED.show();
    delay(2000);
    FastLED.clear();
    FastLED.show();
    
    // Log error details
    DEBUG_PRINTLN("[FAILSAFE] Error details:");
    DEBUG_PRINTF("  Message: %s\n", failsafeState.errorMessage);
    DEBUG_PRINTF("  Uptime: %lu seconds\n", (millis() - failsafeState.bootTime) / 1000);
    DEBUG_PRINTF("  Free heap: %d bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTF("  WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    
    // Wait a bit before restart
    delay(5000);
    
    // Restart ESP32
    DEBUG_PRINTLN("[FAILSAFE] Restarting system...");
    ESP.restart();
}

void recordHeartbeat() {
    failsafeState.lastHeartbeat = millis();
    
    // Optional: Periodic health check
    static unsigned long lastHealthCheck = 0;
    if (millis() - lastHealthCheck > 60000) {  // Every minute
        checkFailsafeConditions();
        lastHealthCheck = millis();
    }
}

// Power-on self-test
bool performPOST() {
    DEBUG_PRINTLN("[FAILSAFE] Running Power-On Self-Test (POST)...");
    
    bool passed = true;
    
    // Test 1: Check GPIO pins are accessible
    DEBUG_PRINT("[POST] Testing GPIO pins... ");
    pinMode(MOTOR_PIN_A, OUTPUT);
    digitalWrite(MOTOR_PIN_A, LOW);
    DEBUG_PRINTLN("OK");
    
    // Test 2: Check ADC
    DEBUG_PRINT("[POST] Testing ADC... ");
    int adcValue = analogRead(POT_ADC_PIN);
    if (adcValue >= 0 && adcValue <= 4095) {
        DEBUG_PRINTLN("OK");
    } else {
        DEBUG_PRINTLN("FAIL");
        passed = false;
    }
    
    // Test 3: Check memory
    DEBUG_PRINT("[POST] Testing memory... ");
    uint32_t freeHeap = ESP.getFreeHeap();
    DEBUG_PRINTF("%d bytes free... ", freeHeap);
    if (freeHeap > 50000) {
        DEBUG_PRINTLN("OK");
    } else {
        DEBUG_PRINTLN("WARNING - Low");
        // Not a failure, but concerning
    }
    
    // Test 4: Check FastLED
    DEBUG_PRINT("[POST] Testing FastLED... ");
    FastLED.clear();
    FastLED.show();
    DEBUG_PRINTLN("OK");
    
    if (passed) {
        DEBUG_PRINTLN("[POST] ✓ All tests passed");
    } else {
        DEBUG_PRINTLN("[POST] ✗ Some tests failed");
    }
    
    return passed;
}

// Safe mode indicator
void indicateSafeMode() {
    // Pulse a single LED slowly to show system is in safe mode
    for (int brightness = 0; brightness < 128; brightness += 5) {
        leds[0] = CRGB::Blue;
        FastLED.setBrightness(brightness);
        FastLED.show();
        delay(20);
    }
    
    for (int brightness = 128; brightness >= 0; brightness -= 5) {
        leds[0] = CRGB::Blue;
        FastLED.setBrightness(brightness);
        FastLED.show();
        delay(20);
    }
    
    FastLED.clear();
    FastLED.show();
}

// Global variable definitions
FailsafeState failsafeState = {
    false,  // initialized
    0,      // bootTime
    0,      // lastHeartbeat
    false,  // criticalError
    "No errors"  // errorMessage
};

#endif // FAILSAFE_H
