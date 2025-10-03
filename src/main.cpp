/**
 * ESP32 Home Automation & Creative Control Panel
 * Main Firmware File
 * 
 * Author: DJ Kruger
 * Version: 1.0.0
 * GitHub: https://github.com/ideagazm/esp32-home-automation
 * 
 * This firmware integrates multiple control systems:
 * - Motorized potentiometer volume control with fail-safe
 * - 12V LED lighting control (relay + PWM dimming)
 * - Touch screen and LED matrix for visual effects
 * - IR remote control
 * - MQTT/WiFi communication
 * - Watchdog timer for stability
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>

// Project headers
#include "config.h"
#include "motor_control.h"
#include "relay_control.h"
#include "led_effects.h"
#include "mqtt_handler.h"
#include "failsafe.h"
#include "system_monitor.h"
#include "diagnostic_interface.h"
#include "ota_updater.h"
#include "config_manager.h"

// Optional modules (comment out if not using)
#include "touch_handler.h"
#include "ir_handler.h"

// ========================================
// GLOBAL OBJECTS
// ========================================
WiFiClient espClient;
PubSubClient mqttClient(espClient);
CRGB leds[NUM_LEDS];

// ========================================
// GLOBAL VARIABLES
// ========================================
unsigned long lastPotRead = 0;
unsigned long lastMQTTAttempt = 0;
int currentVolume = FAILSAFE_VOLUME;
bool lightsState = FAILSAFE_LIGHTS_STATE;
int brightness = FAILSAFE_BRIGHTNESS;

// ========================================
// FUNCTION PROTOTYPES
// ========================================
void setupWiFi();
void setupMQTT();
void setupGPIO();
void setupLEDs();
void handlePotentiometer();
void handleMQTT();
void heartbeat();

// Forward declarations for LED effects
void setVolumeVisualization(int volume);

// ========================================
// SETUP
// ========================================
void setup() {
    // Initialize serial communication
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    
    DEBUG_PRINTLN("\n\n=================================");
    DEBUG_PRINTLN("ESP32 Home Automation");
    DEBUG_PRINTF("Version: %s\n", FIRMWARE_VERSION);
    DEBUG_PRINTF("Build: %s %s\n", BUILD_DATE, BUILD_TIME);
    DEBUG_PRINTLN("=================================\n");
    
    // Initialize fail-safe state first
    DEBUG_PRINTLN("[SETUP] Initializing fail-safe state...");
    initFailsafe();
    
    // Setup GPIO pins
    DEBUG_PRINTLN("[SETUP] Configuring GPIO pins...");
    setupGPIO();
    
    // Initialize motor control
    DEBUG_PRINTLN("[SETUP] Initializing motor control...");
    initMotorControl();
    
    // Initialize relay control
    DEBUG_PRINTLN("[SETUP] Initializing relay control...");
    initRelayControl();
    
    // Initialize LED strip
    DEBUG_PRINTLN("[SETUP] Initializing LED strip...");
    setupLEDs();
    
    // Initialize LED matrix
    DEBUG_PRINTLN("[SETUP] Initializing LED matrix...");
    FastLED.addLeds<MATRIX_TYPE, LED_MATRIX_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    initLEDEffects();
    
    // Initialize touch screen
    DEBUG_PRINTLN("[SETUP] Initializing touch screen...");
    initTouchScreen();
    
    // Initialize IR receiver
    DEBUG_PRINTLN("[SETUP] Initializing IR receiver...");
    initIRReceiver();
    
    // Connect to WiFi
    DEBUG_PRINTLN("[SETUP] Connecting to WiFi...");
    setupWiFi();
    
    // Connect to MQTT
    DEBUG_PRINTLN("[SETUP] Connecting to MQTT broker...");
    setupMQTT();
    
    // Initialize system monitoring
    DEBUG_PRINTLN("[SETUP] Initializing system monitoring...");
    initSystemMonitor();
    
    // Initialize diagnostic interface
    DEBUG_PRINTLN("[SETUP] Initializing diagnostic interface...");
    initDiagnosticInterface();
    
    // Initialize configuration manager
    DEBUG_PRINTLN("[SETUP] Initializing configuration manager...");
    initConfigManager();
    
    // Initialize OTA updater
    DEBUG_PRINTLN("[SETUP] Initializing OTA updater...");
    initOTAUpdater();
    
    // Initialize watchdog timer
    DEBUG_PRINTLN("[SETUP] Initializing watchdog timer...");
    // esp_task_wdt_init(WATCHDOG_TIMEOUT / 1000, true);
    // esp_task_wdt_add(NULL);
    
    DEBUG_PRINTLN("\n[SETUP] System initialized successfully!");
    DEBUG_PRINTLN("Ready to accept commands.\n");
    
    // First heartbeat
    heartbeat();
}

// ========================================
// MAIN LOOP
// ========================================
void loop() {
    // Handle MQTT connection and messages
    handleMQTT();
    
    // Read potentiometer position
    if (millis() - lastPotRead >= POT_READ_INTERVAL) {
        handlePotentiometer();
        lastPotRead = millis();
    }
    
    // Handle touch input (if enabled)
    if (handleTouch()) {
        // Touch was handled by main interface
    } else {
        // Check if diagnostic interface should handle touch
        extern DiagnosticInterfaceState diagInterface;
        if (diagInterface.active) {
            // Get touch coordinates (this would need to be implemented)
            // For now, we'll handle diagnostic touch in the touch handler
        }
    }
    
    // Update display
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate >= 100) { // Update display every 100ms
        updateDisplay();
        updateDiagnosticInterface(); // Update diagnostic interface if active
        lastDisplayUpdate = millis();
    }
    
    // Handle IR commands (if enabled)
    handleIRInput();
    
    // Update LED effects
    updateEffects();
    
    // Handle serial diagnostic commands
    static String serialCommand = "";
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialCommand.length() > 0) {
                serialCommand.trim();
                if (serialCommand.startsWith("diag ")) {
                    String diagCmd = serialCommand.substring(5);
                    handleSerialDiagnosticCommand(diagCmd.c_str());
                } else if (serialCommand == "diag") {
                    handleSerialDiagnosticCommand("help");
                }
                serialCommand = "";
            }
        } else if (c >= 32 && c <= 126) { // Printable characters
            serialCommand += c;
        }
    }
    
    // Update system health monitoring
    static unsigned long lastHealthCheck = 0;
    if (millis() - lastHealthCheck >= HEALTH_CHECK_INTERVAL) {
        updateSystemHealth();
        lastHealthCheck = millis();
    }
    
    // Handle OTA updates
    handleOTAUpdates();
    
    // Handle configuration auto-save
    static unsigned long lastConfigSave = 0;
    if (millis() - lastConfigSave >= 30000) { // Auto-save every 30 seconds if changed
        if (configManager.hasChanged()) {
            configManager.save();
        }
        lastConfigSave = millis();
    }
    
    // Record heartbeat for fail-safe monitoring
    recordHeartbeat();
    
    // Reset watchdog timer
    // esp_task_wdt_reset();
    
    // Small delay to prevent watchdog triggers
    delay(LOOP_DELAY);
}

// ========================================
// WIFI SETUP
// ========================================
void setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    DEBUG_PRINT("Connecting to WiFi");
    
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && 
           millis() - startAttempt < WIFI_TIMEOUT) {
        delay(500);
        DEBUG_PRINT(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTLN(" Connected!");
        DEBUG_PRINTF("IP Address: %s\n", WiFi.localIP().toString().c_str());
        DEBUG_PRINTF("Signal Strength: %d dBm\n", WiFi.RSSI());
    } else {
        DEBUG_PRINTLN(" Failed!");
        DEBUG_PRINTLN("WARNING: Running in offline mode");
    }
}

// ========================================
// MQTT SETUP
// ========================================
void setupMQTT() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    
    // Try to connect
    if (mqttConnect()) {
        DEBUG_PRINTLN("MQTT connected successfully");
    } else {
        DEBUG_PRINTLN("MQTT connection failed - will retry");
    }
}

// ========================================
// GPIO SETUP
// ========================================
void setupGPIO() {
    // Motor control pins
    pinMode(MOTOR_PIN_A, OUTPUT);
    pinMode(MOTOR_PIN_B, OUTPUT);
    pinMode(MOTOR_PWM_PIN, OUTPUT);
    pinMode(POT_ADC_PIN, INPUT);
    
    // Relay control pins
    pinMode(RELAY_1_PIN, OUTPUT);
    pinMode(RELAY_2_PIN, OUTPUT);
    pinMode(RELAY_3_PIN, OUTPUT);
    pinMode(RELAY_4_PIN, OUTPUT);
    
    // LED control pins
    pinMode(LED_STRIP_PIN, OUTPUT);
    
    // Set all outputs to fail-safe state
    digitalWrite(MOTOR_PIN_A, LOW);
    digitalWrite(MOTOR_PIN_B, LOW);
    digitalWrite(MOTOR_PWM_PIN, LOW);
    digitalWrite(RELAY_1_PIN, LOW);
    digitalWrite(RELAY_2_PIN, LOW);
    digitalWrite(RELAY_3_PIN, LOW);
    digitalWrite(RELAY_4_PIN, LOW);
    digitalWrite(LED_STRIP_PIN, LOW);
}

// ========================================
// LED SETUP
// ========================================
void setupLEDs() {
    // Configure PWM for LED strip
    ledcSetup(LED_STRIP_PWM_CHANNEL, LED_STRIP_PWM_FREQUENCY, LED_STRIP_PWM_RESOLUTION);
    ledcAttachPin(LED_STRIP_PIN, LED_STRIP_PWM_CHANNEL);
    ledcWrite(LED_STRIP_PWM_CHANNEL, 0); // Off by default
}

// ========================================
// POTENTIOMETER HANDLER
// ========================================
void handlePotentiometer() {
    int potValue = analogRead(POT_ADC_PIN);
    
    // Validate ADC reading
    if (potValue < 0 || potValue > 4095) {
        DEBUG_PRINTF("[POT] Invalid ADC reading: %d\n", potValue);
        return;
    }
    
    // Map to 0-100 volume range
    int targetVolume = map(potValue, POT_MIN_VALUE, POT_MAX_VALUE, 0, 100);
    targetVolume = constrain(targetVolume, 0, 100);
    
    // Only update if change is significant (debouncing)
    if (abs(targetVolume - currentVolume) > 2) {  // 2% threshold
        currentVolume = targetVolume;
        
        DEBUG_PRINTF("[POT] Volume: %d%% (ADC: %d)\n", currentVolume, potValue);
        
        // Trigger volume visualization on LED matrix
        setVolumeVisualization(currentVolume);
        
        // Publish to MQTT and notify state change
        publishVolume(currentVolume);
        publishStateChange("volume", "changed");
    }
}

// ========================================
// MQTT HANDLER
// ========================================
void handleMQTT() {
    static bool wasConnected = false;
    bool isConnected = mqttClient.connected();
    
    if (!isConnected) {
        // Handle disconnection
        if (wasConnected) {
            DEBUG_PRINTLN("[MQTT] Connection lost");
            wasConnected = false;
        }
        
        unsigned long now = millis();
        if (now - lastMQTTAttempt > MQTT_RECONNECT_DELAY) {
            lastMQTTAttempt = now;
            DEBUG_PRINTLN("[MQTT] Attempting to reconnect...");
            if (mqttConnect()) {
                wasConnected = true;
            }
        }
    } else {
        if (!wasConnected) {
            wasConnected = true;
        }
        mqttClient.loop();
        
        // Process any queued commands
        processQueuedCommands();
    }
}

// ========================================
// HEARTBEAT AND DIAGNOSTICS
// ========================================
void heartbeat() {
    static unsigned long lastHeartbeat = 0;
    static unsigned long lastDiagnostics = 0;
    static unsigned long lastHealthReport = 0;
    static unsigned long lastErrorLog = 0;
    unsigned long now = millis();
    
    // Publish heartbeat every 30 seconds
    if (now - lastHeartbeat >= 30000) {
        publishHeartbeat();
        lastHeartbeat = now;
    }
    
    // Publish detailed diagnostics every 5 minutes
    if (now - lastDiagnostics >= 300000) {
        publishDiagnostics();
        lastDiagnostics = now;
    }
    
    // Publish health report every 2 minutes
    if (now - lastHealthReport >= 120000) {
        publishHealthReport();
        lastHealthReport = now;
    }
    
    // Publish error log every 10 minutes (if there are errors)
    if (now - lastErrorLog >= 600000) {
        publishErrorLog();
        lastErrorLog = now;
    }
    
    // Publish comprehensive status
    if (mqttClient.connected()) {
        publishStatus();
    }
}
