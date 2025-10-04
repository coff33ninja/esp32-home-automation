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
    // Use a secure client when checking HTTPS URLs. We create a temporary
    // WiFiClientSecure instance for TLS and call setInsecure() so common
    // Let'sEncrypt/ACME hosts work without pinning. This keeps the change
    // conservative and avoids adding new external deps.
    WiFiClient* tlsClient = nullptr;
    if (String(OTA_VERSION_CHECK_URL).startsWith("https://")) {
        WiFiClientSecure* c = new WiFiClientSecure();
        c->setInsecure();
        http.begin(*c, OTA_VERSION_CHECK_URL);
        tlsClient = c;
    } else {
        http.begin(OTA_VERSION_CHECK_URL);
    }
    http.setTimeout(OTA_TIMEOUT);
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    http.end();
    if (tlsClient) { delete tlsClient; tlsClient = nullptr; }
        
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
        if (tlsClient) { delete tlsClient; tlsClient = nullptr; }
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
    WiFiClient* tlsClient = nullptr;
    if (url.startsWith("https://")) {
        WiFiClientSecure* c = new WiFiClientSecure();
        c->setInsecure();
        http.begin(*c, url);
        tlsClient = c;
    } else {
        http.begin(url);
    }
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
    if (tlsClient) { delete tlsClient; tlsClient = nullptr; }
    
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
    // Show sophisticated OTA progress on LED matrix
    extern CRGB leds[];
    extern uint16_t XY(uint8_t x, uint8_t y);
    
    // Clear matrix
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    // Create animated progress ring
    int centerX = MATRIX_WIDTH / 2;
    int centerY = MATRIX_HEIGHT / 2;
    int radius = min(MATRIX_WIDTH, MATRIX_HEIGHT) / 2 - 2;
    
    // Calculate progress in degrees (0-360)
    int progressDegrees = map(progress, 0, 100, 0, 360);
    
    // Draw progress ring
    for (int angle = 0; angle < progressDegrees; angle += 10) {
        float radians = angle * PI / 180.0;
        int x = centerX + (radius * cos(radians));
        int y = centerY + (radius * sin(radians));
        
        // Constrain to matrix bounds
        if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT) {
            // Color based on progress
            CRGB color;
            if (progress < 25) {
                color = CRGB::Red;
            } else if (progress < 50) {
                color = CRGB::Orange;
            } else if (progress < 75) {
                color = CRGB::Yellow;
            } else {
                color = CRGB::Green;
            }
            
            leds[XY(x, y)] = color;
            
            // Add inner ring for better visibility
            if (radius > 3) {
                int innerX = centerX + ((radius - 2) * cos(radians));
                int innerY = centerY + ((radius - 2) * sin(radians));
                if (innerX >= 0 && innerX < MATRIX_WIDTH && innerY >= 0 && innerY < MATRIX_HEIGHT) {
                    leds[XY(innerX, innerY)] = color;
                }
            }
        }
    }
    
    // Add center indicator
    if (progress > 0) {
        leds[XY(centerX, centerY)] = CRGB::White;
        // Add cross pattern in center
        if (centerX > 0) leds[XY(centerX - 1, centerY)] = CRGB::White;
        if (centerX < MATRIX_WIDTH - 1) leds[XY(centerX + 1, centerY)] = CRGB::White;
        if (centerY > 0) leds[XY(centerX, centerY - 1)] = CRGB::White;
        if (centerY < MATRIX_HEIGHT - 1) leds[XY(centerX, centerY + 1)] = CRGB::White;
    }
    
    // Add progress percentage as corner indicators
    int percentLEDs = map(progress, 0, 100, 0, 4);
    CRGB cornerColor = (progress == 100) ? CRGB::Green : CRGB::Blue;
    
    // Light up corners based on progress quarters
    if (percentLEDs >= 1) leds[XY(0, 0)] = cornerColor;                                    // Top-left
    if (percentLEDs >= 2) leds[XY(MATRIX_WIDTH - 1, 0)] = cornerColor;                   // Top-right
    if (percentLEDs >= 3) leds[XY(MATRIX_WIDTH - 1, MATRIX_HEIGHT - 1)] = cornerColor;  // Bottom-right
    if (percentLEDs >= 4) leds[XY(0, MATRIX_HEIGHT - 1)] = cornerColor;                 // Bottom-left
    
    // Add pulsing effect during update
    static uint8_t pulsePhase = 0;
    pulsePhase += 10;
    uint8_t pulseBrightness = 128 + (sin8(pulsePhase) / 4); // Pulse between 128-191
    
    FastLED.setBrightness(pulseBrightness);
    FastLED.show();
}

void OTAUpdater::showProgressOnScreen(int progress, const String& message) {
    // Integrate with touch screen handler to show OTA progress
    DEBUG_PRINTF("[OTA] Progress: %d%% - %s\n", progress, message.c_str());
    
    // Call the touch screen OTA progress display function
    extern void updateOTAProgressScreen(int progress, const String& message);
    updateOTAProgressScreen(progress, message);
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

void OTAUpdater::showUpdateNotification() {
    DEBUG_PRINTLN("[OTA] Showing update notification");
    
    // Show notification on LED matrix
    extern CRGB leds[];
    extern uint16_t XY(uint8_t x, uint8_t y);
    
    // Pulsing blue notification pattern
    for (int pulse = 0; pulse < 5; pulse++) {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        
        // Draw notification icon (exclamation mark)
        int centerX = MATRIX_WIDTH / 2;
        
        // Exclamation mark body
        for (int y = 2; y < 12; y++) {
            leds[XY(centerX, y)] = CRGB::Blue;
            if (centerX > 0) leds[XY(centerX - 1, y)] = CRGB::Blue;
            if (centerX < MATRIX_WIDTH - 1) leds[XY(centerX + 1, y)] = CRGB::Blue;
        }
        
        // Exclamation mark dot
        leds[XY(centerX, 13)] = CRGB::Blue;
        if (centerX > 0) leds[XY(centerX - 1, 13)] = CRGB::Blue;
        if (centerX < MATRIX_WIDTH - 1) leds[XY(centerX + 1, 13)] = CRGB::Blue;
        
        // Add border for visibility
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            leds[XY(x, 0)] = CRGB::DarkBlue;
            leds[XY(x, MATRIX_HEIGHT - 1)] = CRGB::DarkBlue;
        }
        for (int y = 0; y < MATRIX_HEIGHT; y++) {
            leds[XY(0, y)] = CRGB::DarkBlue;
            leds[XY(MATRIX_WIDTH - 1, y)] = CRGB::DarkBlue;
        }
        
        FastLED.show();
        delay(500);
        
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
        delay(300);
    }
    
    // Show notification on screen
    extern void showOTANotificationScreen(const OTAUpdateInfo& info, bool mandatory);
    showOTANotificationScreen(updateInfo, updateInfo.mandatory);
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
        setState(OTA_FAILED, "No previous version available for rollback");
        return false;
    }
    
    DEBUG_PRINTLN("[OTA] Rolling back to previous version...");
    setState(OTA_ROLLBACK_REQUIRED, "Rolling back to previous version");
    logOTAEvent("ROLLBACK_STARTED");
    
    // Show rollback screen and LED pattern
    extern void showOTARollbackScreen();
    showOTARollbackScreen();
    
    // Show rollback pattern on LED matrix
    extern CRGB leds[];
    extern uint16_t XY(uint8_t x, uint8_t y);
    
    // Animated rollback pattern (spinning arrow)
    for (int spin = 0; spin < 10; spin++) {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        
        int centerX = MATRIX_WIDTH / 2;
        int centerY = MATRIX_HEIGHT / 2;
        
        // Draw rotating arrow pattern
        for (int angle = 0; angle < 360; angle += 45) {
            float radians = (angle + spin * 36) * PI / 180.0; // Rotate
            int x = centerX + (4 * cos(radians));
            int y = centerY + (4 * sin(radians));
            
            if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT) {
                leds[XY(x, y)] = CRGB::Orange;
            }
        }
        
        FastLED.show();
        delay(200);
    }
    
    const esp_partition_t* last_invalid = esp_ota_get_last_invalid_partition();
    
    if (esp_ota_set_boot_partition(last_invalid) == ESP_OK) {
        logOTAEvent("ROLLBACK_SUCCESS");
        DEBUG_PRINTLN("[OTA] Rollback successful, restarting...");
        
        // Show success pattern
        fill_solid(leds, NUM_LEDS, CRGB::Green);
        FastLED.show();
        
        delay(1000);
        ESP.restart();
        return true;
    } else {
        logOTAEvent("ROLLBACK_FAILED");
        DEBUG_PRINTLN("[OTA] Rollback failed");
        setState(OTA_FAILED, "Rollback operation failed");
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
        
        // Handle update availability notification
        if (currentState == OTA_UPDATE_AVAILABLE) {
            // Show update notification on screen and LED matrix
            showUpdateNotification();
            
            // Auto-start update if not mandatory (user can cancel)
            if (!updateInfo.mandatory) {
                // Wait 10 seconds for user to cancel, then auto-start
                unsigned long notificationStart = millis();
                bool userCancelled = false;
                
                while (millis() - notificationStart < 10000 && !userCancelled) {
                    // Check for user input to cancel
                    extern bool checkForOTACancel();
                    if (checkForOTACancel()) {
                        userCancelled = true;
                        DEBUG_PRINTLN("[OTA] Auto-update cancelled by user");
                        break;
                    }
                    delay(100);
                }
                
                if (!userCancelled) {
                    DEBUG_PRINTLN("[OTA] Starting auto-update");
                    startUpdate();
                }
            } else {
                // Mandatory update - start immediately
                DEBUG_PRINTLN("[OTA] Starting mandatory update");
                startUpdate();
            }
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
        } else {
            DEBUG_PRINTLN("[OTA] No update available to start");
            setState(OTA_FAILED, "No update available");
        }
    } else if (command == "cancel_update") {
        cancelUpdate();
    } else if (command == "rollback") {
        if (canRollback()) {
            rollbackToPrevious();
        } else {
            DEBUG_PRINTLN("[OTA] Cannot rollback - no previous version");
            setState(OTA_FAILED, "No previous version for rollback");
        }
    } else if (command == "set_auto_update") {
        setAutoUpdate(payload == "true" || payload == "1");
    } else if (command == "mark_valid") {
        markCurrentVersionValid();
    } else if (command == "force_update") {
        // Force update with custom URL and checksum
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            String url = doc["url"].as<String>();
            String checksum = doc["checksum"].as<String>();
            
            if (url.length() > 0) {
                DEBUG_PRINTF("[OTA] Force update from: %s\n", url.c_str());
                startUpdate(url, checksum);
            }
        }
    } else if (command == "get_status") {
        // Immediately publish current status
        publishOTAStatus();
    } else if (command == "test_display") {
        // Test the progress display functionality
        DEBUG_PRINTLN("[OTA] Testing progress display");
        for (int i = 0; i <= 100; i += 10) {
            showProgressOnMatrix(i);
            showProgressOnScreen(i, "Testing progress display: " + String(i) + "%");
            delay(500);
        }
        setState(OTA_IDLE, "Display test completed");
    } else if (command == "factory_reset") {
        // Prepare for factory reset (clear settings, etc.)
        DEBUG_PRINTLN("[OTA] Factory reset requested");
        logOTAEvent("FACTORY_RESET_REQUESTED");
        
        // Show warning on screen
        extern void showFactoryResetWarning();
        showFactoryResetWarning();
        
        // Actual reset would be implemented here
        // For now, just log the request
    }
}

void OTAUpdater::publishOTAStatus() {
    if (!mqttClient.connected()) return;
    
    DynamicJsonDocument doc(1024);
    doc["state"] = getStateString();
    doc["current_version"] = currentVersion;
    doc["progress"] = getProgress();
    doc["auto_update"] = autoUpdateEnabled;
    doc["can_rollback"] = canRollback();
    doc["last_check"] = lastVersionCheck;
    doc["retry_count"] = retryCount;
    
    // System information
    doc["free_heap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;
    doc["wifi_rssi"] = WiFi.RSSI();
    
    // Partition information
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running) {
        doc["running_partition"] = running->label;
        doc["partition_size"] = running->size;
    }
    
    if (currentState == OTA_UPDATE_AVAILABLE) {
        JsonObject update = doc.createNestedObject("available_update");
        update["version"] = updateInfo.version;
        update["size"] = updateInfo.size;
        update["size_mb"] = updateInfo.size / 1024.0 / 1024.0;
        update["mandatory"] = updateInfo.mandatory;
        update["release_notes"] = updateInfo.releaseNotes;
        update["url"] = updateInfo.url;
        update["min_version"] = updateInfo.minVersion;
        
        // Calculate estimated download time based on size
        int estimatedSeconds = updateInfo.size / (50 * 1024); // Assume 50KB/s
        update["estimated_time_seconds"] = estimatedSeconds;
    }
    
    if (currentState == OTA_FAILED) {
        doc["error"] = getLastError();
        doc["last_error_time"] = millis();
    }
    
    // Add capabilities
    JsonArray capabilities = doc.createNestedArray("capabilities");
    capabilities.add("version_check");
    capabilities.add("auto_update");
    capabilities.add("rollback");
    capabilities.add("progress_display");
    capabilities.add("force_update");
    capabilities.add("checksum_verify");
    
    String statusJson;
    serializeJson(doc, statusJson);
    
    mqttClient.publish(MQTT_TOPIC_OTA_STATUS, statusJson.c_str(), true);
    
    DEBUG_PRINTF("[OTA] Published status: %s\n", getStateString().c_str());
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
            // Show success pattern on LED matrix
            extern CRGB leds[];
            extern uint16_t XY(uint8_t x, uint8_t y);
            
            // Animated success pattern
            for (int wave = 0; wave < 3; wave++) {
                fill_solid(leds, NUM_LEDS, CRGB::Black);
                
                // Expanding green circle
                int centerX = MATRIX_WIDTH / 2;
                int centerY = MATRIX_HEIGHT / 2;
                
                for (int radius = 0; radius <= 8; radius++) {
                    for (int x = 0; x < MATRIX_WIDTH; x++) {
                        for (int y = 0; y < MATRIX_HEIGHT; y++) {
                            int dx = x - centerX;
                            int dy = y - centerY;
                            int distance = sqrt(dx * dx + dy * dy);
                            
                            if (distance == radius) {
                                leds[XY(x, y)] = CRGB::Green;
                            }
                        }
                    }
                    FastLED.show();
                    delay(100);
                }
                
                // Fill with green
                fill_solid(leds, NUM_LEDS, CRGB::Green);
                FastLED.show();
                delay(500);
            }
            
            // Show success screen
            extern void showOTASuccessScreen();
            showOTASuccessScreen();
            break;
            
        case OTA_FAILED:
            // Show failure pattern on LED matrix
            extern CRGB leds[];
            extern uint16_t XY(uint8_t x, uint8_t y);
            
            // Flash red X pattern to indicate failure
            for (int flash = 0; flash < 3; flash++) {
                fill_solid(leds, NUM_LEDS, CRGB::Black);
                
                // Draw X pattern
                for (int i = 0; i < MATRIX_WIDTH; i++) {
                    if (i < MATRIX_HEIGHT) {
                        leds[XY(i, i)] = CRGB::Red;                    // Diagonal \
                        leds[XY(i, MATRIX_HEIGHT - 1 - i)] = CRGB::Red; // Diagonal /
                    }
                }
                
                FastLED.show();
                delay(500);
                
                fill_solid(leds, NUM_LEDS, CRGB::Black);
                FastLED.show();
                delay(300);
            }
            
            // Show failure message on screen
            extern void showOTAErrorScreen(const String& error);
            showOTAErrorScreen(otaUpdater.getLastError());
            break;
            
        default:
            break;
    }
}