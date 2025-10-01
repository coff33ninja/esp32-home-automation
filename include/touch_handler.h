/**
 * Touch Screen Handler Module
 * 
 * Manages ILI9341 touch screen interface with XPT2046 touch controller.
 * Provides display rendering, touch event handling, and calibration.
 */

#ifndef TOUCH_HANDLER_H
#define TOUCH_HANDLER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "relay_control.h"
#include "led_effects.h"

// Touch screen constants
#define TOUCH_THRESHOLD 600         // Minimum pressure for valid touch
#define TOUCH_DEBOUNCE_MS 50       // Debounce time for touch events
#define SCREEN_TIMEOUT_MS 30000    // Screen dim timeout (30 seconds)
#define CALIBRATION_POINTS 4       // Number of calibration points

// Screen states
enum ScreenState {
    SCREEN_OFF,
    SCREEN_DIM,
    SCREEN_ACTIVE,
    SCREEN_CALIBRATING
};

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
void drawButton(int x, int y, int w, int h, const char* text, bool pressed, ButtonID id);
void drawSlider(int x, int y, int w, int h, int value, int maxValue, const char* label, SliderID id);
void drawStatusBar();
TouchEvent readTouch();
bool isPointInRect(int px, int py, int rx, int ry, int rw, int rh);
void saveCalibration();
void loadCalibration();

// Implementation

bool initTouchScreen() {
    DEBUG_PRINTLN("[TOUCH] Initializing touch screen...");
    
    // Initialize TFT display
    tft.init();
    tft.setRotation(1); // Landscape orientation
    tft.fillScreen(COLOR_BACKGROUND);
    
    // Initialize touch controller
    uint16_t calData[5];
    uint8_t calDataOK = 0;
    
    // Load calibration from EEPROM if available
    loadCalibration();
    
    if (touchCalibrated) {
        tft.setTouch(touchCalibration);
        DEBUG_PRINTLN("[TOUCH] Using saved calibration");
    } else {
        DEBUG_PRINTLN("[TOUCH] No calibration found, using defaults");
        // Set default calibration values (may need adjustment)
        uint16_t defaultCal[5] = {300, 3600, 300, 3600, 1};
        tft.setTouch(defaultCal);
    }
    
    // Set initial screen state
    currentScreenState = SCREEN_ACTIVE;
    lastScreenActivity = millis();
    lastTouchTime = 0;
    
    // Clear touch event
    lastTouch.pressed = false;
    lastTouch.released = false;
    lastTouch.held = false;
    lastTouch.pressure = 0;
    
    // Draw initial interface
    drawMainInterface();
    
    touchScreenInitialized = true;
    DEBUG_PRINTLN("[TOUCH] Touch screen initialized successfully");
    return true;
}

void updateDisplay() {
    static unsigned long lastFullUpdate = 0;
    unsigned long now = millis();
    
    // Handle screen timeout progression: ACTIVE -> DIM -> OFF
    if (currentScreenState == SCREEN_ACTIVE && 
        now - lastScreenActivity > SCREEN_TIMEOUT_MS) {
        dimScreen();
    } else if (currentScreenState == SCREEN_DIM && 
               now - lastScreenActivity > SCREEN_TIMEOUT_MS * 2) {
        // Turn off screen after being dimmed for another timeout period
        turnOffScreen();
    }
    
    // Update display content based on state
    switch (currentScreenState) {
        case SCREEN_ACTIVE:
            // Full interface updates every 500ms to avoid flicker
            if (now - lastFullUpdate > 500) {
                drawStatusBar(); // Update dynamic status information
                lastFullUpdate = now;
            }
            break;
            
        case SCREEN_DIM:
            // Minimal updates when dimmed - only critical status
            if (now - lastFullUpdate > 2000) { // Update every 2 seconds
                drawStatusBar();
                lastFullUpdate = now;
            }
            break;
            
        case SCREEN_OFF:
            // Screen is off - show minimal screensaver
            if (now - lastFullUpdate > 5000) { // Update every 5 seconds
                showScreenSaver();
                lastFullUpdate = now;
            }
            break;
            
        case SCREEN_CALIBRATING:
            // Calibration screen is handled separately
            break;
    }
}

bool handleTouch() {
    TouchEvent touch = readTouch();
    
    if (!touch.pressed && !touch.released) {
        return false; // No touch event
    }
    
    lastScreenActivity = millis();
    
    // Wake screen if dimmed or off
    if (currentScreenState != SCREEN_ACTIVE) {
        wakeScreen();
        return true; // Consume the touch event for wake
    }
    
    // Update activity timer for any touch when screen is active
    if (currentScreenState == SCREEN_ACTIVE) {
        lastScreenActivity = millis();
    }
    
    // Handle calibration mode
    if (currentScreenState == SCREEN_CALIBRATING) {
        // Calibration handling would go here
        return true;
    }
    
    // Handle button presses
    if (touch.pressed) {
        DEBUG_PRINTF("[TOUCH] Touch at (%d, %d) pressure: %d\n", 
                     touch.x, touch.y, touch.pressure);
        
        // Check lighting control buttons
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
        
        // Check effect button
        if (isPointInRect(touch.x, touch.y, 10, 130, 90, 40)) {
            DEBUG_PRINTLN("[TOUCH] Next Effect button pressed");
            currentEffect = (LEDEffect)((currentEffect + 1) % 8);
            setEffect(currentEffect);
            drawMainInterface();
            return true;
        }
        
        // Check settings button
        if (isPointInRect(touch.x, touch.y, 220, 240, 80, 30)) {
            DEBUG_PRINTLN("[TOUCH] Settings button pressed");
            showCalibrationScreen(); // For now, settings opens calibration
            return true;
        }
        
        // Check for diagnostic interface access (long press on status bar)
        static unsigned long statusBarPressTime = 0;
        if (isPointInRect(touch.x, touch.y, 0, 275, 320, 20)) {
            if (statusBarPressTime == 0) {
                statusBarPressTime = millis();
            } else if (millis() - statusBarPressTime > 3000) { // 3 second long press
                DEBUG_PRINTLN("[TOUCH] Diagnostic interface activated");
                extern void showDiagnosticInterface();
                showDiagnosticInterface();
                statusBarPressTime = 0;
                return true;
            }
        } else {
            statusBarPressTime = 0;
        }
        
        // Check sliders
        // LED Strip brightness slider
        if (isPointInRect(touch.x, touch.y, 10, 195, 140, 25)) {
            DEBUG_PRINTLN("[TOUCH] LED Strip brightness slider touched");
            int sliderValue = map(touch.x - 10, 0, 140, 0, 255);
            sliderValue = constrain(sliderValue, 0, 255);
            
            brightness = sliderValue;
            ledcWrite(LED_STRIP_PWM_CHANNEL, brightness);
            
            // Publish to MQTT
            if (mqttClient.connected()) {
                char msg[10];
                snprintf(msg, sizeof(msg), "%d", brightness);
                mqttClient.publish("homecontrol/led_brightness", msg);
            }
            
            drawMainInterface();
            return true;
        }
        
        // Matrix brightness slider
        if (isPointInRect(touch.x, touch.y, 160, 195, 140, 25)) {
            DEBUG_PRINTLN("[TOUCH] Matrix brightness slider touched");
            int sliderValue = map(touch.x - 160, 0, 140, 0, 255);
            sliderValue = constrain(sliderValue, 0, 255);
            
            setBrightness(sliderValue);
            
            // Publish to MQTT
            if (mqttClient.connected()) {
                char msg[10];
                snprintf(msg, sizeof(msg), "%d", sliderValue);
                mqttClient.publish("homecontrol/matrix_brightness", msg);
            }
            
            drawMainInterface();
            return true;
        }
        
        // Volume slider
        if (isPointInRect(touch.x, touch.y, 10, 245, 200, 25)) {
            DEBUG_PRINTLN("[TOUCH] Volume slider touched");
            int sliderValue = map(touch.x - 10, 0, 200, 0, 100);
            sliderValue = constrain(sliderValue, 0, 100);
            
            currentVolume = sliderValue;
            
            // Move motor to match volume
            int targetPosition = map(currentVolume, 0, 100, POT_MIN_VALUE, POT_MAX_VALUE);
            moveMotorToPosition(targetPosition);
            
            // Trigger volume visualization on LED matrix
            setVolumeVisualization(currentVolume);
            
            // Publish to MQTT
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
    // This would implement the actual calibration routine
    // For now, return true to indicate success
    DEBUG_PRINTLN("[TOUCH] Performing calibration (placeholder)");
    
    // In a real implementation, this would:
    // 1. Show calibration points at screen corners
    // 2. Wait for user to touch each point
    // 3. Calculate calibration matrix
    // 4. Store calibration values
    
    return true;
}

void setScreenBrightness(uint8_t level) {
    DEBUG_PRINTF("[TOUCH] Setting screen brightness to: %d\n", level);
    
    // If backlight is connected to a PWM pin, control it
    #ifdef TFT_BACKLIGHT_PIN
        // Use PWM to control backlight brightness
        ledcSetup(2, 5000, 8); // Channel 2, 5kHz, 8-bit resolution
        ledcAttachPin(TFT_BACKLIGHT_PIN, 2);
        ledcWrite(2, level);
    #else
        // If no backlight control, just log the request
        DEBUG_PRINTLN("[TOUCH] No backlight control pin defined");
    #endif
    
    // Store current brightness level
    static uint8_t currentScreenBrightness = 255;
    currentScreenBrightness = level;
}

void wakeScreen() {
    if (currentScreenState != SCREEN_ACTIVE) {
        DEBUG_PRINTF("[TOUCH] Waking screen from state: %d\n", currentScreenState);
        
        // Smooth wake-up animation
        for (int brightness = 0; brightness <= 255; brightness += 15) {
            setScreenBrightness(brightness);
            delay(10); // Small delay for smooth transition
        }
        
        currentScreenState = SCREEN_ACTIVE;
        lastScreenActivity = millis(); // Reset activity timer
        drawMainInterface(); // Redraw full interface
        
        DEBUG_PRINTLN("[TOUCH] Screen wake complete");
    }
}

void dimScreen() {
    if (currentScreenState == SCREEN_ACTIVE) {
        DEBUG_PRINTLN("[TOUCH] Dimming screen");
        currentScreenState = SCREEN_DIM;
        setScreenBrightness(30); // Dim to 30/255
        
        // Show dim overlay
        tft.fillRect(0, 0, 320, 240, 0x0000); // Black overlay with transparency effect
        tft.setTextColor(0x4208); // Dark gray
        tft.setTextSize(2);
        tft.drawString("Touch to wake", 80, 110);
    }
}

void turnOffScreen() {
    if (currentScreenState == SCREEN_DIM) {
        DEBUG_PRINTLN("[TOUCH] Turning off screen");
        currentScreenState = SCREEN_OFF;
        setScreenBrightness(0); // Turn off backlight completely
        tft.fillScreen(COLOR_BACKGROUND); // Clear screen
    }
}

void drawMainInterface() {
    tft.fillScreen(COLOR_BACKGROUND);
    
    // Title bar with system status
    tft.fillRect(0, 0, 320, 30, 0x2104); // Dark header
    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(2);
    tft.drawString("ESP32 Home Control", 10, 8);
    
    // Connection status indicators
    tft.setTextSize(1);
    tft.setTextColor(WiFi.status() == WL_CONNECTED ? COLOR_STATUS_OK : COLOR_STATUS_ERROR);
    tft.drawString("WiFi", 250, 10);
    tft.setTextColor(mqttClient.connected() ? COLOR_STATUS_OK : COLOR_STATUS_ERROR);
    tft.drawString("MQTT", 280, 10);
    
    // Main control area
    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(1);
    tft.drawString("Lighting Control", 10, 40);
    
    // Relay control buttons with improved layout
    drawButton(10, 55, 90, 50, "Main\nLights", relayStates.relay1, BTN_RELAY1);
    drawButton(110, 55, 90, 50, "Accent\nLights", relayStates.relay2, BTN_RELAY2);
    drawButton(210, 55, 90, 50, "All\nLights", lightsState, BTN_ALL_LIGHTS);
    
    // LED Matrix Effects section
    tft.drawString("LED Matrix Effects", 10, 115);
    drawButton(10, 130, 90, 40, "Next\nEffect", false, BTN_EFFECT_NEXT);
    
    // Show current effect name
    const char* effectNames[] = {"Off", "Solid", "Rainbow", "Cycle", "Fire", "Sparkle", "Breathing", "Chase"};
    tft.setTextColor(0x7BEF); // Light blue
    tft.drawString("Current:", 110, 135);
    if (currentEffect < 8) {
        tft.drawString(effectNames[currentEffect], 110, 150);
    }
    
    // Control sliders section
    tft.setTextColor(COLOR_TEXT);
    tft.drawString("Brightness Controls", 10, 180);
    
    // LED Strip brightness
    drawSlider(10, 195, 140, 25, brightness, 255, "LED Strip", SLIDER_LED_BRIGHTNESS);
    
    // Matrix brightness  
    extern uint8_t currentBrightness;
    drawSlider(160, 195, 140, 25, currentBrightness, 255, "Matrix", SLIDER_MATRIX_BRIGHTNESS);
    
    // Volume control (if motor control is working)
    tft.drawString("Volume Control", 10, 230);
    drawSlider(10, 245, 200, 25, currentVolume, 100, "Volume", SLIDER_VOLUME);
    
    // Settings button
    drawButton(220, 240, 80, 30, "Settings", false, BTN_SETTINGS);
    
    // Status bar at bottom
    drawStatusBar();
}

void drawButton(int x, int y, int w, int h, const char* text, bool pressed, ButtonID id) {
    uint16_t bgColor = pressed ? COLOR_BUTTON_PRESSED : COLOR_BUTTON;
    uint16_t borderColor = pressed ? COLOR_STATUS_OK : COLOR_TEXT;
    
    // Draw button with 3D effect
    if (pressed) {
        // Pressed button - darker and inset
        tft.fillRect(x + 1, y + 1, w - 2, h - 2, bgColor);
        tft.drawRect(x + 1, y + 1, w - 2, h - 2, borderColor);
    } else {
        // Normal button - raised appearance
        tft.fillRect(x, y, w, h, bgColor);
        tft.drawRect(x, y, w, h, borderColor);
        // Add highlight on top and left
        tft.drawLine(x + 1, y + 1, x + w - 2, y + 1, 0x8410); // Light gray
        tft.drawLine(x + 1, y + 1, x + 1, y + h - 2, 0x8410);
    }
    
    // Draw button text with multi-line support
    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(1);
    
    // Handle multi-line text (split on \n)
    char textCopy[50];
    strncpy(textCopy, text, sizeof(textCopy) - 1);
    textCopy[sizeof(textCopy) - 1] = '\0';
    
    char* line = strtok(textCopy, "\n");
    int lineY = y + (h - 16) / 2; // Start position for first line
    
    while (line != NULL) {
        int textWidth = strlen(line) * 6; // Approximate character width
        int textX = x + (w - textWidth) / 2;
        
        tft.drawString(line, textX, lineY);
        lineY += 10; // Move to next line
        line = strtok(NULL, "\n");
    }
}

void drawSlider(int x, int y, int w, int h, int value, int maxValue, const char* label, SliderID id) {
    // Draw label
    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(1);
    tft.drawString(label, x, y - 12);
    
    // Draw value
    char valueStr[20];
    if (id == SLIDER_VOLUME) {
        snprintf(valueStr, sizeof(valueStr), "%d%%", value);
    } else {
        snprintf(valueStr, sizeof(valueStr), "%d", value);
    }
    tft.drawString(valueStr, x + w - 30, y - 12);
    
    // Draw slider track (rounded rectangle effect)
    int trackY = y + h/4;
    int trackH = h/2;
    tft.fillRect(x + 2, trackY + 2, w - 4, trackH - 4, COLOR_SLIDER_TRACK);
    tft.drawRect(x, trackY, w, trackH, COLOR_TEXT);
    
    // Draw progress fill
    int fillWidth = map(value, 0, maxValue, 0, w - 4);
    if (fillWidth > 0) {
        uint16_t fillColor = (value > maxValue * 0.8) ? COLOR_STATUS_ERROR : 
                            (value > maxValue * 0.5) ? 0xFFE0 : COLOR_STATUS_OK; // Red/Yellow/Green
        tft.fillRect(x + 2, trackY + 2, fillWidth, trackH - 4, fillColor);
    }
    
    // Draw slider thumb
    int thumbPos = map(value, 0, maxValue, 5, w - 15);
    int thumbX = x + thumbPos;
    
    // Thumb with 3D effect
    tft.fillRect(thumbX, y + 2, 10, h - 4, COLOR_SLIDER_THUMB);
    tft.drawRect(thumbX, y + 2, 10, h - 4, COLOR_TEXT);
    tft.drawLine(thumbX + 1, y + 3, thumbX + 8, y + 3, 0xFFFF); // Highlight
    tft.drawLine(thumbX + 1, y + 3, thumbX + 1, y + h - 4, 0xFFFF);
}

void drawStatusBar() {
    // Draw status bar background
    int statusY = 275;
    tft.fillRect(0, statusY - 2, 320, 20, 0x2104); // Dark background
    
    tft.setTextSize(1);
    
    // System status
    tft.setTextColor(COLOR_TEXT);
    tft.drawString("Status:", 5, statusY);
    
    // Memory usage
    uint32_t freeHeap = ESP.getFreeHeap();
    char memStr[20];
    snprintf(memStr, sizeof(memStr), "Mem:%dK", freeHeap / 1024);
    tft.setTextColor(freeHeap > 50000 ? COLOR_STATUS_OK : COLOR_STATUS_ERROR);
    tft.drawString(memStr, 45, statusY);
    
    // WiFi signal strength
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        char rssiStr[15];
        snprintf(rssiStr, sizeof(rssiStr), "WiFi:%ddBm", rssi);
        tft.setTextColor(rssi > -70 ? COLOR_STATUS_OK : (rssi > -85 ? 0xFFE0 : COLOR_STATUS_ERROR));
        tft.drawString(rssiStr, 100, statusY);
    } else {
        tft.setTextColor(COLOR_STATUS_ERROR);
        tft.drawString("WiFi:OFF", 100, statusY);
    }
    
    // MQTT status
    tft.setTextColor(mqttClient.connected() ? COLOR_STATUS_OK : COLOR_STATUS_ERROR);
    tft.drawString(mqttClient.connected() ? "MQTT:OK" : "MQTT:OFF", 170, statusY);
    
    // Current time (uptime)
    unsigned long uptime = millis() / 1000;
    unsigned long hours = uptime / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    char timeStr[15];
    snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu", hours, minutes);
    tft.setTextColor(COLOR_TEXT);
    tft.drawString(timeStr, 240, statusY);
    
    // Touch indicator (show last touch coordinates for debugging)
    if (millis() - lastTouchTime < 2000) { // Show for 2 seconds after touch
        char touchStr[20];
        snprintf(touchStr, sizeof(touchStr), "T:%d,%d", lastTouch.x, lastTouch.y);
        tft.setTextColor(0x7BEF); // Light blue
        tft.drawString(touchStr, 280, statusY);
    }
}

TouchEvent readTouch() {
    TouchEvent event = {0};
    event.timestamp = millis();
    
    uint16_t x, y;
    bool touched = tft.getTouch(&x, &y);
    
    if (touched) {
        // Debounce touch events
        if (event.timestamp - lastTouchTime < TOUCH_DEBOUNCE_MS) {
            return event; // Return empty event
        }
        
        event.x = x;
        event.y = y;
        event.pressure = 1000; // TFT_eSPI doesn't provide pressure, use fixed value
        event.pressed = !lastTouch.pressed; // New press if last wasn't pressed
        event.held = lastTouch.pressed;     // Held if last was also pressed
        event.released = false;
        
        lastTouchTime = event.timestamp;
        
        DEBUG_PRINTF("[TOUCH] Raw touch: (%d, %d)\n", x, y);
    } else {
        // No touch detected
        event.released = lastTouch.pressed; // Released if last was pressed
        event.pressed = false;
        event.held = false;
    }
    
    lastTouch = event;
    return event;
}

bool isPointInRect(int px, int py, int rx, int ry, int rw, int rh) {
    return (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh);
}

void saveCalibration() {
    // Save calibration to EEPROM
    DEBUG_PRINTLN("[TOUCH] Saving calibration to EEPROM");
    
    // This would save the calibration data to EEPROM
    // Implementation depends on your EEPROM library choice
    // For now, just mark as calibrated
    touchCalibrated = true;
}

void showScreenSaver() {
    static int dotX = 160, dotY = 120;
    static int deltaX = 2, deltaY = 1;
    
    // Simple bouncing dot screensaver
    tft.fillScreen(COLOR_BACKGROUND);
    
    // Update dot position
    dotX += deltaX;
    dotY += deltaY;
    
    // Bounce off edges
    if (dotX <= 5 || dotX >= 315) deltaX = -deltaX;
    if (dotY <= 5 || dotY >= 235) deltaY = -deltaY;
    
    // Draw bouncing dot
    tft.fillCircle(dotX, dotY, 3, 0x07E0); // Green dot
    
    // Show minimal status
    tft.setTextColor(0x4208); // Dark gray
    tft.setTextSize(1);
    
    // Show time
    unsigned long uptime = millis() / 1000;
    unsigned long hours = uptime / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu", hours, minutes);
    tft.drawString(timeStr, 140, 220);
    
    // Show system status
    if (WiFi.status() == WL_CONNECTED) {
        tft.drawString("WiFi OK", 10, 220);
    }
    if (mqttClient.connected()) {
        tft.drawString("MQTT OK", 60, 220);
    }
}

void loadCalibration() {
    // Load calibration from EEPROM
    DEBUG_PRINTLN("[TOUCH] Loading calibration from EEPROM");
    
    // This would load calibration data from EEPROM
    // For now, assume no calibration is saved
    touchCalibrated = false;
}

// Global variable definitions
TFT_eSPI tft = TFT_eSPI();
ScreenState currentScreenState = SCREEN_OFF;
TouchEvent lastTouch = {0};
unsigned long lastTouchTime = 0;
unsigned long lastScreenActivity = 0;
bool touchCalibrated = false;
uint16_t touchCalibration[8] = {0};
bool touchScreenInitialized = false;

#endif // TOUCH_HANDLER_H