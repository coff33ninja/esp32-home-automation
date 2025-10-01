# OTA Update System Guide

## Overview

The ESP32 Home Automation system includes a comprehensive Over-The-Air (OTA) update system that allows secure firmware updates without physical access to the device. The system includes progress indication on both the LED matrix and touch screen, rollback capability, and version checking.

## Features

- **Secure Updates**: Checksum verification and rollback capability
- **Progress Indication**: Visual feedback on LED matrix and touch screen
- **Version Checking**: Automatic version compatibility checking
- **Rollback Support**: Ability to rollback to previous firmware if update fails
- **MQTT Integration**: Full control via MQTT commands
- **Auto-Update**: Optional automatic update checking and installation

## MQTT Commands

### Check for Updates
```
Topic: homecontrol/ota/command
Payload: check_update
```

### Start Update
```
Topic: homecontrol/ota/command
Payload: start_update
```

### Cancel Update
```
Topic: homecontrol/ota/command
Payload: cancel_update
```

### Rollback to Previous Version
```
Topic: homecontrol/ota/command
Payload: rollback
```

### Enable/Disable Auto-Update
```
Topic: homecontrol/ota/command
Payload: set_auto_update:true
```

### Mark Current Version as Valid
```
Topic: homecontrol/ota/command
Payload: mark_valid
```

## MQTT Status Topics

### OTA Status
```
Topic: homecontrol/ota/status
Payload: {
  "state": "Idle",
  "current_version": "1.0.0",
  "progress": 0,
  "auto_update": true,
  "can_rollback": false,
  "available_version": "1.1.0",
  "update_size": 1048576,
  "mandatory": false,
  "release_notes": "Bug fixes and improvements"
}
```

### OTA Progress
```
Topic: homecontrol/ota/progress
Payload: {
  "progress": 45,
  "bytes_written": 471859,
  "total_bytes": 1048576
}
```

## Server Setup

### Version Information File

Create a `version.json` file on your web server:

```json
{
  "version": "1.1.0",
  "url": "https://your-server.com/firmware/esp32-home-automation-v1.1.0.bin",
  "checksum": "sha256:abcd1234567890abcdef1234567890abcdef1234567890abcdef1234567890ab",
  "size": 1048576,
  "releaseNotes": "Added OTA update capability, improved LED effects, bug fixes",
  "mandatory": false,
  "minVersion": "1.0.0"
}
```

### Firmware Binary

Upload your compiled firmware binary to the URL specified in the version.json file.

## Configuration

Update the following constants in `config.h`:

```cpp
#define OTA_UPDATE_URL "https://your-server.com/firmware/"
#define OTA_VERSION_CHECK_URL "https://your-server.com/version.json"
```

## Visual Feedback

### LED Matrix Progress Bar

During updates, the LED matrix displays a progress bar:
- **Red**: First third of progress (0-33%)
- **Yellow**: Second third of progress (34-66%)
- **Green**: Final third of progress (67-100%)

### Touch Screen Display

The touch screen shows:
- Update progress percentage
- Current operation (Downloading/Installing)
- Success/failure status

## Safety Features

### Fail-Safe Operation

- System continues normal operation during update checks
- Physical controls remain functional during updates
- Automatic rollback if new firmware fails to boot properly

### Rollback Capability

- Previous firmware version is preserved
- Manual rollback via MQTT command
- Automatic rollback if new firmware doesn't validate within boot cycle

### Update Validation

- Checksum verification before installation
- Version compatibility checking
- Mandatory update enforcement for critical security updates

## Troubleshooting

### Update Fails to Start

1. Check WiFi connection
2. Verify server URL is accessible
3. Check available heap memory (should be >100KB)

### Update Fails During Download

1. Check internet connectivity
2. Verify firmware file exists at specified URL
3. Check server response and file size

### Update Fails During Installation

1. Check available flash space
2. Verify firmware binary is valid
3. Check for memory fragmentation

### Rollback Issues

1. Ensure previous firmware partition exists
2. Check if rollback is available (`can_rollback` status)
3. Verify system hasn't been marked as valid yet

## Home Assistant Integration

Add the following to your Home Assistant configuration:

```yaml
# OTA Update Controls
button:
  - platform: mqtt
    name: "ESP32 Check Updates"
    command_topic: "homecontrol/ota/command"
    payload_press: "check_update"
    
  - platform: mqtt
    name: "ESP32 Start Update"
    command_topic: "homecontrol/ota/command"
    payload_press: "start_update"
    
  - platform: mqtt
    name: "ESP32 Rollback"
    command_topic: "homecontrol/ota/command"
    payload_press: "rollback"

# OTA Status Sensor
sensor:
  - platform: mqtt
    name: "ESP32 OTA Status"
    state_topic: "homecontrol/ota/status"
    value_template: "{{ value_json.state }}"
    json_attributes_topic: "homecontrol/ota/status"
    
  - platform: mqtt
    name: "ESP32 Update Progress"
    state_topic: "homecontrol/ota/progress"
    value_template: "{{ value_json.progress }}"
    unit_of_measurement: "%"

# Auto-Update Switch
switch:
  - platform: mqtt
    name: "ESP32 Auto Update"
    command_topic: "homecontrol/ota/command"
    state_topic: "homecontrol/ota/status"
    value_template: "{{ value_json.auto_update }}"
    payload_on: "set_auto_update:true"
    payload_off: "set_auto_update:false"
```

## Security Considerations

### HTTPS Requirements

- Always use HTTPS for firmware downloads
- Verify SSL certificates in production
- Use strong authentication for firmware server

### Checksum Verification

- Generate SHA256 checksums for all firmware files
- Verify checksums before installation
- Reject updates with invalid checksums

### Version Control

- Use semantic versioning (MAJOR.MINOR.PATCH)
- Implement minimum version requirements
- Test compatibility thoroughly before release

## Development Workflow

### Building Firmware

1. Update version number in `config.h`
2. Compile firmware using PlatformIO
3. Generate SHA256 checksum of binary
4. Update `version.json` with new version info
5. Upload binary and version.json to server

### Testing Updates

1. Test on development hardware first
2. Verify rollback functionality
3. Test with various network conditions
4. Validate all visual feedback systems

### Deployment

1. Upload to staging server first
2. Test with limited devices
3. Monitor for issues
4. Deploy to production server
5. Monitor update success rates

## API Reference

### OTAUpdater Class Methods

```cpp
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

// MQTT integration
void handleMQTTCommand(const String& command, const String& payload);
void publishOTAStatus();
```

### OTA States

- `OTA_IDLE`: System ready, no update in progress
- `OTA_CHECKING_VERSION`: Checking for available updates
- `OTA_UPDATE_AVAILABLE`: Update found and ready to install
- `OTA_DOWNLOADING`: Downloading firmware from server
- `OTA_INSTALLING`: Installing firmware to flash
- `OTA_SUCCESS`: Update completed successfully
- `OTA_FAILED`: Update failed, check error message
- `OTA_ROLLBACK_REQUIRED`: System needs rollback to previous version