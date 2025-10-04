// Minimal OTA UI stubs
#include "ota_updater.h"
#include <TFT_eSPI.h>
#include "touch_handler.h"

extern TFT_eSPI tft;

void updateOTAProgressScreen(int progress, const String &message) {
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  char buf[64];
  snprintf(buf, sizeof(buf), "%s %d%%", message.c_str(), progress);
  tft.drawString(buf, 10, 120);
}

void showOTANotificationScreen(const OTAUpdateInfo &info, bool mandatory) {
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextColor(COLOR_TEXT);
  tft.drawString("OTA Update Available", 10, 50);
}

void showOTASuccessScreen() { /* stub */ }
void showOTAErrorScreen(const String &err) { /* stub */ }
void showOTARollbackScreen() { /* stub */ }
bool checkForOTACancel() { return false; }
