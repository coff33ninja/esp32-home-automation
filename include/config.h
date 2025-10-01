/**
 * ESP32 Home Automation - Configuration File
 * 
 * This file contains all configuration settings, credentials, and pin definitions.
 * 
 * IMPORTANT: Add this file to .gitignore to keep credentials secure!
 */

#ifndef CONFIG_H
#define CONFIG_H

// ========================================
// WIFI CONFIGURATION
// ========================================
#define WIFI_SSID "YourSSID"          // Change this
#define WIFI_PASSWORD "YourPassword"  // Change this
#define WIFI_TIMEOUT 20000            // Connection timeout (ms)

// ========================================
// MQTT CONFIGURATION
// ========================================
#define MQTT_SERVER "192.168.1.xxx"   // Change this
#define MQTT_PORT 1883
#define MQTT_USER ""                  // Leave empty if not required
#define MQTT_PASSWORD ""              // Leave empty if not required
#define MQTT_CLIENT_ID "ESP32_HomeControl"

// MQTT Topics
#define MQTT_TOPIC_VOLUME "homecontrol/volume"
#define MQTT_TOPIC_LIGHTS "homecontrol/lights"
#define MQTT_TOPIC_EFFECTS "homecontrol/effects"
#define MQTT_TOPIC_STATUS "homecontrol/status"
#define MQTT_TOPIC_COMMAND "homecontrol/command"
#define MQTT_TOPIC_OTA_STATUS "homecontrol/ota/status"
#define MQTT_TOPIC_OTA_COMMAND "homecontrol/ota/command"
#define MQTT_TOPIC_OTA_PROGRESS "homecontrol/ota/progress"

// ========================================
// GPIO PIN DEFINITIONS
// ========================================

// Motor Control (L298N H-Bridge)
#define MOTOR_PIN_A 25      // Motor direction A
#define MOTOR_PIN_B 26      // Motor direction B
#define MOTOR_PWM_PIN 27    // Motor PWM speed control
#define POT_ADC_PIN 34      // Potentiometer position feedback (ADC)

// Relay Control
#define RELAY_1_PIN 32      // Main light relay
#define RELAY_2_PIN 33      // Accent light relay
#define RELAY_3_PIN 14      // Reserved
#define RELAY_4_PIN 12      // Reserved

// LED Control
#define LED_STRIP_PIN 13    // 12V LED strip PWM control
#define LED_MATRIX_PIN 21   // WS2812B data pin
#define NUM_LEDS 256        // 16x16 matrix

// IR Control
#define IR_RECV_PIN 35      // IR receiver data pin
#define IR_SEND_PIN 22      // IR transmitter (optional)

// Touch Screen (SPI)
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST 4
#define TOUCH_CS 5
#define TOUCH_IRQ 36
#define TFT_BACKLIGHT_PIN 16    // PWM pin for backlight control (optional)

// ========================================
// HARDWARE PARAMETERS
// ========================================

// Motor Control
#define MOTOR_PWM_FREQUENCY 1000    // Hz
#define MOTOR_PWM_RESOLUTION 8      // 8-bit (0-255)
#define MOTOR_PWM_CHANNEL 0
#define POT_MIN_VALUE 0             // ADC minimum
#define POT_MAX_VALUE 4095          // ADC maximum (12-bit)
#define POT_DEADBAND 20             // Prevent jitter

// LED Strip
#define LED_STRIP_PWM_FREQUENCY 5000
#define LED_STRIP_PWM_RESOLUTION 8
#define LED_STRIP_PWM_CHANNEL 1

// LED Matrix
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define MATRIX_TYPE WS2812B
#define COLOR_ORDER GRB
#define MAX_BRIGHTNESS 128          // Reduce to prevent power issues
#define FRAMES_PER_SECOND 60

// ========================================
// TIMING PARAMETERS
// ========================================
#define LOOP_DELAY 10               // Main loop delay (ms)
#define MQTT_RECONNECT_DELAY 5000   // MQTT reconnect delay (ms)
#define POT_READ_INTERVAL 50        // Potentiometer read interval (ms)
#define WATCHDOG_TIMEOUT 30000      // Watchdog timeout (ms)
#define HEALTH_CHECK_INTERVAL 30000 // Health check interval (ms)

// ========================================
// OTA UPDATE CONFIGURATION
// ========================================
#define OTA_UPDATE_URL "https://your-server.com/firmware/"
#define OTA_VERSION_CHECK_URL "https://your-server.com/version.json"
#define OTA_BUFFER_SIZE 1024
#define OTA_TIMEOUT 30000
#define OTA_MAX_RETRIES 3
#define OTA_CHECK_INTERVAL 3600000  // Check for updates every hour

// ========================================
// FAIL-SAFE SETTINGS
// ========================================
#define FAILSAFE_VOLUME 0           // Mute on boot
#define FAILSAFE_LIGHTS_STATE false // Lights off on boot
#define FAILSAFE_BRIGHTNESS 0       // LEDs off on boot

// ========================================
// SERIAL DEBUG
// ========================================
#define SERIAL_BAUD_RATE 115200
#define DEBUG_ENABLED true          // Set to false to disable debug output

// Debug macro
#if DEBUG_ENABLED
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(x, ...) Serial.printf(x, __VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, ...)
#endif

// ========================================
// VERSION INFO
// ========================================
#define FIRMWARE_VERSION "1.0.0"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

#endif // CONFIG_H
