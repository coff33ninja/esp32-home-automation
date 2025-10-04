// Minimal diagnostic UI and navigation stubs so diagnostic_interface links.
#include "diagnostic_interface.h"
#include "touch_handler.h"

extern TFT_eSPI tft;

void drawDiagnosticMainScreen() {
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.drawString("Diagnostic Main", 10, 10);
}

void drawSystemInfoScreen() { drawDiagnosticMainScreen(); }
void drawMemoryStatusScreen() { drawDiagnosticMainScreen(); }
void drawNetworkStatusScreen() { drawDiagnosticMainScreen(); }
void drawComponentStatusScreen() { drawDiagnosticMainScreen(); }
void drawErrorLogScreen() { drawDiagnosticMainScreen(); }
void drawPerformanceScreen() { drawDiagnosticMainScreen(); }
void drawActionsScreen() { drawDiagnosticMainScreen(); }

void prevDiagnosticScreen() { /* stub */ }
void nextDiagnosticScreen() { /* stub */ }
void prevDiagnosticPage() { if (diagInterface.currentPage>0) diagInterface.currentPage--; }
void nextDiagnosticPage() { if (diagInterface.currentPage<diagInterface.totalPages-1) diagInterface.currentPage++; }
void gotoDiagnosticScreen(DiagnosticScreen screen) { diagInterface.currentScreen = screen; diagInterface.needsRefresh = true; }
void executeDiagnosticAction(DiagnosticAction action) { /* stub: could map to system functions */ }

void handleSerialDiagnosticCommand(const char* cmd) {
  DEBUG_PRINTF("[DIAG_CMD] %s\n", cmd);
  // Basic commands for testing
  if (strcmp(cmd, "help") == 0) {
    DEBUG_PRINTLN("Available: help, status");
  } else if (strcmp(cmd, "status") == 0) {
    printHealthReport();
  }
}
