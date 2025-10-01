"""
Main application coordinator for volume control system.
"""

import sys
import time
import threading
import logging
import psutil
from .config import ConfigManager
from .diagnostics import DiagnosticLogger
from .volume_controller import WindowsVolumeController
from .mqtt_client import MQTTVolumeClient
from .system_monitor import PCSystemMonitor
from .tray_ui import SystemTrayApp
from .constants import TRAY_AVAILABLE

logger = logging.getLogger(__name__)


class VolumeControlApp:
    """Main application class that coordinates all components"""
    
    def __init__(self, config_file="volume_control_config.json"):
        """
        Initialize the application
        
        Args:
            config_file (str): Path to configuration file
        """
        self.config_file = config_file
        self.running = False
        
        # Component instances
        self.config_manager = None
        self.diagnostic_logger = None
        self.volume_controller = None
        self.mqtt_client = None
        self.system_monitor = None
        self.system_tray = None
        
        # Threading
        self.mqtt_thread = None
        self.tray_thread = None
        
        logger.info("Volume Control Application initializing...")
    
    def initialize_components(self):
        """Initialize all application components"""
        try:
            # Initialize configuration manager first
            self.config_manager = ConfigManager(self.config_file)
            logger.info("Configuration manager initialized")
            
            # Initialize diagnostic logger (this sets up logging)
            self.diagnostic_logger = DiagnosticLogger(self.config_manager)
            logger.info("Diagnostic logger initialized")
            
            # Initialize volume controller
            monitored_apps = self.config_manager.get_monitored_apps()
            settings = self.config_manager.get_settings()
            
            self.volume_controller = WindowsVolumeController(
                monitored_apps=monitored_apps,
                update_rate_limit=settings.get("update_rate_limit", 0.1),
                sync_interval=settings.get("sync_interval", 2.0)
            )
            logger.info("Volume controller initialized")
            
            # Initialize MQTT client
            self.mqtt_client = MQTTVolumeClient(self.volume_controller, self.config_manager)
            logger.info("MQTT client initialized")
            
            # Initialize system monitor if enabled
            if settings.get("enable_system_monitoring", True):
                self.system_monitor = PCSystemMonitor(self.mqtt_client, self.config_manager)
                # Set system monitor reference in MQTT client
                self.mqtt_client.set_system_monitor(self.system_monitor)
                logger.info("System monitor initialized")
            
            # Initialize system tray if enabled and available
            if settings.get("enable_tray", True) and TRAY_AVAILABLE:
                self.system_tray = SystemTrayApp(
                    self.mqtt_client, 
                    self.volume_controller, 
                    self.config_manager
                )
                logger.info("System tray initialized")
            elif settings.get("enable_tray", True) and not TRAY_AVAILABLE:
                logger.warning("System tray requested but not available")
            
            logger.info("All components initialized successfully")
            return True
            
        except Exception as e:
            logger.error(f"Error initializing components: {e}")
            if self.diagnostic_logger:
                self.diagnostic_logger.log_error_event("initialization_error", str(e))
            return False
    
    def start(self, enable_tray=True):
        """
        Start the application
        
        Args:
            enable_tray (bool): Whether to enable system tray interface
        """
        try:
            # Initialize components if not already initialized
            if self.config_manager is None:
                if not self.initialize_components():
                    logger.error("Failed to initialize application components")
                    return False
            
            self.running = True
            logger.info("Starting Volume Control Application...")
            
            # Log startup diagnostics
            self._log_startup_diagnostics()
            
            # Start system monitor if available
            if self.system_monitor:
                self.system_monitor.start_monitoring()
                logger.info("System monitoring started")
            
            # Start MQTT client if enabled
            if self.config_manager.get("settings.enable_mqtt", True):
                self.mqtt_thread = threading.Thread(target=self._run_mqtt_client, daemon=True)
                self.mqtt_thread.start()
                logger.info("MQTT client started in background")
            else:
                logger.info("MQTT client is disabled in the configuration.")
            
            # Start system tray if enabled and available
            if enable_tray and self.system_tray and self.system_tray.is_available():
                logger.info("Starting system tray interface...")
                # Run tray in main thread (blocks until quit)
                self.system_tray.start()
            else:
                # Run in console mode
                logger.info("Running in console mode (no system tray)")
                self._run_console_mode()
            
            return True
            
        except KeyboardInterrupt:
            logger.info("Application interrupted by user")
            return True
        except Exception as e:
            logger.error(f"Error starting application: {e}")
            if self.diagnostic_logger:
                self.diagnostic_logger.log_error_event("startup_error", str(e))
            return False
        finally:
            self.stop()
    
    def _run_mqtt_client(self):
        """Run MQTT client in background thread"""
        try:
            self.mqtt_client.start()
        except Exception as e:
            logger.error(f"Error in MQTT client thread: {e}")
            if self.diagnostic_logger:
                self.diagnostic_logger.log_error_event("mqtt_thread_error", str(e))
    
    def _run_console_mode(self):
        """Run in console mode without system tray"""
        try:
            logger.info("Console mode active. Press Ctrl+C to quit.")
            while self.running:
                time.sleep(1)
        except KeyboardInterrupt:
            logger.info("Console mode interrupted")
    
    def stop(self):
        """Stop the application gracefully"""
        try:
            logger.info("Stopping Volume Control Application...")
            self.running = False
            
            # Stop system tray
            if self.system_tray:
                self.system_tray.stop()
                logger.info("System tray stopped")
            
            # Stop system monitor
            if self.system_monitor:
                self.system_monitor.stop_monitoring()
                logger.info("System monitoring stopped")
            
            # Stop MQTT client
            if self.mqtt_client:
                self.mqtt_client.stop()
                logger.info("MQTT client stopped")
            
            # Wait for threads to finish
            if self.mqtt_thread and self.mqtt_thread.is_alive():
                self.mqtt_thread.join(timeout=5)
            
            # Cleanup volume controller
            if self.volume_controller:
                self.volume_controller.cleanup()
                logger.info("Volume controller cleaned up")
            
            # Final diagnostics
            self._log_shutdown_diagnostics()
            
            # Cleanup diagnostic logger
            if self.diagnostic_logger:
                self.diagnostic_logger.cleanup()
            
            logger.info("Application stopped successfully")
            
        except Exception as e:
            logger.error(f"Error during application shutdown: {e}")
    
    def restart_components(self):
        """Restart components (useful for configuration changes)"""
        try:
            logger.info("Restarting application components...")
            
            # Stop components
            if self.mqtt_client:
                self.mqtt_client.stop()
            if self.system_monitor:
                self.system_monitor.stop_monitoring()
            
            # Reload configuration
            self.config_manager = ConfigManager(self.config_file)
            
            # Reinitialize components
            if self.initialize_components():
                # Restart services
                if self.system_monitor:
                    self.system_monitor.start_monitoring()
                
                # MQTT client will be restarted by the main thread
                logger.info("Components restarted successfully")
                return True
            else:
                logger.error("Failed to restart components")
                return False
                
        except Exception as e:
            logger.error(f"Error restarting components: {e}")
            if self.diagnostic_logger:
                self.diagnostic_logger.log_error_event("restart_error", str(e))
            return False
    
    def _log_startup_diagnostics(self):
        """Log diagnostic information at startup"""
        try:
            if not self.diagnostic_logger:
                return
            
            # System information
            system_info = {
                "python_version": sys.version.split()[0],
                "platform": sys.platform,
                "cpu_count": psutil.cpu_count(),
                "memory_total_gb": round(psutil.virtual_memory().total / (1024**3), 2),
                "components_initialized": {
                    "config_manager": self.config_manager is not None,
                    "volume_controller": self.volume_controller is not None,
                    "mqtt_client": self.mqtt_client is not None,
                    "system_monitor": self.system_monitor is not None,
                    "system_tray": self.system_tray is not None
                }
            }
            
            self.diagnostic_logger.log_diagnostic("startup", system_info)
            
            # Configuration summary
            config_summary = {
                "mqtt_broker": self.config_manager.get("mqtt.broker"),
                "mqtt_port": self.config_manager.get("mqtt.port"),
                "monitored_apps_count": len(self.config_manager.get_monitored_apps()),
                "system_monitoring_enabled": self.config_manager.get("settings.enable_system_monitoring"),
                "tray_enabled": self.config_manager.get("settings.enable_tray"),
                "debug_mode": self.config_manager.get("settings.debug")
            }
            
            self.diagnostic_logger.log_diagnostic("configuration", config_summary)
            
            # Component status
            component_status = {
                "volume_controller_device": self.volume_controller.get_audio_device_info() if self.volume_controller else None,
                "mqtt_connection_config": self.mqtt_client.get_connection_status() if self.mqtt_client else None,
                "system_monitor_active": self.system_monitor.is_monitoring() if self.system_monitor else False,
                "tray_available": self.system_tray.is_available() if self.system_tray else False
            }
            
            self.diagnostic_logger.log_diagnostic("component_status", component_status)
            
            logger.info("Startup diagnostics logged")
            
        except Exception as e:
            logger.error(f"Error logging startup diagnostics: {e}")
    
    def _log_shutdown_diagnostics(self):
        """Log diagnostic information at shutdown"""
        try:
            if not self.diagnostic_logger:
                return
            
            # Get final diagnostic summary
            diagnostics = self.diagnostic_logger.get_diagnostic_summary()
            
            shutdown_info = {
                "uptime": diagnostics.get("uptime_formatted", "unknown"),
                "total_errors": sum(diagnostics.get("error_counts", {}).values()),
                "performance_metrics_collected": len(diagnostics.get("performance_metrics", {})),
                "final_memory_usage_mb": diagnostics.get("process_info", {}).get("memory_usage_mb", 0)
            }
            
            self.diagnostic_logger.log_diagnostic("shutdown", shutdown_info)
            logger.info(f"Application shutdown - Uptime: {shutdown_info['uptime']}")
            
        except Exception as e:
            logger.error(f"Error logging shutdown diagnostics: {e}")
    
    def get_status(self):
        """Get current application status"""
        try:
            status = {
                "running": self.running,
                "components": {
                    "config_manager": self.config_manager is not None,
                    "volume_controller": self.volume_controller is not None,
                    "mqtt_client": self.mqtt_client is not None and self.mqtt_client.connected,
                    "system_monitor": self.system_monitor is not None and self.system_monitor.is_monitoring(),
                    "system_tray": self.system_tray is not None and self.system_tray.is_running()
                }
            }
            
            if self.diagnostic_logger:
                diagnostics = self.diagnostic_logger.get_diagnostic_summary()
                status["diagnostics"] = {
                    "uptime": diagnostics.get("uptime_formatted", "unknown"),
                    "memory_usage_mb": diagnostics.get("process_info", {}).get("memory_usage_mb", 0),
                    "error_count": sum(diagnostics.get("error_counts", {}).values())
                }
            
            return status
            
        except Exception as e:
            logger.error(f"Error getting application status: {e}")
            return {"error": str(e)}
    
    def is_running(self):
        """Check if application is running"""
        return self.running