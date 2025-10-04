// Small stubs to satisfy linker for functions referenced across modules
#include "mqtt_handler.h"
#include "module_system.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

// publishStateChange: publish a small JSON payload with component state and metadata
void publishStateChange(const char* component, const char* state) {
  char topic[64];
  snprintf(topic, sizeof(topic), "%s/%s", MQTT_TOPIC_STATUS, component);

  // Build compact JSON payload
  StaticJsonDocument<256> doc;
  doc["component"] = component;
  doc["state"] = state;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["timestamp"] = millis();

  char payload[256];
  size_t len = serializeJson(doc, payload, sizeof(payload));
  (void)len; // ignore unused warning

  publishOrQueue(topic, payload, false);
}

// registerBuiltInModules: no built-ins by default in this minimal stub
bool registerBuiltInModules() {
  // Conservative built-in registration:
  // - Register relay expansion module by wrapping the existing relay expansion functions
  // - Register motor control and LED effects as simple modules (detection via hardware_detection)
  // - Call existing register*Module helpers for sensors/audio

  // Relay expansion module
  PluginModuleConfig relayConfig = {};
  relayConfig.moduleId = 201;
  strcpy(relayConfig.name, "Relay Expansion");
  strcpy(relayConfig.description, "I2C relay expansion (PCF8574)");
  relayConfig.type = PLUGIN_TYPE_RELAY_EXPANSION;
  relayConfig.interface = INTERFACE_I2C;
  relayConfig.capabilities = MODULE_CAP_OUTPUT | MODULE_CAP_HOTPLUG | MODULE_CAP_CONFIGURABLE;
  relayConfig.config.i2c.address = I2C_ADDR_EXPANSION_RELAY;
  relayConfig.config.i2c.clockSpeed = 100000;
  relayConfig.enabled = false;
  relayConfig.detected = false;

  PluginModuleInterface relayInterface = {};
  // Use the functions implemented in module_system.cpp (relay expansion helpers)
  extern bool detectRelayExpansion();
  extern bool initRelayExpansion();
  extern void updateRelayExpansion();
  extern void shutdownRelayExpansion();
  extern bool setRelayExpansionState(int relay, bool state);
  extern bool getRelayExpansionState(int relay);

  relayInterface.detect = []() -> bool { return detectRelayExpansion(); };
  relayInterface.initialize = []() -> bool { return initRelayExpansion(); };
  relayInterface.update = []() { updateRelayExpansion(); };
  relayInterface.shutdown = []() { shutdownRelayExpansion(); };
  relayInterface.writeValue = [](const String& param, int value) -> bool {
    // param format: "relayN" where N is 0-7 or "all"
    if (param.startsWith("relay")) {
      String idx = param.substring(5);
      if (idx == "all") {
        for (int i = 0; i < 8; i++) setRelayExpansionState(i, value != 0);
        return true;
      }
      int r = idx.toInt();
      if (r >= 0 && r < 8) {
        return setRelayExpansionState(r, value != 0);
      }
    }
    return false;
  };
  relayInterface.getStatus = []() -> String {
    String s = "";
    for (int i = 0; i < 8; i++) {
      s += String(getRelayExpansionState(i) ? "1" : "0");
    }
    return s;
  };

  registerModule(relayConfig, relayInterface);

  // Motor control: lightweight module wrapper that uses hardware detection/init functions
  PluginModuleConfig motorConfig = {};
  motorConfig.moduleId = 202;
  strcpy(motorConfig.name, "Motor Control");
  strcpy(motorConfig.description, "Motorized potentiometer control");
  motorConfig.type = PLUGIN_TYPE_ACTUATOR_OUTPUT;
  motorConfig.interface = INTERFACE_PWM;
  motorConfig.capabilities = MODULE_CAP_OUTPUT | MODULE_CAP_PWM | MODULE_CAP_CONFIGURABLE;
  motorConfig.config.pwm.pin = MOTOR_PWM_PIN;
  motorConfig.config.pwm.frequency = MOTOR_PWM_FREQUENCY;
  motorConfig.config.pwm.resolution = MOTOR_PWM_RESOLUTION;
  motorConfig.enabled = false;
  motorConfig.detected = false;

  PluginModuleInterface motorInterface = {};
  extern bool detectMotorControl();
  extern void initMotorControl();
  extern void moveMotorToPosition(int target);
  motorInterface.detect = []() -> bool { return detectMotorControl(); };
  motorInterface.initialize = []() -> bool { initMotorControl(); return true; };
  motorInterface.writeValue = [](const String& param, int value) -> bool {
    if (param == "moveTo") {
      moveMotorToPosition(value);
      return true;
    }
    return false;
  };
  motorInterface.getStatus = []() -> String { return motorControlInitialized ? "initialized" : "not_initialized"; };

  registerModule(motorConfig, motorInterface);

  // LED effects: wrap existing LED effects implementations
  PluginModuleConfig ledConfig = {};
  ledConfig.moduleId = 203;
  strcpy(ledConfig.name, "LED Effects");
  strcpy(ledConfig.description, "LED matrix and strip effects");
  ledConfig.type = PLUGIN_TYPE_ACTUATOR_OUTPUT;
  ledConfig.interface = INTERFACE_GPIO;
  ledConfig.capabilities = MODULE_CAP_OUTPUT | MODULE_CAP_CONFIGURABLE;
  ledConfig.enabled = false;
  ledConfig.detected = false;

  PluginModuleInterface ledInterface = {};
  extern bool detectLEDMatrix();
  extern void initLEDEffects();
  extern void setEffect(int effect);
  extern void setSolidColor(CRGB color);
  extern void setBrightness(uint8_t brightness);

  ledInterface.detect = []() -> bool { return detectLEDMatrix(); };
  ledInterface.initialize = []() -> bool { initLEDEffects(); return true; };
  ledInterface.writeValue = [](const String& param, int value) -> bool {
    if (param == "effect") {
      setEffect((LEDEffect)value);
      return true;
    } else if (param == "brightness") {
      setBrightness((uint8_t)value);
      return true;
    }
    return false;
  };
  ledInterface.getStatus = []() -> String { return "LED effects available"; };

  registerModule(ledConfig, ledInterface);

  // Call existing helper registerers (light sensor, motion, buzzer)
  registerLightSensorModule();
  registerMotionSensorModule();
  registerBuzzerModule();

  return true;
}

// Wrapper implementations moved to src/module_wrappers.cpp
