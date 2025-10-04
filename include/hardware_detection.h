/**
 * Hardware Detection Module
 * 
 * Provides automatic detection of optional hardware modules and graceful
 * fallback when hardware is not present. Implements dynamic feature 
 * enabling/disabling based on detected hardware.
 */

#ifndef HARDWARE_DETECTION_H
#define HARDWARE_DETECTION_H

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "config.h"

// Hardware module types
enum HardwareModule {
    HW_MODULE_MOTOR_CONTROL = 0,
    HW_MODULE_RELAY_CONTROL,
    HW_MODULE_LED_MATRIX,
    HW_MODULE_LED_STRIP,
    HW_MODULE_TOUCH_SCREEN,
    HW_MODULE_IR_RECEIVER,
    HW_MODULE_MQTT_HANDLER,
    HW_MODULE_ADDITIONAL_RELAYS,
    HW_MODULE_TEMPERATURE_SENSOR,
    HW_MODULE_LIGHT_SENSOR,
    HW_MODULE_MOTION_SENSOR,
    HW_MODULE_BUZZER,
    HW_MODULE_COUNT
};

// Hardware detection status
enum HardwareStatus {
    HW_STATUS_NOT_DETECTED = 0,
    HW_STATUS_DETECTED,
    HW_STATUS_INITIALIZED,
    HW_STATUS_ERROR,
    HW_STATUS_DISABLED
};

// Hardware module information structure
struct HardwareModuleInfo {
    HardwareModule module;
    const char* name;
    const char* description;
    HardwareStatus status;
    bool required;
    bool enabled;
    int detectionPin;           // Pin used for detection (-1 if not applicable)
    uint8_t i2cAddress;         // I2C address (0 if not I2C device)
    unsigned long lastCheck;    // Last detection check timestamp
    int errorCount;             // Number of detection errors
    void (*initFunction)();     // Initialization function pointer
    bool (*detectFunction)();   // Detection function pointer
};

// Detection configuration
#define HW_DETECTION_INTERVAL 30000     // Check hardware every 30 seconds
#define HW_DETECTION_RETRY_COUNT 3      // Retry detection 3 times before marking as failed
#define HW_DETECTION_TIMEOUT 1000       // Timeout for detection operations (ms)

// I2C addresses for common sensors/modules
#define I2C_ADDR_TEMP_SENSOR 0x48       // DS18B20 or similar
#define I2C_ADDR_LIGHT_SENSOR 0x23      // BH1750 light sensor
#define I2C_ADDR_MOTION_SENSOR 0x29     // Some motion sensors
#define I2C_ADDR_EXPANSION_RELAY 0x20   // PCF8574 I2C relay expander

// Global variables
extern HardwareModuleInfo hardwareModules[HW_MODULE_COUNT];
extern bool hardwareDetectionInitialized;
extern unsigned long lastHardwareCheck;
extern int totalDetectedModules;
extern int totalEnabledModules;

// Function prototypes
bool initHardwareDetection();
void updateHardwareDetection();
bool detectHardwareModule(HardwareModule module);
HardwareStatus getHardwareStatus(HardwareModule module);
bool isHardwareEnabled(HardwareModule module);
void enableHardwareModule(HardwareModule module, bool enable);
const char* getHardwareModuleName(HardwareModule module);
const char* getHardwareStatusString(HardwareStatus status);
void printHardwareStatus();
void saveHardwareConfiguration();
void loadHardwareConfiguration();
bool hotPlugDetection();
void handleHardwareChange(HardwareModule module, HardwareStatus oldStatus, HardwareStatus newStatus);

// Detection functions for each module
bool detectMotorControl();
bool detectRelayControl();
bool detectLEDMatrix();
bool detectLEDStrip();
bool detectTouchScreen();
bool detectIRReceiver();
bool detectMQTTHandler();
bool detectAdditionalRelays();
bool detectTemperatureSensor();
bool detectLightSensor();
bool detectMotionSensor();
bool detectBuzzer();

// Initialization functions for each module
void initMotorControlModule();
void initRelayControlModule();
void initLEDMatrixModule();
void initLEDStripModule();
void initTouchScreenModule();
void initIRReceiverModule();
void initMQTTHandlerModule();
void initAdditionalRelaysModule();
void initTemperatureSensorModule();
void initLightSensorModule();
void initMotionSensorModule();
void initBuzzerModule();

// Utility functions
bool testI2CDevice(uint8_t address);
bool testSPIDevice(int csPin);
bool testDigitalPin(int pin, bool pullup = true);
bool testAnalogPin(int pin);
void scanI2CDevices();
void reportHardwareToMQTT();
void reportHardwareToDiagnostic();

#endif // HARDWARE_DETECTION_H
