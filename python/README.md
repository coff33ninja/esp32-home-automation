# ESP32 Home Automation - PC Volume Control

This directory contains the Python scripts for integrating your Windows PC with the ESP32 Home Automation system. The script provides bidirectional volume control, per-application volume control, system monitoring, and more.

## Features

-   **Bidirectional Volume Sync**: Synchronizes volume between your PC and ESP32.
-   **Per-Application Volume Control**: Adjust volume for specific applications (e.g., Chrome, Spotify).
-   **MQTT Integration**: Communicates with the ESP32 and other devices via MQTT. Can be disabled.
-   **System Monitoring**: Monitors PC sleep/wake events and system resources.
-   **System Tray Icon**: Provides a convenient way to control volume and see the status from the system tray.
-   **Automatic Updates**: The startup script automatically creates a virtual environment and installs/updates dependencies.
-   **Resilient**: Automatically reconnects to the MQTT broker if the connection is lost.
-   **Configurable**: Settings can be easily modified in a `JSON` configuration file.

## Requirements

-   Windows 10/11
-   Python 3.11 (as specified in `start_volume_control.bat`)
-   `uv` (a fast Python package installer). Can be installed from [https://github.com/astral-sh/uv](https://github.com/astral-sh/uv).

## Quick Start

1.  **Install `uv`**: If you don't have it, install `uv` from [https://github.com/astral-sh/uv](https://github.com/astral-sh/uv).
2.  **Run the startup script**: Double-click `start_volume_control.bat`.

This script will:
-   Create a Python virtual environment in the `.venv` folder.
-   Install all the required dependencies from `requirements.txt`.
-   Start the application with the system tray icon.

## Configuration

The application is configured using the `volume_control_config.json` file. A default file is created if one doesn't exist.

### Key Settings

-   **`mqtt.broker`**: The IP address of your MQTT broker.
-   **`mqtt.port`**: The port of your MQTT broker (usually 1883).
-   **`apps`**: A list of application executable names to monitor for per-application volume control (e.g., `"chrome.exe"`, `"spotify.exe"`).
-   **`settings.enable_mqtt`**: Set to `false` to disable the MQTT client if you are not using it. This will prevent connection errors if you don't have an MQTT broker running.
-   **`settings.enable_tray`**: Set to `false` to run the application in console mode without the system tray icon.

## Usage

### Running the application

The easiest way to run the application is to use the `start_volume_control.bat` script.

You can also run the application manually from the command line:

```bash
# Activate the virtual environment
.venv\Scripts\activate

# Run the main script
python main.py

# To run with the system tray icon
python main.py --tray

# To run without the system tray icon
python main.py --no-tray
```

### MQTT Topics

The application uses MQTT to communicate with the ESP32. Here are the main topics:

#### Subscribed Topics (Listens for messages on these topics)

-   `homecontrol/volume`: Sets the main system volume (payload: 0-100).
-   `homecontrol/command`: For commands like `MUTE`, `UNMUTE`, `STATUS`.
-   `homecontrol/pc/app_volume`: Sets volume for a specific application. Payload is a JSON object, e.g., `{"app": "spotify.exe", "volume": 50}`.

#### Published Topics (Sends messages to these topics)

-   `homecontrol/pc/status`: The current status of the PC client (online/offline).
-   `homecontrol/pc/volume`: The current main system volume.
-   `homecontrol/pc/power`: PC power events (e.g., sleep, wake).
-   `homecontrol/pc/system`: System metrics like CPU and memory usage.

## Troubleshooting

-   **`uv` is not installed**: Install it from the official repository.
-   **MQTT Connection Failed**:
    -   Check the `broker` IP address in `volume_control_config.json`.
    -   Ensure your MQTT broker is running and accessible from your PC.
    -   If you are not using MQTT, set `"enable_mqtt": false` in the configuration file.
-   **System Tray Icon Not Showing**:
    -   Make sure `"enable_tray": true` in the configuration.
    -   Run `start_volume_control.bat` which will install all dependencies.

## File Structure

-   `main.py`: The main application script.
-   `start_volume_control.bat`: The main startup script for users.
-   `run_volume_control.py`: A wrapper to run `main.py`.
-   `requirements.txt`: A list of Python dependencies.
-   `volume_control_config.json`: The configuration file.
-   `modules/`: A directory containing the application's modules (e.g., `app.py`, `mqtt_client.py`, `volume_controller.py`).
-   `README.md`: This documentation file.
