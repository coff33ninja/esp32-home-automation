#include "led_effects.h"

// Global variable definitions (single translation unit)
int lastVolumeLevel = 0;
unsigned long lastVolumeChange = 0;
bool volumeVisualizationActive = false;
int touchPaintColor = 0x00FF00; // Default green
bool touchInteractionActive = false;
int lastTouchX = 0, lastTouchY = 0;
unsigned long lastTouchTime = 0;
LEDEffect currentEffect = EFFECT_OFF;
CRGB currentColor = CRGB::Black;
uint8_t currentBrightness = FAILSAFE_BRIGHTNESS;

// LED array definition
CRGB leds[NUM_LEDS];
