/**
 * System Monitoring and Diagnostics Module
 * 
 * Implements comprehensive health monitoring, diagnostic data collection,
 * and system status reporting for the ESP32 Home Automation system.
 * 
 * Features:
 * - Continuous memory usage monitoring
 * - WiFi signal strength tracking
 * - Component health checks
 * - Uptime tracking and maintenance reminders
 * - Diagnostic data collection and reporting
 * - Error logging with timestamps
 */

#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "failsafe.h"

// ========================================
// MONITORING CONFIGURATION
// ========================================
#define HEALTH_CHECK_INTERVAL 60000      // Health check every 60 seconds
#define MEMORY_WARNING_THRESHOLD 15000   // Warn when free heap < 15KB
#define MEMORY_CRITICAL_THRESHOLD 8000   // Critical when free heap < 8KB
#define WIFI_WEAK_SIGNAL_THRESHOLD -70   // Weak WiFi signal threshold (dBm)
#define WIFI_POOR_SIGNAL_THRESHOLD -80   // Poor WiFi signal threshold (dBm)
#define UPTIME_MAINTENANCE_REMINDER 604800000  // 7 days in milliseconds
#define MAX_ERROR_LOG_ENTRIES 50         // Maximum error log entries to keep
#define ERROR_LOG_ENTRY_LENGTH 128       // Maximum length of error message

// ========================================
// HEALTH STATUS ENUMS
// ========================================
enum HealthStatus {
    HEALTH_EXCELLENT = 0,
    HEALTH_GOOD = 1,
    HEALTH_WARNING = 2,
    HEALTH_CRITICAL = 3,
    HEALTH_FAILED = 4
};

enum ComponentStatus {
    COMPONENT_OK = 0,
    COMPONENT_WARNING = 1,
    COMPONENT_ERROR = 2,
    COMPONENT_OFFLINE = 3,
    COMPONENT_NOT_PRESENT = 4
};

// ========================================
// DATA STRUCTURES
// ========================================
struct MemoryStats {
    uint32_t freeHeap;
    uint32_t totalHeap;
    uint32_t minFreeHeap;
    uint32_t maxAllocHeap;
    float fragmentationPercent;
    HealthStatus status;
    unsigned long lastUpdate;
};

struct WiFiStats {
    int rssi;
    bool connected;
    uint32_t reconnectCount;
    unsigned long lastDisconnect;
    unsigned long totalDowntime;
    HealthStatus status;
    unsigned long lastUpdate;
};

struct ComponentHealth {
    ComponentStatus motorControl;
    ComponentStatus relayControl;
    ComponentStatus ledMatrix;
    ComponentStatus ledStrip;
    ComponentStatus touchScreen;
    ComponentStatus irReceiver;
    ComponentStatus mqttHandler;
    unsigned long lastUpdate;
};

struct SystemHealth {
    HealthStatus overall;
    MemoryStats memory;
    WiFiStats wifi;
    ComponentHealth components;
    unsigned long uptime;
    unsigned long bootCount;
    bool maintenanceRequired;
    unsigned long lastHealthCheck;
};

struct ErrorLogEntry {
    unsigned long timestamp;
    HealthStatus severity;
    char component[32];
    char message[ERROR_LOG_ENTRY_LENGTH];
    bool valid;
};

struct DiagnosticData {
    // System Information
    char chipModel[32];
    uint8_t chipRevision;
    uint8_t cpuCores;
    uint32_t cpuFreqMHz;
    uint32_t flashSize;
    
    // Runtime Statistics
    unsigned long uptime;
    uint32_t resetReason;
    uint32_t bootCount;
    float temperature;  // If temperature sensor available
    
    // Performance Metrics
    uint32_t loopIterations;
    uint32_t averageLoopTime;
    uint32_t maxLoopTime;
    
    // Network Statistics
    char macAddress[18];
    char ipAddress[16];
    uint32_t wifiReconnects;
    uint32_t mqttReconnects;
    
    unsigned long lastUpdate;
};

// ========================================
// GLOBAL VARIABLES
// ========================================
extern SystemHealth systemHealth;
extern DiagnosticData diagnosticData;
extern ErrorLogEntry errorLog[MAX_ERROR_LOG_ENTRIES];
extern int errorLogIndex;
extern int errorLogCount;

// ========================================
// FUNCTION PROTOTYPES
// ========================================

// Initialization
void initSystemMonitor();

// Health Monitoring
void updateSystemHealth();
void checkMemoryHealth();
void checkWiFiHealth();
void checkComponentHealth();
HealthStatus getOverallHealth();

// Component Health Checks
ComponentStatus checkMotorControlHealth();
ComponentStatus checkRelayControlHealth();
ComponentStatus checkLEDMatrixHealth();
ComponentStatus checkLEDStripHealth();
ComponentStatus checkTouchScreenHealth();
ComponentStatus checkIRReceiverHealth();
ComponentStatus checkMQTTHandlerHealth();

// Diagnostic Data Collection
void updateDiagnosticData();
void collectPerformanceMetrics();
void collectNetworkStatistics();
void collectSystemInformation();

// Error Logging
void logError(HealthStatus severity, const char* component, const char* message);
void logWarning(const char* component, const char* message);
void logCritical(const char* component, const char* message);
void clearErrorLog();
int getErrorLogCount();
ErrorLogEntry* getErrorLogEntry(int index);

// Maintenance and Alerts
bool isMaintenanceRequired();
void checkMaintenanceReminders();
void scheduleMaintenanceReboot();

// Reporting Functions
void printHealthReport();
void printDiagnosticReport();
void printErrorLog();
String getHealthReportJSON();
String getDiagnosticReportJSON();
String getErrorLogJSON();

// Utility Functions
const char* healthStatusToString(HealthStatus status);
const char* componentStatusToString(ComponentStatus status);
float calculateCPUUsage();
uint32_t getSystemTemperature();

// ========================================
// IMPLEMENTATION
// ========================================

// Global variable definitions
SystemHealth systemHealth = {
    HEALTH_EXCELLENT,  // overall
    {0, 0, 0, 0, 0.0, HEALTH_EXCELLENT, 0},  // memory
    {0, false, 0, 0, 0, HEALTH_EXCELLENT, 0},  // wifi
    {COMPONENT_OK, COMPONENT_OK, COMPONENT_OK, COMPONENT_OK, COMPONENT_OK, COMPONENT_OK, COMPONENT_OK, 0},  // components
    0,     // uptime
    0,     // bootCount
    false, // maintenanceRequired
    0      // lastHealthCheck
};

DiagnosticData diagnosticData = {
    "",    // chipModel
    0,     // chipRevision
    0,     // cpuCores
    0,     // cpuFreqMHz
    0,     // flashSize
    0,     // uptime
    0,     // resetReason
    0,     // bootCount
    0.0,   // temperature
    0,     // loopIterations
    0,     // averageLoopTime
    0,     // maxLoopTime
    "",    // macAddress
    "",    // ipAddress
    0,     // wifiReconnects
    0,     // mqttReconnects
    0      // lastUpdate
};

ErrorLogEntry errorLog[MAX_ERROR_LOG_ENTRIES];
int errorLogIndex = 0;
int errorLogCount = 0;

void initSystemMonitor() {
    DEBUG_PRINTLN("[MONITOR] Initializing system monitoring...");
    
    // Initialize error log
    clearErrorLog();
    
    // Collect initial system information
    collectSystemInformation();
    
    // Initialize health monitoring
    systemHealth.bootCount = ESP.getBootCount();
    systemHealth.uptime = millis();
    systemHealth.lastHealthCheck = millis();
    
    // Perform initial health check
    updateSystemHealth();
    
    DEBUG_PRINTLN("[MONITOR] System monitoring initialized");
    logError(HEALTH_GOOD, "MONITOR", "System monitoring started");
}

void updateSystemHealth() {
    unsigned long now = millis();
    
    // Update uptime
    systemHealth.uptime = now;
    
    // Check memory health
    checkMemoryHealth();
    
    // Check WiFi health
    checkWiFiHealth();
    
    // Check component health
    checkComponentHealth();
    
    // Calculate overall health status
    systemHealth.overall = getOverallHealth();
    
    // Check maintenance requirements
    checkMaintenanceReminders();
    
    systemHealth.lastHealthCheck = now;
    
    // Log health status changes
    static HealthStatus lastOverallHealth = HEALTH_EXCELLENT;
    if (systemHealth.overall != lastOverallHealth) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Health status changed: %s -> %s",
                healthStatusToString(lastOverallHealth),
                healthStatusToString(systemHealth.overall));
        logError(systemHealth.overall, "MONITOR", msg);
        lastOverallHealth = systemHealth.overall;
    }
}

void checkMemoryHealth() {
    systemHealth.memory.freeHeap = ESP.getFreeHeap();
    systemHealth.memory.totalHeap = ESP.getHeapSize();
    systemHealth.memory.minFreeHeap = ESP.getMinFreeHeap();
    systemHealth.memory.maxAllocHeap = ESP.getMaxAllocHeap();
    
    // Calculate fragmentation percentage
    uint32_t usedHeap = systemHealth.memory.totalHeap - systemHealth.memory.freeHeap;
    systemHealth.memory.fragmentationPercent = ((float)usedHeap / systemHealth.memory.totalHeap) * 100.0;
    
    // Determine memory health status
    if (systemHealth.memory.freeHeap < MEMORY_CRITICAL_THRESHOLD) {
        systemHealth.memory.status = HEALTH_CRITICAL;
        logCritical("MEMORY", "Critical memory shortage detected");
    } else if (systemHealth.memory.freeHeap < MEMORY_WARNING_THRESHOLD) {
        systemHealth.memory.status = HEALTH_WARNING;
        static unsigned long lastMemoryWarning = 0;
        if (millis() - lastMemoryWarning > 300000) {  // Warn every 5 minutes
            logWarning("MEMORY", "Low memory warning");
            lastMemoryWarning = millis();
        }
    } else if (systemHealth.memory.fragmentationPercent > 85.0) {
        systemHealth.memory.status = HEALTH_WARNING;
    } else if (systemHealth.memory.fragmentationPercent > 70.0) {
        systemHealth.memory.status = HEALTH_GOOD;
    } else {
        systemHealth.memory.status = HEALTH_EXCELLENT;
    }
    
    systemHealth.memory.lastUpdate = millis();
}

void checkWiFiHealth() {
    systemHealth.wifi.connected = (WiFi.status() == WL_CONNECTED);
    
    if (systemHealth.wifi.connected) {
        systemHealth.wifi.rssi = WiFi.RSSI();
        
        // Determine WiFi health based on signal strength
        if (systemHealth.wifi.rssi > -50) {
            systemHealth.wifi.status = HEALTH_EXCELLENT;
        } else if (systemHealth.wifi.rssi > WIFI_WEAK_SIGNAL_THRESHOLD) {
            systemHealth.wifi.status = HEALTH_GOOD;
        } else if (systemHealth.wifi.rssi > WIFI_POOR_SIGNAL_THRESHOLD) {
            systemHealth.wifi.status = HEALTH_WARNING;
            static unsigned long lastWiFiWarning = 0;
            if (millis() - lastWiFiWarning > 600000) {  // Warn every 10 minutes
                char msg[64];
                snprintf(msg, sizeof(msg), "Weak WiFi signal: %d dBm", systemHealth.wifi.rssi);
                logWarning("WIFI", msg);
                lastWiFiWarning = millis();
            }
        } else {
            systemHealth.wifi.status = HEALTH_CRITICAL;
            char msg[64];
            snprintf(msg, sizeof(msg), "Very poor WiFi signal: %d dBm", systemHealth.wifi.rssi);
            logError(HEALTH_CRITICAL, "WIFI", msg);
        }
    } else {
        systemHealth.wifi.status = HEALTH_FAILED;
        systemHealth.wifi.rssi = -100;
        
        // Track disconnection
        static bool wasConnected = true;
        if (wasConnected) {
            systemHealth.wifi.lastDisconnect = millis();
            systemHealth.wifi.reconnectCount++;
            logError(HEALTH_CRITICAL, "WIFI", "WiFi connection lost");
            wasConnected = false;
        }
        
        // Update total downtime
        if (systemHealth.wifi.lastDisconnect > 0) {
            systemHealth.wifi.totalDowntime += millis() - systemHealth.wifi.lastDisconnect;
        }
    }
    
    systemHealth.wifi.lastUpdate = millis();
}

void checkComponentHealth() {
    // Check each component's health status
    systemHealth.components.motorControl = checkMotorControlHealth();
    systemHealth.components.relayControl = checkRelayControlHealth();
    systemHealth.components.ledMatrix = checkLEDMatrixHealth();
    systemHealth.components.ledStrip = checkLEDStripHealth();
    systemHealth.components.touchScreen = checkTouchScreenHealth();
    systemHealth.components.irReceiver = checkIRReceiverHealth();
    systemHealth.components.mqttHandler = checkMQTTHandlerHealth();
    
    systemHealth.components.lastUpdate = millis();
}

ComponentStatus checkMotorControlHealth() {
    // Check if motor control is responding
    // This is a basic check - in a real implementation, you might test motor movement
    extern bool motorControlInitialized;
    if (!motorControlInitialized) {
        return COMPONENT_NOT_PRESENT;
    }
    
    // Check if motor is stuck or not responding
    // This would require additional sensors or feedback mechanisms
    return COMPONENT_OK;
}

ComponentStatus checkRelayControlHealth() {
    // Check relay control functionality
    extern bool relayControlInitialized;
    if (!relayControlInitialized) {
        return COMPONENT_NOT_PRESENT;
    }
    
    // Basic health check - could be enhanced with relay feedback
    return COMPONENT_OK;
}

ComponentStatus checkLEDMatrixHealth() {
    // Check LED matrix functionality
    // This could involve testing a few pixels or checking for data line issues
    return COMPONENT_OK;
}

ComponentStatus checkLEDStripHealth() {
    // Check LED strip functionality
    return COMPONENT_OK;
}

ComponentStatus checkTouchScreenHealth() {
    // Check touch screen functionality
    extern bool touchScreenInitialized;
    if (!touchScreenInitialized) {
        return COMPONENT_NOT_PRESENT;
    }
    
    // Could check for touch responsiveness or display issues
    return COMPONENT_OK;
}

ComponentStatus checkIRReceiverHealth() {
    // Check IR receiver functionality
    extern bool irReceiverInitialized;
    if (!irReceiverInitialized) {
        return COMPONENT_NOT_PRESENT;
    }
    
    return COMPONENT_OK;
}

ComponentStatus checkMQTTHandlerHealth() {
    // Check MQTT connection and functionality
    extern PubSubClient mqttClient;
    if (!mqttClient.connected()) {
        return COMPONENT_ERROR;
    }
    
    return COMPONENT_OK;
}

HealthStatus getOverallHealth() {
    // Determine overall system health based on component health
    HealthStatus worstStatus = HEALTH_EXCELLENT;
    
    // Memory health is critical
    if (systemHealth.memory.status > worstStatus) {
        worstStatus = systemHealth.memory.status;
    }
    
    // WiFi health affects overall status but isn't critical for basic operation
    if (systemHealth.wifi.status == HEALTH_FAILED) {
        if (worstStatus < HEALTH_WARNING) {
            worstStatus = HEALTH_WARNING;
        }
    } else if (systemHealth.wifi.status > worstStatus && systemHealth.wifi.status != HEALTH_FAILED) {
        worstStatus = systemHealth.wifi.status;
    }
    
    // Component failures affect overall health
    ComponentStatus components[] = {
        systemHealth.components.motorControl,
        systemHealth.components.relayControl,
        systemHealth.components.ledMatrix,
        systemHealth.components.ledStrip,
        systemHealth.components.touchScreen,
        systemHealth.components.irReceiver,
        systemHealth.components.mqttHandler
    };
    
    for (int i = 0; i < 7; i++) {
        if (components[i] == COMPONENT_ERROR) {
            if (worstStatus < HEALTH_WARNING) {
                worstStatus = HEALTH_WARNING;
            }
        }
    }
    
    return worstStatus;
}

void logError(HealthStatus severity, const char* component, const char* message) {
    // Add entry to circular buffer
    ErrorLogEntry* entry = &errorLog[errorLogIndex];
    
    entry->timestamp = millis();
    entry->severity = severity;
    strncpy(entry->component, component, sizeof(entry->component) - 1);
    entry->component[sizeof(entry->component) - 1] = '\0';
    strncpy(entry->message, message, sizeof(entry->message) - 1);
    entry->message[sizeof(entry->message) - 1] = '\0';
    entry->valid = true;
    
    // Update indices
    errorLogIndex = (errorLogIndex + 1) % MAX_ERROR_LOG_ENTRIES;
    if (errorLogCount < MAX_ERROR_LOG_ENTRIES) {
        errorLogCount++;
    }
    
    // Print to serial for debugging
    DEBUG_PRINTF("[%s] %s: %s\n", 
                healthStatusToString(severity), component, message);
}

void logWarning(const char* component, const char* message) {
    logError(HEALTH_WARNING, component, message);
}

void logCritical(const char* component, const char* message) {
    logError(HEALTH_CRITICAL, component, message);
}

void clearErrorLog() {
    for (int i = 0; i < MAX_ERROR_LOG_ENTRIES; i++) {
        errorLog[i].valid = false;
    }
    errorLogIndex = 0;
    errorLogCount = 0;
}

bool isMaintenanceRequired() {
    return systemHealth.maintenanceRequired;
}

void checkMaintenanceReminders() {
    // Check if system has been running for more than 7 days
    if (systemHealth.uptime > UPTIME_MAINTENANCE_REMINDER) {
        if (!systemHealth.maintenanceRequired) {
            systemHealth.maintenanceRequired = true;
            logWarning("MAINTENANCE", "System uptime > 7 days, maintenance recommended");
        }
    }
    
    // Check for memory fragmentation
    if (systemHealth.memory.fragmentationPercent > 90.0) {
        if (!systemHealth.maintenanceRequired) {
            systemHealth.maintenanceRequired = true;
            logWarning("MAINTENANCE", "High memory fragmentation, restart recommended");
        }
    }
    
    // Check for excessive WiFi reconnections
    if (systemHealth.wifi.reconnectCount > 100) {
        if (!systemHealth.maintenanceRequired) {
            systemHealth.maintenanceRequired = true;
            logWarning("MAINTENANCE", "Excessive WiFi reconnections detected");
        }
    }
}

const char* healthStatusToString(HealthStatus status) {
    switch (status) {
        case HEALTH_EXCELLENT: return "EXCELLENT";
        case HEALTH_GOOD: return "GOOD";
        case HEALTH_WARNING: return "WARNING";
        case HEALTH_CRITICAL: return "CRITICAL";
        case HEALTH_FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

const char* componentStatusToString(ComponentStatus status) {
    switch (status) {
        case COMPONENT_OK: return "OK";
        case COMPONENT_WARNING: return "WARNING";
        case COMPONENT_ERROR: return "ERROR";
        case COMPONENT_OFFLINE: return "OFFLINE";
        case COMPONENT_NOT_PRESENT: return "NOT_PRESENT";
        default: return "UNKNOWN";
    }
}

void collectSystemInformation() {
    // Get chip information
    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);
    
    strncpy(diagnosticData.chipModel, ESP.getChipModel(), sizeof(diagnosticData.chipModel) - 1);
    diagnosticData.chipRevision = chipInfo.revision;
    diagnosticData.cpuCores = chipInfo.cores;
    diagnosticData.cpuFreqMHz = ESP.getCpuFreqMHz();
    diagnosticData.flashSize = ESP.getFlashChipSize();
    diagnosticData.resetReason = esp_reset_reason();
    diagnosticData.bootCount = ESP.getBootCount();
    
    // Get network information
    strncpy(diagnosticData.macAddress, WiFi.macAddress().c_str(), sizeof(diagnosticData.macAddress) - 1);
    if (WiFi.status() == WL_CONNECTED) {
        strncpy(diagnosticData.ipAddress, WiFi.localIP().toString().c_str(), sizeof(diagnosticData.ipAddress) - 1);
    } else {
        strcpy(diagnosticData.ipAddress, "0.0.0.0");
    }
    
    diagnosticData.lastUpdate = millis();
}

void updateDiagnosticData() {
    diagnosticData.uptime = millis();
    collectNetworkStatistics();
    collectPerformanceMetrics();
    diagnosticData.lastUpdate = millis();
}

void collectNetworkStatistics() {
    diagnosticData.wifiReconnects = systemHealth.wifi.reconnectCount;
    // MQTT reconnects would need to be tracked in mqtt_handler.h
    // diagnosticData.mqttReconnects = mqttReconnectCount;
}

void collectPerformanceMetrics() {
    // These would need to be tracked in the main loop
    // For now, just update the timestamp
    static unsigned long lastLoopTime = 0;
    static uint32_t loopCount = 0;
    static uint32_t totalLoopTime = 0;
    static uint32_t maxLoop = 0;
    
    unsigned long currentTime = micros();
    if (lastLoopTime > 0) {
        uint32_t loopTime = currentTime - lastLoopTime;
        totalLoopTime += loopTime;
        loopCount++;
        
        if (loopTime > maxLoop) {
            maxLoop = loopTime;
        }
        
        if (loopCount > 0) {
            diagnosticData.averageLoopTime = totalLoopTime / loopCount;
            diagnosticData.maxLoopTime = maxLoop;
            diagnosticData.loopIterations = loopCount;
        }
    }
    lastLoopTime = currentTime;
}

uint32_t getSystemTemperature() {
    // ESP32 doesn't have a built-in temperature sensor accessible via Arduino
    // This would require an external temperature sensor like DS18B20
    // For now, return 0 to indicate no temperature available
    
    // If you have a temperature sensor connected, implement reading here
    // Example for DS18B20:
    // return temperatureSensor.readTemperature();
    
    return 0;
}

String getHealthReportJSON() {
    String json = "{";
    json += "\"overall\":\"" + String(healthStatusToString(systemHealth.overall)) + "\",";
    json += "\"uptime\":" + String(systemHealth.uptime / 1000) + ",";
    json += "\"maintenance_required\":" + String(systemHealth.maintenanceRequired ? "true" : "false") + ",";
    
    json += "\"memory\":{";
    json += "\"free_heap\":" + String(systemHealth.memory.freeHeap) + ",";
    json += "\"total_heap\":" + String(systemHealth.memory.totalHeap) + ",";
    json += "\"fragmentation\":" + String(systemHealth.memory.fragmentationPercent, 1) + ",";
    json += "\"status\":\"" + String(healthStatusToString(systemHealth.memory.status)) + "\"";
    json += "},";
    
    json += "\"wifi\":{";
    json += "\"connected\":" + String(systemHealth.wifi.connected ? "true" : "false") + ",";
    json += "\"rssi\":" + String(systemHealth.wifi.rssi) + ",";
    json += "\"reconnect_count\":" + String(systemHealth.wifi.reconnectCount) + ",";
    json += "\"status\":\"" + String(healthStatusToString(systemHealth.wifi.status)) + "\"";
    json += "},";
    
    json += "\"components\":{";
    json += "\"motor\":\"" + String(componentStatusToString(systemHealth.components.motorControl)) + "\",";
    json += "\"relay\":\"" + String(componentStatusToString(systemHealth.components.relayControl)) + "\",";
    json += "\"led_matrix\":\"" + String(componentStatusToString(systemHealth.components.ledMatrix)) + "\",";
    json += "\"led_strip\":\"" + String(componentStatusToString(systemHealth.components.ledStrip)) + "\",";
    json += "\"touch\":\"" + String(componentStatusToString(systemHealth.components.touchScreen)) + "\",";
    json += "\"ir\":\"" + String(componentStatusToString(systemHealth.components.irReceiver)) + "\",";
    json += "\"mqtt\":\"" + String(componentStatusToString(systemHealth.components.mqttHandler)) + "\"";
    json += "}";
    
    json += "}";
    return json;
}

void printHealthReport() {
    DEBUG_PRINTLN("\n========== SYSTEM HEALTH REPORT ==========");
    DEBUG_PRINTF("Overall Health: %s\n", healthStatusToString(systemHealth.overall));
    DEBUG_PRINTF("Uptime: %lu seconds\n", systemHealth.uptime / 1000);
    DEBUG_PRINTF("Maintenance Required: %s\n", systemHealth.maintenanceRequired ? "YES" : "NO");
    
    DEBUG_PRINTLN("\n--- Memory Status ---");
    DEBUG_PRINTF("Free Heap: %lu bytes\n", systemHealth.memory.freeHeap);
    DEBUG_PRINTF("Total Heap: %lu bytes\n", systemHealth.memory.totalHeap);
    DEBUG_PRINTF("Fragmentation: %.1f%%\n", systemHealth.memory.fragmentationPercent);
    DEBUG_PRINTF("Status: %s\n", healthStatusToString(systemHealth.memory.status));
    
    DEBUG_PRINTLN("\n--- WiFi Status ---");
    DEBUG_PRINTF("Connected: %s\n", systemHealth.wifi.connected ? "YES" : "NO");
    DEBUG_PRINTF("Signal Strength: %d dBm\n", systemHealth.wifi.rssi);
    DEBUG_PRINTF("Reconnect Count: %lu\n", systemHealth.wifi.reconnectCount);
    DEBUG_PRINTF("Status: %s\n", healthStatusToString(systemHealth.wifi.status));
    
    DEBUG_PRINTLN("\n--- Component Status ---");
    DEBUG_PRINTF("Motor Control: %s\n", componentStatusToString(systemHealth.components.motorControl));
    DEBUG_PRINTF("Relay Control: %s\n", componentStatusToString(systemHealth.components.relayControl));
    DEBUG_PRINTF("LED Matrix: %s\n", componentStatusToString(systemHealth.components.ledMatrix));
    DEBUG_PRINTF("LED Strip: %s\n", componentStatusToString(systemHealth.components.ledStrip));
    DEBUG_PRINTF("Touch Screen: %s\n", componentStatusToString(systemHealth.components.touchScreen));
    DEBUG_PRINTF("IR Receiver: %s\n", componentStatusToString(systemHealth.components.irReceiver));
    DEBUG_PRINTF("MQTT Handler: %s\n", componentStatusToString(systemHealth.components.mqttHandler));
    
    DEBUG_PRINTLN("==========================================\n");
}

#endif // SYSTEM_MONITOR_H