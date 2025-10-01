"""
System tray interface for volume control.
"""

import logging
from .constants import TRAY_AVAILABLE

# Optional imports for system tray
if TRAY_AVAILABLE:
    try:
        import pystray
        from PIL import Image, ImageDraw
    except ImportError:
        TRAY_AVAILABLE = False

logger = logging.getLogger(__name__)


class SystemTrayApp:
    """System tray application for volume control"""
    
    def __init__(self, mqtt_client, volume_controller, config_manager):
        """
        Initialize system tray application
        
        Args:
            mqtt_client: MQTT client instance
            volume_controller: Volume controller instance
            config_manager: Configuration manager instance
        """
        self.mqtt_client = mqtt_client
        self.volume_controller = volume_controller
        self.config_manager = config_manager
        self.icon = None
        self.running = False
        
        # Get configuration
        settings = config_manager.get_settings()
        self.enable_tray = settings.get("enable_tray", True)
        
        if not TRAY_AVAILABLE:
            logger.warning("System tray not available - pystray/PIL not installed")
            return
        
        if not self.enable_tray:
            logger.info("System tray disabled in configuration")
            return
        
        # Create tray icon
        self.create_icon()
        logger.info("System tray application initialized")
    
    def create_icon(self):
        """Create system tray icon with dynamic volume indicator"""
        if not TRAY_AVAILABLE:
            return
        
        try:
            # Create icon image with current volume level
            image = self._create_volume_icon()
            
            # Create comprehensive menu
            menu = pystray.Menu(
                pystray.MenuItem("Volume Control", self.show_volume_dialog),
                pystray.MenuItem("Mute/Unmute", self.toggle_mute),
                pystray.Menu.SEPARATOR,
                pystray.MenuItem("App Volumes", pystray.Menu(
                    pystray.MenuItem("Refresh Apps", self.refresh_apps),
                    pystray.MenuItem("Show App Volumes", self.show_app_volumes)
                )),
                pystray.Menu.SEPARATOR,
                pystray.MenuItem("MQTT", pystray.Menu(
                    pystray.MenuItem("Connection Status", self.show_mqtt_status),
                    pystray.MenuItem("Reconnect", self.force_mqtt_reconnect),
                    pystray.MenuItem("Publish Status", self.publish_status)
                )),
                pystray.MenuItem("System", pystray.Menu(
                    pystray.MenuItem("System Info", self.show_system_info),
                    pystray.MenuItem("Publish System Status", self.publish_system_status)
                )),
                pystray.Menu.SEPARATOR,
                pystray.MenuItem("Settings", self.show_settings),
                pystray.MenuItem("About", self.show_about),
                pystray.Menu.SEPARATOR,
                pystray.MenuItem("Exit", self.quit_application)
            )
            
            self.icon = pystray.Icon("ESP32VolumeControl", image, "ESP32 Volume Control", menu)
            logger.info("System tray icon created")
            
        except Exception as e:
            logger.error(f"Error creating tray icon: {e}")
            self.icon = None
    
    def _create_volume_icon(self):
        """Create icon image with volume level indicator"""
        try:
            # Create base image
            image = Image.new('RGB', (64, 64), color='blue')
            draw = ImageDraw.Draw(image)
            
            # Draw speaker base
            draw.rectangle([10, 20, 25, 45], fill='white')  # Speaker body
            draw.polygon([(25, 25), (35, 15), (35, 50), (25, 40)], fill='white')  # Speaker cone
            
            # Get current volume and mute status
            volume = self.volume_controller.get_volume()
            is_muted = self.volume_controller.is_muted()
            
            if is_muted:
                # Draw mute indicator (X)
                draw.line([(40, 20), (55, 35)], fill='red', width=3)
                draw.line([(40, 35), (55, 20)], fill='red', width=3)
            else:
                # Draw volume level bars
                bars = int(volume / 25)  # 0-4 bars
                for i in range(4):  # Always draw 4 bar positions
                    x = 40 + i * 4
                    height = 5 + i * 3
                    y1 = 32 - height // 2
                    y2 = 32 + height // 2
                    
                    if i < bars:
                        # Active bar
                        draw.rectangle([x, y1, x + 2, y2], fill='white')
                    else:
                        # Inactive bar (dimmed)
                        draw.rectangle([x, y1, x + 2, y2], fill='gray')
            
            return image
            
        except Exception as e:
            logger.error(f"Error creating volume icon: {e}")
            # Return simple fallback icon
            image = Image.new('RGB', (64, 64), color='blue')
            draw = ImageDraw.Draw(image)
            draw.rectangle([10, 10, 54, 54], fill='white')
            return image
    
    def update_icon(self):
        """Update tray icon to reflect current volume and mute status"""
        if not TRAY_AVAILABLE or not self.icon:
            return
        
        try:
            # Create updated icon image
            new_image = self._create_volume_icon()
            self.icon.icon = new_image
            logger.debug("Tray icon updated")
            
        except Exception as e:
            logger.error(f"Error updating tray icon: {e}")
    
    def show_volume_dialog(self, icon, item):
        """Show current volume information"""
        try:
            current_volume = self.volume_controller.get_volume()
            is_muted = self.volume_controller.is_muted()
            mute_status = " (MUTED)" if is_muted else ""
            
            logger.info(f"Current volume: {current_volume}%{mute_status}")
            
            # Also show app volumes
            app_volumes = self.volume_controller.get_all_app_volumes()
            if app_volumes:
                logger.info(f"App volumes: {app_volumes}")
            
            # Update icon to reflect current state
            self.update_icon()
            
        except Exception as e:
            logger.error(f"Error showing volume dialog: {e}")
    
    def toggle_mute(self, icon, item):
        """Toggle mute state"""
        try:
            if self.volume_controller.is_muted():
                self.volume_controller.unmute()
                logger.info("Audio unmuted via tray")
            else:
                self.volume_controller.mute()
                logger.info("Audio muted via tray")
            
            # Update icon to reflect new mute state
            self.update_icon()
            
        except Exception as e:
            logger.error(f"Error toggling mute: {e}")
    
    def refresh_apps(self, icon, item):
        """Refresh monitored applications"""
        try:
            self.volume_controller.refresh_applications()
            if hasattr(self.mqtt_client, 'publish_app_volumes'):
                self.mqtt_client.publish_app_volumes()
            logger.info("Applications refreshed via tray")
            
        except Exception as e:
            logger.error(f"Error refreshing apps: {e}")
    
    def show_app_volumes(self, icon, item):
        """Show current application volumes"""
        try:
            app_volumes = self.volume_controller.get_all_app_volumes()
            if app_volumes:
                logger.info(f"Application volumes: {app_volumes}")
                for app, volume in app_volumes.items():
                    print(f"  {app}: {volume}%")
            else:
                logger.info("No monitored applications found")
                print("No monitored applications currently running")
                
        except Exception as e:
            logger.error(f"Error showing app volumes: {e}")
    
    def show_mqtt_status(self, icon, item):
        """Show MQTT connection status"""
        try:
            if hasattr(self.mqtt_client, 'get_connection_status'):
                status_info = self.mqtt_client.get_connection_status()
                status = "Connected" if status_info.get("connected") else "Disconnected"
                broker = status_info.get("broker", "Unknown")
                port = status_info.get("port", "Unknown")
                
                logger.info(f"MQTT Status: {status} to {broker}:{port}")
                print(f"MQTT Status: {status}")
                print(f"Broker: {broker}:{port}")
                print(f"Client ID: {status_info.get('client_id', 'Unknown')}")
            else:
                status = "Connected" if self.mqtt_client.connected else "Disconnected"
                logger.info(f"MQTT Status: {status}")
                print(f"MQTT Status: {status}")
                
        except Exception as e:
            logger.error(f"Error showing MQTT status: {e}")
    
    def force_mqtt_reconnect(self, icon, item):
        """Force MQTT reconnection"""
        try:
            if hasattr(self.mqtt_client, 'force_reconnect'):
                self.mqtt_client.force_reconnect()
                logger.info("MQTT reconnection requested via tray")
            else:
                logger.warning("MQTT reconnection not available")
                
        except Exception as e:
            logger.error(f"Error forcing MQTT reconnect: {e}")
    
    def publish_status(self, icon, item):
        """Publish current status to MQTT"""
        try:
            self.mqtt_client.publish_status("online")
            logger.info("Status published to MQTT via tray")
            
        except Exception as e:
            logger.error(f"Error publishing status: {e}")
    
    def show_system_info(self, icon, item):
        """Show system information"""
        try:
            if hasattr(self.mqtt_client, 'system_monitor') and self.mqtt_client.system_monitor:
                metrics = self.mqtt_client.system_monitor.get_system_metrics()
                logger.info("System information:")
                print("System Information:")
                print(f"  CPU: {metrics.get('cpu_percent', 'N/A')}%")
                print(f"  Memory: {metrics.get('memory', {}).get('percent', 'N/A')}%")
                print(f"  Power State: {metrics.get('power_state', 'N/A')}")
                print(f"  Processes: {metrics.get('process_count', 'N/A')}")
            else:
                logger.warning("System monitor not available")
                print("System monitor not available")
                
        except Exception as e:
            logger.error(f"Error showing system info: {e}")
    
    def publish_system_status(self, icon, item):
        """Publish system status to MQTT"""
        try:
            if hasattr(self.mqtt_client, 'system_monitor') and self.mqtt_client.system_monitor:
                self.mqtt_client.system_monitor.publish_system_status()
                logger.info("System status published to MQTT via tray")
            else:
                logger.warning("System monitor not available")
                
        except Exception as e:
            logger.error(f"Error publishing system status: {e}")
    
    def show_settings(self, icon, item):
        """Show settings information"""
        try:
            logger.info("Settings information:")
            print("Current Settings:")
            
            # Show MQTT settings
            mqtt_config = self.config_manager.get_mqtt_config()
            print(f"  MQTT Broker: {mqtt_config.get('broker', 'N/A')}")
            print(f"  MQTT Port: {mqtt_config.get('port', 'N/A')}")
            
            # Show monitored apps
            apps = self.config_manager.get_monitored_apps()
            print(f"  Monitored Apps: {len(apps)} configured")
            
            # Show settings
            settings = self.config_manager.get_settings()
            print(f"  Debug Mode: {settings.get('debug', False)}")
            print(f"  System Monitoring: {settings.get('enable_system_monitoring', False)}")
            
        except Exception as e:
            logger.error(f"Error showing settings: {e}")
    
    def show_about(self, icon, item):
        """Show about information"""
        try:
            about_text = """ESP32 Home Automation - PC Volume Control
Version: 2.0.0
Author: DJ Kruger

Features:
- Bidirectional volume control with ESP32
- Per-application volume control
- MQTT integration with auto-reconnection
- System monitoring and power event detection
- Sleep/wake detection and notification
- System tray integration
- Configurable settings and validation

Dependencies:
- pycaw (Windows audio control)
- paho-mqtt (MQTT communication)
- psutil (system monitoring)
- pystray & PIL (system tray, optional)
- pywin32 & WMI (Windows monitoring, optional)"""
            
            logger.info("About dialog requested")
            print(about_text)
            
        except Exception as e:
            logger.error(f"Error showing about: {e}")
    
    def quit_application(self, icon, item):
        """Quit the application"""
        try:
            logger.info("Application quit requested via tray")
            self.running = False
            
            # Stop the icon
            if self.icon:
                self.icon.stop()
            
            # Signal MQTT client to stop
            if hasattr(self.mqtt_client, 'stop'):
                self.mqtt_client.stop()
            else:
                self.mqtt_client.running = False
                
        except Exception as e:
            logger.error(f"Error quitting application: {e}")
    
    def start(self):
        """Start system tray interface"""
        if not TRAY_AVAILABLE:
            logger.warning("Cannot start system tray - not available")
            return False
        
        if not self.enable_tray:
            logger.info("System tray disabled in configuration")
            return False
        
        if not self.icon:
            logger.error("Cannot start system tray - icon not created")
            return False
        
        try:
            self.running = True
            logger.info("Starting system tray interface...")
            self.icon.run()  # This blocks until quit
            return True
            
        except Exception as e:
            logger.error(f"Error starting system tray: {e}")
            return False
    
    def stop(self):
        """Stop system tray interface"""
        try:
            self.running = False
            if self.icon:
                self.icon.stop()
            logger.info("System tray interface stopped")
            
        except Exception as e:
            logger.error(f"Error stopping system tray: {e}")
    
    def is_available(self):
        """Check if system tray is available"""
        return TRAY_AVAILABLE and self.enable_tray and self.icon is not None
    
    def is_running(self):
        """Check if system tray is running"""
        return self.running