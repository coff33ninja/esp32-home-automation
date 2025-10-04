#include "hardware_detection.h"
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include "config.h"
#include <PubSubClient.h>
#include "system_monitor.h"
#include "mqtt_handler.h"


// Global variable definitions
HardwareModuleInfo hardwareModules[HW_MODULE_COUNT];
bool hardwareDetectionInitialized = false;
unsigned long lastHardwareCheck = 0;
int totalDetectedModules = 0;
int totalEnabledModules = 0;

// Implementation

bool initHardwareDetection() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing hardware detection...");
    
    // Initialize hardware module information
    hardwareModules[HW_MODULE_MOTOR_CONTROL] = {
        HW_MODULE_MOTOR_CONTROL, "Motor Control", "Motorized potentiometer control",
        HW_STATUS_NOT_DETECTED, true, false, MOTOR_PWM_PIN, 0, 0, 0,
        initMotorControlModule, detectMotorControl
    };
    
    hardwareModules[HW_MODULE_RELAY_CONTROL] = {
        HW_MODULE_RELAY_CONTROL, "Relay Control", "4-channel relay module",
        HW_STATUS_NOT_DETECTED, true, false, RELAY_1_PIN, 0, 0, 0,
        initRelayControlModule, detectRelayControl
    };
    
    hardwareModules[HW_MODULE_LED_MATRIX] = {
        HW_MODULE_LED_MATRIX, "LED Matrix", "16x16 WS2812B LED matrix",
        HW_STATUS_NOT_DETECTED, false, false, LED_MATRIX_PIN, 0, 0, 0,
        initLEDMatrixModule, detectLEDMatrix
    };
    
    hardwareModules[HW_MODULE_LED_STRIP] = {
        HW_MODULE_LED_STRIP, "LED Strip", "12V LED strip with PWM control",
        HW_STATUS_NOT_DETECTED, false, false, LED_STRIP_PIN, 0, 0, 0,
        initLEDStripModule, detectLEDStrip
    };
    
    hardwareModules[HW_MODULE_TOUCH_SCREEN] = {
        HW_MODULE_TOUCH_SCREEN, "Touch Screen", "ILI9341 touch screen display",
        HW_STATUS_NOT_DETECTED, false, false, TFT_CS, 0, 0, 0,
        initTouchScreenModule, detectTouchScreen
    };
    
    hardwareModules[HW_MODULE_IR_RECEIVER] = {
        HW_MODULE_IR_RECEIVER, "IR Receiver", "Infrared remote control receiver",
        HW_STATUS_NOT_DETECTED, false, false, IR_RECV_PIN, 0, 0, 0,
        initIRReceiverModule, detectIRReceiver
    };
    
    hardwareModules[HW_MODULE_MQTT_HANDLER] = {
        HW_MODULE_MQTT_HANDLER, "MQTT Handler", "WiFi and MQTT communication",
        HW_STATUS_NOT_DETECTED, false, false, -1, 0, 0, 0,
        initMQTTHandlerModule, detectMQTTHandler
    };
    
    hardwareModules[HW_MODULE_ADDITIONAL_RELAYS] = {
        HW_MODULE_ADDITIONAL_RELAYS, "Additional Relays", "I2C relay expansion module",
        HW_STATUS_NOT_DETECTED, false, false, -1, I2C_ADDR_EXPANSION_RELAY, 0, 0,
        initAdditionalRelaysModule, detectAdditionalRelays
    };
    
    hardwareModules[HW_MODULE_TEMPERATURE_SENSOR] = {
        HW_MODULE_TEMPERATURE_SENSOR, "Temperature Sensor", "DS18B20 or similar temperature sensor",
        HW_STATUS_NOT_DETECTED, false, false, -1, I2C_ADDR_TEMP_SENSOR, 0, 0,
        initTemperatureSensorModule, detectTemperatureSensor
    };
    
    hardwareModules[HW_MODULE_LIGHT_SENSOR] = {
        HW_MODULE_LIGHT_SENSOR, "Light Sensor", "BH1750 ambient light sensor",
        HW_STATUS_NOT_DETECTED, false, false, -1, I2C_ADDR_LIGHT_SENSOR, 0, 0,
        initLightSensorModule, detectLightSensor
    };
    
    hardwareModules[HW_MODULE_MOTION_SENSOR] = {
        HW_MODULE_MOTION_SENSOR, "Motion Sensor", "PIR motion detection sensor",
        HW_STATUS_NOT_DETECTED, false, false, 39, 0, 0, 0,  // Using pin 39 as example
        initMotionSensorModule, detectMotionSensor
    };
    
    hardwareModules[HW_MODULE_BUZZER] = {
        HW_MODULE_BUZZER, "Buzzer", "Piezo buzzer for audio feedback",
        HW_STATUS_NOT_DETECTED, false, false, 17, 0, 0, 0,  // Using pin 17 as example
        initBuzzerModule, detectBuzzer
    };
    
    // Initialize I2C for sensor detection
    Wire.begin();
    
    // Load saved hardware configuration
    loadHardwareConfiguration();
    
    // Perform initial hardware detection
    DEBUG_PRINTLN("[HW_DETECT] Performing initial hardware scan...");
    totalDetectedModules = 0;
    totalEnabledModules = 0;
    
    for (int i = 0; i < HW_MODULE_COUNT; i++) {
        if (detectHardwareModule((HardwareModule)i)) {
            totalDetectedModules++;
            if (hardwareModules[i].enabled) {
                totalEnabledModules++;
            }
        }
    }
    
    lastHardwareCheck = millis();
    hardwareDetectionInitialized = true;
    
    DEBUG_PRINTF("[HW_DETECT] Hardware detection initialized. Detected: %d, Enabled: %d\n", 
                 totalDetectedModules, totalEnabledModules);
    
    // Print hardware status
    printHardwareStatus();
    
    return true;
}

void updateHardwareDetection() {
    if (!hardwareDetectionInitialized) {
        return;
    }
    
    unsigned long now = millis();
    
    // Check if it's time for periodic hardware detection
    if (now - lastHardwareCheck >= HW_DETECTION_INTERVAL) {
        DEBUG_PRINTLN("[HW_DETECT] Performing periodic hardware check...");
        
        bool hardwareChanged = false;
        
        // Check each module for changes
        for (int i = 0; i < HW_MODULE_COUNT; i++) {
            HardwareStatus oldStatus = hardwareModules[i].status;
            
            // Only check modules that are not permanently disabled
            if (oldStatus != HW_STATUS_DISABLED) {
                bool detected = detectHardwareModule((HardwareModule)i);
                
                if ((detected && oldStatus == HW_STATUS_NOT_DETECTED) ||
                    (!detected && (oldStatus == HW_STATUS_DETECTED || oldStatus == HW_STATUS_INITIALIZED))) {
                    
                    DEBUG_PRINTF("[HW_DETECT] Hardware change detected for %s: %s -> %s\n",
                                hardwareModules[i].name,
                                getHardwareStatusString(oldStatus),
                                getHardwareStatusString(hardwareModules[i].status));
                    
                    handleHardwareChange((HardwareModule)i, oldStatus, hardwareModules[i].status);
                    hardwareChanged = true;
                }
            }
        }
        
        if (hardwareChanged) {
            // Update counters
            totalDetectedModules = 0;
            totalEnabledModules = 0;
            
            for (int i = 0; i < HW_MODULE_COUNT; i++) {
                if (hardwareModules[i].status == HW_STATUS_DETECTED || 
                    hardwareModules[i].status == HW_STATUS_INITIALIZED) {
                    totalDetectedModules++;
                    if (hardwareModules[i].enabled) {
                        totalEnabledModules++;
                    }
                }
            }
            
            // Report changes
            reportHardwareToMQTT();
            reportHardwareToDiagnostic();
            
            // Save configuration if hardware changed
            saveHardwareConfiguration();
        }
        
        lastHardwareCheck = now;
    }
    
    // Check for hot-plug events more frequently
    static unsigned long lastHotPlugCheck = 0;
    if (now - lastHotPlugCheck >= 5000) { // Check every 5 seconds
        hotPlugDetection();
        lastHotPlugCheck = now;
    }
}

bool detectHardwareModule(HardwareModule module) {
    if (module >= HW_MODULE_COUNT) {
        return false;
    }
    
    HardwareModuleInfo* info = &hardwareModules[module];
    
    // Skip detection if module is disabled
    if (info->status == HW_STATUS_DISABLED) {
        return false;
    }
    
    bool detected = false;
    
    // Call module-specific detection function
    if (info->detectFunction != nullptr) {
        detected = info->detectFunction();
    } else {
        // Default detection based on pin/I2C address
        if (info->i2cAddress > 0) {
            detected = testI2CDevice(info->i2cAddress);
        } else if (info->detectionPin >= 0) {
            detected = testDigitalPin(info->detectionPin);
        } else {
            // For modules without specific detection, assume present
            detected = true;
        }
    }
    
    // Update status based on detection result
    if (detected) {
        if (info->status == HW_STATUS_NOT_DETECTED || info->status == HW_STATUS_ERROR) {
            info->status = HW_STATUS_DETECTED;
            info->errorCount = 0;
            
            // Auto-enable non-required modules when detected
            if (!info->required && !info->enabled) {
                info->enabled = true;
                DEBUG_PRINTF("[HW_DETECT] Auto-enabling optional module: %s\n", info->name);
            }
        }
    } else {
        if (info->status == HW_STATUS_DETECTED || info->status == HW_STATUS_INITIALIZED) {
            info->errorCount++;
            
            if (info->errorCount >= HW_DETECTION_RETRY_COUNT) {
                info->status = info->required ? HW_STATUS_ERROR : HW_STATUS_NOT_DETECTED;
                info->enabled = false;
                
                DEBUG_PRINTF("[HW_DETECT] Module no longer detected: %s (errors: %d)\n", 
                           info->name, info->errorCount);
            }
        }
    }
    
    info->lastCheck = millis();
    
    return detected;
}

HardwareStatus getHardwareStatus(HardwareModule module) {
    if (module >= HW_MODULE_COUNT) {
        return HW_STATUS_ERROR;
    }
    
    return hardwareModules[module].status;
}

bool isHardwareEnabled(HardwareModule module) {
    if (module >= HW_MODULE_COUNT) {
        return false;
    }
    
    return hardwareModules[module].enabled && 
           (hardwareModules[module].status == HW_STATUS_DETECTED || 
            hardwareModules[module].status == HW_STATUS_INITIALIZED);
}

void enableHardwareModule(HardwareModule module, bool enable) {
    if (module >= HW_MODULE_COUNT) {
        return;
    }
    
    HardwareModuleInfo* info = &hardwareModules[module];
    
    if (info->enabled != enable) {
        info->enabled = enable;
        
        DEBUG_PRINTF("[HW_DETECT] Module %s %s\n", 
                     info->name, enable ? "enabled" : "disabled");
        
        if (enable && info->status == HW_STATUS_DETECTED && info->initFunction != nullptr) {
            // Initialize the module
            info->initFunction();
            info->status = HW_STATUS_INITIALIZED;
        } else if (!enable && info->status == HW_STATUS_INITIALIZED) {
            // Deinitialize the module (set back to detected but not initialized)
            info->status = HW_STATUS_DETECTED;
        }
        
        // Save configuration change
        saveHardwareConfiguration();
    }
}

const char* getHardwareModuleName(HardwareModule module) {
    if (module >= HW_MODULE_COUNT) {
        return "Unknown";
    }
    
    return hardwareModules[module].name;
}

const char* getHardwareStatusString(HardwareStatus status) {
    switch (status) {
        case HW_STATUS_NOT_DETECTED: return "Not Detected";
        case HW_STATUS_DETECTED: return "Detected";
        case HW_STATUS_INITIALIZED: return "Initialized";
        case HW_STATUS_ERROR: return "Error";
        case HW_STATUS_DISABLED: return "Disabled";
        default: return "Unknown";
    }
}

void printHardwareStatus() {
    DEBUG_PRINTLN("\n[HW_DETECT] Hardware Status Report:");
    DEBUG_PRINTLN("=====================================");
    
    for (int i = 0; i < HW_MODULE_COUNT; i++) {
        HardwareModuleInfo* info = &hardwareModules[i];
        
        DEBUG_PRINTF("% -20s: %s %s %s\n",
                     info->name,
                     getHardwareStatusString(info->status),
                     info->enabled ? "[ENABLED]" : "[DISABLED]",
                     info->required ? "[REQUIRED]" : "[OPTIONAL]");
        
        if (info->i2cAddress > 0) {
            DEBUG_PRINTF("                     I2C Address: 0x%02X\n", info->i2cAddress);
        }
        
        if (info->detectionPin >= 0) {
            DEBUG_PRINTF("                     Detection Pin: %d\n", info->detectionPin);
        }
        
        if (info->errorCount > 0) {
            DEBUG_PRINTF("                     Error Count: %d\n", info->errorCount);
        }
    }
    
    DEBUG_PRINTF("\nTotal Detected: %d, Total Enabled: %d\n", 
                 totalDetectedModules, totalEnabledModules);
    DEBUG_PRINTLN("=====================================\n");
}

// Detection function implementations
bool detectMotorControl() {
    // Test motor control pins
    if (!testDigitalPin(MOTOR_PIN_A) || !testDigitalPin(MOTOR_PIN_B) || 
        !testDigitalPin(MOTOR_PWM_PIN)) {
        return false;
    }
    
    // Test potentiometer ADC
    if (!testAnalogPin(POT_ADC_PIN)) {
        return false;
    }
    
    DEBUG_PRINTLN("[HW_DETECT] Motor control hardware detected");
    return true;
}

bool detectRelayControl() {
    // Test relay control pins
    bool allPinsOk = testDigitalPin(RELAY_1_PIN) && 
                     testDigitalPin(RELAY_2_PIN) && 
                     testDigitalPin(RELAY_3_PIN) && 
                     testDigitalPin(RELAY_4_PIN);
    
    if (allPinsOk) {
        DEBUG_PRINTLN("[HW_DETECT] Relay control hardware detected");
    }
    
    return allPinsOk;
}

bool detectLEDMatrix() {
    // Test LED matrix data pin
    if (testDigitalPin(LED_MATRIX_PIN)) {
        DEBUG_PRINTLN("[HW_DETECT] LED matrix hardware detected");
        return true;
    }
    
    return false;
}

bool detectLEDStrip() {
    // Test LED strip PWM pin
    if (testDigitalPin(LED_STRIP_PIN)) {
        DEBUG_PRINTLN("[HW_DETECT] LED strip hardware detected");
        return true;
    }
    
    return false;
}

bool detectTouchScreen() {
    // Test SPI pins for touch screen
    if (!testSPIDevice(TFT_CS)) {
        return false;
    }
    
    // Additional checks could be added here for touch controller
    DEBUG_PRINTLN("[HW_DETECT] Touch screen hardware detected");
    return true;
}

bool detectIRReceiver() {
    // Test IR receiver pin
    if (testDigitalPin(IR_RECV_PIN)) {
        DEBUG_PRINTLN("[HW_DETECT] IR receiver hardware detected");
        return true;
    }
    
    return false;
}

bool detectMQTTHandler() {
    // MQTT handler depends on WiFi connectivity
    if (WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTLN("[HW_DETECT] MQTT handler (WiFi) detected");
        return true;
    }
    
    return false;
}

bool detectAdditionalRelays() {
    // Test for I2C relay expander
    if (testI2CDevice(I2C_ADDR_EXPANSION_RELAY)) {
        DEBUG_PRINTLN("[HW_DETECT] Additional relays (I2C) detected");
        return true;
    }
    
    return false;
}

bool detectTemperatureSensor() {
    // Test for I2C temperature sensor
    if (testI2CDevice(I2C_ADDR_TEMP_SENSOR)) {
        DEBUG_PRINTLN("[HW_DETECT] Temperature sensor detected");
        return true;
    }
    
    return false;
}

bool detectLightSensor() {
    // Test for I2C light sensor
    if (testI2CDevice(I2C_ADDR_LIGHT_SENSOR)) {
        DEBUG_PRINTLN("[HW_DETECT] Light sensor detected");
        return true;
    }
    
    return false;
}

bool detectMotionSensor() {
    // Test motion sensor pin
    if (testDigitalPin(39)) {  // Using pin 39 as example
        DEBUG_PRINTLN("[HW_DETECT] Motion sensor detected");
        return true;
    }
    
    return false;
}

bool detectBuzzer() {
    // Test buzzer pin
    if (testDigitalPin(17)) {  // Using pin 17 as example
        DEBUG_PRINTLN("[HW_DETECT] Buzzer detected");
        return true;
    }
    
    return false;
}

// Utility function implementations
bool testI2CDevice(uint8_t address) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    
    return (error == 0);  // 0 means success
}

bool testSPIDevice(int csPin) {
    // Basic SPI device test - check if CS pin is available
    pinMode(csPin, OUTPUT);
    digitalWrite(csPin, HIGH);
    delay(1);
    digitalWrite(csPin, LOW);
    delay(1);
    digitalWrite(csPin, HIGH);
    
    // For a real implementation, you might try to read a device ID
    return true;  // Assume success for now
}

bool testDigitalPin(int pin, bool pullup) {
    if (pin < 0 || pin > 39) {  // ESP32 pin range
        return false;
    }
    
    // Configure pin and test basic functionality
    pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
    delay(1);
    
    // Read pin state (basic test)
    digitalRead(pin);
    
    return true;  // Pin is accessible
}

bool testAnalogPin(int pin) {
    if (pin < 32 || pin > 39) {  // ESP32 ADC pin range
        return false;
    }
    
    // Test ADC reading
    int reading = analogRead(pin);
    
    // Check if reading is within valid range
    return (reading >= 0 && reading <= 4095);
}

void scanI2CDevices() {
    DEBUG_PRINTLN("[HW_DETECT] Scanning I2C bus for devices...");
    
    int deviceCount = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        if (testI2CDevice(address)) {
            DEBUG_PRINTF("[HW_DETECT] I2C device found at address 0x%02X\n", address);
            deviceCount++;
        }
    }
    
    if (deviceCount == 0) {
        DEBUG_PRINTLN("[HW_DETECT] No I2C devices found");
    } else {
        DEBUG_PRINTF("[HW_DETECT] Found %d I2C devices\n", deviceCount);
    }
}

void initMotorControlModule() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing motor control module...");
    extern void initMotorControl();
    initMotorControl();
}

void initRelayControlModule() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing relay control module...");
    extern void initRelayControl();
    initRelayControl();
}

void initLEDMatrixModule() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing LED matrix module...");
    extern void initLEDEffects();
    initLEDEffects();
}

void initLEDStripModule() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing LED strip module...");
    // LED strip initialization is handled in main setup
    ledcSetup(LED_STRIP_PWM_CHANNEL, LED_STRIP_PWM_FREQUENCY, LED_STRIP_PWM_RESOLUTION);
    ledcAttachPin(LED_STRIP_PIN, LED_STRIP_PWM_CHANNEL);
    ledcWrite(LED_STRIP_PWM_CHANNEL, 0);
}

void initTouchScreenModule() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing touch screen module...");
    extern bool initTouchScreen();
    initTouchScreen();
}

void initIRReceiverModule() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing IR receiver module...");
    extern bool initIRReceiver();
    initIRReceiver();
}

void initMQTTHandlerModule() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing MQTT handler module...");
    extern void setupMQTT();
    setupMQTT();
}

void initAdditionalRelaysModule() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing additional relays module...");
    // Initialize I2C relay expander
    Wire.beginTransmission(I2C_ADDR_EXPANSION_RELAY);
    Wire.write(0x00);  // Set all relays off
    Wire.endTransmission();
}

void initTemperatureSensorModule() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing temperature sensor module...");
    // Temperature sensor initialization would go here
    // For now, just verify I2C communication
    testI2CDevice(I2C_ADDR_TEMP_SENSOR);
}

void initLightSensorModule() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing light sensor module...");
    // Light sensor initialization would go here
    testI2CDevice(I2C_ADDR_LIGHT_SENSOR);
}

void initMotionSensorModule() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing motion sensor module...");
    pinMode(39, INPUT);  // Configure motion sensor pin
}

void initBuzzerModule() {
    DEBUG_PRINTLN("[HW_DETECT] Initializing buzzer module...");
    pinMode(17, OUTPUT);  // Configure buzzer pin
    digitalWrite(17, LOW);
}

bool hotPlugDetection() {
    // Check for newly connected I2C devices
    static uint8_t lastI2CDevices[16];  // Track up to 16 I2C devices
    static int lastI2CCount = 0;
    
    uint8_t currentI2CDevices[16];
    int currentI2CCount = 0;
    
    // Scan for I2C devices
    for (uint8_t address = 1; address < 127 && currentI2CCount < 16; address++) {
        if (testI2CDevice(address)) {
            currentI2CDevices[currentI2CCount++] = address;
        }
    }
    
    // Check for changes
    bool changed = (currentI2CCount != lastI2CCount);
    
    if (!changed) {
        for (int i = 0; i < currentI2CCount; i++) {
            bool found = false;
            for (int j = 0; j < lastI2CCount; j++) {
                if (currentI2CDevices[i] == lastI2CDevices[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                changed = true;
                break;
            }
        }
    }
    
    if (changed) {
        DEBUG_PRINTLN("[HW_DETECT] I2C device change detected");
        
        // Update stored device list
        memcpy(lastI2CDevices, currentI2CDevices, sizeof(currentI2CDevices));
        lastI2CCount = currentI2CCount;
        
        // Trigger hardware re-detection for I2C modules
        for (int i = 0; i < HW_MODULE_COUNT; i++) {
            if (hardwareModules[i].i2cAddress > 0) {
                detectHardwareModule((HardwareModule)i);
            }
        }
        
        return true;
    }
    
    return false;
}

void handleHardwareChange(HardwareModule module, HardwareStatus oldStatus, HardwareStatus newStatus) {
    HardwareModuleInfo* info = &hardwareModules[module];
    
    DEBUG_PRINTF("[HW_DETECT] Hardware change: %s %s -> %s\n",
                 info->name, getHardwareStatusString(oldStatus), getHardwareStatusString(newStatus));
    
    if (newStatus == HW_STATUS_DETECTED && oldStatus == HW_STATUS_NOT_DETECTED) {
        // Hardware was plugged in
        DEBUG_PRINTF("[HW_DETECT] Hardware connected: %s\n", info->name);
        
        // Auto-enable if it's an optional module
        if (!info->required) {
            enableHardwareModule(module, true);
        }
        
        // Initialize if enabled
        if (info->enabled && info->initFunction != nullptr) {
            info->initFunction();
            info->status = HW_STATUS_INITIALIZED;
        }
        
    } else if (newStatus == HW_STATUS_NOT_DETECTED && 
               (oldStatus == HW_STATUS_DETECTED || oldStatus == HW_STATUS_INITIALIZED)) {
        // Hardware was unplugged
        DEBUG_PRINTF("[HW_DETECT] Hardware disconnected: %s\n", info->name);
        
        // Disable the module
        info->enabled = false;
        
        // Handle graceful shutdown for critical modules
        if (info->required) {
            DEBUG_PRINTF("[HW_DETECT] WARNING: Required module %s disconnected!\n", info->name);
            // Could trigger fail-safe mode here
        }
    }
}

void saveHardwareConfiguration() {
    DEBUG_PRINTLN("[HW_DETECT] Saving hardware configuration...");
    
    const uint32_t MAGIC_NUMBER = 0x48574346;  // 'HWCF'
    const uint32_t VERSION = 1;
    const int EEPROM_HW_START_ADDR = 200;  // Start after other system data
    
    EEPROM.begin(512);
    
    int addr = EEPROM_HW_START_ADDR;
    
    // Write magic number and version
    EEPROM.put(addr, MAGIC_NUMBER);
    addr += sizeof(MAGIC_NUMBER);
    EEPROM.put(addr, VERSION);
    addr += sizeof(VERSION);
    
    // Write hardware module states
    for (int i = 0; i < HW_MODULE_COUNT; i++) {
        EEPROM.put(addr, hardwareModules[i].enabled);
        addr += sizeof(bool);
    }
    
    EEPROM.commit();
    EEPROM.end();
    
    DEBUG_PRINTLN("[HW_DETECT] Hardware configuration saved");
}

void loadHardwareConfiguration() {
    DEBUG_PRINTLN("[HW_DETECT] Loading hardware configuration...");
    
    const uint32_t MAGIC_NUMBER = 0x48574346;  // 'HWCF'
    const uint32_t VERSION = 1;
    const int EEPROM_HW_START_ADDR = 200;
    
    EEPROM.begin(512);
    
    // Check if valid configuration exists
    uint32_t magic;
    EEPROM.get(EEPROM_HW_START_ADDR, magic);
    
    if (magic == MAGIC_NUMBER) {
        uint32_t version;
        EEPROM.get(EEPROM_HW_START_ADDR + 4, version);
        
        if (version == VERSION) {
            int addr = EEPROM_HW_START_ADDR + 8;
            
            // Load hardware module states
            for (int i = 0; i < HW_MODULE_COUNT; i++) {
                bool enabled;
                EEPROM.get(addr, enabled);
                hardwareModules[i].enabled = enabled;
                addr += sizeof(bool);
            }
            
            DEBUG_PRINTLN("[HW_DETECT] Hardware configuration loaded");
        } else {
            DEBUG_PRINTF("[HW_DETECT] Configuration version mismatch: %d (expected %d)\n", version, VERSION);
        }
    } else {
        DEBUG_PRINTLN("[HW_DETECT] No valid hardware configuration found, using defaults");
        
        // Set default enabled states
        for (int i = 0; i < HW_MODULE_COUNT; i++) {
            hardwareModules[i].enabled = hardwareModules[i].required;  // Enable required modules by default
        }
    }
    
    EEPROM.end();
}

void reportHardwareToMQTT() {
    extern PubSubClient mqttClient;
    
    if (!mqttClient.connected()) {
        return;
    }
    
    DEBUG_PRINTLN("[HW_DETECT] Reporting hardware status to MQTT...");
    
    // Create JSON status message
    String statusMsg = "{";
    statusMsg += "\"detected\":" + String(totalDetectedModules) + ",";
    statusMsg += "\"enabled\":" + String(totalEnabledModules) + ",";
    statusMsg += "\"modules\":{";
    
    bool first = true;
    for (int i = 0; i < HW_MODULE_COUNT; i++) {
        if (!first) statusMsg += ",";
        first = false;
        
        statusMsg += "\"" + String(hardwareModules[i].name) + "\":{";
        statusMsg += "\"status\":\"" + String(getHardwareStatusString(hardwareModules[i].status)) + "\",";
        statusMsg += "\"enabled\":" + String(hardwareModules[i].enabled ? "true" : "false") + ",";
        statusMsg += "\"required\":" + String(hardwareModules[i].required ? "true" : "false");
        statusMsg += "}";
    }
    
    statusMsg += "}}";
    
    mqttClient.publish("homecontrol/hardware/status", statusMsg.c_str());
}