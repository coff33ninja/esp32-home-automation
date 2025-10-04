// Minimal MQTT/messaging helper stubs used during build stabilization.
#include "mqtt_handler.h"
#include <PubSubClient.h>
#include "config.h"

extern PubSubClient mqttClient;

bool publishOrQueue(const char* topic, const char* payload, bool retain) {
  if (mqttClient.connected()) {
    mqttClient.publish(topic, payload, retain);
    return true;
  }
  return false;
}

void publishHomeAssistantDiscovery() { /* stub */ }
void publishDeviceAvailability(bool online) { /* stub */ }
void publishAllStates() { /* stub */ }
void publishHeartbeat() { /* stub */ }
void publishDiagnostics() { /* stub */ }
void publishHealthReport() { /* stub */ }
void publishErrorLog() { /* stub */ }
void publishLights(bool on) {
  publishOrQueue(MQTT_TOPIC_LIGHTS, on ? "ON" : "OFF", true);
}
void publishVolume(int vol) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", vol);
  publishOrQueue(MQTT_TOPIC_VOLUME, buf, true);
}

void processQueuedCommands() { /* stub */ }
