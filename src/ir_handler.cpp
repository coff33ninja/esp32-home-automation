#include "ir_handler.h"
#include "led_effects.h"
#include "config.h"
#include <PubSubClient.h>

// File-scope externs to avoid declaring objects with non-trivial destructors
// inside switch-case scopes in executeIRCommand.
extern PubSubClient mqttClient;
extern int currentVolume;
extern bool lightsState;
extern int brightness;
extern bool volumeMuted;

// Single-definition IR globals (moved from header)
IRrecv irReceiver(IR_RECV_PIN);
decode_results irResults;
IRCodeMapping irCodeMap[DEFAULT_IR_CODES_COUNT];
IRLearningState learningState = IR_LEARNING_IDLE;
IRCommand learningCommand = IR_UNKNOWN;
unsigned long lastIRTime = 0;
unsigned long learningStartTime = 0;
bool irEnabled = true;
bool irReceiverInitialized = false;

// Minimal IR helper implementations to satisfy external callers.
bool initIRReceiver() {
    if (irReceiverInitialized) return true;
    // Begin receiver
    irReceiver.enableIRIn();
    irReceiverInitialized = true;
    DEBUG_PRINTLN("[IR] IR receiver initialized");
    return true;
}

void startIRLearning(IRCommand command) {
    learningState = IR_LEARNING_ACTIVE;
    learningCommand = command;
    learningStartTime = millis();
    DEBUG_PRINTF("[IR] Start learning for command %d\n", command);
}

void stopIRLearning() {
    learningState = IR_LEARNING_IDLE;
    learningCommand = IR_UNKNOWN;
    DEBUG_PRINTLN("[IR] Stop learning");
}

IRCommand decodeIRCommand(decode_results* results) {
    if (!results) return IR_UNKNOWN;
    uint32_t value = results->value;
    decode_type_t proto = results->decode_type;
    // Linear search through map
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; ++i) {
        if (irCodeMap[i].learned && irCodeMap[i].code == value && irCodeMap[i].protocol == proto) {
            return irCodeMap[i].command;
        }
    }
    return IR_UNKNOWN;
}

void addIRCodeMapping(uint32_t code, decode_type_t protocol, IRCommand command, const char* name) {
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; ++i) {
        if (!irCodeMap[i].learned) {
            irCodeMap[i].code = code;
            irCodeMap[i].protocol = protocol;
            irCodeMap[i].command = command;
            irCodeMap[i].name = name;
            irCodeMap[i].learned = true;
            DEBUG_PRINTF("[IR] Added mapping %s = 0x%08X\n", name, code);
            return;
        }
    }
    DEBUG_PRINTLN("[IR] IR code map full, cannot add mapping");
}

bool learnIRCode(IRCommand command, const char* commandName) {
    // Simple blocking learn for now: wait up to IR_LEARNING_TIMEOUT_MS for a code
    unsigned long start = millis();
    startIRLearning(command);
    while (millis() - start < IR_LEARNING_TIMEOUT_MS) {
        if (irReceiver.decode(&irResults)) {
            addIRCodeMapping(irResults.value, irResults.decode_type, command, commandName);
            irReceiver.resume();
            learningState = IR_LEARNING_SUCCESS;
            stopIRLearning();
            return true;
        }
        delay(10);
    }
    learningState = IR_LEARNING_TIMEOUT;
    stopIRLearning();
    return false;
}

IRLearningState getIRLearningState() {
    return learningState;
}

void saveIRCodes() {
    // Minimal stub: write learned flags and codes to EEPROM sequentially
    // This is a best-effort placeholder; real implementation should use config manager
    int addr = 0;
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; ++i) {
        EEPROM.put(addr, irCodeMap[i]);
        addr += sizeof(IRCodeMapping);
    }
    EEPROM.commit();
    DEBUG_PRINTLN("[IR] IR codes saved (stub)");
}

void loadIRCodes() {
    int addr = 0;
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; ++i) {
        EEPROM.get(addr, irCodeMap[i]);
        addr += sizeof(IRCodeMapping);
    }
    DEBUG_PRINTLN("[IR] IR codes loaded (stub)");
}

void resetIRCodes() {
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; ++i) {
        irCodeMap[i].learned = false;
        irCodeMap[i].code = 0;
        irCodeMap[i].protocol = decode_type_t::UNKNOWN;
        irCodeMap[i].command = IR_UNKNOWN;
        irCodeMap[i].name = "";
    }
    DEBUG_PRINTLN("[IR] IR codes reset");
}

int getLearnedCodeCount() {
    int c = 0;
    for (int i = 0; i < DEFAULT_IR_CODES_COUNT; ++i) if (irCodeMap[i].learned) ++c;
    return c;
}

void showIRLearningInterface() { /* UI stub - left intentionally minimal */ }
void showIRCodeList() { /* UI stub */ }
bool deleteIRCode(IRCommand command) { return false; }
void showIRLearningProgress(IRCommand command, const char* commandName) { /* UI stub */ }

void handleIRInput() {
    if (!irReceiverInitialized) return;
    if (irReceiver.decode(&irResults)) {
        lastIRTime = millis();
        IRCommand cmd = decodeIRCommand(&irResults);
        if (cmd != IR_UNKNOWN) {
            executeIRCommand(cmd);
        } else if (learningState == IR_LEARNING_ACTIVE && learningCommand != IR_UNKNOWN) {
            // In learning mode, capture code
            addIRCodeMapping(irResults.value, irResults.decode_type, learningCommand, "learned");
            learningState = IR_LEARNING_SUCCESS;
            stopIRLearning();
        }
        irReceiver.resume();
    }
}


void executeIRCommand(IRCommand command) {
    switch (command) {
        case IR_VOLUME_UP: {
            if (currentVolume < 100) {
                currentVolume = min(100, currentVolume + 5);
                int targetPosition = map(currentVolume, 0, 100, POT_MIN_VALUE, POT_MAX_VALUE);
                moveMotorToPosition(targetPosition);
                setVolumeVisualization(currentVolume);
                DEBUG_PRINTF("[IR] Volume up: %d%%\n", currentVolume);

                // Publish to MQTT
                if (mqttClient.connected()) {
                    char msg[10];
                    snprintf(msg, sizeof(msg), "%d", currentVolume);
                    mqttClient.publish(MQTT_TOPIC_VOLUME, msg);
                }
            }
        } break;

        case IR_VOLUME_DOWN: {
            if (currentVolume > 0) {
                currentVolume = max(0, currentVolume - 5);
                int targetPosition = map(currentVolume, 0, 100, POT_MIN_VALUE, POT_MAX_VALUE);
                moveMotorToPosition(targetPosition);
                setVolumeVisualization(currentVolume);
                DEBUG_PRINTF("[IR] Volume down: %d%%\n", currentVolume);

                // Publish to MQTT
                if (mqttClient.connected()) {
                    char msg[10];
                    snprintf(msg, sizeof(msg), "%d", currentVolume);
                    mqttClient.publish(MQTT_TOPIC_VOLUME, msg);
                }
            }
        } break;

        case IR_VOLUME_MUTE: {
            volumeMuted = !volumeMuted;
            if (volumeMuted) {
                setVolumeVisualization(0);
                DEBUG_PRINTLN("[IR] Volume muted");
            } else {
                setVolumeVisualization(currentVolume);
                DEBUG_PRINTLN("[IR] Volume unmuted");
            }
        } break;

        case IR_LIGHTS_TOGGLE: {
            lightsState = !lightsState;
            setAllRelays(lightsState);
            DEBUG_PRINTF("[IR] All lights: %s\n", lightsState ? "ON" : "OFF");

            // Publish to MQTT
            if (mqttClient.connected()) {
                mqttClient.publish(MQTT_TOPIC_LIGHTS, lightsState ? "ON" : "OFF");
            }
        } break;

        case IR_LIGHTS_ALL_ON: {
            lightsState = true;
            setAllRelays(true);
            DEBUG_PRINTLN("[IR] All lights ON");
        } break;

        case IR_LIGHTS_ALL_OFF: {
            lightsState = false;
            setAllRelays(false);
            DEBUG_PRINTLN("[IR] All lights OFF");
        } break;

        case IR_RELAY_1_TOGGLE: {
            toggleRelay(1);
            DEBUG_PRINTLN("[IR] Relay 1 toggled");
        } break;

        case IR_RELAY_2_TOGGLE: {
            toggleRelay(2);
            DEBUG_PRINTLN("[IR] Relay 2 toggled");
        } break;

        case IR_RELAY_3_TOGGLE: {
            toggleRelay(3);
            DEBUG_PRINTLN("[IR] Relay 3 toggled");
        } break;

        case IR_RELAY_4_TOGGLE: {
            toggleRelay(4);
            DEBUG_PRINTLN("[IR] Relay 4 toggled");
        } break;

        case IR_EFFECT_NEXT: {
            // Use LEDEffect type from led_effects.h
            extern LEDEffect currentEffect;
            currentEffect = (LEDEffect)((currentEffect + 1) % 8); // Cycle through 8 effects
            setEffect(currentEffect);
            DEBUG_PRINTF("[IR] Next effect: %d\n", currentEffect);

            // Publish to MQTT
            if (mqttClient.connected()) {
                char msg[10];
                snprintf(msg, sizeof(msg), "%d", (int)currentEffect);
                mqttClient.publish(MQTT_TOPIC_EFFECTS, msg);
            }
        } break;

        case IR_EFFECT_PREV: {
            extern LEDEffect currentEffect;
            currentEffect = (LEDEffect)((currentEffect - 1 + 8) % 8); // Cycle backwards
            setEffect(currentEffect);
            DEBUG_PRINTF("[IR] Previous effect: %d\n", currentEffect);
        } break;

        case IR_EFFECT_OFF: {
            setEffect(0); // Effect 0 is typically "off"
            DEBUG_PRINTLN("[IR] Effects OFF");
        } break;

        case IR_BRIGHTNESS_UP: {
            if (brightness < 255) {
                brightness = min(255, brightness + 25);
                setBrightness(brightness);
                DEBUG_PRINTF("[IR] Brightness up: %d\n", brightness);
            }
        } break;

        case IR_BRIGHTNESS_DOWN: {
            if (brightness > 0) {
                brightness = max(0, brightness - 25);
                setBrightness(brightness);
                DEBUG_PRINTF("[IR] Brightness down: %d\n", brightness);
            }
        } break;

        case IR_POWER_TOGGLE: {
            // Toggle between active and standby modes
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
        } break;

        case IR_STANDBY: {
            // Force standby mode
            setAllRelays(false);
            setEffect(0);
            setBrightness(0);
            DEBUG_PRINTLN("[IR] Standby mode activated");
        } break;

        case IR_SETTINGS: {
            // Open settings/calibration screen
            showCalibrationScreen();
            DEBUG_PRINTLN("[IR] Settings screen opened");
        } break;

        case IR_CALIBRATE: {
            // Start calibration mode
            showCalibrationScreen();
            DEBUG_PRINTLN("[IR] Calibration started");
        } break;

        default: {
            DEBUG_PRINTF("[IR] Unknown command: %d\n", command);
        } break;
    }
}
