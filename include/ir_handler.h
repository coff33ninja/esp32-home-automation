/**
 * IR Remote Control Handler Module
 * 
 * Manages IR remote control input using IRremoteESP8266 library.
 * Provides IR signal decoding, command mapping, and learning functionality.
 */

#ifndef IR_HANDLER_H
#define IR_HANDLER_H

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include "config.h"

// IR command types
enum IRCommand {
    IR_UNKNOWN = 0,
    IR_VOLUME_UP,
    IR_VOLUME_DOWN,
    IR_VOLUME_MUTE,
    IR_LIGHTS_TOGGLE,
    IR_LIGHTS_ALL_ON,
    IR_LIGHTS_ALL_OFF,
    IR_RELAY_1_TOGGLE,
    IR_RELAY_2_TOGGLE,
    IR_RELAY_3_TOGGLE,
    IR_RELAY_4_TOGGLE,
    IR_EFFECT_NEXT,
    IR_EFFECT_PREV,
    IR_EFFECT_OFF,
    IR_BRIGHTNESS_UP,
    IR_BRIGHTNESS_DOWN,
    IR_POWER_TOGGLE,
    IR_STANDBY,
    IR_SETTINGS,
    IR_CALIBRATE
};

// IR code storage structure
struct IRCodeMapping {
    uint32_t code;
    decode_type_t protocol;
    IRCommand command;
    const char* name;
    bool learned;
};

// Default IR codes for common remotes (NEC protocol)
// These can be overridden by learned codes
#define DEFAULT_IR_CODES_COUNT 20

// IR receiver constants
#define IR_BUFFER_SIZE 1024
#define IR_TIMEOUT_MS 15
#define IR_REPEAT_DELAY_MS 200
#define IR_LEARNING_TIMEOUT_MS 30000

// Learning mode states
enum IRLearningState {
    IR_LEARNING_IDLE,
    IR_LEARNING_ACTIVE,
    IR_LEARNING_SUCCESS,
    IR_LEARNING_TIMEOUT,
    IR_LEARNING_ERROR
};

// Forward declarations for system functions
extern void toggleRelay(int channel);
extern void setAllRelays(bool state);
extern void setEffect(int effect);
extern void setBrightness(uint8_t brightness);
extern void setVolumeVisualization(int volume);
extern void moveMotorToPosition(int targetPosition);
extern void wakeScreen();
extern void showCalibrationScreen();

// External references
extern int currentVolume;
extern bool lightsState;
extern int brightness;
extern bool volumeMuted;

// Global variables
extern IRrecv irReceiver;
extern decode_results irResults;
extern IRCodeMapping irCodeMap[DEFAULT_IR_CODES_COUNT];
extern IRLearningState learningState;
extern IRCommand learningCommand;
extern unsigned long lastIRTime;
extern unsigned long learningStartTime;
extern bool irEnabled;
extern bool irReceiverInitialized;

// Function prototypes
bool initIRReceiver();
void handleIRInput();
IRCommand decodeIRCommand(decode_results* results);
void executeIRCommand(IRCommand command);
void addIRCodeMapping(uint32_t code, decode_type_t protocol, IRCommand command, const char* name);
bool learnIRCode(IRCommand command, const char* commandName);
void startIRLearning(IRCommand command);
void stopIRLearning();
IRLearningState getIRLearningState();
void saveIRCodes();
void loadIRCodes();
void resetIRCodes();
const char* getIRCommandName(IRCommand command);
const char* getProtocolName(decode_type_t protocol);
void printIRCode(decode_results* results);
bool isIRCodeValid(decode_results* results);
void showIRLearningInterface();
void showIRCodeList();
bool deleteIRCode(IRCommand command);
int getLearnedCodeCount();
void showIRLearningProgress(IRCommand command, const char* commandName);

// Implementation

bool initIRReceiver() {
    DEBUG_PRINTLN("[IR] Initializing IR receiver...");
    
    // Initialize IR receiver
    irReceiver.enableIRIn();
    irReceiver.blink13(true); // Enable LED feedback on pin 13 if available
    
    // Initialize variables
    lastIRTime = 0;
    learningState = IR_LEARNING_IDLE;
    learningCommand = IR_UNKNOWN;
    irEnabled = true;
    
    // Load saved IR codes from EEPROM
    loadIRCodes();
    
    irReceiverInitialized = true;
    DEBUG_PRINTF("[IR] IR receiver initialized on pin %d\n", IR_RECV_PIN);
    DEBUG_PRINTF("[IR] Buffer size: %d, Timeout: %dms\n", IR_BUFFER_SIZE, IR_TIMEOUT_MS);
    
    return true;
}

void handleIRInput() {
    if (!irEnabled) {
        return;
    }
    
    // Check for IR input
    if (irReceiver.decode(&irResults)) {
        unsigned long currentTime = millis();
        
        // Debounce IR input to prevent rapid repeats
        if (currentTime - lastIRTime < IR_REPEAT_DELAY_MS) {
            irReceiver.resume(); // Prepare for next IR signal
            return;
        }
        
        lastIRTime = currentTime;
        
        // Print received IR code for debugging
        printIRCode(&irResults);
        
        // Handle learning mode
        if (learningState == IR_LEARNING_ACTIVE) {
            if (isIRCodeValid(&irResults)) {
                // Store the learned code
                addIRCodeMapping(irResults.value, irResults.decode_type, learningCommand, 
                               getIRCommandName(learningCommand));
                learningState = IR_LEARNING_SUCCESS;
                DEBUG_PRINTF("[IR] Successfully learned code for command: %s\n", 
                           getIRCommandName(learningCommand));
                saveIRCodes();
            } else {
                DEBUG_PRINTLN("[IR] Invalid code received during learning");
            }
            irReceiver.resume();
            return;
        }
        
        // Normal operation - decode and execute command
        if (isIRCodeValid(&irResults)) {
            IRCommand command = decodeIRCommand(&irResults);
            if (command != IR_UNKNOWN) {
                DEBUG_PRINTF("[IR] Executing command: %s\n", getIRCommandName(command));
                executeIRCommand(command);
                
                // Wake screen on any IR command
                wakeScreen();
            } else {
                DEBUG_PRINTF("[IR] Unknown IR code: 0x%08X (%s)\n", 
                           irResults.value, getProtocolName(irResults.decode_type));
            }
        }
        
        irReceiver.resume(); // Prepare for next IR signal
    }
    
    // Handle learning timeout
    if (learningState == IR_LEARNING_ACTIVE && 
        millis() - learningStartTime > IR_LEARNING_TIMEOUT_MS) {
        learningState = IR_LEARNING_TIMEOUT;
        DEBUG_PRINTLN("[IR] Learning mode timeout");
    }
}

IRCommand decodeIRCommand(decode_results* results) {
    // Search through the code mapping table
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; i++) {
        if (irCodeMap[i].code == results->value && 
            irCodeMap[i].protocol == results->decode_type) {
            return irCodeMap[i].command;
        }
    }
    
    return IR_UNKNOWN;
}

void executeIRCommand(IRCommand command) {
    switch (command) {
        case IR_VOLUME_UP:
            if (currentVolume < 100) {
                currentVolume = min(100, currentVolume + 5);
                int targetPosition = map(currentVolume, 0, 100, POT_MIN_VALUE, POT_MAX_VALUE);
                moveMotorToPosition(targetPosition);
                setVolumeVisualization(currentVolume);
                DEBUG_PRINTF("[IR] Volume up: %d%%\n", currentVolume);
                
                // Publish to MQTT
                extern PubSubClient mqttClient;
                if (mqttClient.connected()) {
                    char msg[10];
                    snprintf(msg, sizeof(msg), "%d", currentVolume);
                    mqttClient.publish(MQTT_TOPIC_VOLUME, msg);
                }
            }
            break;
            
        case IR_VOLUME_DOWN:
            if (currentVolume > 0) {
                currentVolume = max(0, currentVolume - 5);
                int targetPosition = map(currentVolume, 0, 100, POT_MIN_VALUE, POT_MAX_VALUE);
                moveMotorToPosition(targetPosition);
                setVolumeVisualization(currentVolume);
                DEBUG_PRINTF("[IR] Volume down: %d%%\n", currentVolume);
                
                // Publish to MQTT
                extern PubSubClient mqttClient;
                if (mqttClient.connected()) {
                    char msg[10];
                    snprintf(msg, sizeof(msg), "%d", currentVolume);
                    mqttClient.publish(MQTT_TOPIC_VOLUME, msg);
                }
            }
            break;
            
        case IR_VOLUME_MUTE:
            volumeMuted = !volumeMuted;
            if (volumeMuted) {
                setVolumeVisualization(0);
                DEBUG_PRINTLN("[IR] Volume muted");
            } else {
                setVolumeVisualization(currentVolume);
                DEBUG_PRINTLN("[IR] Volume unmuted");
            }
            break;
            
        case IR_LIGHTS_TOGGLE:
            lightsState = !lightsState;
            setAllRelays(lightsState);
            DEBUG_PRINTF("[IR] All lights: %s\n", lightsState ? "ON" : "OFF");
            
            // Publish to MQTT
            extern PubSubClient mqttClient;
            if (mqttClient.connected()) {
                mqttClient.publish(MQTT_TOPIC_LIGHTS, lightsState ? "ON" : "OFF");
            }
            break;
            
        case IR_LIGHTS_ALL_ON:
            lightsState = true;
            setAllRelays(true);
            DEBUG_PRINTLN("[IR] All lights ON");
            break;
            
        case IR_LIGHTS_ALL_OFF:
            lightsState = false;
            setAllRelays(false);
            DEBUG_PRINTLN("[IR] All lights OFF");
            break;
            
        case IR_RELAY_1_TOGGLE:
            toggleRelay(1);
            DEBUG_PRINTLN("[IR] Relay 1 toggled");
            break;
            
        case IR_RELAY_2_TOGGLE:
            toggleRelay(2);
            DEBUG_PRINTLN("[IR] Relay 2 toggled");
            break;
            
        case IR_RELAY_3_TOGGLE:
            toggleRelay(3);
            DEBUG_PRINTLN("[IR] Relay 3 toggled");
            break;
            
        case IR_RELAY_4_TOGGLE:
            toggleRelay(4);
            DEBUG_PRINTLN("[IR] Relay 4 toggled");
            break;
            
        case IR_EFFECT_NEXT:
            {
                extern int currentEffect;
                currentEffect = (currentEffect + 1) % 8; // Cycle through 8 effects
                setEffect(currentEffect);
                DEBUG_PRINTF("[IR] Next effect: %d\n", currentEffect);
                
                // Publish to MQTT
                extern PubSubClient mqttClient;
                if (mqttClient.connected()) {
                    char msg[10];
                    snprintf(msg, sizeof(msg), "%d", currentEffect);
                    mqttClient.publish(MQTT_TOPIC_EFFECTS, msg);
                }
            }
            break;
            
        case IR_EFFECT_PREV:
            {
                extern int currentEffect;
                currentEffect = (currentEffect - 1 + 8) % 8; // Cycle backwards
                setEffect(currentEffect);
                DEBUG_PRINTF("[IR] Previous effect: %d\n", currentEffect);
            }
            break;
            
        case IR_EFFECT_OFF:
            setEffect(0); // Effect 0 is typically "off"
            DEBUG_PRINTLN("[IR] Effects OFF");
            break;
            
        case IR_BRIGHTNESS_UP:
            if (brightness < 255) {
                brightness = min(255, brightness + 25);
                setBrightness(brightness);
                DEBUG_PRINTF("[IR] Brightness up: %d\n", brightness);
            }
            break;
            
        case IR_BRIGHTNESS_DOWN:
            if (brightness > 0) {
                brightness = max(0, brightness - 25);
                setBrightness(brightness);
                DEBUG_PRINTF("[IR] Brightness down: %d\n", brightness);
            }
            break;
            
        case IR_POWER_TOGGLE:
            // Toggle between active and standby modes
            {
                static bool standbyMode = false;
                standbyMode = !standbyMode;
                if (standbyMode) {
                    // Enter standby: turn off lights and effects
                    setAllRelays(false);
                    setEffect(0);
                    setBrightness(0);
                    DEBUG_PRINTLN("[IR] Entering standby mode");
                } else {
                    // Exit standby: restore previous state
                    setBrightness(128); // Default brightness
                    DEBUG_PRINTLN("[IR] Exiting standby mode");
                }
            }
            break;
            
        case IR_STANDBY:
            // Force standby mode
            setAllRelays(false);
            setEffect(0);
            setBrightness(0);
            DEBUG_PRINTLN("[IR] Standby mode activated");
            break;
            
        case IR_SETTINGS:
            // Open settings/calibration screen
            showCalibrationScreen();
            DEBUG_PRINTLN("[IR] Settings screen opened");
            break;
            
        case IR_CALIBRATE:
            // Start calibration mode
            showCalibrationScreen();
            DEBUG_PRINTLN("[IR] Calibration started");
            break;
            
        default:
            DEBUG_PRINTF("[IR] Unknown command: %d\n", command);
            break;
    }
}

void addIRCodeMapping(uint32_t code, decode_type_t protocol, IRCommand command, const char* name) {
    // Check for conflicts - same code mapped to different command
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; i++) {
        if (irCodeMap[i].code == code && irCodeMap[i].protocol == protocol && 
            irCodeMap[i].command != command && irCodeMap[i].command != IR_UNKNOWN) {
            DEBUG_PRINTF("[IR] Conflict detected: Code 0x%08X already mapped to %s\n", 
                       code, getIRCommandName(irCodeMap[i].command));
            DEBUG_PRINTF("[IR] Replacing with new mapping: %s\n", name);
            
            // Replace the conflicting mapping
            irCodeMap[i].command = command;
            strncpy((char*)irCodeMap[i].name, name, 20);
            irCodeMap[i].learned = true;
            return;
        }
    }
    
    // Find empty slot or replace existing mapping for same command
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; i++) {
        if (irCodeMap[i].command == command || irCodeMap[i].command == IR_UNKNOWN) {
            irCodeMap[i].code = code;
            irCodeMap[i].protocol = protocol;
            irCodeMap[i].command = command;
            strncpy((char*)irCodeMap[i].name, name, 20);
            irCodeMap[i].learned = true;
            DEBUG_PRINTF("[IR] Added mapping: %s -> 0x%08X (%s)\n", 
                       name, code, getProtocolName(protocol));
            return;
        }
    }
    
    DEBUG_PRINTLN("[IR] Warning: IR code mapping table full");
}

bool learnIRCode(IRCommand command, const char* commandName) {
    if (learningState == IR_LEARNING_ACTIVE) {
        DEBUG_PRINTLN("[IR] Learning already in progress");
        return false;
    }
    
    startIRLearning(command);
    DEBUG_PRINTF("[IR] Learning mode started for: %s\n", commandName);
    DEBUG_PRINTLN("[IR] Press the desired button on your remote...");
    
    return true;
}

void startIRLearning(IRCommand command) {
    learningState = IR_LEARNING_ACTIVE;
    learningCommand = command;
    learningStartTime = millis();
}

void stopIRLearning() {
    learningState = IR_LEARNING_IDLE;
    learningCommand = IR_UNKNOWN;
}

IRLearningState getIRLearningState() {
    return learningState;
}

void saveIRCodes() {
    // Save IR codes to EEPROM
    DEBUG_PRINTLN("[IR] Saving IR codes to EEPROM");
    
    #include <EEPROM.h>
    
    // EEPROM layout: 
    // Address 0-3: Magic number (0xDEADBEEF) to verify valid data
    // Address 4-7: Version number
    // Address 8+: IR code mappings
    
    const uint32_t MAGIC_NUMBER = 0xDEADBEEF;
    const uint32_t VERSION = 1;
    const int EEPROM_START_ADDR = 100; // Start after other system data
    
    EEPROM.begin(512); // Initialize EEPROM with 512 bytes
    
    int addr = EEPROM_START_ADDR;
    
    // Write magic number
    EEPROM.put(addr, MAGIC_NUMBER);
    addr += sizeof(MAGIC_NUMBER);
    
    // Write version
    EEPROM.put(addr, VERSION);
    addr += sizeof(VERSION);
    
    // Write IR code mappings
    int learnedCount = 0;
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; i++) {
        if (irCodeMap[i].learned && irCodeMap[i].command != IR_UNKNOWN) {
            // Write the mapping structure
            EEPROM.put(addr, irCodeMap[i]);
            addr += sizeof(IRCodeMapping);
            learnedCount++;
        }
    }
    
    // Write count of learned codes
    EEPROM.put(EEPROM_START_ADDR + 8, learnedCount);
    
    EEPROM.commit(); // Save changes to flash
    EEPROM.end();
    
    DEBUG_PRINTF("[IR] Saved %d learned IR codes to EEPROM\n", learnedCount);
}

void loadIRCodes() {
    DEBUG_PRINTLN("[IR] Loading IR codes from EEPROM");
    
    #include <EEPROM.h>
    
    // Clear the mapping table first
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; i++) {
        irCodeMap[i].code = 0;
        irCodeMap[i].protocol = UNKNOWN;
        irCodeMap[i].command = IR_UNKNOWN;
        irCodeMap[i].name = "";
        irCodeMap[i].learned = false;
    }
    
    const uint32_t MAGIC_NUMBER = 0xDEADBEEF;
    const uint32_t VERSION = 1;
    const int EEPROM_START_ADDR = 100;
    
    EEPROM.begin(512);
    
    // Check if valid data exists in EEPROM
    uint32_t magic;
    EEPROM.get(EEPROM_START_ADDR, magic);
    
    if (magic == MAGIC_NUMBER) {
        // Valid data found, load learned codes
        uint32_t version;
        EEPROM.get(EEPROM_START_ADDR + 4, version);
        
        if (version == VERSION) {
            int learnedCount;
            EEPROM.get(EEPROM_START_ADDR + 8, learnedCount);
            
            int addr = EEPROM_START_ADDR + 12;
            int loadedCount = 0;
            
            for (int i = 0; i < learnedCount && i < DEFAULT_IR_CODES_COUNT; i++) {
                IRCodeMapping mapping;
                EEPROM.get(addr, mapping);
                
                if (mapping.command != IR_UNKNOWN) {
                    irCodeMap[loadedCount] = mapping;
                    loadedCount++;
                }
                
                addr += sizeof(IRCodeMapping);
            }
            
            DEBUG_PRINTF("[IR] Loaded %d learned IR codes from EEPROM\n", loadedCount);
        } else {
            DEBUG_PRINTF("[IR] EEPROM version mismatch: %d (expected %d)\n", version, VERSION);
        }
    } else {
        DEBUG_PRINTLN("[IR] No valid IR codes found in EEPROM, using defaults");
    }
    
    EEPROM.end();
    
    // Add default mappings for any unmapped commands
    // These serve as fallbacks if no learned codes exist
    if (irCodeMap[0].command == IR_UNKNOWN) {
        // No learned codes, use defaults
        addIRCodeMapping(0xFF18E7, NEC, IR_VOLUME_UP, "Volume Up");
        addIRCodeMapping(0xFF4AB5, NEC, IR_VOLUME_DOWN, "Volume Down");
        addIRCodeMapping(0xFF38C7, NEC, IR_VOLUME_MUTE, "Mute");
        addIRCodeMapping(0xFF5AA5, NEC, IR_LIGHTS_TOGGLE, "Lights Toggle");
        addIRCodeMapping(0xFF42BD, NEC, IR_EFFECT_NEXT, "Next Effect");
        addIRCodeMapping(0xFF52AD, NEC, IR_BRIGHTNESS_UP, "Brightness Up");
        addIRCodeMapping(0xFF7A85, NEC, IR_BRIGHTNESS_DOWN, "Brightness Down");
        addIRCodeMapping(0xFF02FD, NEC, IR_POWER_TOGGLE, "Power");
        addIRCodeMapping(0xFF22DD, NEC, IR_RELAY_1_TOGGLE, "Relay 1");
        addIRCodeMapping(0xFF12ED, NEC, IR_RELAY_2_TOGGLE, "Relay 2");
        
        DEBUG_PRINTLN("[IR] Default IR codes loaded");
    }
}

void resetIRCodes() {
    DEBUG_PRINTLN("[IR] Resetting IR codes to defaults");
    
    // Clear learned codes and reload defaults
    loadIRCodes();
    saveIRCodes();
}

const char* getIRCommandName(IRCommand command) {
    switch (command) {
        case IR_VOLUME_UP: return "Volume Up";
        case IR_VOLUME_DOWN: return "Volume Down";
        case IR_VOLUME_MUTE: return "Mute";
        case IR_LIGHTS_TOGGLE: return "Lights Toggle";
        case IR_LIGHTS_ALL_ON: return "All Lights On";
        case IR_LIGHTS_ALL_OFF: return "All Lights Off";
        case IR_RELAY_1_TOGGLE: return "Relay 1";
        case IR_RELAY_2_TOGGLE: return "Relay 2";
        case IR_RELAY_3_TOGGLE: return "Relay 3";
        case IR_RELAY_4_TOGGLE: return "Relay 4";
        case IR_EFFECT_NEXT: return "Next Effect";
        case IR_EFFECT_PREV: return "Previous Effect";
        case IR_EFFECT_OFF: return "Effects Off";
        case IR_BRIGHTNESS_UP: return "Brightness Up";
        case IR_BRIGHTNESS_DOWN: return "Brightness Down";
        case IR_POWER_TOGGLE: return "Power Toggle";
        case IR_STANDBY: return "Standby";
        case IR_SETTINGS: return "Settings";
        case IR_CALIBRATE: return "Calibrate";
        default: return "Unknown";
    }
}

const char* getProtocolName(decode_type_t protocol) {
    switch (protocol) {
        case NEC: return "NEC";
        case SONY: return "SONY";
        case RC5: return "RC5";
        case RC6: return "RC6";
        case SAMSUNG: return "SAMSUNG";
        case LG: return "LG";
        case PANASONIC: return "PANASONIC";
        case JVC: return "JVC";
        default: return "UNKNOWN";
    }
}

void printIRCode(decode_results* results) {
    DEBUG_PRINTF("[IR] Received: 0x%08X (%d bits) Protocol: %s", 
                results->value, results->bits, getProtocolName(results->decode_type));
    
    if (results->overflow) {
        DEBUG_PRINT(" [OVERFLOW]");
    }
    
    if (results->repeat) {
        DEBUG_PRINT(" [REPEAT]");
    }
    
    DEBUG_PRINTLN();
    
    // Print raw timing data for debugging
    if (DEBUG_ENABLED && results->rawlen > 0) {
        DEBUG_PRINT("[IR] Raw data: ");
        for (uint16_t i = 1; i < results->rawlen; i++) {
            DEBUG_PRINTF("%d ", results->rawbuf[i] * RAWTICK);
            if (i % 10 == 0) DEBUG_PRINTLN();
        }
        DEBUG_PRINTLN();
    }
}

bool isIRCodeValid(decode_results* results) {
    // Check if the received IR code is valid
    if (results->value == 0 || results->value == 0xFFFFFFFF) {
        return false; // Invalid or repeat code
    }
    
    if (results->bits < 8 || results->bits > 64) {
        return false; // Invalid bit count
    }
    
    if (results->overflow) {
        return false; // Buffer overflow
    }
    
    return true;
}

void showIRLearningInterface() {
    // This function would be called from the touch screen interface
    // to show the IR learning menu
    DEBUG_PRINTLN("[IR] Showing IR learning interface");
    
    // The actual implementation would be in the touch handler
    // This is a placeholder for the interface integration
    extern TFT_eSPI tft;
    
    tft.fillScreen(0x0000); // Black background
    tft.setTextColor(0xFFFF); // White text
    tft.setTextSize(2);
    tft.drawString("IR Learning Mode", 50, 20);
    
    tft.setTextSize(1);
    tft.drawString("Select command to learn:", 20, 60);
    
    // Draw command buttons (simplified)
    const char* commands[] = {
        "Volume Up", "Volume Down", "Mute",
        "Lights Toggle", "Next Effect", "Power"
    };
    
    for (int i = 0; i < 6; i++) {
        int x = 20 + (i % 2) * 140;
        int y = 90 + (i / 2) * 40;
        tft.drawRect(x, y, 120, 30, 0xFFFF);
        tft.drawString(commands[i], x + 5, y + 10);
    }
    
    tft.drawString("Touch command to learn", 50, 220);
}

void showIRCodeList() {
    // Show list of learned IR codes
    DEBUG_PRINTLN("[IR] Showing learned IR codes");
    
    extern TFT_eSPI tft;
    
    tft.fillScreen(0x0000);
    tft.setTextColor(0xFFFF);
    tft.setTextSize(2);
    tft.drawString("Learned IR Codes", 30, 10);
    
    tft.setTextSize(1);
    int y = 40;
    int count = 0;
    
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT && count < 10; i++) {
        if (irCodeMap[i].learned && irCodeMap[i].command != IR_UNKNOWN) {
            char line[50];
            snprintf(line, sizeof(line), "%s: 0x%08X", 
                   irCodeMap[i].name, irCodeMap[i].code);
            tft.drawString(line, 10, y);
            y += 15;
            count++;
        }
    }
    
    if (count == 0) {
        tft.drawString("No learned codes", 10, 40);
    }
    
    tft.drawString("Press back to return", 10, 220);
}

bool deleteIRCode(IRCommand command) {
    // Delete a learned IR code
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; i++) {
        if (irCodeMap[i].command == command && irCodeMap[i].learned) {
            irCodeMap[i].code = 0;
            irCodeMap[i].protocol = UNKNOWN;
            irCodeMap[i].command = IR_UNKNOWN;
            irCodeMap[i].name = "";
            irCodeMap[i].learned = false;
            
            DEBUG_PRINTF("[IR] Deleted learned code for command: %s\n", 
                       getIRCommandName(command));
            
            saveIRCodes(); // Save changes to EEPROM
            return true;
        }
    }
    
    return false;
}

int getLearnedCodeCount() {
    int count = 0;
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; i++) {
        if (irCodeMap[i].learned && irCodeMap[i].command != IR_UNKNOWN) {
            count++;
        }
    }
    return count;
}

void showIRLearningProgress(IRCommand command, const char* commandName) {
    // Show learning progress on screen
    extern TFT_eSPI tft;
    
    tft.fillScreen(0x0000);
    tft.setTextColor(0xFFFF);
    tft.setTextSize(2);
    tft.drawString("Learning Mode", 60, 50);
    
    tft.setTextSize(1);
    char msg[50];
    snprintf(msg, sizeof(msg), "Learning: %s", commandName);
    tft.drawString(msg, 50, 100);
    
    tft.drawString("Point remote at device", 30, 130);
    tft.drawString("Press the button now...", 40, 150);
    
    // Show timeout countdown
    unsigned long remaining = (IR_LEARNING_TIMEOUT_MS - (millis() - learningStartTime)) / 1000;
    snprintf(msg, sizeof(msg), "Timeout: %lu seconds", remaining);
    tft.drawString(msg, 60, 180);
    
    // Show cancel instruction
    tft.setTextColor(0xF800); // Red
    tft.drawString("Touch screen to cancel", 30, 210);
}

// Global variable definitions
IRrecv irReceiver(IR_RECV_PIN, IR_BUFFER_SIZE, IR_TIMEOUT_MS, true);
decode_results irResults;
IRCodeMapping irCodeMap[DEFAULT_IR_CODES_COUNT];
IRLearningState learningState = IR_LEARNING_IDLE;
IRCommand learningCommand = IR_UNKNOWN;
unsigned long lastIRTime = 0;
unsigned long learningStartTime = 0;
bool irEnabled = true;
bool irReceiverInitialized = false;

#endif // IR_HANDLER_H