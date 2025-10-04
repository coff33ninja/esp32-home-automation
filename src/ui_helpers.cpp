// Minimal UI and utility helper stubs to satisfy linker while keeping behavior safe.
#include <TFT_eSPI.h>
#include "touch_handler.h"
#include "ota_updater.h"
#include "diagnostic_interface.h"

extern TFT_eSPI tft;

void drawMainInterface() {
  // Minimal placeholder: clear screen and show a simple header
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.drawString("Home Control", 10, 10);
}

void drawStatusBar() {
  // Minimal placeholder: draw a thin status bar
  tft.fillRect(0, 220, 320, 20, 0x8410);
}

void showScreenSaver() {
  tft.fillScreen(0x0000);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.drawString("Screensaver", 60, 100);
}

TouchEvent readTouch() {
  // Non-blocking stub: return no touch
  TouchEvent t = {0};
  t.pressed = false;
  t.released = false;
  t.held = false;
  t.x = 0;
  t.y = 0;
  t.pressure = 0;
  t.timestamp = millis();
  return t;
}

bool isPointInRect(int px, int py, int rx, int ry, int rw, int rh) {
  return (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh);
}

void loadCalibration() {
  // stub - mark calibration as not found
}

void saveCalibration() {
  // stub
}

void showConfigurationMenu() {
  // Delegate to the module list drawing in touch_handler.cpp if available
  extern void drawModuleList();
  drawModuleList();
}

void showFactoryResetWarning() {
  tft.fillScreen(0x0000);
  tft.setTextColor(COLOR_STATUS_ERROR);
  tft.setTextSize(2);
  tft.drawString("Factory Reset!", 50, 100);
}
