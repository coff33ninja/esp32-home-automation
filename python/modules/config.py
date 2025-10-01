"""
Configuration management for the volume control system.
"""

import json
import os
import logging
from .constants import DEFAULT_CONFIG

logger = logging.getLogger(__name__)


class ConfigManager:
    """Enhanced configuration management with validation and persistence"""
    
    def __init__(self, config_file="volume_control_config.json"):
        self.config_file = config_file
        self.config = self.load_config()
        self.validate_config()
        logger.info(f"Configuration loaded from {config_file}")
    
    def load_config(self):
        """Load configuration from file"""
        try:
            if os.path.exists(self.config_file):
                with open(self.config_file, 'r') as f:
                    loaded_config = json.load(f)
                # Merge with defaults to ensure all keys exist
                config = self._merge_configs(DEFAULT_CONFIG, loaded_config)
            else:
                config = DEFAULT_CONFIG.copy()
                self.save_config(config)
                logger.info(f"Created default configuration file: {self.config_file}")
            
            return config
            
        except Exception as e:
            logger.error(f"Error loading configuration: {e}")
            logger.info("Using default configuration")
            return DEFAULT_CONFIG.copy()
    
    def _merge_configs(self, default, loaded):
        """Recursively merge configurations"""
        result = default.copy()
        for key, value in loaded.items():
            if key in result and isinstance(result[key], dict) and isinstance(value, dict):
                result[key] = self._merge_configs(result[key], value)
            else:
                result[key] = value
        return result
    
    def validate_config(self):
        """Validate configuration values using schema"""
        from .constants import CONFIG_SCHEMA
        
        try:
            validation_errors = []
            
            # Validate each section
            for section_name, section_schema in CONFIG_SCHEMA.items():
                section_data = self.config.get(section_name, {})
                
                for field_name, field_schema in section_schema.items():
                    field_path = f"{section_name}.{field_name}"
                    value = section_data.get(field_name)
                    
                    # Check required fields
                    if field_schema.get("required", False) and value is None:
                        validation_errors.append(f"Required field missing: {field_path}")
                        continue
                    
                    if value is None:
                        continue
                    
                    # Check type
                    expected_type = field_schema.get("type")
                    if expected_type and not isinstance(value, expected_type):
                        validation_errors.append(f"Invalid type for {field_path}: expected {expected_type.__name__}, got {type(value).__name__}")
                        # Reset to default
                        self._reset_to_default(section_name, field_name)
                        continue
                    
                    # Check numeric ranges
                    if expected_type in (int, float):
                        min_val = field_schema.get("min")
                        max_val = field_schema.get("max")
                        if min_val is not None and value < min_val:
                            validation_errors.append(f"Value too small for {field_path}: {value} < {min_val}")
                            self._reset_to_default(section_name, field_name)
                        elif max_val is not None and value > max_val:
                            validation_errors.append(f"Value too large for {field_path}: {value} > {max_val}")
                            self._reset_to_default(section_name, field_name)
                    
                    # Check choices
                    choices = field_schema.get("choices")
                    if choices and value not in choices:
                        validation_errors.append(f"Invalid choice for {field_path}: {value} not in {choices}")
                        self._reset_to_default(section_name, field_name)
                    
                    # Check list item types
                    if expected_type == list:
                        item_type = field_schema.get("item_type")
                        if item_type:
                            for i, item in enumerate(value):
                                if not isinstance(item, item_type):
                                    validation_errors.append(f"Invalid item type in {field_path}[{i}]: expected {item_type.__name__}")
            
            # Special validation for MQTT broker
            mqtt_broker = self.config.get("mqtt", {}).get("broker")
            if mqtt_broker == "192.168.1.xxx":
                validation_errors.append("MQTT broker not configured - please update configuration")
            
            # Log validation results
            if validation_errors:
                for error in validation_errors:
                    logger.warning(f"Configuration validation: {error}")
            else:
                logger.debug("Configuration validation completed successfully")
            
        except Exception as e:
            logger.error(f"Error validating configuration: {e}")
    
    def _reset_to_default(self, section_name, field_name):
        """Reset a configuration field to its default value"""
        try:
            default_value = DEFAULT_CONFIG[section_name][field_name]
            self.config[section_name][field_name] = default_value
            logger.info(f"Reset {section_name}.{field_name} to default: {default_value}")
        except KeyError:
            logger.error(f"No default value found for {section_name}.{field_name}")
    
    def save_config(self, config=None):
        """Save configuration to file"""
        try:
            config_to_save = config or self.config
            with open(self.config_file, 'w') as f:
                json.dump(config_to_save, f, indent=2)
            logger.debug(f"Configuration saved to {self.config_file}")
            return True
        except Exception as e:
            logger.error(f"Error saving configuration: {e}")
            return False
    
    def get(self, key_path, default=None):
        """Get configuration value using dot notation (e.g., 'mqtt.broker')"""
        try:
            keys = key_path.split('.')
            value = self.config
            for key in keys:
                value = value[key]
            return value
        except (KeyError, TypeError):
            return default
    
    def set(self, key_path, value):
        """Set configuration value using dot notation"""
        try:
            keys = key_path.split('.')
            config = self.config
            for key in keys[:-1]:
                if key not in config:
                    config[key] = {}
                config = config[key]
            config[keys[-1]] = value
            self.save_config()
            logger.debug(f"Configuration updated: {key_path} = {value}")
        except Exception as e:
            logger.error(f"Error setting configuration {key_path}: {e}")
    
    def get_mqtt_config(self):
        """Get MQTT configuration as a dictionary"""
        return self.config.get("mqtt", {})
    
    def get_topics(self):
        """Get MQTT topics configuration"""
        return self.config.get("topics", {})
    
    def get_monitored_apps(self):
        """Get list of monitored applications"""
        return self.config.get("apps", [])
    
    def get_settings(self):
        """Get application settings"""
        return self.config.get("settings", {})


def create_default_config():
    """Create and return default configuration"""
    return DEFAULT_CONFIG.copy()


def load_config(config_file):
    """Load configuration from JSON file (legacy function for compatibility)"""
    try:
        if os.path.exists(config_file):
            with open(config_file, 'r') as f:
                return json.load(f)
        else:
            return create_default_config()
    except Exception as e:
        logger.error(f"Error loading configuration from {config_file}: {e}")
        return {}


def save_config(config, config_file):
    """Save configuration to JSON file (legacy function for compatibility)"""
    try:
        with open(config_file, 'w') as f:
            json.dump(config, f, indent=2)
        return True
    except Exception as e:
        logger.error(f"Error saving configuration: {e}")
        return False