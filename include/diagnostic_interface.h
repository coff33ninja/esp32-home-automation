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
#include <cstdio>      // for snprintf
#include <cstring>     // for strncpy, strlen, strcpy
#include <cstdlib>     // for standard library functions
#include <algorithm>   // for min, max
#include <TFT_eSPI.h>
#include <PubSubClient.h>
#include "config.h"
#include "system_monitor.h"
#include "hardware_detection.h"
#include "module_system.h"

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

// Forward declarations for types from system_monitor
// (keep the header lightweight; full implementations live in src/)
typedef struct DiagnosticInterfaceState {
    DiagnosticScreen currentScreen;
    int currentPage;
    int totalPages;
    unsigned long lastRefresh;
    unsigned long screenStartTime;
    bool active;
    bool needsRefresh;
} DiagnosticInterfaceState;

extern DiagnosticInterfaceState diagInterface;

// Core API for diagnostic interface (implemented in src/diagnostic_interface.cpp)
void initDiagnosticInterface();
void showDiagnosticInterface();
void hideDiagnosticInterface();
void updateDiagnosticInterface();
bool handleDiagnosticTouch(int x, int y);
void refreshDiagnosticData();

// Display object (defined in touch handler translation unit)
extern TFT_eSPI tft;

// Drawing helpers (implemented in src/ui_helpers.cpp / diagnostic UI files)
void drawDiagnosticMainScreen();
void drawSystemInfoScreen();
void drawMemoryStatusScreen();
void drawNetworkStatusScreen();
void drawComponentStatusScreen();
void drawErrorLogScreen();
void drawPerformanceScreen();
void drawActionsScreen();

// Navigation helpers
void prevDiagnosticScreen();
void nextDiagnosticScreen();
void prevDiagnosticPage();
void nextDiagnosticPage();
void gotoDiagnosticScreen(DiagnosticScreen screen);

// Action execution
void executeDiagnosticAction(DiagnosticAction action);

// Serial diagnostic command handler
void handleSerialDiagnosticCommand(const char* cmd);

#endif // DIAGNOSTIC_INTERFACE_H