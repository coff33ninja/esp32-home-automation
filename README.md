# 🎛️ ESP32 Home Automation & Creative Control Panel

> **Note:** This project is not currently aligned with the plan laid out in Notion.

A modular ESP32-based home automation system featuring motorized volume control, LED lighting, touch screen interface, and IR remote control.

## 📋 Project Overview

This project combines tactile analog control with modern smart home automation. Control system audio, lighting, and visual effects through multiple interfaces including physical knobs, touch screens, IR remotes, and smart home integration.

### Key Features

- **🔊 Audio Control**: Per-application and global volume control via motorized potentiometer
- **💡 Smart Lighting**: On/off switching and PWM dimming for 12V LED strips
- **🎨 LED Matrix**: Interactive 16x16 WS2812B matrix with touch-based color painting
- **📱 Touch Screen**: ILI9341 display for visual control and effects
- **🎮 IR Remote**: Universal remote control support
- **🌐 WiFi/MQTT**: Smart home integration with Home Assistant
- **💻 PC Integration**: Python-based volume control with PyCAW
- **🛡️ Fail-Safe Design**: Default-safe states prevent sudden loud audio or bright lights

## 🏗️ Project Structure

```
esp32-home-automation/
├── src/
│   └── main.cpp           # Main firmware file
├── include/
│   ├── config.h           # Configuration and pin definitions
│   ├── motor_control.h    # Motorized potentiometer control
│   ├── relay_control.h    # Relay and lighting control
│   ├── led_effects.h      # LED matrix effects
│   ├── touch_handler.h    # Touch screen handler
│   ├── ir_handler.h       # IR remote handler
│   ├── mqtt_handler.h     # MQTT communication
│   └── failsafe.h         # Fail-safe logic
├── python/
│   ├── volume_control.py  # PyCAW volume controller
│   └── requirements.txt   # Python dependencies
├── docs/
│   ├── pinout.md          # GPIO pinout reference
│   ├── wiring.md          # Wiring diagrams
│   └── components.md      # Component list and sources
├── platformio.ini         # PlatformIO configuration
├── README.md              # This file
└── LICENSE                # Project license
```

## 🔧 Hardware Requirements

### Core Components
- **ESP32-WROOM-32 DevKit** - Main microcontroller
- **Alps RK27112MC Motorized Potentiometer** - Volume control
- **L298N H-Bridge Driver** - Motor control
- **4-Channel Relay Module** - Lighting control
- **5V Power Supply** - ESP32 power
- **12V Power Supply** - LED strips and motor

### Optional Components
- **ILI9341 2.8" Touch Screen** - Visual interface
- **WS2812B 16x16 LED Matrix** - Visual effects
- **TSOP4838 IR Receiver** - Remote control
- **12V LED Strips** - Ambient lighting

## 📦 Software Dependencies

### ESP32 Firmware (PlatformIO)
```ini
platform = espressif32
framework = arduino

lib_deps = 
    bblanchon/ArduinoJson @ ^6.21.3
    knolleary/PubSubClient @ ^2.8
    bodmer/TFT_eSPI @ ^2.5.0
    fastled/FastLED @ ^3.6.0
    crankyoldgit/IRremoteESP8266 @ ^2.8.4
```

### Python Integration
```bash
pip install pycaw
pip install paho-mqtt
pip install pyserial
```

## 🚀 Quick Start

### 1. Clone the Repository
```bash
git clone https://github.com/ideagazm/esp32-home-automation.git
cd esp32-home-automation
```

### 2. Configure Settings
Edit `include/config.h` with your settings:
```cpp
// WiFi credentials
#define WIFI_SSID "YourSSID"
#define WIFI_PASSWORD "YourPassword"

// MQTT broker
#define MQTT_SERVER "192.168.1.xxx"
#define MQTT_PORT 1883
```

### 3. Build and Upload (PlatformIO)
```bash
pio run                    # Build
pio run --target upload    # Upload to ESP32
pio run --target monitor   # Monitor serial output
```

### 4. Start Python Integration (Windows)
```bash
cd python
python volume_control.py
```

## 📖 Documentation

- **[Pinout Reference](docs/pinout.md)** - Complete GPIO pin assignments
- **[Wiring Guide](docs/wiring.md)** - Circuit diagrams and connections
- **[Component List](docs/components.md)** - Bill of materials with sources
- **[Notion Documentation](https://notion.so/...)** - Detailed project documentation

## 🎯 Implementation Phases

### Phase 1: Core System ✅
- [x] Motorized pot control
- [x] Relay switching
- [x] Fail-safe logic
- [x] Basic MQTT communication

### Phase 2: Touch & Matrix 🚧
- [ ] Touch screen calibration
- [ ] LED matrix integration
- [ ] Color painting interface
- [ ] Visual effects library

### Phase 3: Advanced Features 📋
- [ ] IR remote control
- [ ] PC wake functionality
- [ ] Voice control (Home Assistant)
- [ ] OTA updates

## 🛡️ Safety Features

- **Fail-Safe State**: System boots to muted audio and lights off
- **Watchdog Timer**: Automatic recovery from crashes
- **Power Monitoring**: Detects and handles power failures
- **Smooth Motor Control**: PID loop prevents jerky movement

## 🤝 Contributing

This is a personal project, but feedback and suggestions are welcome! Feel free to open issues or submit pull requests.

## 📄 License

[Add your chosen license here - MIT, GPL, etc.]

## 🙏 Acknowledgments

- ESP32 Community
- PlatformIO Team
- Arduino Libraries Contributors
- [List any specific tutorials or resources you used]

## 📞 Contact

- GitHub: [@ideagazm](https://github.com/ideagazm)
- Project Link: [ESP32 Home Automation](https://github.com/ideagazm/esp32-home-automation)

---

**⚠️ Disclaimer**: This project involves working with mains voltage for lighting control. Always follow electrical safety guidelines and local codes. If unsure, consult a licensed electrician.
