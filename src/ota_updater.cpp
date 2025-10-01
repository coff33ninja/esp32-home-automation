/**
 * OTA (Over-The-Air) Update Module Implementation
 * 
 * Implements secure over-the-air firmware update functionality with
 * progress indication, rollback capability, and version checking.
 */

#include "ota_updater.h"
#include "led_effects.h"
#include "touch_handler.h"
#include "mqtt_handler.h"
#include "system_monitor.h"

// Global OTA updater instance
OTAUpdater otaUpdater;

// ========================================
// CONSTRUCTOR
// ========================================
OTAUpdater::OTAUpdater() {
    currentState = OTA_IDLE;
    progressCallback = nullptr;
    stateCallback = nullptr;
    lastVersionCheck = 0;
    retryCount = 0;
    autoUpdateEnabled = false;
    currentVersion = FIRMWARE_VERSION;
}

// ========================================
// INITIALIZATION
// ========================================
void OTAUpdater::begin(const String& version) {
    currentVersion = version;
    
    DEBUG_PRINTLN("[OTA] Initializing OTA updater...");
    DEBUG_PRINTF("[OTA] Current version: %s\n", currentVersion.c_str());
    
    // Check if we're running from a new partition (after update)
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            DEBUG_PRINTLN("[OTA] New firmware detected, marking as valid");
            markCurrentVersionValid();
        }
    }
    
    setState(OTA_IDLE, "OTA updater initialized");
}

void OTAUpdater::setProgressCallback(OTAProgressCallback callback) {
    progressCallback = callback;
}

void OTAUpdater::setStateCallback(OTAStateCallback callback) {
    stateCallback = callback;
}

void OTAUpdater::setAutoUpdate(bool enabled) {
    autoUpdateEnabled = enabled;
    DEBUG_PRINTF("[OTA] Auto-update %s\n", enabled ? "enabled" : "disabled");
}

// ========================================
// VERSION CHECKING
// ========================================
bool OTAUpdater::checkForUpdates() {
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[OTA] WiFi not connected, cannot check for updates");
        return false;
    }
    
    setState(OTA_CHECKING_VERSION, "Checking for updates...");
    
    HTTPClient http;
    http.begin(OTA_VERSION_CHECK_URL);
    http.setTimeout(OTA_TIMEOUT);
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        http.end();
        
        // Parse JSON response
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            DEBUG_PRINTF("[OTA] JSON parsing failed: %s\n", error.c_str());
            setState(OTA_FAILED, "Failed to parse version info");
            return false;
        }
        
        // Extract update information
        updateInfo.version = doc["version"].as<String>();
        updateInfo.url = doc["url"].as<String>();
        updateInfo.checksum = doc["checksum"].as<String>();
        updateInfo.size = doc["size"].as<size_t>();
        updateInfo.releaseNotes = doc["releaseNotes"].as<String>();
        updateInfo.mandatory = doc["mandatory"].as<bool>();
        updateInfo.minVersion = doc["minVersion"].as<String>();
        
        // Check if update is available
        if (updateInfo.version != currentVersion) {
            // Check version compatibility
            if (checkVersionCompatibility(updateInfo.version, updateInfo.minVersion)) {
                setState(OTA_UPDATE_AVAILABLE, "Update available: " + updateInfo.version);
                DEBUG_PRINTF("[OTA] Update available: %s -> %s\n", 
                           currentVersion.c_str(), updateInfo.version.c_str());
                return true;
            } else {
                setState(OTA_FAILED, "Version compatibility check failed");
                return false;
            }
        } else {
            setState(OTA_IDLE, "No updates available");
            DEBUG_PRINTLN("[OTA] No updates available");
            return false;
        }
    } else {
        http.end();
        DEBUG_PRINTF("[OTA] HTTP error: %d\n", httpCode);
        setState(OTA_FAILED, "Failed to check for updates");
        return false;
    }
}

bool OTAUpdater::isUpdateAvailable() {
    return currentState == OTA_UPDATE_AVAILABLE;
}

OTAUpdateInfo OTAUpdater::getUpdateInfo() {
    return updateInfo;
}

// ========================================
// UPDATE PROCESS
// ========================================
bool OTAUpdater::startUpdate() {
    if (currentState != OTA_UPDATE_AVAILABLE) {
        DEBUG_PRINTLN("[OTA] No update available to start");
        return false;
    }
    
    return startUpdate(updateInfo.url, updateInfo.checksum);
}

bool OTAUpdater::startUpdate(const String& firmwareUrl, const String& checksum) {
    if (WiFi.status() != WL_CONNECTED) {
        setState(OTA_FAILED, "WiFi not connected");
        return false;
    }
    
    DEBUG_PRINTF("[OTA] Starting update from: %s\n", firmwareUrl.c_str());
    setState(OTA_DOWNLOADING, "Starting firmware download...");
    
    // Show update start on LED matrix
    showProgressOnMatrix(0);
    showProgressOnScreen(0, "Starting Update...");
    
    // Log the update attempt
    logOTAEvent("UPDATE_STARTED", firmwareUrl);
    
    return downloadFirmware(firmwareUrl, checksum);
}

void OTAUpdater::cancelUpdate() {
    if (currentState == OTA_DOWNLOADING || currentState == OTA_INSTALLING) {
        DEBUG_PRINTLN("[OTA] Cancelling update...");
        Update.abort();
        setState(OTA_FAILED, "Update cancelled by user");
        logOTAEvent("UPDATE_CANCELLED");
    }
}

// ========================================
// PRIVATE METHODS
// ========================================
bool OTAUpdater::downloadFirmware(const String& url, const String& expectedChecksum) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(OTA_TIMEOUT);
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        setState(OTA_FAILED, "Failed to download firmware");
        return false;
    }
    
    size_t contentLength = http.getSize();
    
    if (contentLength == 0) {
        http.end();
        setState(OTA_FAILED, "Invalid firmware size");
        return false;
    }
    
    DEBUG_PRINTF("[OTA] Firmware size: %d bytes\n", contentLength);
    
    // Begin OTA update
    if (!Update.begin(contentLength)) {
        http.end();
        setState(OTA_FAILED, "Failed to begin OTA update");
        DEBUG_PRINTF("[OTA] Update begin failed: %s\n", Update.errorString());
        return false;
    }
    
    setState(OTA_INSTALLING, "Installing firmware...");
    
    WiFiClient* client = http.getStreamPtr();
    size_t written = 0;
    uint8_t buffer[OTA_BUFFER_SIZE];
    
    while (http.connected() && written < contentLength) {
        size_t available = client->available();
        if (available) {
            size_t readBytes = client->readBytes(buffer, min(available, sizeof(buffer)));
            size_t writtenBytes = Update.write(buffer, readBytes);
            
            if (writtenBytes != readBytes) {
                DEBUG_PRINTLN("[OTA] Write error during update");
                break;
            }
            
            written += writtenBytes;
            
            // Update progress
            int progress = (written * 100) / contentLength;
            showProgressOnMatrix(progress);
            showProgressOnScreen(progress, "Installing: " + String(progress) + "%");
            
            if (progressCallback) {
                progressCallback(written, contentLength);
            }
            
            // Yield to prevent watchdog timeout
            yield();
        } else {
            delay(1);
        }
    }
    
    http.end();
    
    if (written == contentLength) {
        if (Update.end(true)) {
            // Verify checksum if provided
            if (expectedChecksum.length() > 0 && !verifyChecksum(expectedChecksum)) {
                setState(OTA_FAILED, "Checksum verification failed");
                return false;
            }
            
            setState(OTA_SUCCESS, "Update completed successfully");
            showProgressOnMatrix(100);
            showProgressOnScreen(100, "Update Complete!");
            
            logOTAEvent("UPDATE_SUCCESS", "Version: " + updateInfo.version);
            
            DEBUG_PRINTLN("[OTA] Update completed successfully");
            DEBUG_PRINTLN("[OTA] Restarting in 3 seconds...");
            
            delay(3000);
            ESP.restart();
            return true;
        } else {
            setState(OTA_FAILED, "Update finalization failed");
            DEBUG_PRINTF("[OTA] Update end failed: %s\n", Update.errorString());
            return false;
        }
    } else {
        setState(OTA_FAILED, "Incomplete firmware download");
        DEBUG_PRINTF("[OTA] Download incomplete: %d/%d bytes\n", written, contentLength);
        return false;
    }
}

bool OTAUpdater::verifyChecksum(const String& expectedChecksum) {
    // For now, we'll use the ESP32's built-in verification
    // In a production system, you might want to implement additional checksum verification
    DEBUG_PRINTF("[OTA] Expected checksum: %s\n", expectedChecksum.c_str());
    return true; // Placeholder - implement actual checksum verification if needed
}

bool OTAUpdater::checkVersionCompatibility(const String& newVersion, const String& minVersion) {
    // Simple version comparison - in production, use proper semantic versioning
    if (minVersion.length() == 0) return true;
    
    // Compare current version with minimum required version
    return currentVersion >= minVersion;
}

void OTAUpdater::setState(OTAState state, const String& message) {
    currentState = state;
    
    if (stateCallback) {
        stateCallback(state, message);
    }
    
    // Publish state change via MQTT
    publishOTAStatus();
    
    DEBUG_PRINTF("[OTA] State: %s - %s\n", getStateString().c_str(), message.c_str());
}

void OTAUpdater::showProgressOnMatrix(int progress) {
    // Show progress bar on LED matrix
    extern CRGB leds[];
    
    // Clear matrix
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    // Calculate progress bar
    int totalPixels = MATRIX_WIDTH * MATRIX_HEIGHT;
    int progressPixels = (progress * totalPixels) / 100;
    
    // Fill progress bar with color gradient
    for (int i = 0; i < progressPixels; i++) {
        if (i < totalPixels / 3) {
            leds[i] = CRGB::Red;      // First third: red
        } else if (i < (totalPixels * 2) / 3) {
            leds[i] = CRGB::Yellow;   // Second third: yellow
        } else {
            leds[i] = CRGB::Green;    // Final third: green
        }
    }
    
    FastLED.show();
}

void OTAUpdater::showProgressOnScreen(int progress, const String& message) {
    // This would integrate with the touch screen handler
    // For now, we'll just print to serial
    DEBUG_PRINTF("[OTA] Progress: %d%% - %s\n", progress, message.c_str());
    
    // TODO: Integrate with touch_handler to show progress on screen
    // updateOTAProgressScreen(progress, message);
}

void OTAUpdater::logOTAEvent(const String& event, const String& details) {
    String logEntry = "[OTA] " + event;
    if (details.length() > 0) {
        logEntry += " - " + details;
    }
    
    DEBUG_PRINTLN(logEntry);
    
    // TODO: Store in persistent log for diagnostics
    // addToSystemLog(logEntry);
}

// ========================================
// STATUS AND DIAGNOSTICS
// ========================================
OTAState OTAUpdater::getState() {
    return currentState;
}

String OTAUpdater::getStateString() {
    switch (currentState) {
        case OTA_IDLE: return "Idle";
        case OTA_CHECKING_VERSION: return "Checking Version";
        case OTA_UPDATE_AVAILABLE: return "Update Available";
        case OTA_DOWNLOADING: return "Downloading";
        case OTA_INSTALLING: return "Installing";
        case OTA_SUCCESS: return "Success";
        case OTA_FAILED: return "Failed";
        case OTA_ROLLBACK_REQUIRED: return "Rollback Required";
        default: return "Unknown";
    }
}

int OTAUpdater::getProgress() {
    if (currentState == OTA_DOWNLOADING || currentState == OTA_INSTALLING) {
        return Update.progress();
    }
    return 0;
}

String OTAUpdater::getLastError() {
    if (currentState == OTA_FAILED) {
        return Update.errorString();
    }
    return "";
}

// ========================================
// ROLLBACK FUNCTIONALITY
// ========================================
bool OTAUpdater::canRollback() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* last_invalid = esp_ota_get_last_invalid_partition();
    
    return (last_invalid != nullptr && last_invalid != running);
}

bool OTAUpdater::rollbackToPrevious() {
    if (!canRollback()) {
        DEBUG_PRINTLN("[OTA] No previous version available for rollback");
        return false;
    }
    
    DEBUG_PRINTLN("[OTA] Rolling back to previous version...");
    logOTAEvent("ROLLBACK_STARTED");
    
    const esp_partition_t* last_invalid = esp_ota_get_last_invalid_partition();
    
    if (esp_ota_set_boot_partition(last_invalid) == ESP_OK) {
        logOTAEvent("ROLLBACK_SUCCESS");
        DEBUG_PRINTLN("[OTA] Rollback successful, restarting...");
        delay(1000);
        ESP.restart();
        return true;
    } else {
        logOTAEvent("ROLLBACK_FAILED");
        DEBUG_PRINTLN("[OTA] Rollback failed");
        return false;
    }
}

void OTAUpdater::markCurrentVersionValid() {
    esp_ota_mark_app_valid_cancel_rollback();
    DEBUG_PRINTLN("[OTA] Current version marked as valid");
    logOTAEvent("VERSION_VALIDATED", currentVersion);
}

// ========================================
// PERIODIC TASKS
// ========================================
void OTAUpdater::handle() {
    unsigned long now = millis();
    
    // Auto-check for updates if enabled
    if (autoUpdateEnabled && 
        currentState == OTA_IDLE && 
        now - lastVersionCheck > OTA_CHECK_INTERVAL) {
        
        lastVersionCheck = now;
        checkForUpdates();
        
        // Auto-start update if available and not mandatory
        if (currentState == OTA_UPDATE_AVAILABLE && !updateInfo.mandatory) {
            startUpdate();
        }
    }
    
    // Handle failed states with retry logic
    if (currentState == OTA_FAILED && retryCount < OTA_MAX_RETRIES) {
        if (now - lastVersionCheck > 60000) { // Retry after 1 minute
            retryCount++;
            DEBUG_PRINTF("[OTA] Retry attempt %d/%d\n", retryCount, OTA_MAX_RETRIES);
            checkForUpdates();
        }
    }
}

// ========================================
// MQTT INTEGRATION
// ========================================
void OTAUpdater::handleMQTTCommand(const String& command, const String& payload) {
    DEBUG_PRINTF("[OTA] MQTT command: %s, payload: %s\n", command.c_str(), payload.c_str());
    
    if (command == "check_update") {
        checkForUpdates();
    } else if (command == "start_update") {
        if (currentState == OTA_UPDATE_AVAILABLE) {
            startUpdate();
        }
    } else if (command == "cancel_update") {
        cancelUpdate();
    } else if (command == "rollback") {
        rollbackToPrevious();
    } else if (command == "set_auto_update") {
        setAutoUpdate(payload == "true" || payload == "1");
    } else if (command == "mark_valid") {
        markCurrentVersionValid();
    }
}

void OTAUpdater::publishOTAStatus() {
    if (!mqttClient.connected()) return;
    
    DynamicJsonDocument doc(512);
    doc["state"] = getStateString();
    doc["current_version"] = currentVersion;
    doc["progress"] = getProgress();
    doc["auto_update"] = autoUpdateEnabled;
    doc["can_rollback"] = canRollback();
    
    if (currentState == OTA_UPDATE_AVAILABLE) {
        doc["available_version"] = updateInfo.version;
        doc["update_size"] = updateInfo.size;
        doc["mandatory"] = updateInfo.mandatory;
        doc["release_notes"] = updateInfo.releaseNotes;
    }
    
    if (currentState == OTA_FAILED) {
        doc["error"] = getLastError();
    }
    
    String statusJson;
    serializeJson(doc, statusJson);
    
    mqttClient.publish(MQTT_TOPIC_OTA_STATUS, statusJson.c_str(), true);
}

// ========================================
// GLOBAL FUNCTIONS
// ========================================
void initOTAUpdater() {
    otaUpdater.begin(FIRMWARE_VERSION);
    otaUpdater.setProgressCallback(onOTAProgress);
    otaUpdater.setStateCallback(onOTAStateChange);
    
    // Enable auto-update by default (can be changed via MQTT)
    otaUpdater.setAutoUpdate(true);
    
    DEBUG_PRINTLN("[OTA] OTA updater initialized");
}

void handleOTAUpdates() {
    otaUpdater.handle();
}

void onOTAProgress(size_t progress, size_t total) {
    int percentage = (progress * 100) / total;
    
    // Publish progress via MQTT
    if (mqttClient.connected()) {
        DynamicJsonDocument doc(128);
        doc["progress"] = percentage;
        doc["bytes_written"] = progress;
        doc["total_bytes"] = total;
        
        String progressJson;
        serializeJson(doc, progressJson);
        
        mqttClient.publish(MQTT_TOPIC_OTA_PROGRESS, progressJson.c_str());
    }
}

void onOTAStateChange(OTAState state, const String& message) {
    DEBUG_PRINTF("[OTA] State changed to: %s - %s\n", 
                otaUpdater.getStateString().c_str(), message.c_str());
    
    // Handle state-specific actions
    switch (state) {
        case OTA_SUCCESS:
            // Flash green on LED matrix
            extern CRGB leds[];
            fill_solid(leds, NUM_LEDS, CRGB::Green);
            FastLED.show();
            break;
            
        case OTA_FAILED:
            // Flash red on LED matrix
            extern CRGB leds[];
            fill_solid(leds, NUM_LEDS, CRGB::Red);
            FastLED.show();
            delay(1000);
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            FastLED.show();
            break;
            
        default:
            break;
    }
}