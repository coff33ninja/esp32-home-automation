/**
 * Configuration Manager Implementation
 *
 * Manages persistent storage and retrieval of system configuration settings.
 * Supports EEPROM and SPIFFS storage with validation and backup functionality.
 */

#include "config_manager.h"
#include "mqtt_handler.h"

// Global configuration manager instance
ConfigManager configManager;

// ========================================
// CONSTRUCTOR
// ========================================
ConfigManager::ConfigManager() {
  configLoaded = false;
  configChanged = false;
  lastSave = 0;
  memset(&config, 0, sizeof(SystemConfig));
}

// ========================================
// INITIALIZATION
// ========================================
bool ConfigManager::begin() {
  DEBUG_PRINTLN("[CONFIG] Initializing configuration manager...");

  // Initialize EEPROM
  if (!EEPROM.begin(CONFIG_EEPROM_SIZE)) {
    DEBUG_PRINTLN("[CONFIG] Failed to initialize EEPROM");
    return false;
  }

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    DEBUG_PRINTLN("[CONFIG] Failed to initialize SPIFFS");
    return false;
  }

  // Set default configuration
  setDefaults();

  DEBUG_PRINTLN("[CONFIG] Configuration manager initialized");
  return true;
}

bool ConfigManager::load() {
  DEBUG_PRINTLN("[CONFIG] Loading configuration...");

  // Try to load from SPIFFS first (preferred method)
  if (loadFromSPIFFS()) {
    DEBUG_PRINTLN("[CONFIG] Configuration loaded from SPIFFS");
    configLoaded = true;
    configChanged = false;
    return true;
  }

  // Fallback to EEPROM
  if (loadFromEEPROM()) {
    DEBUG_PRINTLN("[CONFIG] Configuration loaded from EEPROM");
    // Migrate to SPIFFS for future use
    saveToSPIFFS();
    configLoaded = true;
    configChanged = false;
    return true;
  }

  // No valid configuration found, use defaults
  DEBUG_PRINTLN("[CONFIG] No valid configuration found, using defaults");
  setDefaults();
  save(true); // Force save defaults
  configLoaded = true;
  return true;
}

bool ConfigManager::save(bool force) {
  if (!configLoaded) {
    DEBUG_PRINTLN("[CONFIG] Cannot save - configuration not loaded");
    return false;
  }

  if (!force && !configChanged) {
    return true; // Nothing to save
  }

  DEBUG_PRINTLN("[CONFIG] Saving configuration...");

  // Update checksum
  config.checksum = calculateChecksum(config);

  // Create backup before saving
  createBackup();

  // Save to both SPIFFS and EEPROM for redundancy
  bool spiffsOk = saveToSPIFFS();
  bool eepromOk = saveToEEPROM();

  if (spiffsOk || eepromOk) {
    configChanged = false;
    lastSave = millis();
    DEBUG_PRINTLN("[CONFIG] Configuration saved successfully");
    return true;
  } else {
    DEBUG_PRINTLN("[CONFIG] Failed to save configuration");
    return false;
  }
}

bool ConfigManager::reset() {
  DEBUG_PRINTLN("[CONFIG] Resetting configuration to defaults");

  setDefaults();
  configChanged = true;

  return save(true);
}

bool ConfigManager::backup() { return createBackup(); }

bool ConfigManager::restore() { return restoreFromBackup(); }

// ========================================
// PRIVATE METHODS
// ========================================
uint16_t ConfigManager::calculateChecksum(const SystemConfig &cfg) {
  uint16_t checksum = 0;
  const uint8_t *data = (const uint8_t *)&cfg;

  // Skip the checksum field itself
  for (size_t i = 0; i < sizeof(SystemConfig); i++) {
    if (i >= offsetof(SystemConfig, checksum) &&
        i < offsetof(SystemConfig, checksum) + sizeof(cfg.checksum)) {
      continue;
    }
    checksum ^= data[i];
    checksum = (checksum << 1) | (checksum >> 15); // Rotate left
  }

  return checksum;
}

bool ConfigManager::validateConfig(const SystemConfig &cfg) {
  // Check magic number and version
  if (cfg.magicNumber != CONFIG_MAGIC_NUMBER) {
    DEBUG_PRINTLN("[CONFIG] Invalid magic number");
    return false;
  }

  if (cfg.version > CONFIG_VERSION) {
    DEBUG_PRINTLN("[CONFIG] Unsupported configuration version");
    return false;
  }

  // Validate checksum
  uint16_t expectedChecksum = calculateChecksum(cfg);
  if (cfg.checksum != expectedChecksum) {
    DEBUG_PRINTF("[CONFIG] Checksum mismatch: expected %04X, got %04X\n",
                 expectedChecksum, cfg.checksum);
    return false;
  }

  // Validate critical fields
  if (!VALIDATE_PORT(cfg.mqttPort)) {
    DEBUG_PRINTLN("[CONFIG] Invalid MQTT port");
    return false;
  }

  if (!VALIDATE_TIMEOUT(cfg.wifiTimeout)) {
    DEBUG_PRINTLN("[CONFIG] Invalid WiFi timeout");
    return false;
  }

  if (!VALIDATE_RANGE(cfg.motor.motorPWMFrequency, 100, 20000)) {
    DEBUG_PRINTLN("[CONFIG] Invalid motor PWM frequency");
    return false;
  }

  return true;
}

void ConfigManager::setDefaults() {
  DEBUG_PRINTLN("[CONFIG] Setting default configuration");

  memset(&config, 0, sizeof(SystemConfig));

  // Header
  config.magicNumber = CONFIG_MAGIC_NUMBER;
  config.version = CONFIG_VERSION;

  // WiFi defaults
  strncpy(config.wifiSSID, WIFI_SSID, CONFIG_MAX_STRING_LENGTH - 1);
  strncpy(config.wifiPassword, WIFI_PASSWORD, CONFIG_MAX_STRING_LENGTH - 1);
  config.wifiAutoConnect = true;
  config.wifiTimeout = WIFI_TIMEOUT;

  // MQTT defaults
  strncpy(config.mqttServer, MQTT_SERVER, CONFIG_MAX_STRING_LENGTH - 1);
  config.mqttPort = MQTT_PORT;
  strncpy(config.mqttUser, MQTT_USER, CONFIG_MAX_STRING_LENGTH - 1);
  strncpy(config.mqttPassword, MQTT_PASSWORD, CONFIG_MAX_STRING_LENGTH - 1);
  strncpy(config.mqttClientID, MQTT_CLIENT_ID, CONFIG_MAX_STRING_LENGTH - 1);
  config.mqttAutoConnect = true;
  config.mqttReconnectDelay = MQTT_RECONNECT_DELAY;

  // Motor defaults
  config.motor.motorPWMFrequency = MOTOR_PWM_FREQUENCY;
  config.motor.motorPWMResolution = MOTOR_PWM_RESOLUTION;
  config.motor.potDeadband = POT_DEADBAND;
  config.motor.motorEnabled = true;
  config.motor.motorReversed = false;
  config.motor.motorCalibrationMin = POT_MIN_VALUE;
  config.motor.motorCalibrationMax = POT_MAX_VALUE;

  // LED Strip defaults
  config.ledStrip.pwmFrequency = LED_STRIP_PWM_FREQUENCY;
  config.ledStrip.pwmResolution = LED_STRIP_PWM_RESOLUTION;
  config.ledStrip.stripEnabled = true;
  config.ledStrip.maxBrightness = 255;
  config.ledStrip.autoOn = false;

  // LED Matrix defaults
  config.ledMatrix.maxBrightness = MAX_BRIGHTNESS;
  config.ledMatrix.defaultEffect = 0; // Off
  config.ledMatrix.matrixEnabled = true;
  config.ledMatrix.frameRate = FRAMES_PER_SECOND;
  config.ledMatrix.autoEffects = false;
  config.ledMatrix.effectChangeInterval = 30000; // 30 seconds

  // Touch Screen defaults
  config.touchScreen.touchEnabled = true;
  memset(config.touchScreen.calibrationData, 0,
         sizeof(config.touchScreen.calibrationData));
  config.touchScreen.calibrated = false;
  config.touchScreen.screenTimeout = 30000; // 30 seconds
  config.touchScreen.dimBrightness = 30;
  config.touchScreen.autoWake = true;

  // IR defaults
  config.infrared.irEnabled = true;
  config.infrared.receiverPin = IR_RECV_PIN;
  config.infrared.learningMode = false;
  memset(config.infrared.learnedCodes, 0, sizeof(config.infrared.learnedCodes));
  config.infrared.codeCount = 0;

  // System defaults
  config.system.debugEnabled = DEBUG_ENABLED;
  config.system.serialBaudRate = SERIAL_BAUD_RATE;
  config.system.watchdogEnabled = true;
  config.system.watchdogTimeout = WATCHDOG_TIMEOUT;
  config.system.failsafeEnabled = true;
  config.system.healthCheckInterval = HEALTH_CHECK_INTERVAL;

  // OTA defaults
  config.ota.autoUpdate = false; // Disabled by default for safety
  config.ota.checkInterval = OTA_CHECK_INTERVAL;
  strncpy(config.ota.updateServer, OTA_UPDATE_URL,
          CONFIG_MAX_STRING_LENGTH - 1);
  config.ota.allowBeta = false;
  config.ota.requireConfirmation = true;

  // User preferences
  config.preferences.defaultVolume = FAILSAFE_VOLUME;
  config.preferences.lightsOnBoot = FAILSAFE_LIGHTS_STATE;
  config.preferences.defaultBrightness = FAILSAFE_BRIGHTNESS;
  config.preferences.muteOnBoot = false;
  config.preferences.rememberLastState = true;

  configChanged = true;
}

bool ConfigManager::loadFromEEPROM() {
  DEBUG_PRINTLN("[CONFIG] Loading from EEPROM...");

  SystemConfig tempConfig;

  // Read configuration from EEPROM
  for (size_t i = 0; i < sizeof(SystemConfig); i++) {
    ((uint8_t *)&tempConfig)[i] = EEPROM.read(CONFIG_EEPROM_START + i);
  }

  // Validate configuration
  if (validateConfig(tempConfig)) {
    config = tempConfig;
    return true;
  }

  return false;
}

bool ConfigManager::saveToEEPROM() {
  DEBUG_PRINTLN("[CONFIG] Saving to EEPROM...");

  // Write configuration to EEPROM
  for (size_t i = 0; i < sizeof(SystemConfig); i++) {
    EEPROM.write(CONFIG_EEPROM_START + i, ((uint8_t *)&config)[i]);
  }

  return EEPROM.commit();
}

bool ConfigManager::loadFromSPIFFS() {
  DEBUG_PRINTLN("[CONFIG] Loading from SPIFFS...");

  if (!SPIFFS.exists(CONFIG_FILE_PATH)) {
    DEBUG_PRINTLN("[CONFIG] Configuration file does not exist");
    return false;
  }

  File file = SPIFFS.open(CONFIG_FILE_PATH, "r");
  if (!file) {
    DEBUG_PRINTLN("[CONFIG] Failed to open configuration file");
    return false;
  }

  String jsonString = file.readString();
  file.close();

  return setConfigFromJSON(jsonString);
}

bool ConfigManager::saveToSPIFFS() {
  DEBUG_PRINTLN("[CONFIG] Saving to SPIFFS...");

  File file = SPIFFS.open(CONFIG_FILE_PATH, "w");
  if (!file) {
    DEBUG_PRINTLN("[CONFIG] Failed to create configuration file");
    return false;
  }

  String jsonString = getConfigJSON();
  size_t written = file.print(jsonString);
  file.close();

  return written == jsonString.length();
}

bool ConfigManager::createBackup() {
  DEBUG_PRINTLN("[CONFIG] Creating configuration backup...");

  if (!SPIFFS.exists(CONFIG_FILE_PATH)) {
    return true; // No file to backup
  }

  // Copy current config to backup
  File source = SPIFFS.open(CONFIG_FILE_PATH, "r");
  if (!source) {
    return false;
  }

  File backup = SPIFFS.open(CONFIG_BACKUP_PATH, "w");
  if (!backup) {
    source.close();
    return false;
  }

  // Copy file contents
  while (source.available()) {
    backup.write(source.read());
  }

  source.close();
  backup.close();

  return true;
}

bool ConfigManager::restoreFromBackup() {
  DEBUG_PRINTLN("[CONFIG] Restoring from backup...");

  if (!SPIFFS.exists(CONFIG_BACKUP_PATH)) {
    DEBUG_PRINTLN("[CONFIG] No backup file found");
    return false;
  }

  // Copy backup to main config
  File backup = SPIFFS.open(CONFIG_BACKUP_PATH, "r");
  if (!backup) {
    return false;
  }

  File config = SPIFFS.open(CONFIG_FILE_PATH, "w");
  if (!config) {
    backup.close();
    return false;
  }

  // Copy file contents
  while (backup.available()) {
    config.write(backup.read());
  }

  backup.close();
  config.close();

  // Try to load the restored configuration
  return loadFromSPIFFS();
}

// ========================================
// PUBLIC CONFIGURATION ACCESS
// ========================================
SystemConfig &ConfigManager::getConfig() { return config; }

const SystemConfig &ConfigManager::getConfig() const { return config; }

bool ConfigManager::isLoaded() const { return configLoaded; }

bool ConfigManager::hasChanged() const { return configChanged; }

void ConfigManager::markChanged() { configChanged = true; }

// ========================================
// CONFIGURATION SETTERS
// ========================================
void ConfigManager::setWiFiCredentials(const String &ssid,
                                       const String &password) {
  strncpy(config.wifiSSID, ssid.c_str(), CONFIG_MAX_STRING_LENGTH - 1);
  strncpy(config.wifiPassword, password.c_str(), CONFIG_MAX_STRING_LENGTH - 1);
  config.wifiSSID[CONFIG_MAX_STRING_LENGTH - 1] = '\0';
  config.wifiPassword[CONFIG_MAX_STRING_LENGTH - 1] = '\0';
  markChanged();
}

void ConfigManager::setWiFiAutoConnect(bool enabled) {
  config.wifiAutoConnect = enabled;
  markChanged();
}

void ConfigManager::setWiFiTimeout(int timeout) {
  if (VALIDATE_TIMEOUT(timeout)) {
    config.wifiTimeout = timeout;
    markChanged();
  }
}

void ConfigManager::setMQTTServer(const String &server, int port) {
  strncpy(config.mqttServer, server.c_str(), CONFIG_MAX_STRING_LENGTH - 1);
  config.mqttServer[CONFIG_MAX_STRING_LENGTH - 1] = '\0';

  if (VALIDATE_PORT(port)) {
    config.mqttPort = port;
  }

  markChanged();
}

void ConfigManager::setMQTTCredentials(const String &user,
                                       const String &password) {
  strncpy(config.mqttUser, user.c_str(), CONFIG_MAX_STRING_LENGTH - 1);
  strncpy(config.mqttPassword, password.c_str(), CONFIG_MAX_STRING_LENGTH - 1);
  config.mqttUser[CONFIG_MAX_STRING_LENGTH - 1] = '\0';
  config.mqttPassword[CONFIG_MAX_STRING_LENGTH - 1] = '\0';
  markChanged();
}

void ConfigManager::setMQTTClientID(const String &clientID) {
  strncpy(config.mqttClientID, clientID.c_str(), CONFIG_MAX_STRING_LENGTH - 1);
  config.mqttClientID[CONFIG_MAX_STRING_LENGTH - 1] = '\0';
  markChanged();
}

void ConfigManager::setMQTTAutoConnect(bool enabled) {
  config.mqttAutoConnect = enabled;
  markChanged();
}

// ========================================
// JSON SERIALIZATION
// ========================================
String ConfigManager::getConfigJSON() const {
  DynamicJsonDocument doc(2048);

  // Header
  doc["version"] = config.version;

  // WiFi
  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["ssid"] = config.wifiSSID;
  wifi["password"] = config.wifiPassword;
  wifi["auto_connect"] = config.wifiAutoConnect;
  wifi["timeout"] = config.wifiTimeout;

  // MQTT
  JsonObject mqtt = doc.createNestedObject("mqtt");
  mqtt["server"] = config.mqttServer;
  mqtt["port"] = config.mqttPort;
  mqtt["user"] = config.mqttUser;
  mqtt["password"] = config.mqttPassword;
  mqtt["client_id"] = config.mqttClientID;
  mqtt["auto_connect"] = config.mqttAutoConnect;
  mqtt["reconnect_delay"] = config.mqttReconnectDelay;

  // Hardware
  JsonObject hardware = doc.createNestedObject("hardware");

  JsonObject motor = hardware.createNestedObject("motor");
  motor["pwm_frequency"] = config.motor.motorPWMFrequency;
  motor["pwm_resolution"] = config.motor.motorPWMResolution;
  motor["deadband"] = config.motor.potDeadband;
  motor["enabled"] = config.motor.motorEnabled;
  motor["reversed"] = config.motor.motorReversed;
  motor["cal_min"] = config.motor.motorCalibrationMin;
  motor["cal_max"] = config.motor.motorCalibrationMax;

  JsonObject ledStrip = hardware.createNestedObject("led_strip");
  ledStrip["pwm_frequency"] = config.ledStrip.pwmFrequency;
  ledStrip["pwm_resolution"] = config.ledStrip.pwmResolution;
  ledStrip["enabled"] = config.ledStrip.stripEnabled;
  ledStrip["max_brightness"] = config.ledStrip.maxBrightness;
  ledStrip["auto_on"] = config.ledStrip.autoOn;

  JsonObject ledMatrix = hardware.createNestedObject("led_matrix");
  ledMatrix["max_brightness"] = config.ledMatrix.maxBrightness;
  ledMatrix["default_effect"] = config.ledMatrix.defaultEffect;
  ledMatrix["enabled"] = config.ledMatrix.matrixEnabled;
  ledMatrix["frame_rate"] = config.ledMatrix.frameRate;
  ledMatrix["auto_effects"] = config.ledMatrix.autoEffects;
  ledMatrix["effect_interval"] = config.ledMatrix.effectChangeInterval;

  // System
  JsonObject system = doc.createNestedObject("system");
  system["debug"] = config.system.debugEnabled;
  system["baud_rate"] = config.system.serialBaudRate;
  system["watchdog"] = config.system.watchdogEnabled;
  system["watchdog_timeout"] = config.system.watchdogTimeout;
  system["failsafe"] = config.system.failsafeEnabled;
  system["health_interval"] = config.system.healthCheckInterval;

  // OTA
  JsonObject ota = doc.createNestedObject("ota");
  ota["auto_update"] = config.ota.autoUpdate;
  ota["check_interval"] = config.ota.checkInterval;
  ota["server"] = config.ota.updateServer;
  ota["allow_beta"] = config.ota.allowBeta;
  ota["require_confirmation"] = config.ota.requireConfirmation;

  // Preferences
  JsonObject prefs = doc.createNestedObject("preferences");
  prefs["default_volume"] = config.preferences.defaultVolume;
  prefs["lights_on_boot"] = config.preferences.lightsOnBoot;
  prefs["default_brightness"] = config.preferences.defaultBrightness;
  prefs["mute_on_boot"] = config.preferences.muteOnBoot;
  prefs["remember_state"] = config.preferences.rememberLastState;

  String result;
  serializeJsonPretty(doc, result);
  return result;
}

bool ConfigManager::setConfigFromJSON(const String &json) {
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    DEBUG_PRINTF("[CONFIG] JSON parsing failed: %s\n", error.c_str());
    return false;
  }

  // Create temporary config to validate before applying
  SystemConfig tempConfig = config;

  // Parse JSON into config structure
  if (doc.containsKey("wifi")) {
    JsonObject wifi = doc["wifi"];
    if (wifi.containsKey("ssid")) {
      strncpy(tempConfig.wifiSSID, wifi["ssid"], CONFIG_MAX_STRING_LENGTH - 1);
    }
    if (wifi.containsKey("password")) {
      strncpy(tempConfig.wifiPassword, wifi["password"],
              CONFIG_MAX_STRING_LENGTH - 1);
    }
    if (wifi.containsKey("auto_connect")) {
      tempConfig.wifiAutoConnect = wifi["auto_connect"];
    }
    if (wifi.containsKey("timeout")) {
      tempConfig.wifiTimeout = wifi["timeout"];
    }
  }

  if (doc.containsKey("mqtt")) {
    JsonObject mqtt = doc["mqtt"];
    if (mqtt.containsKey("server")) {
      strncpy(tempConfig.mqttServer, mqtt["server"],
              CONFIG_MAX_STRING_LENGTH - 1);
    }
    if (mqtt.containsKey("port")) {
      tempConfig.mqttPort = mqtt["port"];
    }
    if (mqtt.containsKey("user")) {
      strncpy(tempConfig.mqttUser, mqtt["user"], CONFIG_MAX_STRING_LENGTH - 1);
    }
    if (mqtt.containsKey("password")) {
      strncpy(tempConfig.mqttPassword, mqtt["password"],
              CONFIG_MAX_STRING_LENGTH - 1);
    }
  }

  // Validate the temporary config
  tempConfig.magicNumber = CONFIG_MAGIC_NUMBER;
  tempConfig.version = CONFIG_VERSION;
  tempConfig.checksum = calculateChecksum(tempConfig);

  if (validateConfig(tempConfig)) {
    config = tempConfig;
    markChanged();
    return true;
  }

  return false;
}

void ConfigManager::printConfig() const {
  DEBUG_PRINTLN("[CONFIG] Current Configuration:");
  DEBUG_PRINTF("  Version: %d\n", config.version);
  DEBUG_PRINTF("  WiFi SSID: %s\n", config.wifiSSID);
  DEBUG_PRINTF("  MQTT Server: %s:%d\n", config.mqttServer, config.mqttPort);
  DEBUG_PRINTF("  Motor Enabled: %s\n",
               config.motor.motorEnabled ? "Yes" : "No");
  DEBUG_PRINTF("  Touch Enabled: %s\n",
               config.touchScreen.touchEnabled ? "Yes" : "No");
  DEBUG_PRINTF("  IR Enabled: %s\n", config.infrared.irEnabled ? "Yes" : "No");
  DEBUG_PRINTF("  Debug Enabled: %s\n",
               config.system.debugEnabled ? "Yes" : "No");
}

// ========================================
// GLOBAL FUNCTIONS
// ========================================
void initConfigManager() {
  if (!configManager.begin()) {
    DEBUG_PRINTLN("[CONFIG] Failed to initialize configuration manager");
    return;
  }

  if (!configManager.load()) {
    DEBUG_PRINTLN("[CONFIG] Failed to load configuration");
    return;
  }

  DEBUG_PRINTLN("[CONFIG] Configuration manager initialized successfully");
  configManager.printConfig();
}

bool loadSystemConfig() { return configManager.load(); }

bool saveSystemConfig() { return configManager.save(); }

void resetToDefaults() { configManager.reset(); }

void handleConfigurationCommands(const String &command, const String &payload) {
  DEBUG_PRINTF("[CONFIG] Handling command: %s\n", command.c_str());

  if (command == "get_config") {
    // Publish current configuration
    publishConfigurationStatus();
  } else if (command == "set_wifi") {
    // Set WiFi credentials from JSON payload
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
      String ssid = doc["ssid"].as<String>();
      String password = doc["password"].as<String>();
      configManager.setWiFiCredentials(ssid, password);
      configManager.save();
    }
  } else if (command == "set_mqtt") {
    // Set MQTT configuration from JSON payload
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
      String server = doc["server"].as<String>();
      int port = doc["port"].as<int>();
      configManager.setMQTTServer(server, port);

      if (doc.containsKey("user")) {
        String user = doc["user"].as<String>();
        String pass = doc["password"].as<String>();
        configManager.setMQTTCredentials(user, pass);
      }
      configManager.save();
    }
  } else if (command == "reset_config") {
    // Reset to factory defaults
    configManager.reset();
  } else if (command == "backup_config") {
    // Create configuration backup
    configManager.backup();
  } else if (command == "restore_config") {
    // Restore from backup (use public wrapper)
    configManager.restore();
  } else if (command == "save_config") {
    // Force save current configuration
    configManager.save(true);
  } else if (command == "factory_reset") {
    // Perform factory reset with confirmation
    DEBUG_PRINTLN("[CONFIG] Factory reset requested");

    // Show warning on screen
    extern void showFactoryResetWarning();
    showFactoryResetWarning();

    // Wait for confirmation (this would be handled via another MQTT command)
    // For now, just log the request
  } else if (command == "confirm_factory_reset") {
    // Confirmed factory reset
    DEBUG_PRINTLN("[CONFIG] Factory reset confirmed - resetting to defaults");

    // Clear all stored configuration
    SPIFFS.remove(CONFIG_FILE_PATH);
    SPIFFS.remove(CONFIG_BACKUP_PATH);

    // Clear EEPROM
    for (int i = 0; i < CONFIG_EEPROM_SIZE; i++) {
      EEPROM.write(i, 0xFF);
    }
    EEPROM.commit();

    // Reset to defaults and save
    configManager.reset();

    // Restart system
    delay(1000);
    ESP.restart();
  } else if (command == "export_config") {
    // Export configuration as JSON
    String configJson = configManager.getConfigJSON();
    mqttClient.publish("homecontrol/config/export", configJson.c_str(), false);
  } else if (command == "import_config") {
    // Import configuration from JSON payload
    if (configManager.setConfigFromJSON(payload)) {
      configManager.save(true);
      DEBUG_PRINTLN("[CONFIG] Configuration imported successfully");
    } else {
      DEBUG_PRINTLN("[CONFIG] Failed to import configuration");
    }
  }
}

void publishConfigurationStatus() {
  if (!mqttClient.connected())
    return;

  String configJson = configManager.getConfigJSON();
  mqttClient.publish("homecontrol/config/status", configJson.c_str(), true);

  // Also publish a summary
  DynamicJsonDocument summary(512);
  const SystemConfig &cfg = configManager.getConfig();

  summary["loaded"] = configManager.isLoaded();
  summary["changed"] = configManager.hasChanged();
  summary["version"] = cfg.version;
  summary["wifi_configured"] = strlen(cfg.wifiSSID) > 0;
  summary["mqtt_configured"] = strlen(cfg.mqttServer) > 0;
  summary["motor_enabled"] = cfg.motor.motorEnabled;
  summary["touch_enabled"] = cfg.touchScreen.touchEnabled;
  summary["ir_enabled"] = cfg.infrared.irEnabled;

  String summaryJson;
  serializeJson(summary, summaryJson);
  mqttClient.publish("homecontrol/config/summary", summaryJson.c_str(), true);
}

// Additional configuration methods
void ConfigManager::setMotorConfig(int pwmFreq, int pwmRes, int deadband,
                                   bool enabled) {
  if (VALIDATE_RANGE(pwmFreq, 100, 20000)) {
    config.motor.motorPWMFrequency = pwmFreq;
  }
  if (VALIDATE_RANGE(pwmRes, 8, 16)) {
    config.motor.motorPWMResolution = pwmRes;
  }
  if (VALIDATE_RANGE(deadband, 0, 100)) {
    config.motor.potDeadband = deadband;
  }
  config.motor.motorEnabled = enabled;
  markChanged();
}

void ConfigManager::setMotorCalibration(int minVal, int maxVal, bool reversed) {
  if (minVal < maxVal) {
    config.motor.motorCalibrationMin = minVal;
    config.motor.motorCalibrationMax = maxVal;
  }
  config.motor.motorReversed = reversed;
  markChanged();
}

void ConfigManager::setLEDStripConfig(int pwmFreq, int pwmRes, int maxBright,
                                      bool enabled) {
  if (VALIDATE_RANGE(pwmFreq, 100, 20000)) {
    config.ledStrip.pwmFrequency = pwmFreq;
  }
  if (VALIDATE_RANGE(pwmRes, 8, 16)) {
    config.ledStrip.pwmResolution = pwmRes;
  }
  if (VALIDATE_RANGE(maxBright, 0, 255)) {
    config.ledStrip.maxBrightness = maxBright;
  }
  config.ledStrip.stripEnabled = enabled;
  markChanged();
}

void ConfigManager::setLEDMatrixConfig(int maxBright, int defaultEffect,
                                       bool enabled) {
  if (VALIDATE_RANGE(maxBright, 0, 255)) {
    config.ledMatrix.maxBrightness = maxBright;
  }
  if (VALIDATE_RANGE(defaultEffect, 0, 15)) {
    config.ledMatrix.defaultEffect = defaultEffect;
  }
  config.ledMatrix.matrixEnabled = enabled;
  markChanged();
}

void ConfigManager::setTouchScreenConfig(bool enabled, int timeout,
                                         int dimBright) {
  config.touchScreen.touchEnabled = enabled;
  if (VALIDATE_TIMEOUT(timeout)) {
    config.touchScreen.screenTimeout = timeout;
  }
  if (VALIDATE_RANGE(dimBright, 0, 255)) {
    config.touchScreen.dimBrightness = dimBright;
  }
  markChanged();
}

void ConfigManager::setTouchCalibration(const uint16_t *calibData) {
  if (calibData) {
    memcpy(config.touchScreen.calibrationData, calibData,
           sizeof(config.touchScreen.calibrationData));
    config.touchScreen.calibrated = true;
    markChanged();
  }
}

void ConfigManager::setIRConfig(bool enabled, int pin, bool learningMode) {
  config.infrared.irEnabled = enabled;
  if (VALIDATE_RANGE(pin, 0, 39)) { // Valid GPIO pins for ESP32
    config.infrared.receiverPin = pin;
  }
  config.infrared.learningMode = learningMode;
  markChanged();
}

void ConfigManager::setSystemConfig(bool debug, int baudRate, bool watchdog) {
  config.system.debugEnabled = debug;
  if (VALIDATE_RANGE(baudRate, 9600, 921600)) {
    config.system.serialBaudRate = baudRate;
  }
  config.system.watchdogEnabled = watchdog;
  markChanged();
}

void ConfigManager::setOTAConfig(bool autoUpdate, int checkInterval,
                                 const String &server) {
  config.ota.autoUpdate = autoUpdate;
  if (VALIDATE_RANGE(checkInterval, 300000,
                     86400000)) { // 5 minutes to 24 hours
    config.ota.checkInterval = checkInterval;
  }
  strncpy(config.ota.updateServer, server.c_str(),
          CONFIG_MAX_STRING_LENGTH - 1);
  config.ota.updateServer[CONFIG_MAX_STRING_LENGTH - 1] = '\0';
  markChanged();
}

void ConfigManager::setUserPreferences(int volume, bool lightsOn,
                                       int brightness) {
  config.preferences.defaultVolume = constrain(volume, 0, 100);
  config.preferences.lightsOnBoot = lightsOn;
  config.preferences.defaultBrightness = constrain(brightness, 0, 255);
  markChanged();
}