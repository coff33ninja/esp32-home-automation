/**
 * MQTT Handler Module (declarations)
 *
 * Implementations are in src/mqtt_handler.cpp to avoid multiple-definition
 * linker errors. Keep this header limited to externs, types and prototypes.
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
#include "system_monitor.h"

// External references
extern PubSubClient mqttClient;
extern int currentVolume;
extern bool lightsState;
extern int brightness;

// Command Queue limits
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

// Global command queue (defined in src/mqtt_handler.cpp)
extern QueuedCommand commandQueue[MAX_QUEUED_COMMANDS];
extern int queueHead;
extern int queueTail;
extern int queueCount;

// Function prototypes (implementations in src/mqtt_handler.cpp)
bool mqttConnect();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleCommand(const char* command);
void publishStatus();
void publishVolume(int volume);
void publishLights(bool state);
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

#endif // MQTT_HANDLER_H
