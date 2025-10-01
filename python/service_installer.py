"""
Windows Service Installer for ESP32 Volume Control
=================================================

This script allows the volume control application to run as a Windows service,
ensuring it starts automatically with the system and runs in the background.

Usage:
    python service_installer.py install    # Install service
    python service_installer.py remove     # Remove service
    python service_installer.py start      # Start service
    python service_installer.py stop       # Stop service
    python service_installer.py restart    # Restart service

Requirements:
    pip install pywin32

Author: DJ Kruger
Version: 1.0.0
"""

import sys
import os
import time
import logging
import win32serviceutil
import win32service
import win32event
import servicemanager
from pathlib import Path

# Add current directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    from volume_control import VolumeControlApp
except ImportError:
    print("Error: Could not import volume_control module")
    sys.exit(1)

class ESP32VolumeControlService(win32serviceutil.ServiceFramework):
    """Windows service for ESP32 Volume Control"""
    
    _svc_name_ = "ESP32VolumeControl"
    _svc_display_name_ = "ESP32 Home Automation Volume Control"
    _svc_description_ = "Provides bidirectional volume control between PC and ESP32 home automation system"
    
    def __init__(self, args):
        win32serviceutil.ServiceFramework.__init__(self, args)
        self.hWaitStop = win32event.CreateEvent(None, 0, 0, None)
        self.app = None
        self.running = False
        
        # Setup service logging
        self.setup_service_logging()
        
    def setup_service_logging(self):
        """Setup logging for the service"""
        try:
            # Create logs directory if it doesn't exist
            log_dir = Path(__file__).parent / "logs"
            log_dir.mkdir(exist_ok=True)
            
            # Setup service-specific logging
            log_file = log_dir / "service.log"
            
            logging.basicConfig(
                level=logging.INFO,
                format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
                handlers=[
                    logging.FileHandler(log_file),
                    logging.StreamHandler()
                ]
            )
            
            self.logger = logging.getLogger("ESP32VolumeService")
            self.logger.info("Service logging initialized")
            
        except Exception as e:
            # Fallback to event log if file logging fails
            servicemanager.LogErrorMsg(f"Failed to setup file logging: {e}")
    
    def SvcStop(self):
        """Stop the service"""
        try:
            self.logger.info("Service stop requested")
            self.ReportServiceStatus(win32service.SERVICE_STOP_PENDING)
            
            # Signal the main thread to stop
            self.running = False
            win32event.SetEvent(self.hWaitStop)
            
            # Stop the application
            if self.app:
                self.app.cleanup()
            
            self.logger.info("Service stopped successfully")
            
        except Exception as e:
            self.logger.error(f"Error stopping service: {e}")
            servicemanager.LogErrorMsg(f"Error stopping service: {e}")
    
    def SvcDoRun(self):
        """Main service execution"""
        try:
            self.logger.info("ESP32 Volume Control Service starting...")
            servicemanager.LogMsg(
                servicemanager.EVENTLOG_INFORMATION_TYPE,
                servicemanager.PYS_SERVICE_STARTED,
                (self._svc_name_, '')
            )
            
            self.running = True
            self.main()
            
        except Exception as e:
            self.logger.error(f"Service execution error: {e}")
            servicemanager.LogErrorMsg(f"Service execution error: {e}")
    
    def main(self):
        """Main service logic"""
        try:
            self.logger.info("Initializing Volume Control Application")
            
            # Create application instance (no tray in service mode)
            config_file = os.path.join(os.path.dirname(__file__), "volume_control_config.json")
            self.app = VolumeControlApp(config_file=config_file, enable_tray=False)
            
            # Initialize the application
            if not self.app.initialize():
                self.logger.error("Failed to initialize application")
                return
            
            self.logger.info("Application initialized, starting main loop")
            
            # Start the application in a separate thread
            import threading
            app_thread = threading.Thread(target=self._run_app, daemon=True)
            app_thread.start()
            
            # Wait for stop signal
            while self.running:
                # Wait for stop event or timeout
                rc = win32event.WaitForSingleObject(self.hWaitStop, 5000)  # 5 second timeout
                
                if rc == win32event.WAIT_OBJECT_0:
                    # Stop event was signaled
                    break
                elif rc == win32event.WAIT_TIMEOUT:
                    # Timeout - check if app is still running
                    if not app_thread.is_alive():
                        self.logger.warning("Application thread died, restarting...")
                        app_thread = threading.Thread(target=self._run_app, daemon=True)
                        app_thread.start()
                
                # Log periodic status
                if int(time.time()) % 300 == 0:  # Every 5 minutes
                    self.logger.info("Service running normally")
            
            self.logger.info("Service main loop ended")
            
        except Exception as e:
            self.logger.error(f"Error in service main loop: {e}")
            servicemanager.LogErrorMsg(f"Service main loop error: {e}")
    
    def _run_app(self):
        """Run the application (called in separate thread)"""
        try:
            self.logger.info("Starting application thread")
            
            # Start MQTT client
            if self.app.mqtt_client:
                self.app.mqtt_client.start()
            
        except Exception as e:
            self.logger.error(f"Error in application thread: {e}")

class ServiceManager:
    """Manage the Windows service"""
    
    @staticmethod
    def install_service():
        """Install the service"""
        try:
            print("Installing ESP32 Volume Control Service...")
            
            # Get the path to the Python executable and this script
            python_exe = sys.executable
            script_path = os.path.abspath(__file__)
            
            # Install the service
            win32serviceutil.InstallService(
                ESP32VolumeControlService,
                ESP32VolumeControlService._svc_name_,
                ESP32VolumeControlService._svc_display_name_,
                description=ESP32VolumeControlService._svc_description_,
                startType=win32service.SERVICE_AUTO_START,
                exeName=python_exe,
                exeArgs=f'"{script_path}"'
            )
            
            print("Service installed successfully!")
            print("Use 'python service_installer.py start' to start the service")
            
        except Exception as e:
            print(f"Error installing service: {e}")
            return False
        
        return True
    
    @staticmethod
    def remove_service():
        """Remove the service"""
        try:
            print("Removing ESP32 Volume Control Service...")
            
            # Stop the service first if it's running
            try:
                win32serviceutil.StopService(ESP32VolumeControlService._svc_name_)
                print("Service stopped")
            except Exception:
                pass  # Service might not be running
            
            # Remove the service
            win32serviceutil.RemoveService(ESP32VolumeControlService._svc_name_)
            print("Service removed successfully!")
            
        except Exception as e:
            print(f"Error removing service: {e}")
            return False
        
        return True
    
    @staticmethod
    def start_service():
        """Start the service"""
        try:
            print("Starting ESP32 Volume Control Service...")
            win32serviceutil.StartService(ESP32VolumeControlService._svc_name_)
            print("Service started successfully!")
            
        except Exception as e:
            print(f"Error starting service: {e}")
            return False
        
        return True
    
    @staticmethod
    def stop_service():
        """Stop the service"""
        try:
            print("Stopping ESP32 Volume Control Service...")
            win32serviceutil.StopService(ESP32VolumeControlService._svc_name_)
            print("Service stopped successfully!")
            
        except Exception as e:
            print(f"Error stopping service: {e}")
            return False
        
        return True
    
    @staticmethod
    def restart_service():
        """Restart the service"""
        try:
            print("Restarting ESP32 Volume Control Service...")
            ServiceManager.stop_service()
            time.sleep(2)  # Wait a moment
            ServiceManager.start_service()
            print("Service restarted successfully!")
            
        except Exception as e:
            print(f"Error restarting service: {e}")
            return False
        
        return True
    
    @staticmethod
    def get_service_status():
        """Get service status"""
        try:
            import win32service
            
            # Open service manager
            scm = win32service.OpenSCManager(None, None, win32service.SC_MANAGER_ENUMERATE_SERVICE)
            
            try:
                # Open the service
                service = win32service.OpenService(
                    scm, 
                    ESP32VolumeControlService._svc_name_, 
                    win32service.SERVICE_QUERY_STATUS
                )
                
                try:
                    # Query service status
                    status = win32service.QueryServiceStatus(service)
                    
                    state_map = {
                        win32service.SERVICE_STOPPED: "Stopped",
                        win32service.SERVICE_START_PENDING: "Starting",
                        win32service.SERVICE_STOP_PENDING: "Stopping",
                        win32service.SERVICE_RUNNING: "Running",
                        win32service.SERVICE_CONTINUE_PENDING: "Continuing",
                        win32service.SERVICE_PAUSE_PENDING: "Pausing",
                        win32service.SERVICE_PAUSED: "Paused"
                    }
                    
                    state = state_map.get(status[1], f"Unknown ({status[1]})")
                    print(f"Service Status: {state}")
                    return state
                    
                finally:
                    win32service.CloseServiceHandle(service)
            finally:
                win32service.CloseServiceHandle(scm)
                
        except Exception as e:
            print(f"Error getting service status: {e}")
            return None

def main():
    """Main entry point"""
    if len(sys.argv) == 1:
        # No arguments - try to run as service
        try:
            servicemanager.Initialize()
            servicemanager.PrepareToHostSingle(ESP32VolumeControlService)
            servicemanager.StartServiceCtrlDispatcher()
        except win32service.error as details:
            if details.winerror == 1063:  # The service process could not connect to the service controller
                print("This script should be run as a Windows service or with command line arguments.")
                print("Usage:")
                print("  python service_installer.py install    # Install service")
                print("  python service_installer.py remove     # Remove service")
                print("  python service_installer.py start      # Start service")
                print("  python service_installer.py stop       # Stop service")
                print("  python service_installer.py restart    # Restart service")
                print("  python service_installer.py status     # Get service status")
            else:
                print(f"Service error: {details}")
    else:
        # Handle command line arguments
        command = sys.argv[1].lower()
        
        if command == "install":
            ServiceManager.install_service()
        elif command == "remove":
            ServiceManager.remove_service()
        elif command == "start":
            ServiceManager.start_service()
        elif command == "stop":
            ServiceManager.stop_service()
        elif command == "restart":
            ServiceManager.restart_service()
        elif command == "status":
            ServiceManager.get_service_status()
        else:
            print(f"Unknown command: {command}")
            print("Valid commands: install, remove, start, stop, restart, status")

if __name__ == "__main__":
    main()