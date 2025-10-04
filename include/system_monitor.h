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
#include <Preferences.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "failsafe.h"

#include "config.h"
// ========================================
// MONITORING CONFIGURATION
// ========================================
// Use canonical HEALTH_CHECK_INTERVAL from config.h when available.
// If `config.h` doesn't define it for some reason, fall back to 60s.
#ifndef HEALTH_CHECK_INTERVAL
#define HEALTH_CHECK_INTERVAL 60000      // Health check every 60 seconds (fallback)
#endif
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
void resetBootCounter();

// Implementation moved to src/system_monitor.cpp to avoid multiple-definition
// errors. Keep this header limited to types, externs and prototypes.

#endif // SYSTEM_MONITOR_H