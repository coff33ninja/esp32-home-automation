#include "module_system.h"
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include "config.h"
#include "system_monitor.h"

// Global variable definition
ModuleSystemState moduleSystem;

// Implementation

bool initModuleSystem() {
    DEBUG_PRINTLN("[MODULE_SYS] Initializing module system...");
    
    // Initialize module system state
    moduleSystem.initialized = false;
    moduleSystem.registeredCount = 0;
    moduleSystem.activeCount = 0;
    moduleSystem.lastScan = 0;
    
    // Clear all module slots
    for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
        moduleSystem.modules[i].active = false;
        moduleSystem.modules[i].lastUpdate = 0;
        memset(&moduleSystem.modules[i].config, 0, sizeof(PluginModuleConfig));
    }
    
    // Load saved module configuration
    loadModuleConfiguration();
    
    // Register built-in modules
    if (!registerBuiltInModules()) {
        DEBUG_PRINTLN("[MODULE_SYS] Warning: Failed to register some built-in modules");
    }
    
    // Perform initial module scan
    scanForNewModules();
    
    moduleSystem.initialized = true;
    moduleSystem.lastScan = millis();
    
    DEBUG_PRINTF("[MODULE_SYS] Module system initialized. Registered: %d, Active: %d\n",
                 moduleSystem.registeredCount, moduleSystem.activeCount);
    
    return true;
}

void updateModuleSystem() {
    if (!moduleSystem.initialized) {
        return;
    }
    
    unsigned long now = millis();
    
    // Update active modules
    for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
        RegisteredModule* module = &moduleSystem.modules[i];
        
        if (module->active && module->interface.update) {
            // Check if module needs updating
            if (now - module->lastUpdate >= 1000) {  // Update every second
                try {
                    module->interface.update();
                    module->lastUpdate = now;
                    module->config.errorCount = 0;  // Reset error count on successful update
                } catch (...) {
                    module->config.errorCount++;
                    handleModuleError(module->config.moduleId, "Update function failed");
                    
                    // Disable module if too many errors
                    if (module->config.errorCount >= 5) {
                        DEBUG_PRINTF("[MODULE_SYS] Disabling module %s due to repeated errors\n", 
                                   module->config.name);
                        enableModule(module->config.moduleId, false);
                    }
                }
            }
        }
    }
    
    // Periodic module scanning
    if (now - moduleSystem.lastScan >= 30000) {  // Scan every 30 seconds
        scanForNewModules();
        removeDisconnectedModules();
        moduleSystem.lastScan = now;
    }
}

bool registerModule(const PluginModuleConfig& config, const PluginModuleInterface& interface) {
    // Validate configuration
    if (!validateModuleConfig(config)) {
        DEBUG_PRINTF("[MODULE_SYS] Invalid module configuration for %s\n", config.name);
        return false;
    }
    
    // Check if module is already registered
    for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
        if (moduleSystem.modules[i].active && 
            moduleSystem.modules[i].config.moduleId == config.moduleId) {
            DEBUG_PRINTF("[MODULE_SYS] Module %s already registered\n", config.name);
            return false;
        }
    }
    
    // Find empty slot
    for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
        if (!moduleSystem.modules[i].active) {
            moduleSystem.modules[i].config = config;
            moduleSystem.modules[i].interface = interface;
            moduleSystem.modules[i].active = true;
            moduleSystem.modules[i].lastUpdate = 0;
            
            moduleSystem.registeredCount++;
            
            DEBUG_PRINTF("[MODULE_SYS] Registered module: %s (ID: %d)\n", 
                       config.name, config.moduleId);
            
            // Try to detect and initialize the module
            if (interface.detect && interface.detect()) {
                moduleSystem.modules[i].config.detected = true;
                
                if (config.enabled && interface.initialize && interface.initialize()) {
                    moduleSystem.activeCount++;
                    DEBUG_PRINTF("[MODULE_SYS] Module %s initialized and active\n", config.name);
                }
            }
            
            return true;
        }
    }
    
    DEBUG_PRINTLN("[MODULE_SYS] No free slots for new module");
    return false;
}

bool unregisterModule(uint8_t moduleId) {
    for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
        if (moduleSystem.modules[i].active && 
            moduleSystem.modules[i].config.moduleId == moduleId) {
            
            // Shutdown module if active
            if (moduleSystem.modules[i].interface.shutdown) {
                moduleSystem.modules[i].interface.shutdown();
            }
            
            DEBUG_PRINTF("[MODULE_SYS] Unregistered module: %s\n", 
                       moduleSystem.modules[i].config.name);
            
            moduleSystem.modules[i].active = false;
            moduleSystem.registeredCount--;
            
            if (moduleSystem.modules[i].config.enabled) {
                moduleSystem.activeCount--;
            }
            
            return true;
        }
    }
    
    return false;
}

bool enableModule(uint8_t moduleId, bool enable) {
    RegisteredModule* module = getModule(moduleId);
    if (!module) {
        return false;
    }
    
    if (module->config.enabled == enable) {
        return true;  // Already in desired state
    }
    
    module->config.enabled = enable;
    
    if (enable) {
        // Enable module
        if (module->config.detected && module->interface.initialize) {
            if (module->interface.initialize()) {
                moduleSystem.activeCount++;
                DEBUG_PRINTF("[MODULE_SYS] Enabled module: %s\n", module->config.name);
                notifyModuleChange(moduleId, true);
                return true;
            } else {
                module->config.enabled = false;  // Revert on failure
                DEBUG_PRINTF("[MODULE_SYS] Failed to enable module: %s\n", module->config.name);
                return false;
            }
        }
    } else {
        // Disable module
        if (module->interface.shutdown) {
            module->interface.shutdown();
        }
        
        moduleSystem.activeCount--;
        DEBUG_PRINTF("[MODULE_SYS] Disabled module: %s\n", module->config.name);
        notifyModuleChange(moduleId, false);
    }
    
    // Save configuration change
    saveModuleConfiguration();
    
    return true;
}

RegisteredModule* getModule(uint8_t moduleId) {
    for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
        if (moduleSystem.modules[i].active && 
            moduleSystem.modules[i].config.moduleId == moduleId) {
            return &moduleSystem.modules[i];
        }
    }
    
    return nullptr;
}

RegisteredModule* getModuleByName(const char* name) {
    for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
        if (moduleSystem.modules[i].active && 
            strcmp(moduleSystem.modules[i].config.name, name) == 0) {
            return &moduleSystem.modules[i];
        }
    }
    
    return nullptr;
}

void scanForNewModules() {
    DEBUG_PRINTLN("[MODULE_SYS] Scanning for new modules...");
    
    // Check all registered modules for detection status changes
    for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
        RegisteredModule* module = &moduleSystem.modules[i];
        
        if (module->active && module->interface.detect) {
            bool wasDetected = module->config.detected;
            bool isDetected = module->interface.detect();
            
            if (isDetected != wasDetected) {
                module->config.detected = isDetected;
                module->config.lastSeen = millis();
                
                if (isDetected) {
                    DEBUG_PRINTF("[MODULE_SYS] Module detected: %s\n", module->config.name);
                    
                    // Auto-enable if configured to do so
                    if (module->config.enabled && module->interface.initialize) {
                        if (module->interface.initialize()) {
                            moduleSystem.activeCount++;
                        }
                    }
                    
                    notifyModuleChange(module->config.moduleId, true);
                } else {
                    DEBUG_PRINTF("[MODULE_SYS] Module disconnected: %s\n", module->config.name);
                    
                    if (module->config.enabled) {
                        if (module->interface.shutdown) {
                            module->interface.shutdown();
                        }
                        moduleSystem.activeCount--;
                    }
                    
                    notifyModuleChange(module->config.moduleId, false);
                }
            }
        }
    }
}

void removeDisconnectedModules() {
    unsigned long now = millis();
    
    for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
        RegisteredModule* module = &moduleSystem.modules[i];
        
        if (module->active && !module->config.detected) {
            // Check if module has been disconnected for too long
            if (now - module->config.lastSeen > 60000) {  // 1 minute timeout
                DEBUG_PRINTF("[MODULE_SYS] Removing disconnected module: %s\n", module->config.name);
                
                if (module->interface.shutdown) {
                    module->interface.shutdown();
                }
                
                module->active = false;
                moduleSystem.registeredCount--;
                
                if (module->config.enabled) {
                    moduleSystem.activeCount--;
                }
            }
        }
    }
}

// Note: temperature, light, motion and buzzer module implementations
// are provided in src/hardware_detection.cpp to centralize hardware-specific
// detection and control. Duplicate implementations were removed to avoid
// multiple-definition linker errors.

bool registerLightSensorModule() {
    PluginModuleConfig config = {};
    config.moduleId = 102;
    strcpy(config.name, "Light Sensor");
    strcpy(config.description, "I2C ambient light sensor module");
    config.type = PLUGIN_TYPE_SENSOR_INPUT;
    config.interface = INTERFACE_I2C;
    config.capabilities = MODULE_CAP_INPUT | MODULE_CAP_ANALOG | MODULE_CAP_HOTPLUG;
    config.config.i2c.address = I2C_ADDR_LIGHT_SENSOR;
    config.config.i2c.clockSpeed = 100000;
    config.enabled = false;
    config.detected = false;

    PluginModuleInterface interface = {};
    interface.detect = detectLightSensor;
    interface.initialize = initLightSensor;
    interface.update = updateLightSensor;
    interface.shutdown = shutdownLightSensor;
    interface.readValue = [](const String& param, int& value) -> bool {
        if (param == "light_level") {
            value = readLightLevel();
            return true;
        }
        return false;
    };
    interface.getStatus = []() -> String {
        return "Light level: " + String(readLightLevel()) + " lux";
    };

    return registerModule(config, interface);
}

bool registerMotionSensorModule() {
    PluginModuleConfig config = {};
    config.moduleId = 103;
    strcpy(config.name, "Motion Sensor");
    strcpy(config.description, "PIR motion detection sensor");
    config.type = PLUGIN_TYPE_SENSOR_INPUT;
    config.interface = INTERFACE_GPIO;
    config.capabilities = MODULE_CAP_INPUT | MODULE_CAP_DIGITAL | MODULE_CAP_INTERRUPT;
    config.config.gpio.pins[0] = 39;
    config.config.gpio.pinCount = 1;
    config.enabled = false;
    config.detected = false;
    
    PluginModuleInterface interface = {};
    interface.detect = detectMotionSensor;
    interface.initialize = initMotionSensor;
    interface.update = updateMotionSensor;
    interface.shutdown = shutdownMotionSensor;
    interface.readValue = [](const String& param, int& value) -> bool {
        if (param == "motion") {
            value = readMotionState() ? 1 : 0;
            return true;
        }
        return false;
    };
    interface.getStatus = []() -> String {
        return readMotionState() ? "Motion detected" : "No motion";
    };
    
    return registerModule(config, interface);
}

bool registerBuzzerModule() {
    PluginModuleConfig config = {};
    config.moduleId = 104;
    strcpy(config.name, "Buzzer");
    strcpy(config.description, "Piezo buzzer for audio feedback");
    config.type = PLUGIN_TYPE_AUDIO;
    config.interface = INTERFACE_PWM;
    config.capabilities = MODULE_CAP_OUTPUT | MODULE_CAP_PWM;
    config.config.pwm.pin = 17;
    config.config.pwm.frequency = 2000;
    config.config.pwm.resolution = 8;
    config.enabled = false;
    config.detected = false;
    
    PluginModuleInterface interface = {};
    interface.detect = detectBuzzer;
    interface.initialize = initBuzzer;
    interface.update = updateBuzzer;
    interface.shutdown = shutdownBuzzer;
    interface.writeValue = [](const String& param, int value) -> bool {
        if (param == "tone") {
            playTone(value, 500);  // Play tone for 500ms
            return true;
        }
        return false;
    };
    interface.getStatus = []() -> String {
        return "Buzzer ready";
    };
    
    return registerModule(config, interface);
}

// Relay expansion module implementation
static uint8_t relayExpansionState = 0x00;

bool detectRelayExpansion() {
    return testI2CDevice(I2C_ADDR_EXPANSION_RELAY);
}

bool initRelayExpansion() {
    Wire.beginTransmission(I2C_ADDR_EXPANSION_RELAY);
    Wire.write(0x00);  // Set all relays off
    Wire.endTransmission();
    
    relayExpansionState = 0x00;
    DEBUG_PRINTLN("[MODULE] Relay expansion initialized");
    return true;
}

void updateRelayExpansion() {
    // Send current relay state to the module
    Wire.beginTransmission(I2C_ADDR_EXPANSION_RELAY);
    Wire.write(relayExpansionState);
    Wire.endTransmission();
}

bool setRelayExpansionState(int relay, bool state) {
    if (relay < 0 || relay > 7) {
        return false;
    }
    
    if (state) {
        relayExpansionState |= (1 << relay);
    } else {
        relayExpansionState &= ~(1 << relay);
    }
    
    DEBUG_PRINTF("[MODULE] Relay expansion %d set to %s\n", relay, state ? "ON" : "OFF");
    return true;
}

bool getRelayExpansionState(int relay) {
    if (relay < 0 || relay > 7) {
        return false;
    }
    
    return (relayExpansionState & (1 << relay)) != 0;
}

void shutdownRelayExpansion() {
    // Turn off all relays
    relayExpansionState = 0x00;
    updateRelayExpansion();
    DEBUG_PRINTLN("[MODULE] Relay expansion shutdown");
}

// Note: temperature, light, motion and buzzer module implementations
// are centralized in src/hardware_detection.cpp to avoid multiple
// definitions across translation units. The register*Module functions
// in this file reference those implementations via the headers.

// Utility function implementations
String moduleTypeToString(PluginModuleType type) {
    switch (type) {
        case PLUGIN_TYPE_RELAY_EXPANSION: return "Relay Expansion";
        case PLUGIN_TYPE_SENSOR_INPUT: return "Sensor Input";
        case PLUGIN_TYPE_ACTUATOR_OUTPUT: return "Actuator Output";
        case PLUGIN_TYPE_COMMUNICATION: return "Communication";
        case PLUGIN_TYPE_DISPLAY: return "Display";
        case PLUGIN_TYPE_AUDIO: return "Audio";
        case PLUGIN_TYPE_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

String interfaceTypeToString(ModuleInterface interface) {
    switch (interface) {
        case INTERFACE_I2C: return "I2C";
        case INTERFACE_SPI: return "SPI";
        case INTERFACE_UART: return "UART";
        case INTERFACE_GPIO: return "GPIO";
        case INTERFACE_ANALOG: return "Analog";
        case INTERFACE_PWM: return "PWM";
        case INTERFACE_ONEWIRE: return "OneWire";
        default: return "Unknown";
    }
}

void printModuleSystemStatus() {
    DEBUG_PRINTLN("\n[MODULE_SYS] Module System Status:");
    DEBUG_PRINTLN("===================================");
    DEBUG_PRINTF("Registered: %d, Active: %d\n", moduleSystem.registeredCount, moduleSystem.activeCount);
    DEBUG_PRINTLN("-----------------------------------");
    
    for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
        RegisteredModule* module = &moduleSystem.modules[i];
        
        if (module->active) {
            DEBUG_PRINTF("ID: %d, Name: %s\n", module->config.moduleId, module->config.name);
            DEBUG_PRINTF("  Type: %s, Interface: %s\n", 
                       moduleTypeToString(module->config.type).c_str(),
                       interfaceTypeToString(module->config.interface).c_str());
            DEBUG_PRINTF("  Status: %s %s %s\n",
                       module->config.detected ? "DETECTED" : "NOT_DETECTED",
                       module->config.enabled ? "ENABLED" : "DISABLED",
                       (module->config.detected && module->config.enabled) ? "ACTIVE" : "INACTIVE");
            
            if (module->interface.getStatus) {
                DEBUG_PRINTF("  Info: %s\n", module->interface.getStatus().c_str());
            }
            
            DEBUG_PRINTLN();
        }
    }
    
    DEBUG_PRINTLN("===================================\n");
}

void handleModuleError(uint8_t moduleId, const String& error) {
    RegisteredModule* module = getModule(moduleId);
    if (module) {
        DEBUG_PRINTF("[MODULE_SYS] Error in module %s: %s\n", module->config.name, error.c_str());
        
        // Call module's error handler if available
        if (module->interface.onError) {
            module->interface.onError(module->config.errorCount, error);
        }
        
        // Log error to system
        extern void logError(const char* component, const char* message, int severity);
        String errorMsg = String(module->config.name) + ": " + error;
        logError("Module", errorMsg.c_str(), 1);  // Info level
    }
}

void notifyModuleChange(uint8_t moduleId, bool connected) {
    RegisteredModule* module = getModule(moduleId);
    if (module) {
        DEBUG_PRINTF("[MODULE_SYS] Module %s %s\n", 
                   module->config.name, connected ? "connected" : "disconnected");
        
        // Publish to MQTT
        publishModuleStatus();
        
        // Update diagnostic interface
        extern void updateSystemHealth();
        updateSystemHealth();
    }
}

void publishModuleStatus() {
    extern PubSubClient mqttClient;
    
    if (!mqttClient.connected()) {
        return;
    }
    
    String statusMsg = "{\"modules\":[";
    bool first = true;
    
    for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
        RegisteredModule* module = &moduleSystem.modules[i];
        
        if (module->active) {
            if (!first) statusMsg += ",";
            first = false;
            
            statusMsg += "{";
            statusMsg += "\"id\":" + String(module->config.moduleId) + ",";
            statusMsg += "\"name\":\"" + String(module->config.name) + "\",";
            statusMsg += "\"type\":\"" + moduleTypeToString(module->config.type) + "\",";
            statusMsg += "\"detected\":" + String(module->config.detected ? "true" : "false") + ",";
            statusMsg += "\"enabled\":" + String(module->config.enabled ? "true" : "false");
            
            if (module->interface.getStatus) {
                statusMsg += ",\"status\":\"" + module->interface.getStatus() + "\"";
            }
            
            statusMsg += "}";
        }
    }
    
    statusMsg += "]}";
    
    mqttClient.publish("homecontrol/modules/status", statusMsg.c_str());
}