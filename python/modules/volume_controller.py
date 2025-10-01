"""
Windows volume controller with per-application support.
"""

import sys
import time
import threading
import logging
from ctypes import cast, POINTER
from comtypes import CLSCTX_ALL
from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume, ISimpleAudioVolume

logger = logging.getLogger(__name__)


class WindowsVolumeController:
    """Enhanced Windows volume controller with per-application support"""
    
    def __init__(self, monitored_apps=None, update_rate_limit=0.1, sync_interval=2.0):
        """
        Initialize the volume controller
        
        Args:
            monitored_apps (list): List of application executables to monitor
            update_rate_limit (float): Minimum seconds between volume updates
            sync_interval (float): Seconds between volume sync checks
        """
        self.monitored_apps = monitored_apps or []
        self.update_rate_limit = update_rate_limit
        self.sync_interval = sync_interval
        
        self.devices = None
        self.interface = None
        self.volume = None
        self.last_update = 0
        self.current_volume = 0
        self.app_volumes = {}  # Store per-app volume controls
        self.last_sync = 0
        self.volume_lock = threading.Lock()
        
        try:
            self._initialize_audio()
            self._scan_applications()
            logger.info("Enhanced volume controller initialized successfully")
        except Exception as e:
            logger.error(f"Failed to initialize volume controller: {e}")
            raise
    
    def _initialize_audio(self):
        """Initialize audio interface"""
        try:
            # Get default audio device
            self.devices = AudioUtilities.GetSpeakers()
            self.interface = self.devices.Activate(
                IAudioEndpointVolume._iid_, 
                CLSCTX_ALL, 
                None
            )
            self.volume = cast(self.interface, POINTER(IAudioEndpointVolume))
            
            # Get current volume
            self.current_volume = int(self.volume.GetMasterVolumeLevelScalar() * 100)
            logger.info(f"Current system volume: {self.current_volume}%")
            
        except Exception as e:
            logger.error(f"Failed to initialize audio interface: {e}")
            raise
    
    def set_volume(self, level):
        """
        Set system volume level
        
        Args:
            level (int): Volume level 0-100
        
        Returns:
            bool: True if successful, False otherwise
        """
        try:
            # Rate limiting
            current_time = time.time()
            if current_time - self.last_update < self.update_rate_limit:
                return False
            
            # Validate range
            level = max(0, min(100, level))
            
            # Convert percentage to scalar (0.0 - 1.0)
            scalar_level = level / 100.0
            
            # Set volume
            self.volume.SetMasterVolumeLevelScalar(scalar_level, None)
            self.current_volume = level
            self.last_update = current_time
            
            logger.debug(f"Volume set to {level}%")
            return True
            
        except Exception as e:
            logger.error(f"Error setting volume: {e}")
            return False
    
    def get_volume(self):
        """
        Get current system volume level
        
        Returns:
            int: Current volume level 0-100
        """
        try:
            scalar = self.volume.GetMasterVolumeLevelScalar()
            return int(scalar * 100)
        except Exception as e:
            logger.error(f"Error getting volume: {e}")
            return self.current_volume
    
    def mute(self):
        """Mute system audio"""
        try:
            self.volume.SetMute(1, None)
            logger.info("Audio muted")
            return True
        except Exception as e:
            logger.error(f"Error muting audio: {e}")
            return False
    
    def unmute(self):
        """Unmute system audio"""
        try:
            self.volume.SetMute(0, None)
            logger.info("Audio unmuted")
            return True
        except Exception as e:
            logger.error(f"Error unmuting audio: {e}")
            return False
    
    def is_muted(self):
        """Check if audio is muted"""
        try:
            return bool(self.volume.GetMute())
        except Exception as e:
            logger.error(f"Error checking mute status: {e}")
            return False
    
    def _scan_applications(self):
        """Scan for audio applications and get their volume controls"""
        try:
            sessions = AudioUtilities.GetAllSessions()
            self.app_volumes.clear()
            
            for session in sessions:
                if session.Process and session.Process.name():
                    app_name = session.Process.name()
                    if app_name in self.monitored_apps:
                        volume_control = session._ctl.QueryInterface(ISimpleAudioVolume)
                        self.app_volumes[app_name] = {
                            'control': volume_control,
                            'session': session,
                            'volume': int(volume_control.GetMasterVolume() * 100)
                        }
                        logger.debug(f"Found audio app: {app_name}")
            
            logger.info(f"Monitoring {len(self.app_volumes)} audio applications")
            
        except Exception as e:
            logger.error(f"Error scanning applications: {e}")
    
    def get_app_volume(self, app_name):
        """Get volume for specific application"""
        try:
            if app_name in self.app_volumes:
                control = self.app_volumes[app_name]['control']
                volume = int(control.GetMasterVolume() * 100)
                self.app_volumes[app_name]['volume'] = volume
                return volume
            return None
        except Exception as e:
            logger.error(f"Error getting volume for {app_name}: {e}")
            return None
    
    def set_app_volume(self, app_name, level):
        """Set volume for specific application"""
        try:
            if app_name in self.app_volumes:
                level = max(0, min(100, level))
                scalar_level = level / 100.0
                
                control = self.app_volumes[app_name]['control']
                control.SetMasterVolume(scalar_level, None)
                self.app_volumes[app_name]['volume'] = level
                
                logger.info(f"Set {app_name} volume to {level}%")
                return True
            else:
                logger.warning(f"Application {app_name} not found")
                return False
        except Exception as e:
            logger.error(f"Error setting volume for {app_name}: {e}")
            return False
    
    def get_all_app_volumes(self):
        """Get volumes for all monitored applications"""
        volumes = {}
        for app_name in self.app_volumes:
            volume = self.get_app_volume(app_name)
            if volume is not None:
                volumes[app_name] = volume
        return volumes
    
    def refresh_applications(self):
        """Refresh the list of monitored applications"""
        self._scan_applications()
    
    def sync_volume_from_system(self):
        """Check if system volume changed externally and return new volume"""
        try:
            with self.volume_lock:
                current_time = time.time()
                if current_time - self.last_sync < self.sync_interval:
                    return None
                
                system_volume = self.get_volume()
                if abs(system_volume - self.current_volume) > 1:  # 1% threshold
                    self.current_volume = system_volume
                    self.last_sync = current_time
                    logger.debug(f"System volume changed externally to {system_volume}%")
                    return system_volume
                
                self.last_sync = current_time
                return None
        except Exception as e:
            logger.error(f"Error syncing volume: {e}")
            return None
    
    def update_monitored_apps(self, monitored_apps):
        """Update the list of monitored applications"""
        self.monitored_apps = monitored_apps or []
        self.refresh_applications()
        logger.info(f"Updated monitored apps: {self.monitored_apps}")
    
    def get_audio_device_info(self):
        """Get information about the current audio device"""
        try:
            if self.devices:
                return {
                    "device_name": str(self.devices),
                    "current_volume": self.get_volume(),
                    "is_muted": self.is_muted(),
                    "monitored_apps": len(self.app_volumes)
                }
            return None
        except Exception as e:
            logger.error(f"Error getting audio device info: {e}")
            return None
    
    def cleanup(self):
        """Clean up resources"""
        try:
            self.app_volumes.clear()
            logger.info("Volume controller cleaned up")
        except Exception as e:
            logger.error(f"Error during cleanup: {e}")