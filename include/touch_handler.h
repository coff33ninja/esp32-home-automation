/**
 * Touch Screen Handler Module
 *
 * Manages ILI9341 touch screen interface with XPT2046 touch controller.
 * Provides display rendering, touch event handling, and calibration.
 */

#ifndef TOUCH_HANDLER_H
#define TOUCH_HANDLER_H

#include "config.h"
#include "led_effects.h"
#include "relay_control.h"
#include "ota_updater.h"
#include "config_manager.h"
#include <Arduino.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

// Touch screen constants
#define TOUCH_THRESHOLD 600     // Minimum pressure for valid touch
#define TOUCH_DEBOUNCE_MS 50    // Debounce time for touch events
#define SCREEN_TIMEOUT_MS 30000 // Screen dim timeout (30 seconds)
#define CALIBRATION_POINTS 4    // Number of calibration points

// Screen states
enum ScreenState { SCREEN_OFF, SCREEN_DIM, SCREEN_ACTIVE, SCREEN_CALIBRATING };

// Touch event structure
struct TouchEvent {
  uint16_t x;
  uint16_t y;
  uint16_t pressure;
  bool pressed;
  bool released;
  bool held;
  unsigned long timestamp;
};

// Screen layout constants
#define BUTTON_HEIGHT 60
#define BUTTON_WIDTH 100
#define SLIDER_HEIGHT 30
#define SLIDER_WIDTH 200
#define MARGIN 10

// Colors (RGB565 format)
#define COLOR_BACKGROUND 0x0000     // Black
#define COLOR_BUTTON 0x4A69         // Dark blue
#define COLOR_BUTTON_PRESSED 0x7BEF // Light blue
#define COLOR_TEXT 0xFFFF           // White
#define COLOR_SLIDER_TRACK 0x39C7   // Gray
#define COLOR_SLIDER_THUMB 0xF800   // Red
#define COLOR_STATUS_OK 0x07E0      // Green
#define COLOR_STATUS_ERROR 0xF800   // Red

// Button IDs
enum ButtonID {
  BTN_RELAY1 = 1,
  BTN_RELAY2,
  BTN_RELAY3,
  BTN_RELAY4,
  BTN_ALL_LIGHTS,
  BTN_EFFECT_NEXT,
  BTN_SETTINGS,
  BTN_CALIBRATE
};

// Slider IDs
enum SliderID {
  SLIDER_LED_BRIGHTNESS = 10,
  SLIDER_MATRIX_BRIGHTNESS,
  SLIDER_VOLUME
};

// External references from other modules
extern RelayStates relayStates;
extern LEDEffect currentEffect;
extern int brightness;
extern int currentVolume;
extern bool lightsState;
extern PubSubClient mqttClient;

// Forward declarations
void toggleRelay(RelayChannel channel);
void setAllRelays(bool state);
void setEffect(LEDEffect effect);
void setBrightness(uint8_t brightness);
void setVolumeVisualization(int volume);
void moveMotorToPosition(int targetPosition);

// Global variables
extern TFT_eSPI tft;
extern ScreenState currentScreenState;
extern TouchEvent lastTouch;
extern unsigned long lastTouchTime;
extern unsigned long lastScreenActivity;
extern bool touchCalibrated;
extern uint16_t touchCalibration[8]; // 4 points, x,y each
extern bool touchScreenInitialized;

// Function prototypes
bool initTouchScreen();
void updateDisplay();
bool handleTouch();
void showCalibrationScreen();
bool performCalibration();
void setScreenBrightness(uint8_t level);
void wakeScreen();
void dimScreen();
void turnOffScreen();
void showScreenSaver();
void drawMainInterface();
void drawButton(int x, int y, int w, int h, const char *text, bool pressed,
                int id);
void drawSlider(int x, int y, int w, int h, int value, int maxValue,
                const char *label, SliderID id);
void drawStatusBar();
TouchEvent readTouch();
bool isPointInRect(int px, int py, int rx, int ry, int rw, int rh);
void saveCalibration();
void loadCalibration();
void updateOTAProgressScreen(int progress, const String &message);
void showOTAErrorScreen(const String &error);
void showOTASuccessScreen();
void showOTARollbackScreen();
void showOTANotificationScreen(const OTAUpdateInfo &info, bool mandatory);
bool checkForOTACancel();
void showFactoryResetWarning();
void showConfigurationMenu();
void showWiFiConfigScreen();
void showMQTTConfigScreen();
void showHardwareConfigScreen();
void showSystemConfigScreen();
void handleConfigurationTouch(int x, int y);
bool showVirtualKeyboard(char* buffer, int maxLength, const char* title);
void drawVirtualKeyboard();
void handleKeyboardTouch(int x, int y, char* buffer, int maxLength);

// Implementations moved to src/touch_handler.cpp to avoid multiple definitions.

#endif // TOUCH_HANDLER_H