#include "system_monitor.h"
#include <WiFi.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_chip_info.h>

// Include PubSubClient to get the full type so we can query connection state.
#include <PubSubClient.h>
extern PubSubClient mqttClient;

// Global variable definitions
SystemHealth systemHealth = {
    HEALTH_EXCELLENT,
    {0, 0, 0, 0, 0.0, HEALTH_EXCELLENT, 0},
    {0, false, 0, 0, 0, HEALTH_EXCELLENT, 0},
    {COMPONENT_OK, COMPONENT_OK, COMPONENT_OK, COMPONENT_OK, COMPONENT_OK, COMPONENT_OK, COMPONENT_OK, 0},
    0,
    0,
    false,
    0
};

DiagnosticData diagnosticData = {
    "",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0.0,
    0,
    0,
    0,
    "",
    "",
    0,
    0,
    0
};

ErrorLogEntry errorLog[MAX_ERROR_LOG_ENTRIES];
int errorLogIndex = 0;
int errorLogCount = 0;

// Boot counter using NVS
Preferences bootPreferences;

void initSystemMonitor() {
    DEBUG_PRINTLN("[MONITOR] Initializing system monitoring...");

    // Initialize error log
    clearErrorLog();

    // Initialize boot counter using NVS/Preferences
    bootPreferences.begin("system", false);  // false = read/write mode
    systemHealth.bootCount = bootPreferences.getUInt("bootCount", 0);
    systemHealth.bootCount++;  // Increment boot count
    bootPreferences.putUInt("bootCount", systemHealth.bootCount);
    bootPreferences.end();

    DEBUG_PRINTF("[MONITOR] Boot count: %lu\n", systemHealth.bootCount);

    // Collect initial system information
    collectSystemInformation();

    // Initialize health monitoring
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
    extern bool motorControlInitialized;
    if (!motorControlInitialized) {
        return COMPONENT_NOT_PRESENT;
    }
    return COMPONENT_OK;
}

ComponentStatus checkRelayControlHealth() {
    extern bool relayControlInitialized;
    if (!relayControlInitialized) {
        return COMPONENT_NOT_PRESENT;
    }
    return COMPONENT_OK;
}

ComponentStatus checkLEDMatrixHealth() {
    return COMPONENT_OK;
}

ComponentStatus checkLEDStripHealth() {
    return COMPONENT_OK;
}

ComponentStatus checkTouchScreenHealth() {
    extern bool touchScreenInitialized;
    if (!touchScreenInitialized) {
        return COMPONENT_NOT_PRESENT;
    }
    return COMPONENT_OK;
}

ComponentStatus checkIRReceiverHealth() {
    extern bool irReceiverInitialized;
    if (!irReceiverInitialized) {
        return COMPONENT_NOT_PRESENT;
    }
    return COMPONENT_OK;
}

ComponentStatus checkMQTTHandlerHealth() {
    extern PubSubClient mqttClient;
    if (!mqttClient.connected()) {
        return COMPONENT_ERROR;
    }
    return COMPONENT_OK;
}

HealthStatus getOverallHealth() {
    HealthStatus worstStatus = HEALTH_EXCELLENT;

    if (systemHealth.memory.status > worstStatus) {
        worstStatus = systemHealth.memory.status;
    }

    if (systemHealth.wifi.status == HEALTH_FAILED) {
        if (worstStatus < HEALTH_WARNING) {
            worstStatus = HEALTH_WARNING;
        }
    } else if (systemHealth.wifi.status > worstStatus && systemHealth.wifi.status != HEALTH_FAILED) {
        worstStatus = systemHealth.wifi.status;
    }

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
    ErrorLogEntry* entry = &errorLog[errorLogIndex];

    entry->timestamp = millis();
    entry->severity = severity;
    strncpy(entry->component, component, sizeof(entry->component) - 1);
    entry->component[sizeof(entry->component) - 1] = '\0';
    strncpy(entry->message, message, sizeof(entry->message) - 1);
    entry->message[sizeof(entry->message) - 1] = '\0';
    entry->valid = true;

    errorLogIndex = (errorLogIndex + 1) % MAX_ERROR_LOG_ENTRIES;
    if (errorLogCount < MAX_ERROR_LOG_ENTRIES) {
        errorLogCount++;
    }

    DEBUG_PRINTF("[%s] %s: %s\n", healthStatusToString(severity), component, message);
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
    if (systemHealth.uptime > UPTIME_MAINTENANCE_REMINDER) {
        if (!systemHealth.maintenanceRequired) {
            systemHealth.maintenanceRequired = true;
            logWarning("MAINTENANCE", "System uptime > 7 days, maintenance recommended");
        }
    }

    if (systemHealth.memory.fragmentationPercent > 90.0) {
        if (!systemHealth.maintenanceRequired) {
            systemHealth.maintenanceRequired = true;
            logWarning("MAINTENANCE", "High memory fragmentation, restart recommended");
        }
    }

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
    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);

    strncpy(diagnosticData.chipModel, ESP.getChipModel(), sizeof(diagnosticData.chipModel) - 1);
    diagnosticData.chipRevision = chipInfo.revision;
    diagnosticData.cpuCores = chipInfo.cores;
    diagnosticData.cpuFreqMHz = ESP.getCpuFreqMHz();
    diagnosticData.flashSize = ESP.getFlashChipSize();
    diagnosticData.resetReason = esp_reset_reason();
    diagnosticData.bootCount = systemHealth.bootCount;

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
}

void collectPerformanceMetrics() {
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

void resetBootCounter() {
    DEBUG_PRINTLN("[MONITOR] Resetting boot counter");

    bootPreferences.begin("system", false);
    bootPreferences.putUInt("bootCount", 0);
    bootPreferences.end();

    systemHealth.bootCount = 0;
    diagnosticData.bootCount = 0;

    logError(HEALTH_GOOD, "MONITOR", "Boot counter reset to 0");
    DEBUG_PRINTLN("[MONITOR] Boot counter reset completed");
}
