/**
 * Diagnostic Interface Module
 * 
 * Provides diagnostic interface accessible via touch screen and serial commands.
 * Displays system information, component states, error logs, and allows
 * diagnostic operations.
 */

#ifndef DIAGNOSTIC_INTERFACE_H
#define DIAGNOSTIC_INTERFACE_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "system_monitor.h"

// ========================================
// DIAGNOSTIC INTERFACE CONFIGURATION
// ========================================
#define DIAG_SCREEN_TIMEOUT 60000        // Diagnostic screen timeout (60 seconds)
#define DIAG_REFRESH_INTERVAL 2000       // Refresh diagnostic data every 2 seconds
#define DIAG_LOG_ENTRIES_PER_PAGE 8      // Number of log entries per page
#define DIAG_MAX_PAGES 10                // Maximum diagnostic pages

// ========================================
// DIAGNOSTIC SCREEN STATES
// ========================================
enum DiagnosticScreen {
    DIAG_SCREEN_MAIN = 0,
    DIAG_SCREEN_SYSTEM_INFO,
    DIAG_SCREEN_MEMORY_STATUS,
    DIAG_SCREEN_NETWORK_STATUS,
    DIAG_SCREEN_COMPONENT_STATUS,
    DIAG_SCREEN_ERROR_LOG,
    DIAG_SCREEN_PERFORMANCE,
    DIAG_SCREEN_ACTIONS,
    DIAG_SCREEN_COUNT
};

// ========================================
// DIAGNOSTIC ACTIONS
// ========================================
enum DiagnosticAction {
    DIAG_ACTION_NONE = 0,
    DIAG_ACTION_CLEAR_ERRORS,
    DIAG_ACTION_RESTART_SYSTEM,
    DIAG_ACTION_RESTART_WIFI,
    DIAG_ACTION_RESTART_MQTT,
    DIAG_ACTION_FACTORY_RESET,
    DIAG_ACTION_MEMORY_TEST,
    DIAG_ACTION_COMPONENT_TEST,
    DIAG_ACTION_EXPORT_LOGS
};

// ========================================
// DIAGNOSTIC INTERFACE STATE
// ========================================
struct DiagnosticInterfaceState {
    DiagnosticScreen currentScreen;
    int currentPage;
    int totalPages;
    unsigned long lastRefresh;
    unsigned long screenStartTime;
    bool active;
    bool needsRefresh;
};

// ========================================
// EXTERNAL REFERENCES
// ========================================
extern TFT_eSPI tft;
extern SystemHealth systemHealth;
extern DiagnosticData diagnosticData;
extern ErrorLogEntry errorLog[];
extern int errorLogCount;

// ========================================
// GLOBAL VARIABLES
// ========================================
extern DiagnosticInterfaceState diagInterface;

// ========================================
// FUNCTION PROTOTYPES
// ========================================

// Interface Management
void initDiagnosticInterface();
void showDiagnosticInterface();
void hideDiagnosticInterface();
void updateDiagnosticInterface();
bool handleDiagnosticTouch(int x, int y);
void refreshDiagnosticData();

// Screen Drawing Functions
void drawDiagnosticMainScreen();
void drawSystemInfoScreen();
void drawMemoryStatusScreen();
void drawNetworkStatusScreen();
void drawComponentStatusScreen();
void drawErrorLogScreen();
void drawPerformanceScreen();
void drawActionsScreen();

// Navigation Functions
void nextDiagnosticScreen();
void prevDiagnosticScreen();
void gotoDiagnosticScreen(DiagnosticScreen screen);
void nextDiagnosticPage();
void prevDiagnosticPage();

// Action Functions
void executeDiagnosticAction(DiagnosticAction action);
void clearErrorLog();
void restartSystem();
void restartWiFi();
void restartMQTT();
void performFactoryReset();
void runMemoryTest();
void runComponentTest();
void exportLogs();

// Utility Functions
void drawDiagnosticHeader(const char* title);
void drawDiagnosticButton(int x, int y, int w, int h, const char* text, bool enabled = true);
void drawDiagnosticProgressBar(int x, int y, int w, int h, int value, int maxValue, const char* label);
void drawDiagnosticStatusIndicator(int x, int y, const char* label, bool status, const char* value = nullptr);
const char* formatUptime(unsigned long uptime);
const char* formatMemorySize(uint32_t bytes);
void drawScrollableText(int x, int y, int w, int h, const char* text, int scrollOffset = 0);

// Serial Diagnostic Commands
void handleSerialDiagnosticCommand(const char* command);
void printSerialDiagnosticHelp();
void printSerialSystemInfo();
void printSerialMemoryStatus();
void printSerialNetworkStatus();
void printSerialComponentStatus();
void printSerialErrorLog();
void printSerialPerformanceStats();

// ========================================
// IMPLEMENTATION
// ========================================

// Global variable definitions
DiagnosticInterfaceState diagInterface = {
    DIAG_SCREEN_MAIN,  // currentScreen
    0,                 // currentPage
    1,                 // totalPages
    0,                 // lastRefresh
    0,                 // screenStartTime
    false,             // active
    true               // needsRefresh
};

void initDiagnosticInterface() {
    DEBUG_PRINTLN("[DIAG] Initializing diagnostic interface...");
    
    diagInterface.currentScreen = DIAG_SCREEN_MAIN;
    diagInterface.currentPage = 0;
    diagInterface.totalPages = 1;
    diagInterface.lastRefresh = 0;
    diagInterface.screenStartTime = 0;
    diagInterface.active = false;
    diagInterface.needsRefresh = true;
    
    DEBUG_PRINTLN("[DIAG] Diagnostic interface initialized");
}

void showDiagnosticInterface() {
    DEBUG_PRINTLN("[DIAG] Showing diagnostic interface");
    
    diagInterface.active = true;
    diagInterface.screenStartTime = millis();
    diagInterface.needsRefresh = true;
    diagInterface.currentScreen = DIAG_SCREEN_MAIN;
    diagInterface.currentPage = 0;
    
    // Clear screen and show main diagnostic screen
    tft.fillScreen(0x0000); // Black background
    drawDiagnosticMainScreen();
}

void hideDiagnosticInterface() {
    DEBUG_PRINTLN("[DIAG] Hiding diagnostic interface");
    
    diagInterface.active = false;
    
    // Return to main interface
    extern void drawMainInterface();
    drawMainInterface();
}

void updateDiagnosticInterface() {
    if (!diagInterface.active) {
        return;
    }
    
    unsigned long now = millis();
    
    // Check for timeout
    if (now - diagInterface.screenStartTime > DIAG_SCREEN_TIMEOUT) {
        hideDiagnosticInterface();
        return;
    }
    
    // Refresh data periodically
    if (now - diagInterface.lastRefresh > DIAG_REFRESH_INTERVAL) {
        refreshDiagnosticData();
        diagInterface.needsRefresh = true;
        diagInterface.lastRefresh = now;
    }
    
    // Redraw screen if needed
    if (diagInterface.needsRefresh) {
        switch (diagInterface.currentScreen) {
            case DIAG_SCREEN_MAIN:
                drawDiagnosticMainScreen();
                break;
            case DIAG_SCREEN_SYSTEM_INFO:
                drawSystemInfoScreen();
                break;
            case DIAG_SCREEN_MEMORY_STATUS:
                drawMemoryStatusScreen();
                break;
            case DIAG_SCREEN_NETWORK_STATUS:
                drawNetworkStatusScreen();
                break;
            case DIAG_SCREEN_COMPONENT_STATUS:
                drawComponentStatusScreen();
                break;
            case DIAG_SCREEN_ERROR_LOG:
                drawErrorLogScreen();
                break;
            case DIAG_SCREEN_PERFORMANCE:
                drawPerformanceScreen();
                break;
            case DIAG_SCREEN_ACTIONS:
                drawActionsScreen();
                break;
        }
        diagInterface.needsRefresh = false;
    }
}

bool handleDiagnosticTouch(int x, int y) {
    if (!diagInterface.active) {
        return false;
    }
    
    // Reset timeout
    diagInterface.screenStartTime = millis();
    
    // Handle navigation buttons (common to all screens)
    if (y >= 210 && y <= 240) { // Bottom navigation area
        if (x >= 10 && x <= 60) { // Previous button
            prevDiagnosticScreen();
            return true;
        } else if (x >= 70 && x <= 120) { // Next button
            nextDiagnosticScreen();
            return true;
        } else if (x >= 130 && x <= 180) { // Page up
            prevDiagnosticPage();
            return true;
        } else if (x >= 190 && x <= 240) { // Page down
            nextDiagnosticPage();
            return true;
        } else if (x >= 250 && x <= 310) { // Exit button
            hideDiagnosticInterface();
            return true;
        }
    }
    
    // Handle screen-specific touches
    switch (diagInterface.currentScreen) {
        case DIAG_SCREEN_MAIN:
            // Handle main screen menu touches
            if (x >= 10 && x <= 150 && y >= 40 && y <= 200) {
                int item = (y - 40) / 25;
                if (item < DIAG_SCREEN_COUNT - 1) {
                    gotoDiagnosticScreen((DiagnosticScreen)(item + 1));
                }
                return true;
            }
            break;
            
        case DIAG_SCREEN_ACTIONS:
            // Handle action button touches
            if (x >= 10 && x <= 150) {
                int action = (y - 40) / 30;
                if (action >= 0 && action < 8) {
                    executeDiagnosticAction((DiagnosticAction)(action + 1));
                }
                return true;
            }
            break;
            
        default:
            // Other screens don't have interactive elements
            break;
    }
    
    return true; // Consume the touch event
}

void refreshDiagnosticData() {
    // Update system health data
    updateSystemHealth();
    updateDiagnosticData();
}

void drawDiagnosticMainScreen() {
    tft.fillScreen(0x0000);
    drawDiagnosticHeader("System Diagnostics");
    
    tft.setTextColor(0xFFFF);
    tft.setTextSize(1);
    
    // Menu items
    const char* menuItems[] = {
        "1. System Information",
        "2. Memory Status", 
        "3. Network Status",
        "4. Component Status",
        "5. Error Log",
        "6. Performance Stats",
        "7. Diagnostic Actions"
    };
    
    for (int i = 0; i < 7; i++) {
        int y = 40 + i * 25;
        tft.drawString(menuItems[i], 10, y);
        
        // Add status indicators
        uint16_t statusColor = 0x07E0; // Green by default
        const char* statusText = "OK";
        
        switch (i) {
            case 1: // Memory
                if (systemHealth.memory.status >= HEALTH_WARNING) {
                    statusColor = (systemHealth.memory.status >= HEALTH_CRITICAL) ? 0xF800 : 0xFFE0;
                    statusText = (systemHealth.memory.status >= HEALTH_CRITICAL) ? "CRIT" : "WARN";
                }
                break;
            case 2: // Network
                if (!systemHealth.wifi.connected) {
                    statusColor = 0xF800;
                    statusText = "OFF";
                } else if (systemHealth.wifi.status >= HEALTH_WARNING) {
                    statusColor = 0xFFE0;
                    statusText = "WEAK";
                }
                break;
            case 4: // Error Log
                if (errorLogCount > 0) {
                    statusColor = 0xFFE0;
                    char errorCountStr[10];
                    snprintf(errorCountStr, sizeof(errorCountStr), "%d", errorLogCount);
                    statusText = errorCountStr;
                }
                break;
        }
        
        tft.setTextColor(statusColor);
        tft.drawString(statusText, 200, y);
        tft.setTextColor(0xFFFF);
    }
    
    // Overall system health
    tft.setTextSize(2);
    tft.drawString("System Health:", 10, 200);
    
    uint16_t healthColor = 0x07E0; // Green
    const char* healthText = healthStatusToString(systemHealth.overall);
    
    if (systemHealth.overall >= HEALTH_WARNING) {
        healthColor = (systemHealth.overall >= HEALTH_CRITICAL) ? 0xF800 : 0xFFE0;
    }
    
    tft.setTextColor(healthColor);
    tft.drawString(healthText, 150, 200);
    
    // Navigation help
    tft.setTextColor(0x8410); // Gray
    tft.setTextSize(1);
    tft.drawString("Touch menu item or use navigation buttons", 10, 225);
}

void drawSystemInfoScreen() {
    tft.fillScreen(0x0000);
    drawDiagnosticHeader("System Information");
    
    tft.setTextColor(0xFFFF);
    tft.setTextSize(1);
    
    int y = 35;
    
    // Chip information
    tft.drawString("Chip Model:", 10, y);
    tft.drawString(diagnosticData.chipModel, 120, y);
    y += 15;
    
    char tempStr[50];
    snprintf(tempStr, sizeof(tempStr), "Revision: %d", diagnosticData.chipRevision);
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "CPU Cores: %d", diagnosticData.cpuCores);
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "CPU Freq: %lu MHz", diagnosticData.cpuFreqMHz);
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "Flash Size: %s", formatMemorySize(diagnosticData.flashSize));
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    // Runtime information
    y += 10;
    tft.setTextColor(0x7BEF); // Light blue
    tft.drawString("Runtime Information:", 10, y);
    tft.setTextColor(0xFFFF);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "Uptime: %s", formatUptime(diagnosticData.uptime / 1000));
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "Boot Count: %lu", diagnosticData.bootCount);
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "Reset Reason: %lu", diagnosticData.resetReason);
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    // Firmware information
    y += 10;
    tft.setTextColor(0x7BEF);
    tft.drawString("Firmware:", 10, y);
    tft.setTextColor(0xFFFF);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "Version: %s", FIRMWARE_VERSION);
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "Build: %s %s", BUILD_DATE, BUILD_TIME);
    tft.drawString(tempStr, 10, y);
}

void drawMemoryStatusScreen() {
    tft.fillScreen(0x0000);
    drawDiagnosticHeader("Memory Status");
    
    tft.setTextColor(0xFFFF);
    tft.setTextSize(1);
    
    int y = 35;
    
    // Current memory status
    char tempStr[50];
    snprintf(tempStr, sizeof(tempStr), "Free Heap: %s", formatMemorySize(systemHealth.memory.freeHeap));
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "Total Heap: %s", formatMemorySize(systemHealth.memory.totalHeap));
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "Min Free: %s", formatMemorySize(systemHealth.memory.minFreeHeap));
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "Max Alloc: %s", formatMemorySize(systemHealth.memory.maxAllocHeap));
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    // Memory usage visualization
    y += 10;
    tft.setTextColor(0x7BEF);
    tft.drawString("Memory Usage:", 10, y);
    y += 15;
    
    // Draw memory usage bar
    int usedHeap = systemHealth.memory.totalHeap - systemHealth.memory.freeHeap;
    int usagePercent = (usedHeap * 100) / systemHealth.memory.totalHeap;
    
    drawDiagnosticProgressBar(10, y, 200, 20, usagePercent, 100, "Heap Usage");
    y += 30;
    
    // Fragmentation bar
    drawDiagnosticProgressBar(10, y, 200, 20, (int)systemHealth.memory.fragmentationPercent, 100, "Fragmentation");
    y += 30;
    
    // Memory status
    tft.setTextColor(0xFFFF);
    tft.drawString("Status:", 10, y);
    
    uint16_t statusColor = 0x07E0; // Green
    if (systemHealth.memory.status >= HEALTH_WARNING) {
        statusColor = (systemHealth.memory.status >= HEALTH_CRITICAL) ? 0xF800 : 0xFFE0;
    }
    
    tft.setTextColor(statusColor);
    tft.drawString(healthStatusToString(systemHealth.memory.status), 60, y);
    
    // Memory thresholds
    y += 25;
    tft.setTextColor(0x8410); // Gray
    tft.drawString("Thresholds:", 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "Warning: %s", formatMemorySize(MEMORY_WARNING_THRESHOLD));
    tft.drawString(tempStr, 10, y);
    y += 12;
    
    snprintf(tempStr, sizeof(tempStr), "Critical: %s", formatMemorySize(MEMORY_CRITICAL_THRESHOLD));
    tft.drawString(tempStr, 10, y);
}

void drawNetworkStatusScreen() {
    tft.fillScreen(0x0000);
    drawDiagnosticHeader("Network Status");
    
    tft.setTextColor(0xFFFF);
    tft.setTextSize(1);
    
    int y = 35;
    
    // WiFi Status
    tft.setTextColor(0x7BEF);
    tft.drawString("WiFi Status:", 10, y);
    y += 15;
    
    tft.setTextColor(0xFFFF);
    drawDiagnosticStatusIndicator(10, y, "Connected:", systemHealth.wifi.connected);
    y += 15;
    
    if (systemHealth.wifi.connected) {
        char tempStr[50];
        snprintf(tempStr, sizeof(tempStr), "SSID: %s", WiFi.SSID().c_str());
        tft.drawString(tempStr, 10, y);
        y += 15;
        
        snprintf(tempStr, sizeof(tempStr), "IP: %s", WiFi.localIP().toString().c_str());
        tft.drawString(tempStr, 10, y);
        y += 15;
        
        snprintf(tempStr, sizeof(tempStr), "MAC: %s", WiFi.macAddress().c_str());
        tft.drawString(tempStr, 10, y);
        y += 15;
        
        snprintf(tempStr, sizeof(tempStr), "RSSI: %d dBm", systemHealth.wifi.rssi);
        tft.drawString(tempStr, 10, y);
        
        // Signal strength bar
        int signalPercent = map(systemHealth.wifi.rssi, -100, -30, 0, 100);
        signalPercent = constrain(signalPercent, 0, 100);
        
        uint16_t signalColor = 0x07E0; // Green
        if (systemHealth.wifi.rssi < WIFI_WEAK_SIGNAL_THRESHOLD) {
            signalColor = (systemHealth.wifi.rssi < WIFI_POOR_SIGNAL_THRESHOLD) ? 0xF800 : 0xFFE0;
        }
        
        tft.fillRect(120, y, signalPercent * 2, 10, signalColor);
        tft.drawRect(120, y, 200, 10, 0xFFFF);
        y += 20;
        
        snprintf(tempStr, sizeof(tempStr), "Reconnects: %lu", systemHealth.wifi.reconnectCount);
        tft.drawString(tempStr, 10, y);
        y += 15;
    } else {
        tft.setTextColor(0xF800);
        tft.drawString("WiFi Disconnected", 10, y);
        y += 15;
        tft.setTextColor(0xFFFF);
    }
    
    // MQTT Status
    y += 10;
    tft.setTextColor(0x7BEF);
    tft.drawString("MQTT Status:", 10, y);
    y += 15;
    
    extern PubSubClient mqttClient;
    tft.setTextColor(0xFFFF);
    drawDiagnosticStatusIndicator(10, y, "Connected:", mqttClient.connected());
    y += 15;
    
    if (mqttClient.connected()) {
        char tempStr[50];
        snprintf(tempStr, sizeof(tempStr), "Server: %s:%d", MQTT_SERVER, MQTT_PORT);
        tft.drawString(tempStr, 10, y);
        y += 15;
        
        extern int queueCount;
        snprintf(tempStr, sizeof(tempStr), "Queue: %d messages", queueCount);
        tft.drawString(tempStr, 10, y);
        y += 15;
    } else {
        tft.setTextColor(0xF800);
        tft.drawString("MQTT Disconnected", 10, y);
        tft.setTextColor(0xFFFF);
        y += 15;
    }
}

void drawComponentStatusScreen() {
    tft.fillScreen(0x0000);
    drawDiagnosticHeader("Component Status");
    
    tft.setTextColor(0xFFFF);
    tft.setTextSize(1);
    
    int y = 35;
    
    // Component status list
    const char* componentNames[] = {
        "Motor Control",
        "Relay Control", 
        "LED Matrix",
        "LED Strip",
        "Touch Screen",
        "IR Receiver",
        "MQTT Handler"
    };
    
    ComponentStatus componentStatuses[] = {
        systemHealth.components.motorControl,
        systemHealth.components.relayControl,
        systemHealth.components.ledMatrix,
        systemHealth.components.ledStrip,
        systemHealth.components.touchScreen,
        systemHealth.components.irReceiver,
        systemHealth.components.mqttHandler
    };
    
    for (int i = 0; i < 7; i++) {
        tft.drawString(componentNames[i], 10, y);
        
        // Status indicator
        uint16_t statusColor = 0x07E0; // Green (OK)
        const char* statusText = componentStatusToString(componentStatuses[i]);
        
        switch (componentStatuses[i]) {
            case COMPONENT_WARNING:
                statusColor = 0xFFE0; // Yellow
                break;
            case COMPONENT_ERROR:
                statusColor = 0xF800; // Red
                break;
            case COMPONENT_OFFLINE:
                statusColor = 0x8410; // Gray
                break;
            case COMPONENT_NOT_PRESENT:
                statusColor = 0x4208; // Dark gray
                break;
            default:
                statusColor = 0x07E0; // Green
                break;
        }
        
        tft.setTextColor(statusColor);
        tft.drawString(statusText, 150, y);
        tft.setTextColor(0xFFFF);
        
        y += 18;
    }
    
    // Component test results (if available)
    y += 10;
    tft.setTextColor(0x7BEF);
    tft.drawString("Last Component Test:", 10, y);
    y += 15;
    
    tft.setTextColor(0x8410);
    tft.drawString("No recent tests", 10, y);
}

void drawErrorLogScreen() {
    tft.fillScreen(0x0000);
    drawDiagnosticHeader("Error Log");
    
    tft.setTextColor(0xFFFF);
    tft.setTextSize(1);
    
    if (errorLogCount == 0) {
        tft.drawString("No errors logged", 10, 50);
        return;
    }
    
    // Calculate pagination
    int entriesPerPage = DIAG_LOG_ENTRIES_PER_PAGE;
    int totalPages = (errorLogCount + entriesPerPage - 1) / entriesPerPage;
    diagInterface.totalPages = totalPages;
    
    if (diagInterface.currentPage >= totalPages) {
        diagInterface.currentPage = totalPages - 1;
    }
    
    // Show page info
    char pageInfo[30];
    snprintf(pageInfo, sizeof(pageInfo), "Page %d/%d (%d errors)", 
             diagInterface.currentPage + 1, totalPages, errorLogCount);
    tft.drawString(pageInfo, 10, 35);
    
    // Display error entries
    int y = 55;
    int startIndex = diagInterface.currentPage * entriesPerPage;
    int endIndex = min(startIndex + entriesPerPage, errorLogCount);
    
    for (int i = startIndex; i < endIndex; i++) {
        // Get error entry (newest first)
        int logIndex = (errorLogIndex - 1 - i + MAX_ERROR_LOG_ENTRIES) % MAX_ERROR_LOG_ENTRIES;
        ErrorLogEntry* entry = &errorLog[logIndex];
        
        if (!entry->valid) continue;
        
        // Format timestamp
        unsigned long seconds = entry->timestamp / 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours = minutes / 60;
        
        char timeStr[20];
        snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu:%02lu", 
                hours % 24, minutes % 60, seconds % 60);
        
        // Draw entry
        tft.setTextColor(0x8410); // Gray for timestamp
        tft.drawString(timeStr, 10, y);
        
        // Severity color
        uint16_t severityColor = 0x07E0; // Green
        switch (entry->severity) {
            case HEALTH_WARNING:
                severityColor = 0xFFE0; // Yellow
                break;
            case HEALTH_CRITICAL:
                severityColor = 0xF800; // Red
                break;
            case HEALTH_FAILED:
                severityColor = 0xF800; // Red
                break;
        }
        
        tft.setTextColor(severityColor);
        tft.drawString(entry->component, 70, y);
        
        tft.setTextColor(0xFFFF);
        // Truncate message if too long
        char message[40];
        strncpy(message, entry->message, sizeof(message) - 1);
        message[sizeof(message) - 1] = '\0';
        if (strlen(entry->message) > sizeof(message) - 1) {
            strcpy(message + sizeof(message) - 4, "...");
        }
        tft.drawString(message, 120, y);
        
        y += 15;
    }
}

void drawPerformanceScreen() {
    tft.fillScreen(0x0000);
    drawDiagnosticHeader("Performance Stats");
    
    tft.setTextColor(0xFFFF);
    tft.setTextSize(1);
    
    int y = 35;
    
    // Loop performance
    tft.setTextColor(0x7BEF);
    tft.drawString("Loop Performance:", 10, y);
    y += 15;
    
    tft.setTextColor(0xFFFF);
    char tempStr[50];
    snprintf(tempStr, sizeof(tempStr), "Iterations: %lu", diagnosticData.loopIterations);
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "Avg Time: %lu us", diagnosticData.averageLoopTime);
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "Max Time: %lu us", diagnosticData.maxLoopTime);
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    // Calculate loop frequency
    if (diagnosticData.averageLoopTime > 0) {
        float loopFreq = 1000000.0 / diagnosticData.averageLoopTime;
        snprintf(tempStr, sizeof(tempStr), "Frequency: %.1f Hz", loopFreq);
        tft.drawString(tempStr, 10, y);
    }
    y += 25;
    
    // System performance
    tft.setTextColor(0x7BEF);
    tft.drawString("System Performance:", 10, y);
    y += 15;
    
    tft.setTextColor(0xFFFF);
    snprintf(tempStr, sizeof(tempStr), "CPU Freq: %lu MHz", ESP.getCpuFreqMHz());
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    // Temperature (if available)
    float temp = getSystemTemperature();
    if (temp > 0) {
        snprintf(tempStr, sizeof(tempStr), "Temperature: %.1f°C", temp);
        tft.drawString(tempStr, 10, y);
        y += 15;
    }
    
    // Network performance
    y += 10;
    tft.setTextColor(0x7BEF);
    tft.drawString("Network Performance:", 10, y);
    y += 15;
    
    tft.setTextColor(0xFFFF);
    snprintf(tempStr, sizeof(tempStr), "WiFi Reconnects: %lu", diagnosticData.wifiReconnects);
    tft.drawString(tempStr, 10, y);
    y += 15;
    
    snprintf(tempStr, sizeof(tempStr), "MQTT Reconnects: %lu", diagnosticData.mqttReconnects);
    tft.drawString(tempStr, 10, y);
}

void drawActionsScreen() {
    tft.fillScreen(0x0000);
    drawDiagnosticHeader("Diagnostic Actions");
    
    tft.setTextColor(0xFFFF);
    tft.setTextSize(1);
    
    // Action buttons
    const char* actions[] = {
        "Clear Error Log",
        "Restart System",
        "Restart WiFi",
        "Restart MQTT",
        "Factory Reset",
        "Memory Test",
        "Component Test",
        "Export Logs"
    };
    
    for (int i = 0; i < 8; i++) {
        int y = 40 + i * 20;
        
        // Determine if action is safe/dangerous
        bool dangerous = (i == 1 || i == 4); // Restart System, Factory Reset
        uint16_t textColor = dangerous ? 0xF800 : 0xFFFF; // Red for dangerous actions
        
        tft.setTextColor(textColor);
        char actionText[30];
        snprintf(actionText, sizeof(actionText), "%d. %s", i + 1, actions[i]);
        tft.drawString(actionText, 10, y);
    }
    
    // Warning for dangerous actions
    tft.setTextColor(0xFFE0); // Yellow
    tft.drawString("Warning: Red actions are destructive!", 10, 200);
}

// Navigation functions
void nextDiagnosticScreen() {
    diagInterface.currentScreen = (DiagnosticScreen)((diagInterface.currentScreen + 1) % DIAG_SCREEN_COUNT);
    diagInterface.currentPage = 0;
    diagInterface.needsRefresh = true;
}

void prevDiagnosticScreen() {
    diagInterface.currentScreen = (DiagnosticScreen)((diagInterface.currentScreen - 1 + DIAG_SCREEN_COUNT) % DIAG_SCREEN_COUNT);
    diagInterface.currentPage = 0;
    diagInterface.needsRefresh = true;
}

void gotoDiagnosticScreen(DiagnosticScreen screen) {
    diagInterface.currentScreen = screen;
    diagInterface.currentPage = 0;
    diagInterface.needsRefresh = true;
}

void nextDiagnosticPage() {
    if (diagInterface.currentPage < diagInterface.totalPages - 1) {
        diagInterface.currentPage++;
        diagInterface.needsRefresh = true;
    }
}

void prevDiagnosticPage() {
    if (diagInterface.currentPage > 0) {
        diagInterface.currentPage--;
        diagInterface.needsRefresh = true;
    }
}

// Utility functions
void drawDiagnosticHeader(const char* title) {
    // Header background
    tft.fillRect(0, 0, 320, 25, 0x2104); // Dark blue
    
    // Title
    tft.setTextColor(0xFFFF);
    tft.setTextSize(2);
    tft.drawString(title, 10, 5);
    
    // Navigation buttons
    tft.setTextSize(1);
    tft.drawString("< Prev", 10, 215);
    tft.drawString("Next >", 70, 215);
    tft.drawString("PgUp", 130, 215);
    tft.drawString("PgDn", 190, 215);
    tft.drawString("Exit", 250, 215);
    
    // Draw button borders
    tft.drawRect(8, 210, 52, 15, 0x8410);
    tft.drawRect(68, 210, 52, 15, 0x8410);
    tft.drawRect(128, 210, 52, 15, 0x8410);
    tft.drawRect(188, 210, 52, 15, 0x8410);
    tft.drawRect(248, 210, 62, 15, 0x8410);
}

void drawDiagnosticProgressBar(int x, int y, int w, int h, int value, int maxValue, const char* label) {
    // Label
    tft.setTextColor(0xFFFF);
    tft.drawString(label, x, y - 12);
    
    // Progress bar background
    tft.drawRect(x, y, w, h, 0xFFFF);
    tft.fillRect(x + 1, y + 1, w - 2, h - 2, 0x0000);
    
    // Progress fill
    int fillWidth = (value * (w - 2)) / maxValue;
    uint16_t fillColor = 0x07E0; // Green
    
    if (value > maxValue * 0.8) {
        fillColor = 0xF800; // Red
    } else if (value > maxValue * 0.6) {
        fillColor = 0xFFE0; // Yellow
    }
    
    if (fillWidth > 0) {
        tft.fillRect(x + 1, y + 1, fillWidth, h - 2, fillColor);
    }
    
    // Value text
    char valueStr[20];
    snprintf(valueStr, sizeof(valueStr), "%d%%", (value * 100) / maxValue);
    int textX = x + w - 30;
    tft.setTextColor(0xFFFF);
    tft.drawString(valueStr, textX, y + 5);
}

void drawDiagnosticStatusIndicator(int x, int y, const char* label, bool status, const char* value) {
    tft.setTextColor(0xFFFF);
    tft.drawString(label, x, y);
    
    uint16_t statusColor = status ? 0x07E0 : 0xF800; // Green or Red
    const char* statusText = status ? "YES" : "NO";
    
    if (value != nullptr) {
        statusText = value;
    }
    
    tft.setTextColor(statusColor);
    tft.drawString(statusText, x + 80, y);
}

const char* formatUptime(unsigned long uptime) {
    static char uptimeStr[30];
    
    unsigned long days = uptime / 86400;
    unsigned long hours = (uptime % 86400) / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    unsigned long seconds = uptime % 60;
    
    if (days > 0) {
        snprintf(uptimeStr, sizeof(uptimeStr), "%lud %02lu:%02lu:%02lu", days, hours, minutes, seconds);
    } else {
        snprintf(uptimeStr, sizeof(uptimeStr), "%02lu:%02lu:%02lu", hours, minutes, seconds);
    }
    
    return uptimeStr;
}

const char* formatMemorySize(uint32_t bytes) {
    static char sizeStr[20];
    
    if (bytes >= 1024 * 1024) {
        snprintf(sizeStr, sizeof(sizeStr), "%.1f MB", bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        snprintf(sizeStr, sizeof(sizeStr), "%.1f KB", bytes / 1024.0);
    } else {
        snprintf(sizeStr, sizeof(sizeStr), "%lu B", bytes);
    }
    
    return sizeStr;
}

// Action execution functions
void executeDiagnosticAction(DiagnosticAction action) {
    DEBUG_PRINTF("[DIAG] Executing action: %d\n", action);
    
    switch (action) {
        case DIAG_ACTION_CLEAR_ERRORS:
            clearErrorLog();
            break;
        case DIAG_ACTION_RESTART_SYSTEM:
            restartSystem();
            break;
        case DIAG_ACTION_RESTART_WIFI:
            restartWiFi();
            break;
        case DIAG_ACTION_RESTART_MQTT:
            restartMQTT();
            break;
        case DIAG_ACTION_FACTORY_RESET:
            performFactoryReset();
            break;
        case DIAG_ACTION_MEMORY_TEST:
            runMemoryTest();
            break;
        case DIAG_ACTION_COMPONENT_TEST:
            runComponentTest();
            break;
        case DIAG_ACTION_EXPORT_LOGS:
            exportLogs();
            break;
        default:
            DEBUG_PRINTLN("[DIAG] Unknown action");
            break;
    }
    
    diagInterface.needsRefresh = true;
}

void clearErrorLog() {
    DEBUG_PRINTLN("[DIAG] Clearing error log");
    extern void clearErrorLog();
    clearErrorLog();
    logError(HEALTH_GOOD, "DIAG", "Error log cleared by user");
}

void restartSystem() {
    DEBUG_PRINTLN("[DIAG] Restarting system");
    logError(HEALTH_WARNING, "DIAG", "System restart requested by user");
    
    // Show restart message
    tft.fillScreen(0x0000);
    tft.setTextColor(0xFFFF);
    tft.setTextSize(2);
    tft.drawString("Restarting...", 80, 100);
    
    delay(2000);
    ESP.restart();
}

void restartWiFi() {
    DEBUG_PRINTLN("[DIAG] Restarting WiFi");
    logError(HEALTH_WARNING, "DIAG", "WiFi restart requested by user");
    
    WiFi.disconnect();
    delay(1000);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void restartMQTT() {
    DEBUG_PRINTLN("[DIAG] Restarting MQTT");
    logError(HEALTH_WARNING, "DIAG", "MQTT restart requested by user");
    
    extern PubSubClient mqttClient;
    mqttClient.disconnect();
    delay(1000);
    // MQTT will reconnect automatically in the main loop
}

void performFactoryReset() {
    DEBUG_PRINTLN("[DIAG] Performing factory reset");
    logError(HEALTH_CRITICAL, "DIAG", "Factory reset requested by user");
    
    // Show warning
    tft.fillScreen(0x0000);
    tft.setTextColor(0xF800);
    tft.setTextSize(2);
    tft.drawString("FACTORY RESET", 60, 80);
    tft.setTextSize(1);
    tft.drawString("All settings will be lost!", 50, 120);
    tft.drawString("System will restart in 5 seconds", 30, 140);
    
    delay(5000);
    
    // Clear EEPROM (if used)
    // This would clear all saved settings, calibrations, etc.
    
    ESP.restart();
}

void runMemoryTest() {
    DEBUG_PRINTLN("[DIAG] Running memory test");
    logError(HEALTH_GOOD, "DIAG", "Memory test started");
    
    // Simple memory allocation test
    uint32_t initialFree = ESP.getFreeHeap();
    
    // Try to allocate and free memory blocks
    const int testSizes[] = {1024, 4096, 8192, 16384};
    bool testPassed = true;
    
    for (int i = 0; i < 4; i++) {
        void* ptr = malloc(testSizes[i]);
        if (ptr == nullptr) {
            DEBUG_PRINTF("[DIAG] Memory test failed at %d bytes\n", testSizes[i]);
            testPassed = false;
            break;
        }
        
        // Write test pattern
        memset(ptr, 0xAA, testSizes[i]);
        
        // Verify test pattern
        uint8_t* bytes = (uint8_t*)ptr;
        for (int j = 0; j < testSizes[i]; j++) {
            if (bytes[j] != 0xAA) {
                DEBUG_PRINTF("[DIAG] Memory test failed at byte %d\n", j);
                testPassed = false;
                break;
            }
        }
        
        free(ptr);
        
        if (!testPassed) break;
    }
    
    uint32_t finalFree = ESP.getFreeHeap();
    
    if (testPassed && (finalFree >= initialFree - 100)) { // Allow small variance
        logError(HEALTH_GOOD, "DIAG", "Memory test passed");
        DEBUG_PRINTLN("[DIAG] Memory test passed");
    } else {
        logError(HEALTH_WARNING, "DIAG", "Memory test failed");
        DEBUG_PRINTLN("[DIAG] Memory test failed");
    }
}

void runComponentTest() {
    DEBUG_PRINTLN("[DIAG] Running component test");
    logError(HEALTH_GOOD, "DIAG", "Component test started");
    
    // Test each component
    bool allPassed = true;
    
    // Test motor control
    extern bool motorControlInitialized;
    if (!motorControlInitialized) {
        logError(HEALTH_WARNING, "DIAG", "Motor control not initialized");
        allPassed = false;
    }
    
    // Test relay control
    extern bool relayControlInitialized;
    if (!relayControlInitialized) {
        logError(HEALTH_WARNING, "DIAG", "Relay control not initialized");
        allPassed = false;
    }
    
    // Test touch screen
    extern bool touchScreenInitialized;
    if (!touchScreenInitialized) {
        logError(HEALTH_WARNING, "DIAG", "Touch screen not initialized");
        allPassed = false;
    }
    
    // Test IR receiver
    extern bool irReceiverInitialized;
    if (!irReceiverInitialized) {
        logError(HEALTH_WARNING, "DIAG", "IR receiver not initialized");
        allPassed = false;
    }
    
    if (allPassed) {
        logError(HEALTH_GOOD, "DIAG", "All component tests passed");
        DEBUG_PRINTLN("[DIAG] All component tests passed");
    } else {
        logError(HEALTH_WARNING, "DIAG", "Some component tests failed");
        DEBUG_PRINTLN("[DIAG] Some component tests failed");
    }
}

void exportLogs() {
    DEBUG_PRINTLN("[DIAG] Exporting logs");
    logError(HEALTH_GOOD, "DIAG", "Log export started");
    
    // Export logs via serial
    DEBUG_PRINTLN("\n========== LOG EXPORT ==========");
    printSerialSystemInfo();
    printSerialMemoryStatus();
    printSerialNetworkStatus();
    printSerialComponentStatus();
    printSerialErrorLog();
    printSerialPerformanceStats();
    DEBUG_PRINTLN("========== END LOG EXPORT ==========\n");
}

// Serial diagnostic command handlers
void handleSerialDiagnosticCommand(const char* command) {
    if (strcmp(command, "help") == 0 || strcmp(command, "?") == 0) {
        printSerialDiagnosticHelp();
    } else if (strcmp(command, "info") == 0) {
        printSerialSystemInfo();
    } else if (strcmp(command, "memory") == 0) {
        printSerialMemoryStatus();
    } else if (strcmp(command, "network") == 0) {
        printSerialNetworkStatus();
    } else if (strcmp(command, "components") == 0) {
        printSerialComponentStatus();
    } else if (strcmp(command, "errors") == 0) {
        printSerialErrorLog();
    } else if (strcmp(command, "performance") == 0) {
        printSerialPerformanceStats();
    } else if (strcmp(command, "health") == 0) {
        printHealthReport();
    } else {
        DEBUG_PRINTF("Unknown diagnostic command: %s\n", command);
        DEBUG_PRINTLN("Type 'help' for available commands");
    }
}

void printSerialDiagnosticHelp() {
    DEBUG_PRINTLN("\n========== DIAGNOSTIC COMMANDS ==========");
    DEBUG_PRINTLN("help       - Show this help");
    DEBUG_PRINTLN("info       - System information");
    DEBUG_PRINTLN("memory     - Memory status");
    DEBUG_PRINTLN("network    - Network status");
    DEBUG_PRINTLN("components - Component status");
    DEBUG_PRINTLN("errors     - Error log");
    DEBUG_PRINTLN("performance- Performance statistics");
    DEBUG_PRINTLN("health     - Health report");
    DEBUG_PRINTLN("==========================================\n");
}

void printSerialSystemInfo() {
    DEBUG_PRINTLN("\n========== SYSTEM INFORMATION ==========");
    DEBUG_PRINTF("Chip Model: %s\n", ESP.getChipModel());
    DEBUG_PRINTF("Chip Revision: %d\n", ESP.getChipRevision());
    DEBUG_PRINTF("CPU Cores: %d\n", ESP.getChipCores());
    DEBUG_PRINTF("CPU Frequency: %lu MHz\n", ESP.getCpuFreqMHz());
    DEBUG_PRINTF("Flash Size: %lu bytes\n", ESP.getFlashChipSize());
    DEBUG_PRINTF("Firmware Version: %s\n", FIRMWARE_VERSION);
    DEBUG_PRINTF("Build Date: %s %s\n", BUILD_DATE, BUILD_TIME);
    DEBUG_PRINTF("Uptime: %s\n", formatUptime(millis() / 1000));
    DEBUG_PRINTF("Boot Count: %lu\n", ESP.getBootCount());
    DEBUG_PRINTF("Reset Reason: %d\n", esp_reset_reason());
    DEBUG_PRINTLN("=========================================\n");
}

void printSerialMemoryStatus() {
    DEBUG_PRINTLN("\n========== MEMORY STATUS ==========");
    DEBUG_PRINTF("Free Heap: %s\n", formatMemorySize(ESP.getFreeHeap()));
    DEBUG_PRINTF("Total Heap: %s\n", formatMemorySize(ESP.getHeapSize()));
    DEBUG_PRINTF("Min Free Heap: %s\n", formatMemorySize(ESP.getMinFreeHeap()));
    DEBUG_PRINTF("Max Alloc Heap: %s\n", formatMemorySize(ESP.getMaxAllocHeap()));
    
    uint32_t used = ESP.getHeapSize() - ESP.getFreeHeap();
    float fragmentation = ((float)used / ESP.getHeapSize()) * 100.0;
    DEBUG_PRINTF("Heap Usage: %.1f%%\n", fragmentation);
    DEBUG_PRINTF("Memory Status: %s\n", healthStatusToString(systemHealth.memory.status));
    DEBUG_PRINTLN("===================================\n");
}

void printSerialNetworkStatus() {
    DEBUG_PRINTLN("\n========== NETWORK STATUS ==========");
    DEBUG_PRINTF("WiFi Connected: %s\n", WiFi.status() == WL_CONNECTED ? "YES" : "NO");
    
    if (WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTF("SSID: %s\n", WiFi.SSID().c_str());
        DEBUG_PRINTF("IP Address: %s\n", WiFi.localIP().toString().c_str());
        DEBUG_PRINTF("MAC Address: %s\n", WiFi.macAddress().c_str());
        DEBUG_PRINTF("RSSI: %d dBm\n", WiFi.RSSI());
        DEBUG_PRINTF("Reconnect Count: %lu\n", systemHealth.wifi.reconnectCount);
    }
    
    extern PubSubClient mqttClient;
    DEBUG_PRINTF("MQTT Connected: %s\n", mqttClient.connected() ? "YES" : "NO");
    DEBUG_PRINTF("MQTT Server: %s:%d\n", MQTT_SERVER, MQTT_PORT);
    
    extern int queueCount;
    DEBUG_PRINTF("MQTT Queue: %d messages\n", queueCount);
    DEBUG_PRINTLN("====================================\n");
}

void printSerialComponentStatus() {
    DEBUG_PRINTLN("\n========== COMPONENT STATUS ==========");
    
    const char* componentNames[] = {
        "Motor Control", "Relay Control", "LED Matrix", "LED Strip",
        "Touch Screen", "IR Receiver", "MQTT Handler"
    };
    
    ComponentStatus statuses[] = {
        systemHealth.components.motorControl,
        systemHealth.components.relayControl,
        systemHealth.components.ledMatrix,
        systemHealth.components.ledStrip,
        systemHealth.components.touchScreen,
        systemHealth.components.irReceiver,
        systemHealth.components.mqttHandler
    };
    
    for (int i = 0; i < 7; i++) {
        DEBUG_PRINTF("%-15s: %s\n", componentNames[i], componentStatusToString(statuses[i]));
    }
    
    DEBUG_PRINTLN("======================================\n");
}

void printSerialErrorLog() {
    DEBUG_PRINTLN("\n========== ERROR LOG ==========");
    
    if (errorLogCount == 0) {
        DEBUG_PRINTLN("No errors logged");
    } else {
        DEBUG_PRINTF("Total errors: %d\n\n", errorLogCount);
        
        for (int i = 0; i < min(errorLogCount, 20); i++) { // Show last 20 errors
            int logIndex = (errorLogIndex - 1 - i + MAX_ERROR_LOG_ENTRIES) % MAX_ERROR_LOG_ENTRIES;
            ErrorLogEntry* entry = &errorLog[logIndex];
            
            if (!entry->valid) continue;
            
            unsigned long seconds = entry->timestamp / 1000;
            unsigned long minutes = seconds / 60;
            unsigned long hours = minutes / 60;
            
            DEBUG_PRINTF("[%02lu:%02lu:%02lu] %s (%s): %s\n",
                        hours % 24, minutes % 60, seconds % 60,
                        entry->component,
                        healthStatusToString(entry->severity),
                        entry->message);
        }
    }
    
    DEBUG_PRINTLN("===============================\n");
}

void printSerialPerformanceStats() {
    DEBUG_PRINTLN("\n========== PERFORMANCE STATS ==========");
    DEBUG_PRINTF("Loop Iterations: %lu\n", diagnosticData.loopIterations);
    DEBUG_PRINTF("Average Loop Time: %lu us\n", diagnosticData.averageLoopTime);
    DEBUG_PRINTF("Max Loop Time: %lu us\n", diagnosticData.maxLoopTime);
    
    if (diagnosticData.averageLoopTime > 0) {
        float loopFreq = 1000000.0 / diagnosticData.averageLoopTime;
        DEBUG_PRINTF("Loop Frequency: %.1f Hz\n", loopFreq);
    }
    
    DEBUG_PRINTF("WiFi Reconnects: %lu\n", diagnosticData.wifiReconnects);
    DEBUG_PRINTF("MQTT Reconnects: %lu\n", diagnosticData.mqttReconnects);
    
    float temp = getSystemTemperature();
    if (temp > 0) {
        DEBUG_PRINTF("Temperature: %.1f°C\n", temp);
    }
    
    DEBUG_PRINTLN("=======================================\n");
}

#endif // DIAGNOSTIC_INTERFACE_H