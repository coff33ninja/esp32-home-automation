"""
Constants and configuration defaults for the volume control system.
"""

# MQTT Settings - Default values (can be overridden by configuration)
DEFAULT_MQTT_BROKER = "192.168.1.xxx"  # Change to your broker IP
DEFAULT_MQTT_PORT = 1883
DEFAULT_MQTT_USERNAME = ""  # Leave empty if not required
DEFAULT_MQTT_PASSWORD = ""  # Leave empty if not required
DEFAULT_MQTT_CLIENT_ID = "PCVolumeControl"

# MQTT Topics
MQTT_TOPICS = {
    "volume": "homecontrol/volume",
    "command": "homecontrol/command", 
    "status": "homecontrol/pc/status",
    "pc_volume": "homecontrol/pc/volume",
    "app_volume": "homecontrol/pc/app_volume",
    "pc_power": "homecontrol/pc/power",
    "pc_system": "homecontrol/pc/system"
}

# Application Settings
DEFAULT_UPDATE_RATE_LIMIT = 0.1  # Minimum seconds between volume updates
DEFAULT_SYNC_INTERVAL = 2.0  # Seconds between volume sync checks
DEFAULT_RECONNECT_DELAY = 5.0  # Seconds between reconnection attempts
DEFAULT_DEBUG = True  # Set to False to reduce console output
DEFAULT_CONFIG_FILE = "volume_control_config.json"

# Per-application volume control settings
DEFAULT_MONITORED_APPS = [
    "chrome.exe",
    "firefox.exe", 
    "spotify.exe",
    "discord.exe",
    "vlc.exe",
    "winamp.exe",
    "foobar2000.exe"
]

# Default configuration structure
DEFAULT_CONFIG = {
    "mqtt": {
        "broker": DEFAULT_MQTT_BROKER,
        "port": DEFAULT_MQTT_PORT,
        "username": DEFAULT_MQTT_USERNAME,
        "password": DEFAULT_MQTT_PASSWORD,
        "client_id": DEFAULT_MQTT_CLIENT_ID,
        "keepalive": 60,
        "qos": 1,
        "retain": True
    },
    "topics": MQTT_TOPICS,
    "settings": {
        "update_rate_limit": DEFAULT_UPDATE_RATE_LIMIT,
        "sync_interval": DEFAULT_SYNC_INTERVAL,
        "reconnect_delay": DEFAULT_RECONNECT_DELAY,
        "debug": DEFAULT_DEBUG,
        "enable_mqtt": True,
        "enable_system_monitoring": True,
        "enable_tray": True,
        "monitor_interval": 5,
        "status_publish_interval": 60,
        "log_level": "DEBUG",
        "log_file": "volume_control.log",
        "max_log_size_mb": 10,
        "backup_log_count": 3
    },
    "apps": {
        "monitored": DEFAULT_MONITORED_APPS
    },
    "power_management": {
        "detect_sleep_wake": True,
        "notify_esp32_on_sleep": True,
        "notify_esp32_on_wake": True,
        "idle_threshold_minutes": 30,
        "sleep_detection_method": "activity"  # "activity" or "wmi"
    },
    "diagnostics": {
        "enable_performance_monitoring": True,
        "collect_system_metrics": True,
        "publish_diagnostics": True,
        "diagnostic_interval": 300  # 5 minutes
    }
}

# Configuration validation schema
CONFIG_SCHEMA = {
    "mqtt": {
        "broker": {"type": str, "required": True},
        "port": {"type": int, "min": 1, "max": 65535},
        "username": {"type": str, "required": False},
        "password": {"type": str, "required": False},
        "client_id": {"type": str, "required": True},
        "keepalive": {"type": int, "min": 10, "max": 3600},
        "qos": {"type": int, "min": 0, "max": 2},
        "retain": {"type": bool}
    },
    "topics": {
        "volume": {"type": str, "required": True},
        "command": {"type": str, "required": True},
        "status": {"type": str, "required": True},
        "pc_volume": {"type": str, "required": True},
        "app_volume": {"type": str, "required": True},
        "pc_power": {"type": str, "required": True},
        "pc_system": {"type": str, "required": True}
    },
    "settings": {
        "update_rate_limit": {"type": float, "min": 0.01, "max": 1.0},
        "sync_interval": {"type": float, "min": 0.5, "max": 60.0},
        "reconnect_delay": {"type": float, "min": 1.0, "max": 300.0},
        "debug": {"type": bool},
        "enable_mqtt": {"type": bool},
        "enable_system_monitoring": {"type": bool},
        "enable_tray": {"type": bool},
        "monitor_interval": {"type": int, "min": 1, "max": 60},
        "status_publish_interval": {"type": int, "min": 10, "max": 3600},
        "log_level": {"type": str, "choices": ["DEBUG", "INFO", "WARNING", "ERROR"]},
        "log_file": {"type": str},
        "max_log_size_mb": {"type": int, "min": 1, "max": 100},
        "backup_log_count": {"type": int, "min": 1, "max": 10}
    },
    "apps": {
        "monitored": {"type": list, "item_type": str}
    },
    "power_management": {
        "detect_sleep_wake": {"type": bool},
        "notify_esp32_on_sleep": {"type": bool},
        "notify_esp32_on_wake": {"type": bool},
        "idle_threshold_minutes": {"type": int, "min": 1, "max": 1440},
        "sleep_detection_method": {"type": str, "choices": ["activity", "wmi"]}
    },
    "diagnostics": {
        "enable_performance_monitoring": {"type": bool},
        "collect_system_metrics": {"type": bool},
        "publish_diagnostics": {"type": bool},
        "diagnostic_interval": {"type": int, "min": 60, "max": 3600}
    }
}

# Optional dependency availability flags
TRAY_AVAILABLE = False
WINDOWS_MONITORING = False

try:
    import pystray
    from PIL import Image, ImageDraw
    TRAY_AVAILABLE = True
except ImportError:
    pass

try:
    import win32api
    import win32con
    import win32gui
    import wmi
    WINDOWS_MONITORING = True
except ImportError:
    pass