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
// Additional external dependencies used by IR handler implementations
#include <PubSubClient.h>
#include "led_effects.h"
#include <EEPROM.h>
#include <TFT_eSPI.h>

// Extern declarations for shared objects
extern PubSubClient mqttClient;
extern TFT_eSPI tft;

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

// Note: implementation and globals moved to src/ir_handler.cpp

#endif // IR_HANDLER_H