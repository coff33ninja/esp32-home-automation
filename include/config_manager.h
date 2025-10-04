/**
 * Configuration Manager Module
 * 
 * Manages persistent storage and retrieval of system configuration settings.
 * Supports EEPROM and SPIFFS storage with validation and backup functionality.
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <stdint.h>
#include <EEPROM.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "config.h"

// ========================================
// CONFIGURATION CONSTANTS
// ========================================
#define CONFIG_VERSION 1
#define CONFIG_MAGIC_NUMBER 0xCAFEBABE
#define CONFIG_EEPROM_SIZE 4096
#define CONFIG_EEPROM_START 0
#define CONFIG_FILE_PATH "/config.json"
#define CONFIG_BACKUP_PATH "/config_backup.json"
#define CONFIG_MAX_STRING_LENGTH 64

// ========================================
// CONFIGURATION STRUCTURE
// ========================================
struct SystemConfig {
    // Header information
    uint32_t magicNumber;
    uint16_t version;
    uint16_t checksum;
    
    // WiFi Configuration
    char wifiSSID[CONFIG_MAX_STRING_LENGTH];
    char wifiPassword[CONFIG_MAX_STRING_LENGTH];
    bool wifiAutoConnect;
    int wifiTimeout;
    
    // MQTT Configuration
    char mqttServer[CONFIG_MAX_STRING_LENGTH];
    int mqttPort;
    char mqttUser[CONFIG_MAX_STRING_LENGTH];
    char mqttPassword[CONFIG_MAX_STRING_LENGTH];
    char mqttClientID[CONFIG_MAX_STRING_LENGTH];
    bool mqttAutoConnect;
    int mqttReconnectDelay;
    
    // Hardware Configuration
    struct {
        int motorPWMFrequency;
        int motorPWMResolution;
        int potDeadband;
        bool motorEnabled;
        bool motorReversed;
        int motorCalibrationMin;
        int motorCalibrationMax;
    } motor;
    
    struct {
        int pwmFrequency;
        int pwmResolution;
        bool stripEnabled;
        int maxBrightness;
        bool autoOn;
    } ledStrip;
    
    struct {
        int maxBrightness;
        int defaultEffect;
        bool matrixEnabled;
        int frameRate;
        bool autoEffects;
        int effectChangeInterval;
    } ledMatrix;
    
    struct {
        bool touchEnabled;
        uint16_t calibrationData[8];
        bool calibrated;
        int screenTimeout;
        int dimBrightness;
        bool autoWake;
    } touchScreen;
    
    struct {
        bool irEnabled;
        int receiverPin;
        bool learningMode;
        uint32_t learnedCodes[16];
        int codeCount;
    } infrared;
    
    // System Configuration
    struct {
        bool debugEnabled;
        int serialBaudRate;
        bool watchdogEnabled;
        int watchdogTimeout;
        bool failsafeEnabled;
        int healthCheckInterval;
    } system;
    
    // OTA Configuration
    struct {
        bool autoUpdate;
        int checkInterval;
        char updateServer[CONFIG_MAX_STRING_LENGTH];
        bool allowBeta;
        bool requireConfirmation;
    } ota;
    
    // User Preferences
    struct {
        int defaultVolume;
        bool lightsOnBoot;
        int defaultBrightness;
        bool muteOnBoot;
        bool rememberLastState;
    } preferences;
};

// ========================================
// CONFIGURATION MANAGER CLASS
// ========================================
class ConfigManager {
private:
    SystemConfig config;
    bool configLoaded;
    bool configChanged;
    unsigned long lastSave;
    
    // Private methods
    uint16_t calculateChecksum(const SystemConfig& cfg);
    bool validateConfig(const SystemConfig& cfg);
    void setDefaults();
    bool loadFromEEPROM();
    bool saveToEEPROM();
    bool loadFromSPIFFS();
    bool saveToSPIFFS();
    bool createBackup();
    bool restoreFromBackup();
    
public:
    ConfigManager();
    
    // Initialization and management
    bool begin();
    bool load();
    bool save(bool force = false);
    bool reset();
    bool backup();
    bool restore();
    // Public wrapper that forces a restore from backup (delegates to private method)
    bool restoreFromBackupPublic() { return restoreFromBackup(); }
    
    // Configuration access
    SystemConfig& getConfig();
    const SystemConfig& getConfig() const;
    bool isLoaded() const;
    bool hasChanged() const;
    void markChanged();
    
    // WiFi configuration
    void setWiFiCredentials(const String& ssid, const String& password);
    void setWiFiAutoConnect(bool enabled);
    void setWiFiTimeout(int timeout);
    
    // MQTT configuration
    void setMQTTServer(const String& server, int port);
    void setMQTTCredentials(const String& user, const String& password);
    void setMQTTClientID(const String& clientID);
    void setMQTTAutoConnect(bool enabled);
    
    // Hardware configuration
    void setMotorConfig(int pwmFreq, int pwmRes, int deadband, bool enabled);
    void setMotorCalibration(int minVal, int maxVal, bool reversed);
    void setLEDStripConfig(int pwmFreq, int pwmRes, int maxBright, bool enabled);
    void setLEDMatrixConfig(int maxBright, int defaultEffect, bool enabled);
    void setTouchScreenConfig(bool enabled, int timeout, int dimBright);
    void setTouchCalibration(const uint16_t* calibData);
    void setIRConfig(bool enabled, int pin, bool learningMode);
    
    // System configuration
    void setSystemConfig(bool debug, int baudRate, bool watchdog);
    void setOTAConfig(bool autoUpdate, int checkInterval, const String& server);
    void setUserPreferences(int volume, bool lightsOn, int brightness);
    
    // Utility functions
    String getConfigJSON() const;
    bool setConfigFromJSON(const String& json);
    void printConfig() const;
    size_t getConfigSize() const;
    String getConfigHash() const;
    
    // Auto-save functionality
    void enableAutoSave(int intervalMs = 30000);
    void disableAutoSave();
    void handleAutoSave();
};

// ========================================
// GLOBAL CONFIGURATION MANAGER
// ========================================
extern ConfigManager configManager;

// ========================================
// FUNCTION DECLARATIONS
// ========================================
void initConfigManager();
bool loadSystemConfig();
bool saveSystemConfig();
void resetToDefaults();
void handleConfigurationCommands(const String& command, const String& payload);
void publishConfigurationStatus();

// ========================================
// CONFIGURATION VALIDATION MACROS
// ========================================
#define VALIDATE_RANGE(value, min, max) ((value) >= (min) && (value) <= (max))
#define VALIDATE_STRING(str) (strlen(str) > 0 && strlen(str) < CONFIG_MAX_STRING_LENGTH)
#define VALIDATE_PORT(port) VALIDATE_RANGE(port, 1, 65535)
#define VALIDATE_TIMEOUT(timeout) VALIDATE_RANGE(timeout, 1000, 300000)

#endif // CONFIG_MANAGER_H