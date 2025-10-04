// Minimal config/diagnostic helper stubs
#include "config_manager.h"
#include "module_system.h"
#include "diagnostic_interface.h"

void saveModuleConfiguration() {
  // stub - delegate to config manager if present
}

void loadModuleConfiguration() {
  // stub
}

void reportHardwareToDiagnostic() {
  // stub
}

void logError(const char* component, const char* message, int severity) {
  // Minimal logging to serial
  DEBUG_PRINTF("[LOG] %s: %s (sev=%d)\n", component, message, severity);
}
