// touch_handler.cpp
// Moved implementations from include/touch_handler.h to avoid multiple definitions

#include "../include/touch_handler.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "../include/module_system.h"

// Define globals (previously in header)
TFT_eSPI tft = TFT_eSPI();
ScreenState currentScreenState = SCREEN_OFF;
TouchEvent lastTouch = {0};
unsigned long lastScreenActivity = 0;
bool touchCalibrated = false;
uint16_t touchCalibration[8] = {0};
bool touchScreenInitialized = false;

// Virtual keyboard and toast state (moved here so functions that draw/use them see the symbols)
static bool keyboardActive = false;
static char keyboardBufferInternal[128];
static int keyboardMaxLen = 0;
static int keyboardCursor = 0;
static String keyboardTitle = "";
static bool keyboardConfirmed = false;
static int keyboardPendingModuleId = -1; // module id waiting for keyboard input
static bool keyboardShiftUpper = true; // shift = uppercase
static unsigned long lastBackRepeat = 0;
static bool lastBackPressed = false;

// Toast state
static bool toastActive = false;
static String toastText = "";
static unsigned long toastStart = 0;
static unsigned long toastDuration = 2000; // ms
static bool toastSuccess = true;

// Module configuration UI state (moved up so handlers can reference them)
static bool inModuleListScreen = false;
static int moduleListStartY = 40;
static int moduleListItemHeight = 28;
static int moduleListScroll = 0;
static int itemsPerPage = 6; // approximate number of rows that fit
static bool inModuleDetailsScreen = false;
static uint8_t currentModuleDetailsId = 0;
// Confirmation dialog state
static bool showConfirmDialog = false;
static String confirmText = "";
static int confirmAction = 0; // 1 = enable/disable

// Implementations moved from header

bool initTouchScreen() {
  DEBUG_PRINTLN("[TOUCH] Initializing touch screen...");

  // Initialize TFT display
  tft.init();
  tft.setRotation(1); // Landscape orientation
  tft.fillScreen(COLOR_BACKGROUND);

  // Load calibration from EEPROM if available
  loadCalibration();

  if (touchCalibrated) {
    tft.setTouch(touchCalibration);
    DEBUG_PRINTLN("[TOUCH] Using saved calibration");
  } else {
    DEBUG_PRINTLN("[TOUCH] No calibration found, using defaults");
    uint16_t defaultCal[5] = {300, 3600, 300, 3600, 1};
    tft.setTouch(defaultCal);
  }

  currentScreenState = SCREEN_ACTIVE;
  lastScreenActivity = millis();
  lastTouchTime = 0;

  lastTouch.pressed = false;
  lastTouch.released = false;
  lastTouch.held = false;
  lastTouch.pressure = 0;

  drawMainInterface();

  touchScreenInitialized = true;
  DEBUG_PRINTLN("[TOUCH] Touch screen initialized successfully");
  return true;
}

void updateDisplay() {
  static unsigned long lastFullUpdate = 0;
  unsigned long now = millis();

  if (currentScreenState == SCREEN_ACTIVE &&
      now - lastScreenActivity > SCREEN_TIMEOUT_MS) {
    dimScreen();
  } else if (currentScreenState == SCREEN_DIM &&
             now - lastScreenActivity > SCREEN_TIMEOUT_MS * 2) {
    turnOffScreen();
  }

  switch (currentScreenState) {
  case SCREEN_ACTIVE:
    if (now - lastFullUpdate > 500) {
      drawStatusBar();
      // If keyboard is active, draw it on top
      if (keyboardActive) drawVirtualKeyboard();
      // Draw toast if active
      if (toastActive) {
        // auto-dismiss after duration
        if (millis() - toastStart > toastDuration) toastActive = false;
        else {
          // draw toast box
          int w = 260;
          int x = 30;
          int y = 190;
          uint16_t bg = toastSuccess ? COLOR_STATUS_OK : COLOR_STATUS_ERROR;
          tft.fillRect(x, y, w, 30, bg);
          tft.setTextColor(COLOR_TEXT);
          tft.setTextSize(1);
          tft.drawString(toastText, x + 8, y + 8);
        }
      }
      lastFullUpdate = now;
    }
    break;
  case SCREEN_DIM:
    if (now - lastFullUpdate > 2000) {
      drawStatusBar();
      lastFullUpdate = now;
    }
    break;
  case SCREEN_OFF:
    if (now - lastFullUpdate > 5000) {
      showScreenSaver();
      lastFullUpdate = now;
    }
    break;
  case SCREEN_CALIBRATING:
    break;
  }

}

bool handleTouch() {
  TouchEvent touch = readTouch();

  if (!touch.pressed && !touch.released) {
    return false;
  }

  lastScreenActivity = millis();

  if (currentScreenState != SCREEN_ACTIVE) {
    wakeScreen();
    return true;
  }

  if (currentScreenState == SCREEN_ACTIVE) {
    lastScreenActivity = millis();
  }

  // If virtual keyboard active, route touches to keyboard handler first
  if (keyboardActive) {
    if (touch.pressed || touch.held) {
      handleKeyboardTouch(touch.x, touch.y, keyboardBufferInternal, sizeof(keyboardBufferInternal));
      drawVirtualKeyboard();
      // handle backspace repeat: simple long-press behavior
      int backX = 150, backY = 100 + 6 * 22, backW = 80, backH = 28;
      if (isPointInRect(touch.x, touch.y, backX, backY, backW, backH)) {
        if (!lastBackPressed) {
          lastBackPressed = true;
          lastBackRepeat = millis();
        } else {
          if (millis() - lastBackRepeat > 400) {
            // repeat backspace
            int len = strlen(keyboardBufferInternal);
            if (len > 0) keyboardBufferInternal[len-1] = '\0';
            lastBackRepeat = millis() - 200; // slightly faster repeat
          }
        }
      } else {
        lastBackPressed = false;
      }
    }
    // If keyboard confirmed, process pending module configure
    if (keyboardConfirmed) {
      keyboardConfirmed = false;
      keyboardActive = false;
      // Process result for module if pending
      if (keyboardPendingModuleId >= 0) {
        RegisteredModule* module = getModule((uint8_t)keyboardPendingModuleId);
        if (module) {
          String payload = String(keyboardBufferInternal);
          bool ok = false;
          if (module->interface.configure) {
            ok = module->interface.configure(payload);
          } else {
            // No per-module configure handler available
            ok = false;
          }
          if (ok) {
            saveModuleConfiguration();
            publishModuleStatus();
            // show success toast
            toastText = String("Saved: ") + module->config.name;
            toastSuccess = true;
            toastStart = millis();
            toastActive = true;
          } else {
            toastText = String("Config failed / unsupported: ") + module->config.name;
            toastSuccess = false;
            toastStart = millis();
            toastActive = true;
          }
        }
      }
      keyboardPendingModuleId = -1;
      // redraw current screen
      if (inModuleDetailsScreen) drawModuleDetails(currentModuleDetailsId);
      else if (inModuleListScreen) drawModuleList();
    }
    return true;
  }

  if (currentScreenState == SCREEN_CALIBRATING) {
    return true;
  }

  if (touch.pressed) {
    DEBUG_PRINTF("[TOUCH] Touch at (%d, %d) pressure: %d\n", touch.x, touch.y,
                 touch.pressure);

    if (isPointInRect(touch.x, touch.y, 10, 55, 90, 50)) {
      DEBUG_PRINTLN("[TOUCH] Main Lights button pressed");
      toggleRelay(RELAY_1);
      drawMainInterface();
      return true;
    }

    if (isPointInRect(touch.x, touch.y, 110, 55, 90, 50)) {
      DEBUG_PRINTLN("[TOUCH] Accent Lights button pressed");
      toggleRelay(RELAY_2);
      drawMainInterface();
      return true;
    }

    if (isPointInRect(touch.x, touch.y, 210, 55, 90, 50)) {
      DEBUG_PRINTLN("[TOUCH] All Lights button pressed");
      lightsState = !lightsState;
      setAllRelays(lightsState);
      drawMainInterface();
      return true;
    }

    if (isPointInRect(touch.x, touch.y, 10, 130, 90, 40)) {
      DEBUG_PRINTLN("[TOUCH] Next Effect button pressed");
      currentEffect = (LEDEffect)((currentEffect + 1) % 8);
      setEffect(currentEffect);
      drawMainInterface();
      return true;
    }

    if (isPointInRect(touch.x, touch.y, 220, 240, 80, 30)) {
      DEBUG_PRINTLN("[TOUCH] Settings button pressed");
      showConfigurationMenu();
      return true;
    }

    static unsigned long statusBarPressTime = 0;
    if (isPointInRect(touch.x, touch.y, 0, 275, 320, 20)) {
      if (statusBarPressTime == 0) {
        statusBarPressTime = millis();
      } else if (millis() - statusBarPressTime > 3000) {
        DEBUG_PRINTLN("[TOUCH] Diagnostic interface activated");
        extern void showDiagnosticInterface();
        showDiagnosticInterface();
        statusBarPressTime = 0;
        return true;
      }
    } else {
      statusBarPressTime = 0;
    }

    if (isPointInRect(touch.x, touch.y, 10, 195, 140, 25)) {
      DEBUG_PRINTLN("[TOUCH] LED Strip brightness slider touched");
      int sliderValue = map(touch.x - 10, 0, 140, 0, 255);
      sliderValue = constrain(sliderValue, 0, 255);

      brightness = sliderValue;
      ledcWrite(LED_STRIP_PWM_CHANNEL, brightness);

      if (mqttClient.connected()) {
        char msg[10];
        snprintf(msg, sizeof(msg), "%d", brightness);
        mqttClient.publish("homecontrol/led_brightness", msg);
      }

      drawMainInterface();
      return true;
    }

    if (isPointInRect(touch.x, touch.y, 160, 195, 140, 25)) {
      DEBUG_PRINTLN("[TOUCH] Matrix brightness slider touched");
      int sliderValue = map(touch.x - 160, 0, 140, 0, 255);
      sliderValue = constrain(sliderValue, 0, 255);

      setBrightness(sliderValue);

      if (mqttClient.connected()) {
        char msg[10];
        snprintf(msg, sizeof(msg), "%d", sliderValue);
        mqttClient.publish("homecontrol/matrix_brightness", msg);
      }

      drawMainInterface();
      return true;
    }

    if (isPointInRect(touch.x, touch.y, 10, 245, 200, 25)) {
      DEBUG_PRINTLN("[TOUCH] Volume slider touched");
      int sliderValue = map(touch.x - 10, 0, 200, 0, 100);
      sliderValue = constrain(sliderValue, 0, 100);

      currentVolume = sliderValue;

      int targetPosition =
          map(currentVolume, 0, 100, POT_MIN_VALUE, POT_MAX_VALUE);
      moveMotorToPosition(targetPosition);

      setVolumeVisualization(currentVolume);

      if (mqttClient.connected()) {
        char msg[10];
        snprintf(msg, sizeof(msg), "%d", currentVolume);
        mqttClient.publish(MQTT_TOPIC_VOLUME, msg);
      }

      drawMainInterface();
      return true;
    }
  }

  return false;
}

void showCalibrationScreen() {
  DEBUG_PRINTLN("[TOUCH] Starting touch calibration");

  currentScreenState = SCREEN_CALIBRATING;
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.drawString("Touch Calibration", 50, 50);
  tft.setTextSize(1);
  tft.drawString("Touch the corners as indicated", 30, 100);
  tft.drawString("Press and hold for 2 seconds", 30, 120);

  if (performCalibration()) {
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setTextColor(COLOR_STATUS_OK);
    tft.setTextSize(2);
    tft.drawString("Calibration", 70, 100);
    tft.drawString("Complete!", 80, 130);
    delay(2000);

    saveCalibration();
    touchCalibrated = true;
  } else {
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setTextColor(COLOR_STATUS_ERROR);
    tft.setTextSize(2);
    tft.drawString("Calibration", 70, 100);
    tft.drawString("Failed!", 90, 130);
    delay(2000);
  }

  currentScreenState = SCREEN_ACTIVE;
  drawMainInterface();
}

bool performCalibration() {
  DEBUG_PRINTLN("[TOUCH] Performing calibration (placeholder)");
  return true;
}

void setScreenBrightness(uint8_t level) {
  DEBUG_PRINTF("[TOUCH] Setting screen brightness to: %d\n", level);
#ifdef TFT_BACKLIGHT_PIN
  ledcSetup(2, 5000, 8);
  ledcAttachPin(TFT_BACKLIGHT_PIN, 2);
  ledcWrite(2, level);
#else
  DEBUG_PRINTLN("[TOUCH] No backlight control pin defined");
#endif
  static uint8_t currentScreenBrightness = 255;
  currentScreenBrightness = level;
}

void wakeScreen() {
  if (currentScreenState != SCREEN_ACTIVE) {
    DEBUG_PRINTF("[TOUCH] Waking screen from state: %d\n", currentScreenState);
    for (int brightness = 0; brightness <= 255; brightness += 15) {
      setScreenBrightness(brightness);
      delay(10);
    }
    currentScreenState = SCREEN_ACTIVE;
    lastScreenActivity = millis();
    drawMainInterface();
    DEBUG_PRINTLN("[TOUCH] Screen wake complete");
  }
}

void dimScreen() {
  if (currentScreenState == SCREEN_ACTIVE) {
    DEBUG_PRINTLN("[TOUCH] Dimming screen");
    currentScreenState = SCREEN_DIM;
    setScreenBrightness(30);
    tft.fillRect(0, 0, 320, 240, 0x0000);
    tft.setTextColor(0x4208);
    tft.setTextSize(2);
    tft.drawString("Touch to wake", 80, 110);
  }
}

void turnOffScreen() {
  if (currentScreenState == SCREEN_DIM) {
    DEBUG_PRINTLN("[TOUCH] Turning off screen");
    currentScreenState = SCREEN_OFF;
    setScreenBrightness(0);
    tft.fillScreen(COLOR_BACKGROUND);
  }
}

// showConfigurationMenu() is provided by src/ui_helpers.cpp and delegates to
// drawModuleList(). Keep the module list drawing and handlers here.

void drawModuleList() {
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  int y = moduleListStartY;

  tft.fillRect(0, moduleListStartY - 4, 320, 240 - (moduleListStartY - 4), 0x0000);
  tft.setTextSize(2);
  tft.drawString("Modules", 10, moduleListStartY - 30);
  tft.setTextSize(1);

  extern ModuleSystemState moduleSystem;

  int drawn = 0;
  int visible = 0;
  for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
    RegisteredModule* module = &moduleSystem.modules[i];
    if (!module->active) continue;
    if (visible < moduleListScroll) { visible++; continue; }

    // Draw a simple row with name and enabled state
    tft.fillRect(8, y - 2, 304, moduleListItemHeight - 2, COLOR_BUTTON);
    tft.setTextColor(COLOR_TEXT);
    tft.drawString(String(module->config.moduleId) + ": " + String(module->config.name), 12, y);
    tft.setTextColor(module->config.enabled ? COLOR_STATUS_OK : COLOR_STATUS_ERROR);
    tft.drawString(module->config.enabled ? "ENABLED" : "DISABLED", 220, y);
    y += moduleListItemHeight;
    drawn++;
    visible++;
    if (drawn >= itemsPerPage) break;
  }

  if (drawn == 0) {
    tft.setTextColor(COLOR_TEXT);
    tft.drawString("No modules registered", 10, y);
  }

  // Draw back button
  tft.fillRect(10, 210, 80, 25, COLOR_BUTTON);
  tft.setTextColor(COLOR_TEXT);
  tft.drawString("Back", 20, 214);

  // Draw scroll indicators if necessary
  // Count total active modules
  int totalActive = 0;
  for (int i = 0; i < MAX_PLUGIN_MODULES; i++) if (moduleSystem.modules[i].active) totalActive++;
  if (totalActive > itemsPerPage) {
    // Up arrow
    if (moduleListScroll > 0) tft.drawString("^", 300, moduleListStartY - 28);
    // Down arrow
    if (moduleListScroll + itemsPerPage < totalActive) tft.drawString("v", 300, 200);
  }
}

void drawModuleDetails(uint8_t moduleId) {
  RegisteredModule* module = getModule(moduleId);
  if (!module) return;

  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.drawString(module->config.name, 10, 10);
  tft.setTextSize(1);
  tft.drawString(String("ID: ") + String(module->config.moduleId), 10, 40);
  tft.drawString(String("Type: ") + moduleTypeToString(module->config.type), 10, 60);
  tft.drawString(String("Interface: ") + interfaceTypeToString(module->config.interface), 10, 80);
  tft.drawString(String("Detected: ") + (module->config.detected ? "Yes" : "No"), 10, 100);
  tft.drawString(String("Enabled: ") + (module->config.enabled ? "Yes" : "No"), 10, 120);

  // Action buttons: Enable/Disable | Configure | Back
  tft.fillRect(10, 200, 100, 30, COLOR_BUTTON);
  tft.setTextColor(COLOR_TEXT);
  tft.drawString(module->config.enabled ? "Disable" : "Enable", 18, 208);

  tft.fillRect(115, 200, 90, 30, COLOR_BUTTON);
  tft.setTextColor(COLOR_TEXT);
  tft.drawString("Configure", 125, 208);

  tft.fillRect(220, 200, 80, 30, COLOR_BUTTON);
  tft.setTextColor(COLOR_TEXT);
  tft.drawString("Back", 248, 208);

  // If confirmation overlay active, draw it
  if (showConfirmDialog) {
    tft.fillRect(40, 70, 240, 100, 0xFFFF); // white box
    tft.setTextColor(0x0000);
    tft.drawString(confirmText, 60, 90);
    tft.fillRect(60, 150, 80, 30, COLOR_BUTTON);
    tft.setTextColor(COLOR_TEXT);
    tft.drawString("Confirm", 68, 156);
    tft.fillRect(180, 150, 80, 30, COLOR_BUTTON);
    tft.setTextColor(COLOR_TEXT);
    tft.drawString("Cancel", 200, 156);
  }
}

void handleModuleDetailsTouch(int x, int y) {
  RegisteredModule* module = getModule(currentModuleDetailsId);
  if (!module) return;

  // If confirmation dialog visible, handle its buttons
  if (showConfirmDialog) {
    if (isPointInRect(x, y, 60, 150, 80, 30)) {
      // Confirm
      if (confirmAction == 1) {
        enableModule(module->config.moduleId, !module->config.enabled);
        publishModuleStatus();
      }
      showConfirmDialog = false;
      drawModuleDetails(module->config.moduleId);
      return;
    }
    if (isPointInRect(x, y, 180, 150, 80, 30)) {
      // Cancel
      showConfirmDialog = false;
      drawModuleDetails(module->config.moduleId);
      return;
    }
    return;
  }

  // Back button
  if (isPointInRect(x, y, 220, 200, 80, 30)) {
    inModuleDetailsScreen = false;
    drawModuleList();
    return;
  }

  // Enable/Disable button
  if (isPointInRect(x, y, 10, 200, 100, 30)) {
    // Show confirmation dialog
    confirmText = String(module->config.enabled ? "Disable module?" : "Enable module?");
    confirmAction = 1;
    showConfirmDialog = true;
    drawModuleDetails(module->config.moduleId);
    return;
  }

  // Configure button (non-blocking keyboard)
  if (isPointInRect(x, y, 115, 200, 90, 30)) {
    // Pre-fill keyboard buffer from module's getConfiguration() if available
    if (module->interface.getConfiguration) {
      String existing = module->interface.getConfiguration();
      strncpy(keyboardBufferInternal, existing.c_str(), sizeof(keyboardBufferInternal)-1);
      keyboardBufferInternal[sizeof(keyboardBufferInternal)-1] = '\0';
    } else {
      keyboardBufferInternal[0] = '\0';
    }
    keyboardTitle = String("Config: ") + module->config.name;
    keyboardPendingModuleId = module->config.moduleId;
    keyboardActive = true;
    keyboardConfirmed = false;
    drawVirtualKeyboard();
    return;
  }
}

// Very small virtual keyboard implementation
void drawVirtualKeyboard() {
  // Draw modal background
  tft.fillRect(10, 30, 300, 180, COLOR_BUTTON);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.drawString(keyboardTitle, 20, 36);

  // Draw current buffer
  tft.setTextSize(1);
  tft.drawString(String(keyboardBufferInternal), 20, 70);

  // Simple key layout (rows of characters) - very small subset to keep code compact
  const char* rows[] = {"ABCDEF", "GHIJKL", "MNOPQR", "STUVWX", "YZ0123", "456789"};
  int startY = 100;
  for (int r = 0; r < 6; r++) {
    int startX = 20;
    for (int c = 0; c < (int)strlen(rows[r]); c++) {
      char k = rows[r][c];
      int kx = startX + c * 44;
      int ky = startY + r * 22;
      tft.fillRect(kx, ky, 40, 20, 0xFFFF);
      tft.setTextColor(0x0000);
      tft.drawString(String(k), kx + 12, ky + 3);
    }
  }

  // Special keys: Space, Backspace, Enter, Cancel
  tft.fillRect(20, 100 + 6 * 22, 120, 28, 0xFFFF);
  tft.setTextColor(0x0000);
  tft.drawString("Space", 30, 100 + 6 * 22 + 6);

  tft.fillRect(150, 100 + 6 * 22, 80, 28, 0xFFFF);
  tft.setTextColor(0x0000);
  tft.drawString("Back", 170, 100 + 6 * 22 + 6);

  tft.fillRect(240, 100 + 6 * 22, 70, 28, 0x07E0);
  tft.setTextColor(0x0000);
  tft.drawString("OK", 270, 100 + 6 * 22 + 6);
}

void handleKeyboardTouch(int x, int y, char* buffer, int maxLength) {
  // If user touched OK area
  int okX = 240, okY = 100 + 6 * 22, okW = 70, okH = 28;
  if (isPointInRect(x, y, okX, okY, okW, okH)) {
    keyboardConfirmed = true;
    keyboardActive = false;
    return;
  }
  // Cancel touches outside modal area - simple heuristic
  if (!isPointInRect(x, y, 10, 30, 300, 180)) {
    keyboardActive = false;
    keyboardConfirmed = false;
    return;
  }

  // Space and Back positions
  int spaceX = 20, spaceY = 100 + 6 * 22, spaceW = 120, spaceH = 28;
  int backX = 150, backY = spaceY, backW = 80, backH = 28;
  if (isPointInRect(x, y, spaceX, spaceY, spaceW, spaceH)) {
    int len = strlen(buffer);
    if (len + 1 < maxLength) {
      buffer[len] = ' ';
      buffer[len+1] = '\0';
    }
    return;
  }
  if (isPointInRect(x, y, backX, backY, backW, backH)) {
    int len = strlen(buffer);
    if (len > 0) buffer[len-1] = '\0';
    return;
  }

  // Character keys
  const char* rows[] = {"ABCDEF", "GHIJKL", "MNOPQR", "STUVWX", "YZ0123", "456789"};
  int startY = 100;
  for (int r = 0; r < 6; r++) {
    for (int c = 0; c < (int)strlen(rows[r]); c++) {
      int kx = 20 + c * 44;
      int ky = startY + r * 22;
      if (isPointInRect(x, y, kx, ky, 40, 20)) {
        int len = strlen(buffer);
        if (len + 1 < maxLength) {
          char ch = rows[r][c];
          buffer[len] = ch;
          buffer[len+1] = '\0';
        }
        return;
      }
    }
  }
}

bool showVirtualKeyboard(char* buffer, int maxLength, const char* title) {
  // Setup
  keyboardActive = true;
  keyboardConfirmed = false;
  keyboardTitle = String(title ? title : "Input");
  // copy initial content
  strncpy(keyboardBufferInternal, buffer ? buffer : "", sizeof(keyboardBufferInternal)-1);
  keyboardBufferInternal[sizeof(keyboardBufferInternal)-1] = '\0';

  // Draw keyboard and loop until user confirms or cancels
  drawVirtualKeyboard();
  unsigned long start = millis();
  while (keyboardActive) {
    TouchEvent t = readTouch();
    if (t.pressed) {
      handleKeyboardTouch(t.x, t.y, keyboardBufferInternal, sizeof(keyboardBufferInternal));
      // redraw buffer area
      tft.fillRect(20, 68, 280, 20, COLOR_BUTTON);
      tft.setTextColor(COLOR_TEXT);
      tft.setTextSize(1);
      tft.drawString(String(keyboardBufferInternal), 20, 70);
    }
    // small sleep to avoid busy loop
    delay(50);
    // optional timeout (2 minutes)
    if (millis() - start > 120000) { keyboardActive = false; keyboardConfirmed = false; }
  }

  // Copy out if confirmed
  if (keyboardConfirmed) {
    strncpy(buffer, keyboardBufferInternal, maxLength-1);
    buffer[maxLength-1] = '\0';
    return true;
  }
  return false;
}

void handleModuleConfigTouch(int x, int y) {
  if (!inModuleListScreen) return;

  // Back button
  if (isPointInRect(x, y, 10, 210, 80, 25)) {
    inModuleListScreen = false;
    drawMainInterface();
    return;
  }
  // Determine if user hit up/down arrows
  if (isPointInRect(x, y, 300, moduleListStartY - 28, 16, 16)) {
    if (moduleListScroll > 0) moduleListScroll--;
    drawModuleList();
    return;
  }
  if (isPointInRect(x, y, 300, 200, 16, 16)) {
    // Count total active modules
    int totalActive = 0;
    for (int i = 0; i < MAX_PLUGIN_MODULES; i++) if (moduleSystem.modules[i].active) totalActive++;
    if (moduleListScroll + itemsPerPage < totalActive) moduleListScroll++;
    drawModuleList();
    return;
  }

  // Find which module row was tapped (consider scroll offset)
  int row = (y - moduleListStartY) / moduleListItemHeight;
  if (row < 0 || row >= itemsPerPage) return;

  int seen = 0;
  for (int i = 0; i < MAX_PLUGIN_MODULES; i++) {
    RegisteredModule* module = &moduleSystem.modules[i];
    if (!module->active) continue;
    if (seen < moduleListScroll) { seen++; continue; }
    if (seen - moduleListScroll == row) {
      // Open details for this module
      currentModuleDetailsId = module->config.moduleId;
      inModuleDetailsScreen = true;
      drawModuleDetails(currentModuleDetailsId);
      return;
    }
    seen++;
  }
}

// The rest of the UI drawing functions (drawMainInterface, drawButton, drawSlider,
// drawStatusBar, readTouch, isPointInRect, saveCalibration, showScreenSaver,
// loadCalibration, updateOTAProgressScreen, showOTAErrorScreen, showOTASuccessScreen,
// showOTARollbackScreen, showOTANotificationScreen, checkForOTACancel,
// showFactoryResetWarning, showConfigurationMenu, showWiFiConfigScreen,
// showMQTTConfigScreen, showVirtualKeyboard, drawVirtualKeyboard,
// handleKeyboardTouch) are intentionally retained in the header for now
// except for globals to minimize diffs. If linker errors persist, we will move
// more functions here.
