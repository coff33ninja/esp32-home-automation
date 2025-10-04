// Diagnostic interface implementation (moved out of header to avoid multiple definitions)
#include "diagnostic_interface.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "system_monitor.h"
#include "config.h"

// Global variable definition
DiagnosticInterfaceState diagInterface = {
    DIAG_SCREEN_MAIN,
    0,
    1,
    0,
    0,
    false,
    true
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
    tft.fillScreen(0x0000);
    drawDiagnosticMainScreen();
}

void hideDiagnosticInterface() {
    DEBUG_PRINTLN("[DIAG] Hiding diagnostic interface");
    diagInterface.active = false;
    extern void drawMainInterface();
    drawMainInterface();
}

void updateDiagnosticInterface() {
    if (!diagInterface.active) return;
    unsigned long now = millis();
    if (now - diagInterface.screenStartTime > DIAG_SCREEN_TIMEOUT) {
        hideDiagnosticInterface();
        return;
    }
    if (now - diagInterface.lastRefresh > DIAG_REFRESH_INTERVAL) {
        refreshDiagnosticData();
        diagInterface.needsRefresh = true;
        diagInterface.lastRefresh = now;
    }
    if (diagInterface.needsRefresh) {
        switch (diagInterface.currentScreen) {
            case DIAG_SCREEN_MAIN: drawDiagnosticMainScreen(); break;
            case DIAG_SCREEN_SYSTEM_INFO: drawSystemInfoScreen(); break;
            case DIAG_SCREEN_MEMORY_STATUS: drawMemoryStatusScreen(); break;
            case DIAG_SCREEN_NETWORK_STATUS: drawNetworkStatusScreen(); break;
            case DIAG_SCREEN_COMPONENT_STATUS: drawComponentStatusScreen(); break;
            case DIAG_SCREEN_ERROR_LOG: drawErrorLogScreen(); break;
            case DIAG_SCREEN_PERFORMANCE: drawPerformanceScreen(); break;
            case DIAG_SCREEN_ACTIONS: drawActionsScreen(); break;
            default: break;
        }
        diagInterface.needsRefresh = false;
    }
}

bool handleDiagnosticTouch(int x, int y) {
    if (!diagInterface.active) return false;
    diagInterface.screenStartTime = millis();
    if (y >= 210 && y <= 240) {
        if (x >= 10 && x <= 60) { prevDiagnosticScreen(); return true; }
        if (x >= 70 && x <= 120) { nextDiagnosticScreen(); return true; }
        if (x >= 130 && x <= 180) { prevDiagnosticPage(); return true; }
        if (x >= 190 && x <= 240) { nextDiagnosticPage(); return true; }
        if (x >= 250 && x <= 310) { hideDiagnosticInterface(); return true; }
    }
    switch (diagInterface.currentScreen) {
        case DIAG_SCREEN_MAIN:
            if (x >= 10 && x <= 150 && y >= 40 && y <= 200) {
                int item = (y - 40) / 25;
                if (item < DIAG_SCREEN_COUNT - 1) gotoDiagnosticScreen((DiagnosticScreen)(item + 1));
                return true;
            }
            break;
        case DIAG_SCREEN_ACTIONS:
            if (x >= 10 && x <= 150) {
                int action = (y - 40) / 30;
                if (action >= 0 && action < 8) executeDiagnosticAction((DiagnosticAction)(action + 1));
                return true;
            }
            break;
        default: break;
    }
    return true;
}

void refreshDiagnosticData() {
    updateSystemHealth();
    updateDiagnosticData();
}

// The remaining drawing and utility functions are implemented in ui_helpers.cpp and diagnostic_stubs.cpp
