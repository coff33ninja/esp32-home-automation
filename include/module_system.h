/**
 * Expandable Module System
 * 
 * Provides plugin architecture for additional hardware modules with
 * support for hot-plug detection and dynamic configuration management.
 */

#ifndef MODULE_SYSTEM_H
#define MODULE_SYSTEM_H

#include <Arduino.h>
#include <functional>
#include "hardware_detection.h"

// Module system constants
#define MAX_PLUGIN_MODULES 16
#define MODULE_CONFIG_VERSION 1
#define MODULE_NAME_MAX_LENGTH 32
#define MODULE_DESC_MAX_LENGTH 64

// Plugin module types
enum PluginModuleType {
    PLUGIN_TYPE_RELAY_EXPANSION = 0,
    PLUGIN_TYPE_SENSOR_INPUT,
    PLUGIN_TYPE_ACTUATOR_OUTPUT,
    PLUGIN_TYPE_COMMUNICATION,
    PLUGIN_TYPE_DISPLAY,
    PLUGIN_TYPE_AUDIO,
    PLUGIN_TYPE_CUSTOM
};

// Module interface types
enum ModuleInterface {
    INTERFACE_I2C = 0,
    INTERFACE_SPI,
    INTERFACE_UART,
    INTERFACE_GPIO,
    INTERFACE_ANALOG,
    INTERFACE_PWM,
    INTERFACE_ONEWIRE
};

// Module capability flags
#define MODULE_CAP_INPUT        0x01
#define MODULE_CAP_OUTPUT       0x02
#define MODULE_CAP_ANALOG       0x04
#define MODULE_CAP_DIGITAL      0x08
#define MODULE_CAP_PWM          0x10
#define MODULE_CAP_INTERRUPT    0x20
#define MODULE_CAP_HOTPLUG      0x40
#define MODULE_CAP_CONFIGURABLE 0x80

// Plugin module configuration structure
struct PluginModuleConfig {
    uint8_t moduleId;
    char name[MODULE_NAME_MAX_LENGTH];
    char description[MODULE_DESC_MAX_LENGTH];
    PluginModuleType type;
    ModuleInterface interface;
    uint8_t capabilities;
    
    // Interface-specific configuration
    union {
        struct {
            uint8_t address;
            uint32_t clockSpeed;
        } i2c;
        
        struct {
            int csPin;
            uint32_t clockSpeed;
            uint8_t mode;
        } spi;
        
        struct {
            int txPin;
            int rxPin;
            uint32_t baudRate;
        } uart;
        
        struct {
            int pins[8];
            int pinCount;
        } gpio;
        
        struct {
            int pin;
            int resolution;
        } analog;
        
        struct {
            int pin;
            uint32_t frequency;
            uint8_t resolution;
        } pwm;
        
        struct {
            int pin;
            uint8_t mode;  // OneWire mode
        } onewire;
    } config;
    
    bool enabled;
    bool detected;
    unsigned long lastSeen;
    int errorCount;
};

// Plugin module interface structure
struct PluginModuleInterface {
    // Lifecycle functions
    std::function<bool()> detect;
    std::function<bool()> initialize;
    std::function<void()> update;
    std::function<void()> shutdown;
    
    // Data functions
    std::function<bool(const String&, String&)> readData;
    std::function<bool(const String&, const String&)> writeData;
    std::function<bool(const String&, int&)> readValue;
    std::function<bool(const String&, int)> writeValue;
    
    // Configuration functions
    std::function<bool(const String&)> configure;
    std::function<String()> getStatus;
    std::function<String()> getConfiguration;
    
    // Event handlers
    std::function<void(const String&)> onEvent;
    std::function<void(int, const String&)> onError;
};

// Registered plugin module
struct RegisteredModule {
    PluginModuleConfig config;
    PluginModuleInterface interface;
    bool active;
    unsigned long lastUpdate;
};

// Module system state
struct ModuleSystemState {
    bool initialized;
    int registeredCount;
    int activeCount;
    unsigned long lastScan;
    RegisteredModule modules[MAX_PLUGIN_MODULES];
};

// Global variables
extern ModuleSystemState moduleSystem;

// Function prototypes
bool initModuleSystem();
void updateModuleSystem();
bool registerModule(const PluginModuleConfig& config, const PluginModuleInterface& interface);
bool unregisterModule(uint8_t moduleId);
bool enableModule(uint8_t moduleId, bool enable);
bool configureModule(uint8_t moduleId, const String& configData);
RegisteredModule* getModule(uint8_t moduleId);
RegisteredModule* getModuleByName(const char* name);
int getActiveModuleCount();
int getRegisteredModuleCount();
void scanForNewModules();
void removeDisconnectedModules();
bool isModuleActive(uint8_t moduleId);
String getModuleStatus(uint8_t moduleId);
String getAllModulesStatus();
void saveModuleConfiguration();
void loadModuleConfiguration();
void resetModuleConfiguration();

// Built-in module implementations
bool registerBuiltInModules();
bool registerRelayExpansionModule();
bool registerTemperatureSensorModule();
bool registerLightSensorModule();
bool registerMotionSensorModule();
bool registerBuzzerModule();

// Relay expansion module functions
bool detectRelayExpansion();
bool initRelayExpansion();
void updateRelayExpansion();
bool setRelayExpansionState(int relay, bool state);
bool getRelayExpansionState(int relay);
void shutdownRelayExpansion();

// Temperature sensor module functions
//bool detectTemperatureSensor();
bool initTemperatureSensor();
void updateTemperatureSensor();
float readTemperature();
void shutdownTemperatureSensor();

// Light sensor module functions
//bool detectLightSensor();
bool initLightSensor();
void updateLightSensor();
int readLightLevel();
void shutdownLightSensor();

// Motion sensor module functions
//bool detectMotionSensor();
bool initMotionSensor();
void updateMotionSensor();
bool readMotionState();
void shutdownMotionSensor();

// Buzzer module functions
//bool detectBuzzer();
bool initBuzzer();
void updateBuzzer();
void playTone(int frequency, int duration);
void playMelody(const int* frequencies, const int* durations, int noteCount);
void shutdownBuzzer();

// Utility functions
bool validateModuleConfig(const PluginModuleConfig& config);
String moduleTypeToString(PluginModuleType type);
String interfaceTypeToString(ModuleInterface interface);
void printModuleSystemStatus();
void handleModuleError(uint8_t moduleId, const String& error);
void notifyModuleChange(uint8_t moduleId, bool connected);

// MQTT integration
void publishModuleStatus();
void handleModuleCommand(const String& moduleId, const String& command, const String& value);

// Touch screen integration
void showModuleConfigurationScreen();
void handleModuleConfigTouch(int x, int y);
void drawModuleList();
void drawModuleDetails(uint8_t moduleId);

#endif // MODULE_SYSTEM_H
