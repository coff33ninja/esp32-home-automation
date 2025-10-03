/**
 * MQTT Handler Module
 * 
 * Handles MQTT communication for smart home integration.
 * Manages connections, subscriptions, and message processing.
 */

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include "config.h"
#include "motor_control.h"
#include "relay_control.h"
#include "led_effects.h"
#include "ota_updater.h"

// External references
extern PubSubClient mqttClient;
extern int currentVolume;
extern bool lightsState;
extern int brightness;

// Forward declarations for hardware control functions
void moveMotorToPosition(int targetPosition);
void setRelay(RelayChannel channel, bool state);
void setBrightness(uint8_t brightness);
void setEffect(LEDEffect effect);
void setVolumeVisualization(int volume);

// Forward declarations for health monitoring
const char* healthStatusToString(int status);
const char* componentStatusToString(int status);

// Home Assistant Discovery Topics
#define HA_DISCOVERY_PREFIX "homeassistant"
#define HA_DEVICE_NAME "ESP32 Home Control"
#define HA_DEVICE_ID "esp32_home_control"
#define HA_MANUFACTURER "DIY Electronics"
#define HA_MODEL "ESP32 Home Automation Panel"

// Command Queue Configuration
#define MAX_QUEUED_COMMANDS 20
#define MAX_COMMAND_LENGTH 100

// Command Queue Structure
struct QueuedCommand {
    char topic[50];
    char payload[MAX_COMMAND_LENGTH];
    bool retained;
    unsigned long timestamp;
    bool valid;
};

// Global command queue
extern QueuedCommand commandQueue[MAX_QUEUED_COMMANDS];
extern int queueHead;
extern int queueTail;
extern int queueCount;

// Function prototypes
bool mqttConnect();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishStatus();
void publishVolume(int volume);
void publishLights(bool state);
void handleCommand(const char* command);
void publishHomeAssistantDiscovery();
void publishVolumeDiscovery();
void publishLightsDiscovery();
void publishEffectsDiscovery();
void publishBrightnessDiscovery();
void publishDeviceAvailability(bool available);
void publishAllStates();
bool queueCommand(const char* topic, const char* payload, bool retained = false);
void processQueuedCommands();
void clearCommandQueue();
int getQueuedCommandCount();
bool publishOrQueue(const char* topic, const char* payload, bool retained = false);
void publishHeartbeat();
void publishDiagnostics();
void publishHealthReport();
void publishErrorLog();
void publishStateChange(const char* component, const char* state);

// Implementation

bool mqttConnect() {
    // Attempt to connect
    DEBUG_PRINT("[MQTT] Attempting connection to ");
    DEBUG_PRINT(MQTT_SERVER);
    DEBUG_PRINT(":");
    DEBUG_PRINTLN(MQTT_PORT);
    
    bool connected = false;
    
    // Try with or without credentials
    if (strlen(MQTT_USER) > 0) {
        connected = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
    } else {
        connected = mqttClient.connect(MQTT_CLIENT_ID);
    }
    
    if (connected) {
        DEBUG_PRINTLN("[MQTT] Connected successfully!");
        
        // Subscribe to command topic
        mqttClient.subscribe(MQTT_TOPIC_COMMAND);
        DEBUG_PRINTF("[MQTT] Subscribed to: %s\n", MQTT_TOPIC_COMMAND);
        
        // Subscribe to OTA command topic
        mqttClient.subscribe(MQTT_TOPIC_OTA_COMMAND);
        DEBUG_PRINTF("[MQTT] Subscribed to: %s\n", MQTT_TOPIC_OTA_COMMAND);
        
        // Subscribe to configuration command topic
        mqttClient.subscribe("homecontrol/config/command");
        DEBUG_PRINTLN("[MQTT] Subscribed to: homecontrol/config/command");
        
        // Publish Home Assistant discovery messages
        publishHomeAssistantDiscovery();
        
        // Publish device availability
        publishDeviceAvailability(true);
        
        // Publish current states
        publishAllStates();
        
        // Publish online status
        publishStatus();
        
        return true;
    } else {
        DEBUG_PRINTF("[MQTT] Connection failed, rc=%d\n", mqttClient.state());
        return false;
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Convert payload to string
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    DEBUG_PRINTF("[MQTT] Message received on topic: %s\n", topic);
    DEBUG_PRINTF("[MQTT] Message: %s\n", message);
    
    // Handle different topics
    if (strcmp(topic, MQTT_TOPIC_COMMAND) == 0) {
        handleCommand(message);
    } else if (strcmp(topic, MQTT_TOPIC_OTA_COMMAND) == 0) {
        // Handle OTA commands directly
        extern OTAUpdater otaUpdater;
        otaUpdater.handleMQTTCommand(message, "");
    } else if (strcmp(topic, "homecontrol/config/command") == 0) {
        // Handle configuration commands
        extern void handleConfigurationCommands(const String& command, const String& payload);
        
        // Parse command and payload
        String msgStr = String(message);
        int colonPos = msgStr.indexOf(':');
        if (colonPos != -1) {
            String command = msgStr.substring(0, colonPos);
            String payload = msgStr.substring(colonPos + 1);
            handleConfigurationCommands(command, payload);
        } else {
            handleConfigurationCommands(msgStr, "");
        }
    }
}

void handleCommand(const char* command) {
    // Parse and execute commands
    // Format: "COMMAND:VALUE"
    // Examples: "VOLUME:50", "LIGHTS:ON", "EFFECT:RAINBOW", "BRIGHTNESS:128"
    
    char cmd[32];
    char value[32];
    
    // Split command and value
    const char* colonPos = strchr(command, ':');
    if (colonPos != NULL) {
        int cmdLen = colonPos - command;
        strncpy(cmd, command, cmdLen);
        cmd[cmdLen] = '\0';
        strcpy(value, colonPos + 1);
        
        DEBUG_PRINTF("[MQTT] Command: %s, Value: %s\n", cmd, value);
        
        // Execute command
        if (strcmp(cmd, "VOLUME") == 0) {
            int vol = atoi(value);
            currentVolume = constrain(vol, 0, 100);
            // Move motor to match volume level
            int targetPosition = map(currentVolume, 0, 100, POT_MIN_VALUE, POT_MAX_VALUE);
            moveMotorToPosition(targetPosition);
            // Trigger volume visualization on LED matrix
            setVolumeVisualization(currentVolume);
            publishVolume(currentVolume);
            DEBUG_PRINTF("[MQTT] Volume set to: %d\n", currentVolume);
        }
        else if (strcmp(cmd, "LIGHTS") == 0) {
            if (strcmp(value, "ON") == 0) {
                lightsState = true;
            } else if (strcmp(value, "OFF") == 0) {
                lightsState = false;
            } else if (strcmp(value, "TOGGLE") == 0) {
                lightsState = !lightsState;
            }
            // Control actual relays
            setRelay(RELAY_1, lightsState);
            setRelay(RELAY_2, lightsState);
            publishLights(lightsState);
            DEBUG_PRINTF("[MQTT] Lights: %s\n", lightsState ? "ON" : "OFF");
        }
        else if (strcmp(cmd, "BRIGHTNESS") == 0) {
            brightness = constrain(atoi(value), 0, 255);
            // Control LED strip brightness
            ledcWrite(LED_STRIP_PWM_CHANNEL, brightness);
            DEBUG_PRINTF("[MQTT] LED strip brightness set to: %d\n", brightness);
        }
        else if (strcmp(cmd, "MATRIX_BRIGHTNESS") == 0) {
            int matrixBrightness = constrain(atoi(value), 0, 255);
            setBrightness(matrixBrightness);
            DEBUG_PRINTF("[MQTT] Matrix brightness set to: %d\n", matrixBrightness);
        }
        else if (strncmp(cmd, "RELAY", 5) == 0) {
            // Handle individual relay commands (RELAY1, RELAY2, etc.)
            int relayNum = atoi(cmd + 5); // Extract number after "RELAY"
            if (relayNum >= 1 && relayNum <= 4) {
                bool state = (strcmp(value, "ON") == 0);
                RelayChannel channel = (RelayChannel)(relayNum - 1); // Convert to 0-based enum
                setRelay(channel, state);
                
                // Publish individual relay state
                char relayTopic[100];
                snprintf(relayTopic, sizeof(relayTopic), "%s/relay%d", MQTT_TOPIC_STATUS, relayNum);
                mqttClient.publish(relayTopic, state ? "ON" : "OFF");
                
                DEBUG_PRINTF("[MQTT] Relay %d: %s\n", relayNum, state ? "ON" : "OFF");
            }
        }
        else if (strcmp(cmd, "EFFECT") == 0) {
            DEBUG_PRINTF("[MQTT] Effect command: %s\n", value);
            // Map effect names to enum values
            if (strcmp(value, "OFF") == 0) setEffect(EFFECT_OFF);
            else if (strcmp(value, "RAINBOW") == 0) setEffect(EFFECT_RAINBOW);
            else if (strcmp(value, "FIRE") == 0) setEffect(EFFECT_FIRE);
            else if (strcmp(value, "SPARKLE") == 0) setEffect(EFFECT_SPARKLE);
            else if (strcmp(value, "BREATHING") == 0) setEffect(EFFECT_BREATHING);
            else DEBUG_PRINTF("[MQTT] Unknown effect: %s\n", value);
        }
        else {
            DEBUG_PRINTF("[MQTT] Unknown command: %s\n", cmd);
        }
    } else {
        // Simple commands without values
        if (strcmp(command, "STATUS") == 0) {
            publishStatus();
        } else if (strcmp(command, "REBOOT") == 0) {
            DEBUG_PRINTLN("[MQTT] Reboot requested");
            delay(1000);
            ESP.restart();
        } else if (strncmp(command, "OTA:", 4) == 0) {
            // Handle OTA commands
            String otaCommand = String(command + 4);
            String otaPayload = "";
            
            int colonPos = otaCommand.indexOf(':');
            if (colonPos != -1) {
                otaPayload = otaCommand.substring(colonPos + 1);
                otaCommand = otaCommand.substring(0, colonPos);
            }
            
            extern OTAUpdater otaUpdater;
            otaUpdater.handleMQTTCommand(otaCommand, otaPayload);
        }
    }
}

void publishStatus() {
    char msg[512];
    
    // Get system diagnostics
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    int wifiRSSI = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -100;
    
    // Get current states
    extern LEDEffect currentEffect;
    extern uint8_t currentBrightness;
    extern RelayStates relayStates;
    
    snprintf(msg, sizeof(msg), 
             "{"
             "\"status\":\"online\","
             "\"uptime\":%lu,"
             "\"version\":\"%s\","
             "\"volume\":%d,"
             "\"lights\":%s,"
             "\"led_brightness\":%d,"
             "\"matrix_brightness\":%d,"
             "\"current_effect\":%d,"
             "\"queue_count\":%d,"
             "\"free_heap\":%lu,"
             "\"total_heap\":%lu,"
             "\"heap_usage\":%.1f,"
             "\"wifi_rssi\":%d,"
             "\"wifi_connected\":%s,"
             "\"mqtt_connected\":%s,"
             "\"relay1\":%s,"
             "\"relay2\":%s,"
             "\"relay3\":%s,"
             "\"relay4\":%s"
             "}",
             millis() / 1000, 
             FIRMWARE_VERSION,
             currentVolume,
             lightsState ? "true" : "false",
             brightness,
             currentBrightness,
             currentEffect,
             queueCount,
             freeHeap,
             totalHeap,
             ((float)(totalHeap - freeHeap) / totalHeap) * 100.0,
             wifiRSSI,
             WiFi.status() == WL_CONNECTED ? "true" : "false",
             mqttClient.connected() ? "true" : "false",
             relayStates.relay1 ? "true" : "false",
             relayStates.relay2 ? "true" : "false",
             relayStates.relay3 ? "true" : "false",
             relayStates.relay4 ? "true" : "false");
    
    publishOrQueue(MQTT_TOPIC_STATUS, msg, true);
    DEBUG_PRINTLN("[MQTT] Comprehensive status published/queued");
}

void publishVolume(int volume) {
    char msg[10];
    snprintf(msg, sizeof(msg), "%d", volume);
    publishOrQueue(MQTT_TOPIC_VOLUME, msg);
    
    DEBUG_PRINTF("[MQTT] Volume published/queued: %d\n", volume);
}

void publishLights(bool state) {
    const char* msg = state ? "ON" : "OFF";
    publishOrQueue(MQTT_TOPIC_LIGHTS, msg);
    
    DEBUG_PRINTF("[MQTT] Lights state published/queued: %s\n", msg);
}

void publishHomeAssistantDiscovery() {
    DEBUG_PRINTLN("[MQTT] Publishing Home Assistant discovery messages...");
    
    publishVolumeDiscovery();
    publishLightsDiscovery();
    publishEffectsDiscovery();
    publishBrightnessDiscovery();
    
    DEBUG_PRINTLN("[MQTT] Home Assistant discovery complete");
}

void publishVolumeDiscovery() {
    if (!mqttClient.connected()) return;
    
    // Volume control discovery
    const char* topic = HA_DISCOVERY_PREFIX "/number/" HA_DEVICE_ID "_volume/config";
    
    char config[512];
    snprintf(config, sizeof(config),
        "{"
        "\"name\":\"Volume\","
        "\"unique_id\":\"%s_volume\","
        "\"state_topic\":\"%s\","
        "\"command_topic\":\"%s\","
        "\"min\":0,"
        "\"max\":100,"
        "\"step\":1,"
        "\"unit_of_measurement\":\"%%\","
        "\"icon\":\"mdi:volume-high\","
        "\"device\":{"
            "\"identifiers\":[\"%s\"],"
            "\"name\":\"%s\","
            "\"manufacturer\":\"%s\","
            "\"model\":\"%s\","
            "\"sw_version\":\"%s\""
        "},"
        "\"availability_topic\":\"%s/availability\""
        "}",
        HA_DEVICE_ID,
        MQTT_TOPIC_VOLUME,
        MQTT_TOPIC_COMMAND,
        HA_DEVICE_ID,
        HA_DEVICE_NAME,
        HA_MANUFACTURER,
        HA_MODEL,
        FIRMWARE_VERSION,
        HA_DEVICE_ID
    );
    
    mqttClient.publish(topic, config, true);
    DEBUG_PRINTLN("[MQTT] Volume discovery published");
}

void publishLightsDiscovery() {
    if (!mqttClient.connected()) return;
    
    // Main lights discovery
    const char* topic = HA_DISCOVERY_PREFIX "/light/" HA_DEVICE_ID "_lights/config";
    
    char config[512];
    snprintf(config, sizeof(config),
        "{"
        "\"name\":\"Main Lights\","
        "\"unique_id\":\"%s_lights\","
        "\"state_topic\":\"%s\","
        "\"command_topic\":\"%s\","
        "\"payload_on\":\"LIGHTS:ON\","
        "\"payload_off\":\"LIGHTS:OFF\","
        "\"state_value_template\":\"{{ 'ON' if value == 'ON' else 'OFF' }}\","
        "\"icon\":\"mdi:lightbulb\","
        "\"device\":{"
            "\"identifiers\":[\"%s\"],"
            "\"name\":\"%s\","
            "\"manufacturer\":\"%s\","
            "\"model\":\"%s\","
            "\"sw_version\":\"%s\""
        "},"
        "\"availability_topic\":\"%s/availability\""
        "}",
        HA_DEVICE_ID,
        MQTT_TOPIC_LIGHTS,
        MQTT_TOPIC_COMMAND,
        HA_DEVICE_ID,
        HA_DEVICE_NAME,
        HA_MANUFACTURER,
        HA_MODEL,
        FIRMWARE_VERSION,
        HA_DEVICE_ID
    );
    
    mqttClient.publish(topic, config, true);
    DEBUG_PRINTLN("[MQTT] Lights discovery published");
    
    // Individual relay discoveries
    const char* relayNames[] = {"Relay 1", "Relay 2", "Relay 3", "Relay 4"};
    const char* relayIcons[] = {"mdi:lightbulb", "mdi:lightbulb-outline", "mdi:power-socket", "mdi:power-socket"};
    
    for (int i = 0; i < 4; i++) {
        char relayTopic[100];
        char relayConfig[512];
        
        snprintf(relayTopic, sizeof(relayTopic), 
                HA_DISCOVERY_PREFIX "/switch/" HA_DEVICE_ID "_relay%d/config", i + 1);
        
        snprintf(relayConfig, sizeof(relayConfig),
            "{"
            "\"name\":\"%s\","
            "\"unique_id\":\"%s_relay%d\","
            "\"state_topic\":\"%s/relay%d\","
            "\"command_topic\":\"%s\","
            "\"payload_on\":\"RELAY%d:ON\","
            "\"payload_off\":\"RELAY%d:OFF\","
            "\"state_on\":\"ON\","
            "\"state_off\":\"OFF\","
            "\"icon\":\"%s\","
            "\"device\":{"
                "\"identifiers\":[\"%s\"]"
            "},"
            "\"availability_topic\":\"%s/availability\""
            "}",
            relayNames[i],
            HA_DEVICE_ID, i + 1,
            MQTT_TOPIC_STATUS, i + 1,
            MQTT_TOPIC_COMMAND,
            i + 1,
            i + 1,
            relayIcons[i],
            HA_DEVICE_ID,
            HA_DEVICE_ID
        );
        
        mqttClient.publish(relayTopic, relayConfig, true);
    }
    
    DEBUG_PRINTLN("[MQTT] Relay discoveries published");
}

void publishEffectsDiscovery() {
    if (!mqttClient.connected()) return;
    
    // LED Effects discovery
    const char* topic = HA_DISCOVERY_PREFIX "/select/" HA_DEVICE_ID "_effects/config";
    
    char config[768];
    snprintf(config, sizeof(config),
        "{"
        "\"name\":\"LED Effects\","
        "\"unique_id\":\"%s_effects\","
        "\"state_topic\":\"%s\","
        "\"command_topic\":\"%s\","
        "\"command_template\":\"EFFECT:{{ value }}\","
        "\"options\":[\"OFF\",\"SOLID\",\"RAINBOW\",\"CYCLE\",\"FIRE\",\"SPARKLE\",\"BREATHING\",\"CHASE\"],"
        "\"icon\":\"mdi:led-strip-variant\","
        "\"device\":{"
            "\"identifiers\":[\"%s\"],"
            "\"name\":\"%s\","
            "\"manufacturer\":\"%s\","
            "\"model\":\"%s\","
            "\"sw_version\":\"%s\""
        "},"
        "\"availability_topic\":\"%s/availability\""
        "}",
        HA_DEVICE_ID,
        MQTT_TOPIC_EFFECTS,
        MQTT_TOPIC_COMMAND,
        HA_DEVICE_ID,
        HA_DEVICE_NAME,
        HA_MANUFACTURER,
        HA_MODEL,
        FIRMWARE_VERSION,
        HA_DEVICE_ID
    );
    
    mqttClient.publish(topic, config, true);
    DEBUG_PRINTLN("[MQTT] Effects discovery published");
}

void publishBrightnessDiscovery() {
    if (!mqttClient.connected()) return;
    
    // LED Strip Brightness discovery
    const char* stripTopic = HA_DISCOVERY_PREFIX "/number/" HA_DEVICE_ID "_led_brightness/config";
    
    char stripConfig[512];
    snprintf(stripConfig, sizeof(stripConfig),
        "{"
        "\"name\":\"LED Strip Brightness\","
        "\"unique_id\":\"%s_led_brightness\","
        "\"state_topic\":\"%s/led_brightness\","
        "\"command_topic\":\"%s\","
        "\"command_template\":\"BRIGHTNESS:{{ value }}\","
        "\"min\":0,"
        "\"max\":255,"
        "\"step\":1,"
        "\"icon\":\"mdi:brightness-6\","
        "\"device\":{"
            "\"identifiers\":[\"%s\"]"
        "},"
        "\"availability_topic\":\"%s/availability\""
        "}",
        HA_DEVICE_ID,
        MQTT_TOPIC_STATUS,
        MQTT_TOPIC_COMMAND,
        HA_DEVICE_ID,
        HA_DEVICE_ID
    );
    
    mqttClient.publish(stripTopic, stripConfig, true);
    
    // Matrix Brightness discovery
    const char* matrixTopic = HA_DISCOVERY_PREFIX "/number/" HA_DEVICE_ID "_matrix_brightness/config";
    
    char matrixConfig[512];
    snprintf(matrixConfig, sizeof(matrixConfig),
        "{"
        "\"name\":\"Matrix Brightness\","
        "\"unique_id\":\"%s_matrix_brightness\","
        "\"state_topic\":\"%s/matrix_brightness\","
        "\"command_topic\":\"%s\","
        "\"command_template\":\"MATRIX_BRIGHTNESS:{{ value }}\","
        "\"min\":0,"
        "\"max\":255,"
        "\"step\":1,"
        "\"icon\":\"mdi:brightness-7\","
        "\"device\":{"
            "\"identifiers\":[\"%s\"]"
        "},"
        "\"availability_topic\":\"%s/availability\""
        "}",
        HA_DEVICE_ID,
        MQTT_TOPIC_STATUS,
        MQTT_TOPIC_COMMAND,
        HA_DEVICE_ID,
        HA_DEVICE_ID
    );
    
    mqttClient.publish(matrixTopic, matrixConfig, true);
    DEBUG_PRINTLN("[MQTT] Brightness discoveries published");
}

void publishDeviceAvailability(bool available) {
    if (!mqttClient.connected()) return;
    
    char topic[100];
    snprintf(topic, sizeof(topic), "%s/availability", HA_DEVICE_ID);
    
    const char* payload = available ? "online" : "offline";
    mqttClient.publish(topic, payload, true);
    
    DEBUG_PRINTF("[MQTT] Device availability: %s\n", payload);
}

void publishAllStates() {
    if (!mqttClient.connected()) return;
    
    DEBUG_PRINTLN("[MQTT] Publishing all current states...");
    
    // Publish current volume
    publishVolume(currentVolume);
    
    // Publish current lights state
    publishLights(lightsState);
    
    // Publish current effect
    extern LEDEffect currentEffect;
    const char* effectNames[] = {"OFF", "SOLID", "RAINBOW", "CYCLE", "FIRE", "SPARKLE", "BREATHING", "CHASE"};
    if (currentEffect < 8) {
        mqttClient.publish(MQTT_TOPIC_EFFECTS, effectNames[currentEffect]);
    }
    
    // Publish brightness values
    char brightnessTopic[100];
    char brightnessMsg[10];
    
    snprintf(brightnessTopic, sizeof(brightnessTopic), "%s/led_brightness", MQTT_TOPIC_STATUS);
    snprintf(brightnessMsg, sizeof(brightnessMsg), "%d", brightness);
    mqttClient.publish(brightnessTopic, brightnessMsg);
    
    extern uint8_t currentBrightness;
    snprintf(brightnessTopic, sizeof(brightnessTopic), "%s/matrix_brightness", MQTT_TOPIC_STATUS);
    snprintf(brightnessMsg, sizeof(brightnessMsg), "%d", currentBrightness);
    mqttClient.publish(brightnessTopic, brightnessMsg);
    
    // Publish individual relay states
    extern RelayStates relayStates;
    bool relayArray[] = {relayStates.relay1, relayStates.relay2, relayStates.relay3, relayStates.relay4};
    
    for (int i = 0; i < 4; i++) {
        char relayTopic[100];
        snprintf(relayTopic, sizeof(relayTopic), "%s/relay%d", MQTT_TOPIC_STATUS, i + 1);
        mqttClient.publish(relayTopic, relayArray[i] ? "ON" : "OFF");
    }
    
    DEBUG_PRINTLN("[MQTT] All states published");
}

bool queueCommand(const char* topic, const char* payload, bool retained) {
    if (queueCount >= MAX_QUEUED_COMMANDS) {
        DEBUG_PRINTLN("[MQTT] Command queue full, dropping oldest command");
        // Remove oldest command to make space
        queueHead = (queueHead + 1) % MAX_QUEUED_COMMANDS;
        queueCount--;
    }
    
    // Add new command to queue
    QueuedCommand* cmd = &commandQueue[queueTail];
    strncpy(cmd->topic, topic, sizeof(cmd->topic) - 1);
    cmd->topic[sizeof(cmd->topic) - 1] = '\0';
    strncpy(cmd->payload, payload, sizeof(cmd->payload) - 1);
    cmd->payload[sizeof(cmd->payload) - 1] = '\0';
    cmd->retained = retained;
    cmd->timestamp = millis();
    cmd->valid = true;
    
    queueTail = (queueTail + 1) % MAX_QUEUED_COMMANDS;
    queueCount++;
    
    DEBUG_PRINTF("[MQTT] Command queued: %s -> %s (Queue: %d/%d)\n", 
                topic, payload, queueCount, MAX_QUEUED_COMMANDS);
    
    return true;
}

void processQueuedCommands() {
    if (!mqttClient.connected() || queueCount == 0) {
        return;
    }
    
    DEBUG_PRINTF("[MQTT] Processing %d queued commands...\n", queueCount);
    
    int processed = 0;
    int maxProcessPerCycle = 5; // Limit processing to avoid blocking
    
    while (queueCount > 0 && processed < maxProcessPerCycle) {
        QueuedCommand* cmd = &commandQueue[queueHead];
        
        if (cmd->valid) {
            // Check if command is too old (older than 5 minutes)
            if (millis() - cmd->timestamp > 300000) {
                DEBUG_PRINTF("[MQTT] Dropping expired command: %s\n", cmd->topic);
            } else {
                // Publish the queued command
                bool success = mqttClient.publish(cmd->topic, cmd->payload, cmd->retained);
                if (success) {
                    DEBUG_PRINTF("[MQTT] Published queued: %s -> %s\n", cmd->topic, cmd->payload);
                } else {
                    DEBUG_PRINTF("[MQTT] Failed to publish queued: %s\n", cmd->topic);
                    break; // Stop processing if publish fails
                }
            }
        }
        
        // Remove command from queue
        cmd->valid = false;
        queueHead = (queueHead + 1) % MAX_QUEUED_COMMANDS;
        queueCount--;
        processed++;
    }
    
    if (processed > 0) {
        DEBUG_PRINTF("[MQTT] Processed %d queued commands\n", processed);
    }
}

void clearCommandQueue() {
    queueHead = 0;
    queueTail = 0;
    queueCount = 0;
    
    for (int i = 0; i < MAX_QUEUED_COMMANDS; i++) {
        commandQueue[i].valid = false;
    }
    
    DEBUG_PRINTLN("[MQTT] Command queue cleared");
}

int getQueuedCommandCount() {
    return queueCount;
}

bool publishOrQueue(const char* topic, const char* payload, bool retained) {
    if (mqttClient.connected()) {
        // Direct publish if connected
        bool success = mqttClient.publish(topic, payload, retained);
        if (success) {
            DEBUG_PRINTF("[MQTT] Published: %s -> %s\n", topic, payload);
        } else {
            DEBUG_PRINTF("[MQTT] Publish failed, queuing: %s -> %s\n", topic, payload);
            queueCommand(topic, payload, retained);
        }
        return success;
    } else {
        // Queue if not connected
        DEBUG_PRINTF("[MQTT] Not connected, queuing: %s -> %s\n", topic, payload);
        return queueCommand(topic, payload, retained);
    }
}

void publishHeartbeat() {
    char msg[256];
    unsigned long uptime = millis() / 1000;
    uint32_t freeHeap = ESP.getFreeHeap();
    
    snprintf(msg, sizeof(msg),
             "{"
             "\"timestamp\":%lu,"
             "\"uptime\":%lu,"
             "\"free_heap\":%lu,"
             "\"wifi_rssi\":%d,"
             "\"queue_count\":%d"
             "}",
             millis(),
             uptime,
             freeHeap,
             WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -100,
             queueCount);
    
    char heartbeatTopic[100];
    snprintf(heartbeatTopic, sizeof(heartbeatTopic), "%s/heartbeat", HA_DEVICE_ID);
    
    publishOrQueue(heartbeatTopic, msg);
    DEBUG_PRINTF("[MQTT] Heartbeat published (uptime: %lu, heap: %lu)\n", uptime, freeHeap);
}

void publishDiagnostics() {
    char msg[512];
    
    // Collect detailed diagnostic information
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();
    uint32_t maxAllocHeap = ESP.getMaxAllocHeap();
    
    // Get chip information
    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);
    
    snprintf(msg, sizeof(msg),
             "{"
             "\"chip_model\":\"%s\","
             "\"chip_revision\":%d,"
             "\"cpu_cores\":%d,"
             "\"cpu_freq\":%lu,"
             "\"flash_size\":%lu,"
             "\"free_heap\":%lu,"
             "\"total_heap\":%lu,"
             "\"min_free_heap\":%lu,"
             "\"max_alloc_heap\":%lu,"
             "\"heap_fragmentation\":%.1f,"
             "\"wifi_mac\":\"%s\","
             "\"reset_reason\":%d,"
             "\"boot_count\":%lu"
             "}",
             ESP.getChipModel(),
             chipInfo.revision,
             chipInfo.cores,
             ESP.getCpuFreqMHz() * 1000000UL,
             ESP.getFlashChipSize(),
             freeHeap,
             totalHeap,
             minFreeHeap,
             maxAllocHeap,
             ((float)(totalHeap - freeHeap) / totalHeap) * 100.0,
             WiFi.macAddress().c_str(),
             esp_reset_reason(),
             ESP.getBootCount());
    
    char diagnosticsTopic[100];
    snprintf(diagnosticsTopic, sizeof(diagnosticsTopic), "%s/diagnostics", HA_DEVICE_ID);
    
    publishOrQueue(diagnosticsTopic, msg, true);
    DEBUG_PRINTLN("[MQTT] Diagnostics published");
}

void publishHealthReport() {
    // Publish comprehensive health report
    extern SystemHealth systemHealth;
    
    char msg[768];
    snprintf(msg, sizeof(msg),
             "{"
             "\"overall_health\":\"%s\","
             "\"uptime\":%lu,"
             "\"maintenance_required\":%s,"
             "\"memory\":{"
                 "\"free_heap\":%lu,"
                 "\"total_heap\":%lu,"
                 "\"fragmentation\":%.1f,"
                 "\"status\":\"%s\""
             "},"
             "\"wifi\":{"
                 "\"connected\":%s,"
                 "\"rssi\":%d,"
                 "\"reconnect_count\":%lu,"
                 "\"status\":\"%s\""
             "},"
             "\"components\":{"
                 "\"motor\":\"%s\","
                 "\"relay\":\"%s\","
                 "\"led_matrix\":\"%s\","
                 "\"led_strip\":\"%s\","
                 "\"touch\":\"%s\","
                 "\"ir\":\"%s\","
                 "\"mqtt\":\"%s\""
             "}"
             "}",
             healthStatusToString(systemHealth.overall),
             systemHealth.uptime / 1000,
             systemHealth.maintenanceRequired ? "true" : "false",
             systemHealth.memory.freeHeap,
             systemHealth.memory.totalHeap,
             systemHealth.memory.fragmentationPercent,
             healthStatusToString(systemHealth.memory.status),
             systemHealth.wifi.connected ? "true" : "false",
             systemHealth.wifi.rssi,
             systemHealth.wifi.reconnectCount,
             healthStatusToString(systemHealth.wifi.status),
             componentStatusToString(systemHealth.components.motorControl),
             componentStatusToString(systemHealth.components.relayControl),
             componentStatusToString(systemHealth.components.ledMatrix),
             componentStatusToString(systemHealth.components.ledStrip),
             componentStatusToString(systemHealth.components.touchScreen),
             componentStatusToString(systemHealth.components.irReceiver),
             componentStatusToString(systemHealth.components.mqttHandler));
    
    char healthTopic[100];
    snprintf(healthTopic, sizeof(healthTopic), "%s/health", HA_DEVICE_ID);
    
    publishOrQueue(healthTopic, msg, true);
    DEBUG_PRINTLN("[MQTT] Health report published");
}

void publishErrorLog() {
    // Publish recent error log entries
    extern ErrorLogEntry errorLog[];
    extern int errorLogCount;
    
    if (errorLogCount == 0) {
        return; // No errors to publish
    }
    
    char msg[1024];
    strcpy(msg, "{\"errors\":[");
    
    int published = 0;
    int maxPublish = 5; // Limit to 5 most recent errors
    
    for (int i = 0; i < errorLogCount && published < maxPublish; i++) {
        int index = (errorLogIndex - 1 - i + MAX_ERROR_LOG_ENTRIES) % MAX_ERROR_LOG_ENTRIES;
        ErrorLogEntry* entry = &errorLog[index];
        
        if (entry->valid) {
            if (published > 0) {
                strcat(msg, ",");
            }
            
            char errorEntry[200];
            snprintf(errorEntry, sizeof(errorEntry),
                     "{"
                     "\"timestamp\":%lu,"
                     "\"severity\":\"%s\","
                     "\"component\":\"%s\","
                     "\"message\":\"%s\""
                     "}",
                     entry->timestamp,
                     healthStatusToString(entry->severity),
                     entry->component,
                     entry->message);
            
            strcat(msg, errorEntry);
            published++;
        }
    }
    
    strcat(msg, "]}");
    
    char errorTopic[100];
    snprintf(errorTopic, sizeof(errorTopic), "%s/errors", HA_DEVICE_ID);
    
    publishOrQueue(errorTopic, msg);
    DEBUG_PRINTF("[MQTT] Published %d error log entries\n", published);
}

void publishStateChange(const char* component, const char* state) {
    char msg[128];
    char topic[100];
    
    snprintf(msg, sizeof(msg),
             "{"
             "\"component\":\"%s\","
             "\"state\":\"%s\","
             "\"timestamp\":%lu"
             "}",
             component, state, millis());
    
    snprintf(topic, sizeof(topic), "%s/state_change", HA_DEVICE_ID);
    
    publishOrQueue(topic, msg);
    DEBUG_PRINTF("[MQTT] State change published: %s -> %s\n", component, state);
}

// Global variable definitions
QueuedCommand commandQueue[MAX_QUEUED_COMMANDS];
int queueHead = 0;
int queueTail = 0;
int queueCount = 0;

#endif // MQTT_HANDLER_H
