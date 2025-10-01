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

// Implementation

void initLEDEffects() {
    currentEffect = EFFECT_OFF;
    currentColor = CRGB::Black;
    currentBrightness = FAILSAFE_BRIGHTNESS;
    
    FastLED.setBrightness(currentBrightness);
    clearMatrix();
    
    DEBUG_PRINTLN("[LED] LED effects initialized");
}

void clearMatrix() {
    FastLED.clear();
    FastLED.show();
}

void setEffect(LEDEffect effect) {
    currentEffect = effect;
    DEBUG_PRINTF("[LED] Effect changed to: %d\n", effect);
}

void setSolidColor(CRGB color) {
    currentColor = color;
    fill_solid(leds, NUM_LEDS, color);
    FastLED.show();
    DEBUG_PRINTF("[LED] Color set to RGB(%d,%d,%d)\n", color.r, color.g, color.b);
}

void setBrightness(uint8_t brightness) {
    currentBrightness = constrain(brightness, 0, MAX_BRIGHTNESS);
    FastLED.setBrightness(currentBrightness);
    FastLED.show();
    DEBUG_PRINTF("[LED] Brightness set to: %d\n", currentBrightness);
}

// Convert X,Y coordinates to LED index (for 16x16 matrix)
uint16_t XY(uint8_t x, uint8_t y) {
    uint16_t i;
    
    if (y & 0x01) {
        // Odd rows run backwards
        uint8_t reverseX = (MATRIX_WIDTH - 1) - x;
        i = (y * MATRIX_WIDTH) + reverseX;
    } else {
        // Even rows run forwards
        i = (y * MATRIX_WIDTH) + x;
    }
    
    return i;
}

void updateEffects() {
    // Check if volume visualization should override current effect
    if (volumeVisualizationActive && millis() - lastVolumeChange < 3000) {
        updateVolumeVisualization();
        return;
    } else {
        volumeVisualizationActive = false;
    }
    
    switch (currentEffect) {
        case EFFECT_OFF:
            effectOff();
            break;
        case EFFECT_SOLID_COLOR:
            effectSolidColor();
            break;
        case EFFECT_RAINBOW:
            effectRainbow();
            break;
        case EFFECT_RAINBOW_CYCLE:
            effectRainbowCycle();
            break;
        case EFFECT_FIRE:
            effectFire();
            break;
        case EFFECT_SPARKLE:
            effectSparkle();
            break;
        case EFFECT_BREATHING:
            effectBreathing();
            break;
        case EFFECT_THEATER_CHASE:
            effectTheaterChase();
            break;
        case EFFECT_VOLUME_BAR:
            effectVolumeBar();
            break;
        case EFFECT_VOLUME_CIRCLE:
            effectVolumeCircle();
            break;
        case EFFECT_VOLUME_WAVE:
            effectVolumeWave();
            break;
        case EFFECT_TOUCH_PAINT:
            effectTouchPaint();
            break;
        case EFFECT_TOUCH_RIPPLE:
            effectTouchRipple();
            break;
        case EFFECT_TOUCH_TRAIL:
            effectTouchTrail();
            break;
    }
}

void effectOff() {
    // Matrix off
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
}

void effectSolidColor() {
    // Display current color
    fill_solid(leds, NUM_LEDS, currentColor);
    FastLED.show();
}

void effectRainbow() {
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue, 7);
    FastLED.show();
    hue++;
    delay(20);
}

void effectRainbowCycle() {
    static uint8_t hue = 0;
    
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(hue + (i * 256 / NUM_LEDS), 255, 255);
    }
    
    FastLED.show();
    hue++;
    delay(20);
}

void effectFire() {
    // Simplified fire effect
    static byte heat[NUM_LEDS];
    
    // Cool down every cell
    for (int i = 0; i < NUM_LEDS; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((55 * 10) / NUM_LEDS) + 2));
    }
    
    // Heat from each cell drifts 'up' and diffuses
    for (int k = NUM_LEDS - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }
    
    // Randomly ignite new 'sparks'
    if (random8() < 120) {
        int y = random8(7);
        heat[y] = qadd8(heat[y], random8(160, 255));
    }
    
    // Convert heat to LED colors
    for (int j = 0; j < NUM_LEDS; j++) {
        CRGB color = HeatColor(heat[j]);
        leds[j] = color;
    }
    
    FastLED.show();
    delay(15);
}

void effectSparkle() {
    static unsigned long lastUpdate = 0;
    
    if (millis() - lastUpdate > 50) {
        fadeToBlackBy(leds, NUM_LEDS, 20);
        
        int pos = random16(NUM_LEDS);
        leds[pos] = CHSV(random8(), 255, 255);
        
        FastLED.show();
        lastUpdate = millis();
    }
}

void effectBreathing() {
    static uint8_t brightness = 0;
    static int8_t direction = 1;
    
    brightness += direction * 5;
    
    if (brightness >= 255 || brightness <= 0) {
        direction = -direction;
    }
    
    fill_solid(leds, NUM_LEDS, currentColor);
    FastLED.setBrightness(brightness);
    FastLED.show();
    delay(30);
}

void effectTheaterChase() {
    static uint8_t offset = 0;
    
    for (int i = 0; i < NUM_LEDS; i++) {
        if ((i + offset) % 3 == 0) {
            leds[i] = currentColor;
        } else {
            leds[i] = CRGB::Black;
        }
    }
    
    FastLED.show();
    delay(100);
    offset++;
}

// Volume Visualization Functions

void setVolumeVisualization(int volume) {
    if (volume != lastVolumeLevel) {
        showVolumeChangeAnimation(lastVolumeLevel, volume);
        lastVolumeLevel = volume;
        lastVolumeChange = millis();
        volumeVisualizationActive = true;
        
        DEBUG_PRINTF("[LED] Volume visualization: %d%%\n", volume);
    }
}

void updateVolumeVisualization() {
    // Show volume bar effect during volume changes
    effectVolumeBar();
}

CRGB getVolumeColor(int volume) {
    // Color-coded volume levels
    if (volume == 0) {
        return CRGB::Black;  // Muted
    } else if (volume < 30) {
        return CRGB::Green;  // Low volume - green
    } else if (volume < 70) {
        return CRGB::Yellow; // Medium volume - yellow
    } else if (volume < 90) {
        return CRGB::Orange; // High volume - orange
    } else {
        return CRGB::Red;    // Very high volume - red
    }
}

void showVolumeChangeAnimation(int oldVolume, int newVolume) {
    // Quick flash animation to show volume change
    CRGB changeColor = (newVolume > oldVolume) ? CRGB::Green : CRGB::Red;
    
    // Flash the entire matrix briefly
    fill_solid(leds, NUM_LEDS, changeColor);
    FastLED.setBrightness(100);
    FastLED.show();
    delay(50);
    
    // Return to normal brightness
    FastLED.setBrightness(currentBrightness);
}

void effectVolumeBar() {
    // Clear matrix
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    // Calculate number of LEDs to light based on volume
    int volumeLEDs = map(lastVolumeLevel, 0, 100, 0, MATRIX_HEIGHT);
    CRGB volumeColor = getVolumeColor(lastVolumeLevel);
    
    // Draw vertical volume bars (multiple columns for better visibility)
    for (int col = 6; col < 10; col++) {  // Use middle 4 columns
        for (int row = MATRIX_HEIGHT - 1; row >= MATRIX_HEIGHT - volumeLEDs; row--) {
            if (row >= 0) {
                leds[XY(col, row)] = volumeColor;
            }
        }
    }
    
    // Add peak indicator
    if (volumeLEDs > 0 && volumeLEDs < MATRIX_HEIGHT) {
        for (int col = 5; col < 11; col++) {
            leds[XY(col, MATRIX_HEIGHT - volumeLEDs - 1)] = CRGB::White;
        }
    }
    
    // Show volume percentage as dots on sides
    int percentDots = map(lastVolumeLevel, 0, 100, 0, 10);
    for (int i = 0; i < percentDots; i++) {
        // Left side dots
        if (i < 5) {
            leds[XY(2, 15 - i * 3)] = CRGB::Blue;
        } else {
            // Right side dots
            leds[XY(13, 15 - (i - 5) * 3)] = CRGB::Blue;
        }
    }
    
    FastLED.show();
}

void effectVolumeCircle() {
    // Clear matrix
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    // Draw circular volume indicator
    int centerX = MATRIX_WIDTH / 2;
    int centerY = MATRIX_HEIGHT / 2;
    int maxRadius = min(MATRIX_WIDTH, MATRIX_HEIGHT) / 2 - 1;
    int volumeRadius = map(lastVolumeLevel, 0, 100, 0, maxRadius);
    
    CRGB volumeColor = getVolumeColor(lastVolumeLevel);
    
    // Draw filled circle based on volume
    for (int x = 0; x < MATRIX_WIDTH; x++) {
        for (int y = 0; y < MATRIX_HEIGHT; y++) {
            int dx = x - centerX;
            int dy = y - centerY;
            int distance = sqrt(dx * dx + dy * dy);
            
            if (distance <= volumeRadius) {
                leds[XY(x, y)] = volumeColor;
            } else if (distance <= maxRadius) {
                // Outer ring in dim white
                leds[XY(x, y)] = CRGB(20, 20, 20);
            }
        }
    }
    
    FastLED.show();
}

void effectVolumeWave() {
    static uint8_t waveOffset = 0;
    
    // Clear matrix
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    CRGB volumeColor = getVolumeColor(lastVolumeLevel);
    int amplitude = map(lastVolumeLevel, 0, 100, 0, MATRIX_HEIGHT / 2);
    
    // Draw sine wave based on volume
    for (int x = 0; x < MATRIX_WIDTH; x++) {
        int waveHeight = (sin8((x * 16) + waveOffset) * amplitude) / 255;
        int centerY = MATRIX_HEIGHT / 2;
        
        // Draw wave line
        for (int y = centerY - waveHeight; y <= centerY + waveHeight; y++) {
            if (y >= 0 && y < MATRIX_HEIGHT) {
                leds[XY(x, y)] = volumeColor;
            }
        }
        
        // Add center line
        leds[XY(x, centerY)] = CRGB::White;
    }
    
    waveOffset += 4; // Animate the wave
    FastLED.show();
    delay(50);
}

// Touch Interaction Functions

void handleTouchOnMatrix(int screenX, int screenY, CRGB color) {
    // Map screen coordinates to matrix coordinates
    int matrixX = mapScreenToMatrix(screenX, 320, MATRIX_WIDTH);
    int matrixY = mapScreenToMatrix(screenY, 240, MATRIX_HEIGHT);
    
    // Constrain to matrix bounds
    matrixX = constrain(matrixX, 0, MATRIX_WIDTH - 1);
    matrixY = constrain(matrixY, 0, MATRIX_HEIGHT - 1);
    
    lastTouchX = matrixX;
    lastTouchY = matrixY;
    lastTouchTime = millis();
    touchInteractionActive = true;
    
    DEBUG_PRINTF("[LED] Touch on matrix at (%d, %d) -> (%d, %d)\n", 
                 screenX, screenY, matrixX, matrixY);
    
    // Handle different touch effects based on current effect
    switch (currentEffect) {
        case EFFECT_TOUCH_PAINT:
            // Direct painting
            leds[XY(matrixX, matrixY)] = color;
            // Paint surrounding pixels for better visibility
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    int x = matrixX + dx;
                    int y = matrixY + dy;
                    if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT) {
                        leds[XY(x, y)] = color;
                    }
                }
            }
            FastLED.show();
            break;
            
        case EFFECT_TOUCH_RIPPLE:
            addTouchRipple(matrixX, matrixY, color);
            break;
            
        case EFFECT_TOUCH_TRAIL:
            // Add to trail effect
            leds[XY(matrixX, matrixY)] = color;
            FastLED.show();
            break;
            
        default:
            // For other effects, just add a temporary highlight
            leds[XY(matrixX, matrixY)] = CRGB::White;
            FastLED.show();
            break;
    }
}

void setTouchPaintColor(CRGB color) {
    touchPaintColor = color.r << 16 | color.g << 8 | color.b;
    DEBUG_PRINTF("[LED] Touch paint color set to RGB(%d,%d,%d)\n", color.r, color.g, color.b);
}

void addTouchRipple(int matrixX, int matrixY, CRGB color) {
    // Create expanding ripple effect from touch point
    static struct {
        int x, y;
        int radius;
        CRGB color;
        unsigned long startTime;
        bool active;
    } ripples[5]; // Support up to 5 simultaneous ripples
    
    // Find empty ripple slot
    for (int i = 0; i < 5; i++) {
        if (!ripples[i].active) {
            ripples[i].x = matrixX;
            ripples[i].y = matrixY;
            ripples[i].radius = 0;
            ripples[i].color = color;
            ripples[i].startTime = millis();
            ripples[i].active = true;
            break;
        }
    }
}

void updateTouchEffects() {
    // Update ripple effects
    static struct {
        int x, y;
        int radius;
        CRGB color;
        unsigned long startTime;
        bool active;
    } ripples[5];
    
    for (int i = 0; i < 5; i++) {
        if (ripples[i].active) {
            unsigned long elapsed = millis() - ripples[i].startTime;
            ripples[i].radius = elapsed / 100; // Expand 1 pixel per 100ms
            
            if (ripples[i].radius > 8) {
                ripples[i].active = false; // Ripple finished
            } else {
                // Draw ripple circle
                for (int x = 0; x < MATRIX_WIDTH; x++) {
                    for (int y = 0; y < MATRIX_HEIGHT; y++) {
                        int dx = x - ripples[i].x;
                        int dy = y - ripples[i].y;
                        int distance = sqrt(dx * dx + dy * dy);
                        
                        if (distance == ripples[i].radius) {
                            // Fade color based on age
                            uint8_t fade = 255 - (elapsed * 255 / 800);
                            CRGB fadeColor = ripples[i].color;
                            fadeColor.fadeToBlackBy(255 - fade);
                            leds[XY(x, y)] = fadeColor;
                        }
                    }
                }
            }
        }
    }
}

int mapScreenToMatrix(int screenCoord, int screenSize, int matrixSize) {
    return map(screenCoord, 0, screenSize, 0, matrixSize);
}

void effectTouchPaint() {
    // Persistent painting mode - LEDs stay painted until cleared
    // The actual painting is handled in handleTouchOnMatrix
    // This effect just maintains the painted state
    
    // Fade painted pixels slightly over time for a natural look
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].fadeToBlackBy(1); // Very slow fade
    }
    
    FastLED.show();
    delay(50);
}

void effectTouchRipple() {
    // Clear matrix and update ripples
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    updateTouchEffects();
    FastLED.show();
    delay(50);
}

void effectTouchTrail() {
    // Trail effect - touched pixels fade over time
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].fadeToBlackBy(10); // Moderate fade
    }
    
    // Add sparkle effect to make it more interesting
    if (random8() < 50) {
        int pos = random16(NUM_LEDS);
        leds[pos] = CHSV(random8(), 255, 100);
    }
    
    FastLED.show();
    delay(30);
}

// Global variable definitions
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

#endif // LED_EFFECTS_H
