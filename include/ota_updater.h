/**
 * OTA (Over-The-Air) Update Module
 * 
 * Implements secure over-the-air firmware update functionality with
 * progress indication, rollback capability, and version checking.
 */

#ifndef OTA_UPDATER_H
#define OTA_UPDATER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include "config.h"

// ========================================
// OTA CONFIGURATION
// ========================================
#define OTA_UPDATE_URL "https://your-server.com/firmware/"
#define OTA_VERSION_CHECK_URL "https://your-server.com/version.json"
#define OTA_BUFFER_SIZE 1024
#define OTA_TIMEOUT 30000
#define OTA_MAX_RETRIES 3
#define OTA_CHECK_INTERVAL 3600000  // Check for updates every hour

// ========================================
// OTA UPDATE STATES
// ========================================
enum OTAState {
    OTA_IDLE,
    OTA_CHECKING_VERSION,
    OTA_UPDATE_AVAILABLE,
    OTA_DOWNLOADING,
    OTA_INSTALLING,
    OTA_SUCCESS,
    OTA_FAILED,
    OTA_ROLLBACK_REQUIRED
};

// ========================================
// OTA UPDATE INFO STRUCTURE
// ========================================
struct OTAUpdateInfo {
    String version;
    String url;
    String checksum;
    size_t size;
    String releaseNotes;
    bool mandatory;
    String minVersion;
};

// ========================================
// OTA PROGRESS CALLBACK
// ========================================
typedef void (*OTAProgressCallback)(size_t progress, size_t total);
typedef void (*OTAStateCallback)(OTAState state, const String& message);

// ========================================
// OTA UPDATER CLASS
// ========================================
class OTAUpdater {
private:
    OTAState currentState;
    OTAUpdateInfo updateInfo;
    OTAProgressCallback progressCallback;
    OTAStateCallback stateCallback;
    unsigned long lastVersionCheck;
    int retryCount;
    bool autoUpdateEnabled;
    String currentVersion;
    
    // Private methods
    bool downloadFirmware(const String& url, const String& expectedChecksum);
    bool verifyChecksum(const String& expectedChecksum);
    bool checkVersionCompatibility(const String& newVersion, const String& minVersion);
    void setState(OTAState state, const String& message = "");
    void showProgressOnMatrix(int progress);
    void showProgressOnScreen(int progress, const String& message);
    void logOTAEvent(const String& event, const String& details = "");
    
public:
    OTAUpdater();
    
    // Initialization
    void begin(const String& currentVersion);
    void setProgressCallback(OTAProgressCallback callback);
    void setStateCallback(OTAStateCallback callback);
    void setAutoUpdate(bool enabled);
    
    // Version checking
    bool checkForUpdates();
    bool isUpdateAvailable();
    OTAUpdateInfo getUpdateInfo();
    
    // Update process
    bool startUpdate();
    bool startUpdate(const String& firmwareUrl, const String& checksum = "");
    void cancelUpdate();
    
    // Status and diagnostics
    OTAState getState();
    String getStateString();
    int getProgress();
    String getLastError();
    
    // Rollback functionality
    bool canRollback();
    bool rollbackToPrevious();
    void markCurrentVersionValid();
    
    // Periodic tasks
    void handle();
    
    // MQTT integration
    void handleMQTTCommand(const String& command, const String& payload);
    void publishOTAStatus();
};

// ========================================
// GLOBAL OTA UPDATER INSTANCE
// ========================================
extern OTAUpdater otaUpdater;

// ========================================
// FUNCTION DECLARATIONS
// ========================================
void initOTAUpdater();
void handleOTAUpdates();
void onOTAProgress(size_t progress, size_t total);
void onOTAStateChange(OTAState state, const String& message);

// ========================================
// OTA MQTT TOPICS
// ========================================
#define MQTT_TOPIC_OTA_STATUS "homecontrol/ota/status"
#define MQTT_TOPIC_OTA_COMMAND "homecontrol/ota/command"
#define MQTT_TOPIC_OTA_PROGRESS "homecontrol/ota/progress"

#endif // OTA_UPDATER_H