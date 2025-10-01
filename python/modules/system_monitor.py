"""
PC system monitoring for power events and system status.
"""

import time
import threading
import logging
import psutil
from .constants import WINDOWS_MONITORING

# Optional Windows-specific imports
if WINDOWS_MONITORING:
    try:
        import wmi
    except ImportError:
        WINDOWS_MONITORING = False

logger = logging.getLogger(__name__)


class PCSystemMonitor:
    """Monitor PC system events and status"""
    
    def __init__(self, mqtt_client, config_manager):
        """
        Initialize system monitor
        
        Args:
            mqtt_client: MQTT client instance
            config_manager: Configuration manager instance
        """
        self.mqtt_client = mqtt_client
        self.config_manager = config_manager
        self.monitoring = False
        self.monitor_thread = None
        self.last_power_state = "unknown"
        self.system_info = {}
        self.last_activity_time = time.time()
        self.sleep_start_time = None
        
        # Get configuration
        settings = config_manager.get_settings()
        power_config = config_manager.get("power_management", {})
        
        self.monitor_interval = settings.get("monitor_interval", 5)
        self.status_publish_interval = settings.get("status_publish_interval", 60)
        self.detect_sleep_wake = power_config.get("detect_sleep_wake", True)
        self.idle_threshold_minutes = power_config.get("idle_threshold_minutes", 30)
        self.sleep_detection_method = power_config.get("sleep_detection_method", "activity")
        
        # Initialize WMI if available
        if WINDOWS_MONITORING and self.sleep_detection_method == "wmi":
            try:
                self.wmi = wmi.WMI()
                self._get_system_info()
                logger.info("PC system monitor initialized with WMI support")
            except Exception as e:
                logger.error(f"Failed to initialize WMI: {e}")
                self.wmi = None
                self.sleep_detection_method = "activity"
        else:
            self.wmi = None
            if not WINDOWS_MONITORING:
                logger.warning("Windows monitoring libraries not available")
        
        # Get initial system info
        self._get_system_info()
        logger.info(f"PC system monitor initialized (method: {self.sleep_detection_method})")
    
    def _get_system_info(self):
        """Get system information"""
        try:
            # Basic system info using psutil
            self.system_info = {
                "platform": "Windows",
                "boot_time": psutil.boot_time(),
                "cpu_count": psutil.cpu_count(),
                "cpu_count_logical": psutil.cpu_count(logical=True)
            }
            
            # Enhanced info with WMI if available
            if self.wmi:
                try:
                    # Get computer system info
                    for computer in self.wmi.Win32_ComputerSystem():
                        self.system_info.update({
                            "computer_name": computer.Name,
                            "manufacturer": computer.Manufacturer,
                            "model": computer.Model,
                            "total_memory": int(computer.TotalPhysicalMemory) if computer.TotalPhysicalMemory else 0
                        })
                    
                    # Get OS info
                    for os_info in self.wmi.Win32_OperatingSystem():
                        self.system_info.update({
                            "os_name": os_info.Caption,
                            "os_version": os_info.Version,
                            "os_architecture": os_info.OSArchitecture
                        })
                    
                    # Get CPU info
                    for processor in self.wmi.Win32_Processor():
                        self.system_info.update({
                            "cpu_name": processor.Name,
                            "cpu_cores": processor.NumberOfCores,
                            "cpu_threads": processor.NumberOfLogicalProcessors
                        })
                        break  # Just get first processor
                        
                except Exception as e:
                    logger.warning(f"Error getting WMI system info: {e}")
            
            logger.debug(f"System info collected: {self.system_info}")
            
        except Exception as e:
            logger.error(f"Error getting system info: {e}")
    
    def get_power_state(self):
        """Get current power state"""
        try:
            if self.sleep_detection_method == "wmi" and self.wmi:
                return self._get_power_state_wmi()
            else:
                return self._get_power_state_activity()
                
        except Exception as e:
            logger.error(f"Error getting power state: {e}")
            return "unknown"
    
    def _get_power_state_activity(self):
        """Get power state based on system activity"""
        try:
            # Get CPU usage and memory info
            cpu_percent = psutil.cpu_percent(interval=1)
            memory = psutil.virtual_memory()
            
            # Check idle time
            current_time = time.time()
            idle_minutes = (current_time - self.last_activity_time) / 60
            
            # Determine power state based on activity
            if idle_minutes > self.idle_threshold_minutes:
                return "idle"
            elif cpu_percent < 5 and memory.percent < 50:
                return "low_activity"
            elif cpu_percent > 80:
                return "active_high"
            else:
                return "active"
                
        except Exception as e:
            logger.error(f"Error getting activity-based power state: {e}")
            return "unknown"
    
    def _get_power_state_wmi(self):
        """Get power state using WMI (more accurate but Windows-specific)"""
        try:
            # This is a placeholder for more sophisticated WMI-based detection
            # In practice, you'd query Win32_PowerManagementEvent or similar
            return self._get_power_state_activity()
        except Exception as e:
            logger.error(f"Error getting WMI power state: {e}")
            return "unknown"
    
    def publish_system_status(self):
        """Publish comprehensive system status"""
        try:
            # Get current system metrics
            cpu_percent = psutil.cpu_percent(interval=1)
            memory = psutil.virtual_memory()
            
            # Get disk usage (try C: drive first, fallback to root)
            try:
                disk = psutil.disk_usage('C:')
            except Exception:
                disk = psutil.disk_usage('/')
            
            # Get network info
            network = psutil.net_io_counters()
            
            # Get process count
            process_count = len(psutil.pids())
            
            # Get uptime
            boot_time = psutil.boot_time()
            uptime_seconds = time.time() - boot_time
            
            status_data = {
                "timestamp": time.time(),
                "power_state": self.get_power_state(),
                "uptime_seconds": uptime_seconds,
                "cpu_percent": cpu_percent,
                "memory_percent": memory.percent,
                "memory_available_gb": memory.available / (1024**3),
                "memory_total_gb": memory.total / (1024**3),
                "disk_percent": disk.percent,
                "disk_free_gb": disk.free / (1024**3),
                "disk_total_gb": disk.total / (1024**3),
                "process_count": process_count,
                "network_bytes_sent": network.bytes_sent,
                "network_bytes_recv": network.bytes_recv,
                "system_info": self.system_info,
                "monitoring_config": {
                    "detection_method": self.sleep_detection_method,
                    "idle_threshold_minutes": self.idle_threshold_minutes,
                    "monitor_interval": self.monitor_interval
                }
            }
            
            # Publish through MQTT client
            self.mqtt_client.publish_system_status(status_data)
            logger.debug("System status published")
            
        except Exception as e:
            logger.error(f"Error publishing system status: {e}")
    
    def detect_power_events(self):
        """Enhanced sleep/wake event detection"""
        try:
            if not self.detect_sleep_wake:
                return
                
            current_state = self.get_power_state()
            
            if current_state != self.last_power_state:
                logger.info(f"Power state changed: {self.last_power_state} -> {current_state}")
                
                # Publish power event
                power_event = {
                    "event": "state_change",
                    "from_state": self.last_power_state,
                    "to_state": current_state,
                    "timestamp": time.time(),
                    "system_uptime": time.time() - psutil.boot_time(),
                    "detection_method": self.sleep_detection_method
                }
                
                self.mqtt_client.publish_power_event("state_change", power_event)
                
                # Special handling for sleep events
                if current_state in ["sleep", "hibernate", "idle"] and self.last_power_state in ["active", "active_high", "low_activity"]:
                    logger.info("System entering sleep/idle state - notifying ESP32")
                    self.sleep_start_time = time.time()
                    
                    sleep_event = {
                        "event": "sleep_detected",
                        "sleep_type": current_state,
                        "timestamp": time.time(),
                        "last_activity": self._get_last_activity_time(),
                        "idle_duration": (time.time() - self.last_activity_time) / 60
                    }
                    self.mqtt_client.publish_power_event("sleep_detected", sleep_event)
                
                # Special handling for wake events
                if self.last_power_state in ["sleep", "hibernate", "idle"] and current_state in ["active", "active_high", "low_activity"]:
                    logger.info("System wake detected - notifying ESP32")
                    
                    wake_event = {
                        "event": "wake_detected",
                        "wake_source": "user_activity",
                        "timestamp": time.time(),
                        "sleep_duration": self._calculate_sleep_duration()
                    }
                    self.mqtt_client.publish_power_event("wake_detected", wake_event)
                    
                    # Update activity time
                    self.last_activity_time = time.time()
                    
                    # Refresh applications after wake
                    if hasattr(self.mqtt_client, 'volume_controller'):
                        self.mqtt_client.volume_controller.refresh_applications()
                        # Publish current volume state
                        vol = self.mqtt_client.volume_controller.get_volume()
                        self.mqtt_client.publish_volume_change(vol)
                    
                    # Publish updated system status
                    self.publish_system_status()
                
                self.last_power_state = current_state
                
        except Exception as e:
            logger.error(f"Error detecting power events: {e}")
    
    def _get_last_activity_time(self):
        """Get timestamp of last user activity"""
        try:
            # For now, return the stored last activity time
            # In a more sophisticated implementation, we could check:
            # - Last mouse/keyboard input
            # - Last file system activity
            # - Network activity patterns
            return self.last_activity_time
        except Exception as e:
            logger.error(f"Error getting last activity time: {e}")
            return time.time()
    
    def _calculate_sleep_duration(self):
        """Calculate how long the system was asleep"""
        try:
            if self.sleep_start_time:
                duration = time.time() - self.sleep_start_time
                self.sleep_start_time = None
                return duration
            else:
                # Fallback calculation based on uptime vs expected uptime
                return 0
        except Exception as e:
            logger.error(f"Error calculating sleep duration: {e}")
            return 0
    
    def update_activity_time(self):
        """Update the last activity time (can be called externally)"""
        self.last_activity_time = time.time()
    
    def start_monitoring(self):
        """Start system monitoring"""
        if not self.monitoring:
            self.monitoring = True
            self.monitor_thread = threading.Thread(target=self._monitor_loop, daemon=True)
            self.monitor_thread.start()
            logger.info("System monitoring started")
        else:
            logger.warning("System monitoring already running")
    
    def stop_monitoring(self):
        """Stop system monitoring"""
        if self.monitoring:
            self.monitoring = False
            if self.monitor_thread and self.monitor_thread.is_alive():
                self.monitor_thread.join(timeout=5)
            logger.info("System monitoring stopped")
        else:
            logger.warning("System monitoring not running")
    
    def _monitor_loop(self):
        """Main monitoring loop"""
        last_status_publish = 0
        
        while self.monitoring:
            try:
                current_time = time.time()
                
                # Detect power events
                self.detect_power_events()
                
                # Publish system status periodically
                if current_time - last_status_publish >= self.status_publish_interval:
                    self.publish_system_status()
                    last_status_publish = current_time
                
                time.sleep(self.monitor_interval)
                
            except Exception as e:
                logger.error(f"Error in monitor loop: {e}")
                time.sleep(self.monitor_interval)
        
        logger.info("System monitor loop ended")
    
    def get_system_metrics(self):
        """Get current system metrics"""
        try:
            return {
                "cpu_percent": psutil.cpu_percent(interval=1),
                "memory": psutil.virtual_memory()._asdict(),
                "disk": psutil.disk_usage('C:' if psutil.WINDOWS else '/')._asdict(),
                "network": psutil.net_io_counters()._asdict(),
                "boot_time": psutil.boot_time(),
                "process_count": len(psutil.pids()),
                "power_state": self.get_power_state()
            }
        except Exception as e:
            logger.error(f"Error getting system metrics: {e}")
            return {}
    
    def is_monitoring(self):
        """Check if monitoring is active"""
        return self.monitoring