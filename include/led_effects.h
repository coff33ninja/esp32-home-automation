/**
 * LED Effects Module
 * 
 * Manages LED matrix effects and animations for WS2812B 16x16 matrix.
 * Includes various visual effects and color patterns.
 */

#ifndef LED_EFFECTS_H
#define LED_EFFECTS_H

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

// Effect types
enum LEDEffect {
    EFFECT_OFF,
    EFFECT_SOLID_COLOR,
    EFFECT_RAINBOW,
    EFFECT_RAINBOW_CYCLE,
    EFFECT_FIRE,
    EFFECT_SPARKLE,
    EFFECT_BREATHING,
    EFFECT_THEATER_CHASE,
    EFFECT_VOLUME_BAR,
    EFFECT_VOLUME_CIRCLE,
    EFFECT_VOLUME_WAVE,
    EFFECT_TOUCH_PAINT,
    EFFECT_TOUCH_RIPPLE,
    EFFECT_TOUCH_TRAIL
};

// Volume visualization variables
extern int lastVolumeLevel;
extern unsigned long lastVolumeChange;
extern bool volumeVisualizationActive;

// Touch interaction variables
extern int touchPaintColor;
extern bool touchInteractionActive;
extern int lastTouchX, lastTouchY;
extern unsigned long lastTouchTime;

// Global variables
extern CRGB leds[NUM_LEDS];
extern LEDEffect currentEffect;
extern CRGB currentColor;
extern uint8_t currentBrightness;

// Function prototypes
void initLEDEffects();
void setEffect(LEDEffect effect);
void setSolidColor(CRGB color);
void setBrightness(uint8_t brightness);
void updateEffects();
void clearMatrix();

// Volume visualization functions
void setVolumeVisualization(int volume);
void updateVolumeVisualization();
CRGB getVolumeColor(int volume);
void showVolumeChangeAnimation(int oldVolume, int newVolume);

// Touch interaction functions
void handleTouchOnMatrix(int screenX, int screenY, CRGB color);
void setTouchPaintColor(CRGB color);
void addTouchRipple(int matrixX, int matrixY, CRGB color);
void updateTouchEffects();
int mapScreenToMatrix(int screenCoord, int screenSize, int matrixSize);

// Effect implementations
void effectOff();
void effectSolidColor();
void effectRainbow();
void effectRainbowCycle();
void effectFire();
void effectSparkle();
void effectBreathing();
void effectTheaterChase();
void effectVolumeBar();
void effectVolumeCircle();
void effectVolumeWave();
void effectTouchPaint();
void effectTouchRipple();
void effectTouchTrail();

// Utility functions
uint16_t XY(uint8_t x, uint8_t y);

// Implementation moved to src/led_effects.cpp to avoid multiple-definition
// Global variables are defined in src/led_effects_globals.cpp
#endif // LED_EFFECTS_H
