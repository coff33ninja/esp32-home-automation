"""
MQTT client for volume control communication.
"""

import json
import time
import threading
import logging
import paho.mqtt.client as mqtt

logger = logging.getLogger(__name__)


class MQTTVolumeClient:
    """Enhanced MQTT client with bidirectional sync and per-app control"""
    
    def __init__(self, volume_controller, config_manager):
        """
        Initialize MQTT client
        
        Args:
            volume_controller: Volume controller instance
            config_manager: Configuration manager instance
        """
        self.volume_controller = volume_controller
        self.config_manager = config_manager
        self.connected = False
        self.running = False
        self.sync_thread = None
        self.reconnect_thread = None
        self.system_monitor = None
        
        # Get configuration
        mqtt_config = config_manager.get_mqtt_config()
        self.topics = config_manager.get_topics()
        settings = config_manager.get_settings()
        
        self.broker = mqtt_config.get("broker")
        self.port = mqtt_config.get("port", 1883)
        self.username = mqtt_config.get("username", "")
        self.password = mqtt_config.get("password", "")
        self.client_id = mqtt_config.get("client_id", "PCVolumeControl")
        self.keepalive = mqtt_config.get("keepalive", 60)
        self.qos = mqtt_config.get("qos", 1)
        self.retain = mqtt_config.get("retain", True)
        
        self.sync_interval = settings.get("sync_interval", 2.0)
        self.reconnect_delay = settings.get("reconnect_delay", 5.0)
        
        # Initialize MQTT client
        self.client = mqtt.Client(client_id=self.client_id)
        
        # Set callbacks
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message
        
        # Set authentication if provided
        if self.username and self.password:
            self.client.username_pw_set(self.username, self.password)
        
        # Set will message for clean disconnection
        will_payload = json.dumps({"status": "offline", "client_id": self.client_id})
        self.client.will_set(self.topics.get("status"), will_payload, retain=self.retain)
        
        logger.info(f"MQTT client initialized for broker {self.broker}:{self.port}")
    
    def connect(self):
        """Connect to MQTT broker"""
        try:
            if not self.broker or self.broker == "192.168.1.xxx":
                logger.error("MQTT broker not configured")
                return False
                
            logger.info(f"Connecting to MQTT broker at {self.broker}:{self.port}...")
            self.client.connect(self.broker, self.port, self.keepalive)
            return True
        except Exception as e:
            logger.error(f"Failed to connect to MQTT broker: {e}")
            return False
    
    def on_connect(self, client, userdata, flags, rc):
        """Callback for when client connects to broker"""
        if rc == 0:
            self.connected = True
            logger.info("Connected to MQTT broker successfully")
            
            # Subscribe to topics
            topics_to_subscribe = [
                self.topics.get("volume"),
                self.topics.get("command"),
                self.topics.get("app_volume")
            ]
            
            for topic in topics_to_subscribe:
                if topic:
                    client.subscribe(topic, self.qos)
                    logger.info(f"Subscribed to {topic}")
            
            # Publish online status
            self.publish_status("online")
            
            # Start volume sync thread
            if not self.sync_thread or not self.sync_thread.is_alive():
                self.running = True
                self.sync_thread = threading.Thread(target=self._volume_sync_loop, daemon=True)
                self.sync_thread.start()
                logger.info("Volume sync thread started")
            
        else:
            logger.error(f"Connection failed with code {rc}")
            self.connected = False
    
    def on_disconnect(self, client, userdata, rc):
        """Callback for when client disconnects from broker"""
        self.connected = False
        if rc != 0:
            logger.warning(f"Unexpected disconnection (code: {rc}), attempting to reconnect...")
        else:
            logger.info("Disconnected from MQTT broker")
    
    def on_message(self, client, userdata, msg):
        """
        Callback for when a message is received
        
        Args:
            client: MQTT client instance
            userdata: User data
            msg: MQTT message
        """
        try:
            topic = msg.topic
            payload = msg.payload.decode('utf-8')
            
            logger.debug(f"Received message on {topic}: {payload}")
            
            # Handle volume topic
            if topic == self.topics.get("volume"):
                self._handle_volume_message(payload)
            
            # Handle command topic
            elif topic == self.topics.get("command"):
                self.handle_command(payload)
            
            # Handle per-app volume topic
            elif topic == self.topics.get("app_volume"):
                self.handle_app_volume_command(payload)
        
        except Exception as e:
            logger.error(f"Error processing message: {e}")
    
    def _handle_volume_message(self, payload):
        """Handle volume control messages"""
        try:
            volume_level = int(payload)
            if self.volume_controller.set_volume(volume_level):
                logger.info(f"Volume set to {volume_level}%")
        except ValueError:
            logger.warning(f"Invalid volume value: {payload}")
        except Exception as e:
            logger.error(f"Error handling volume message: {e}")
    
    def handle_command(self, command):
        """
        Handle special commands
        
        Args:
            command (str): Command string
        """
        try:
            command = command.upper()
            
            if command == "MUTE":
                self.volume_controller.mute()
            elif command == "UNMUTE":
                self.volume_controller.unmute()
            elif command == "STATUS":
                self.publish_status("online")
            elif command == "GETVOLUME":
                vol = self.volume_controller.get_volume()
                self.client.publish(self.topics.get("volume"), str(vol), qos=self.qos)
                logger.info(f"Published current volume: {vol}%")
            elif command == "REFRESH_APPS":
                self.volume_controller.refresh_applications()
                self.publish_app_volumes()
            elif command == "GET_APP_VOLUMES":
                self.publish_app_volumes()
            elif command == "GET_SYSTEM_INFO":
                if self.system_monitor:
                    self.system_monitor.publish_system_status()
            elif command == "SLEEP":
                logger.info("Sleep command received - notifying ESP32")
                self._publish_power_event("sleep_requested")
            else:
                logger.warning(f"Unknown command: {command}")
                
        except Exception as e:
            logger.error(f"Error handling command '{command}': {e}")
    
    def handle_app_volume_command(self, payload):
        """
        Handle per-application volume commands
        Format: {"app": "chrome.exe", "volume": 75}
        """
        try:
            data = json.loads(payload)
            app_name = data.get("app")
            volume = data.get("volume")
            
            if app_name and volume is not None:
                if self.volume_controller.set_app_volume(app_name, volume):
                    logger.info(f"Set {app_name} volume to {volume}%")
                    # Publish confirmation
                    self.publish_app_volume_status(app_name, volume)
                else:
                    logger.warning(f"Failed to set volume for {app_name}")
            else:
                logger.warning(f"Invalid app volume command format: {payload}")
                
        except json.JSONDecodeError:
            logger.warning(f"Invalid JSON in app volume command: {payload}")
        except Exception as e:
            logger.error(f"Error handling app volume command: {e}")
    
    def publish_status(self, status):
        """
        Publish PC status to MQTT
        
        Args:
            status (str): Status message
        """
        try:
            volume = self.volume_controller.get_volume()
            muted = self.volume_controller.is_muted()
            
            status_data = {
                "status": status,
                "volume": volume,
                "muted": muted,
                "client_id": self.client_id,
                "timestamp": time.time()
            }
            
            topic = self.topics.get("status")
            if topic:
                self.client.publish(topic, json.dumps(status_data), 
                                  qos=self.qos, retain=self.retain)
                logger.debug(f"Status published: {status_data}")
        except Exception as e:
            logger.error(f"Error publishing status: {e}")
    
    def publish_app_volumes(self):
        """Publish all application volumes"""
        try:
            app_volumes = self.volume_controller.get_all_app_volumes()
            if app_volumes:
                payload = json.dumps(app_volumes)
                topic = f"{self.topics.get('app_volume')}/status"
                self.client.publish(topic, payload, qos=self.qos)
                logger.debug(f"Published app volumes: {app_volumes}")
        except Exception as e:
            logger.error(f"Error publishing app volumes: {e}")
    
    def publish_app_volume_status(self, app_name, volume):
        """Publish individual app volume status"""
        try:
            data = {"app": app_name, "volume": volume, "timestamp": time.time()}
            topic = f"{self.topics.get('app_volume')}/status/{app_name}"
            self.client.publish(topic, json.dumps(data), qos=self.qos)
        except Exception as e:
            logger.error(f"Error publishing app volume status: {e}")
    
    def publish_volume_change(self, volume):
        """Publish volume change to ESP32"""
        try:
            topic = self.topics.get("pc_volume")
            if topic:
                self.client.publish(topic, str(volume), qos=self.qos)
                logger.debug(f"Published volume change: {volume}%")
        except Exception as e:
            logger.error(f"Error publishing volume change: {e}")
    
    def _publish_power_event(self, event_type, details=None):
        """Publish power event"""
        try:
            power_event = {
                "event": event_type,
                "timestamp": time.time()
            }
            if details:
                power_event.update(details)
            
            topic = self.topics.get("pc_power")
            if topic:
                self.client.publish(topic, json.dumps(power_event), qos=self.qos)
                logger.debug(f"Published power event: {power_event}")
        except Exception as e:
            logger.error(f"Error publishing power event: {e}")
    
    def _volume_sync_loop(self):
        """Background thread for volume synchronization"""
        logger.info("Volume sync loop started")
        
        while self.running:
            try:
                if self.connected:
                    # Check for external volume changes
                    new_volume = self.volume_controller.sync_volume_from_system()
                    if new_volume is not None:
                        # Publish volume change to ESP32
                        self.publish_volume_change(new_volume)
                        logger.info(f"Synced volume change to ESP32: {new_volume}%")
                    
                    # Refresh app list periodically (every 30 seconds)
                    if int(time.time()) % 30 == 0:
                        self.volume_controller.refresh_applications()
                
                time.sleep(self.sync_interval)
                
            except Exception as e:
                logger.error(f"Error in volume sync loop: {e}")
                time.sleep(self.sync_interval)
        
        logger.info("Volume sync loop stopped")
    
    def _reconnect_loop(self):
        """Background thread for automatic reconnection"""
        while self.running and not self.connected:
            try:
                logger.info("Attempting to reconnect...")
                if self.connect():
                    break
                time.sleep(self.reconnect_delay)
            except Exception as e:
                logger.error(f"Reconnection error: {e}")
                time.sleep(self.reconnect_delay)
    
    def start(self):
        """Start MQTT client with automatic reconnection"""
        self.running = True
        
        try:
            logger.info("Starting enhanced MQTT client...")
            
            # Start reconnection thread
            self.reconnect_thread = threading.Thread(target=self._reconnect_loop, daemon=True)
            self.reconnect_thread.start()
            
            # Start main loop
            self.client.loop_forever()
            
        except KeyboardInterrupt:
            logger.info("Shutting down...")
            self.stop()
        except Exception as e:
            logger.error(f"Error in MQTT loop: {e}")
        finally:
            self.stop()
    
    def stop(self):
        """Stop MQTT client and cleanup"""
        try:
            self.running = False
            if self.connected:
                self.publish_status("offline")
                self.client.disconnect()
            
            # Wait for threads to finish
            if self.sync_thread and self.sync_thread.is_alive():
                self.sync_thread.join(timeout=2)
            if self.reconnect_thread and self.reconnect_thread.is_alive():
                self.reconnect_thread.join(timeout=2)
            
            logger.info("MQTT client stopped")
        except Exception as e:
            logger.error(f"Error stopping MQTT client: {e}")
    
    def set_system_monitor(self, system_monitor):
        """Set system monitor reference"""
        self.system_monitor = system_monitor
    
    def publish_system_status(self, system_data):
        """Publish system status information"""
        try:
            topic = self.topics.get("pc_system")
            if topic:
                self.client.publish(topic, json.dumps(system_data), 
                                  qos=self.qos, retain=self.retain)
                logger.debug("System status published")
        except Exception as e:
            logger.error(f"Error publishing system status: {e}")
    
    def publish_power_event(self, event_type, details=None):
        """Public method to publish power events"""
        self._publish_power_event(event_type, details)
    
    def get_connection_status(self):
        """Get current connection status"""
        return {
            "connected": self.connected,
            "running": self.running,
            "broker": self.broker,
            "port": self.port,
            "client_id": self.client_id
        }
    
    def force_reconnect(self):
        """Force a reconnection attempt"""
        try:
            if self.connected:
                self.client.disconnect()
            self.connect()
        except Exception as e:
            logger.error(f"Error forcing reconnect: {e}")
    
    def publish_diagnostic_info(self, diagnostic_data):
        """Publish diagnostic information"""
        try:
            # Use system topic for diagnostics
            topic = self.topics.get("pc_system")
            if topic:
                diagnostic_payload = {
                    "type": "diagnostics",
                    "data": diagnostic_data,
                    "timestamp": time.time()
                }
                self.client.publish(f"{topic}/diagnostics", 
                                  json.dumps(diagnostic_payload), qos=self.qos)
                logger.debug("Diagnostic info published")
        except Exception as e:
            logger.error(f"Error publishing diagnostic info: {e}")