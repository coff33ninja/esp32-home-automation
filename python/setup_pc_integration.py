"""
ESP32 Volume Control - PC Integration Setup Script
=================================================

This script sets up the complete PC integration system including:
- Installing required dependencies
- Configuring the system
- Setting up logging
- Creating desktop shortcuts
- Optionally installing as Windows service

Usage:
    python setup_pc_integration.py [--service] [--config-only] [--uninstall]

Options:
    --service       Install as Windows service
    --config-only   Only create/update configuration files
    --uninstall     Uninstall the system

Author: DJ Kruger
Version: 1.0.0
"""

import sys
import json
import argparse
import subprocess
import shutil
from pathlib import Path
import logging

# Setup basic logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class PCIntegrationSetup:
    """Setup and configuration manager for PC integration"""
    
    def __init__(self):
        self.script_dir = Path(__file__).parent
        self.config_file = self.script_dir / "volume_control_config.json"
        self.log_dir = self.script_dir / "logs"
        
    def run_setup(self, install_service=False, config_only=False, uninstall=False):
        """Run the complete setup process"""
        try:
            print("=" * 60)
            print("ESP32 Volume Control - PC Integration Setup")
            print("=" * 60)
            
            if uninstall:
                return self.uninstall_system()
            
            if config_only:
                return self.setup_configuration()
            
            # Full installation
            success = True
            
            # Step 1: Check Python version
            print("1. Checking Python version...")
            if not self.check_python_version():
                return False
            
            # Step 2: Install dependencies
            print("\n2. Installing Python dependencies...")
            if not self.install_dependencies():
                success = False
            
            # Step 3: Setup configuration
            print("\n3. Setting up configuration...")
            if not self.setup_configuration():
                success = False
            
            # Step 4: Setup logging
            print("\n4. Setting up logging...")
            if not self.setup_logging():
                success = False
            
            # Step 5: Create shortcuts
            print("\n5. Creating desktop shortcuts...")
            if not self.create_shortcuts():
                success = False
            
            # Step 6: Install service (optional)
            if install_service:
                print("\n6. Installing Windows service...")
                if not self.install_service():
                    success = False
            
            # Step 7: Test installation
            print("\n7. Testing installation...")
            if not self.test_installation():
                success = False
            
            if success:
                print("\n" + "=" * 60)
                print("Setup completed successfully!")
                print("=" * 60)
                self.print_usage_instructions(install_service)
            else:
                print("\n" + "=" * 60)
                print("Setup completed with some errors. Please check the output above.")
                print("=" * 60)
            
            return success
            
        except Exception as e:
            logger.error(f"Setup failed: {e}")
            return False
    
    def check_python_version(self):
        """Check if Python version is compatible"""
        try:
            version = sys.version_info
            if version.major < 3 or (version.major == 3 and version.minor < 7):
                print(f"❌ Python {version.major}.{version.minor} is not supported. Python 3.7+ required.")
                return False
            
            print(f"✅ Python {version.major}.{version.minor}.{version.micro} is compatible")
            return True
            
        except Exception as e:
            logger.error(f"Error checking Python version: {e}")
            return False
    
    def install_dependencies(self):
        """Install required Python packages"""
        try:
            requirements_file = self.script_dir / "requirements.txt"
            
            if not requirements_file.exists():
                print("❌ requirements.txt not found")
                return False
            
            # Install packages
            cmd = [sys.executable, "-m", "pip", "install", "-r", str(requirements_file)]
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode == 0:
                print("✅ Dependencies installed successfully")
                return True
            else:
                print(f"❌ Failed to install dependencies: {result.stderr}")
                return False
                
        except Exception as e:
            logger.error(f"Error installing dependencies: {e}")
            return False
    
    def setup_configuration(self):
        """Setup configuration files"""
        try:
            # Create default configuration if it doesn't exist
            if not self.config_file.exists():
                config = self.get_default_config()
                
                # Interactive configuration
                print("\nConfiguring MQTT settings...")
                broker = input(f"MQTT Broker IP [{config['mqtt']['broker']}]: ").strip()
                if broker:
                    config['mqtt']['broker'] = broker
                
                port = input(f"MQTT Port [{config['mqtt']['port']}]: ").strip()
                if port:
                    try:
                        config['mqtt']['port'] = int(port)
                    except ValueError:
                        print("Invalid port number, using default")
                
                username = input("MQTT Username (optional): ").strip()
                if username:
                    config['mqtt']['username'] = username
                    password = input("MQTT Password (optional): ").strip()
                    if password:
                        config['mqtt']['password'] = password
                
                # Save configuration
                with open(self.config_file, 'w') as f:
                    json.dump(config, f, indent=2)
                
                print(f"✅ Configuration saved to {self.config_file}")
            else:
                print(f"✅ Configuration file already exists: {self.config_file}")
            
            return True
            
        except Exception as e:
            logger.error(f"Error setting up configuration: {e}")
            return False
    
    def get_default_config(self):
        """Get default configuration"""
        return {
            "mqtt": {
                "broker": "192.168.1.100",
                "port": 1883,
                "username": "",
                "password": "",
                "client_id": "PCVolumeControl",
                "keepalive": 60,
                "qos": 1,
                "retain": True
            },
            "topics": {
                "volume": "homecontrol/volume",
                "command": "homecontrol/command",
                "status": "homecontrol/pc/status",
                "pc_volume": "homecontrol/pc/volume",
                "app_volume": "homecontrol/pc/app_volume",
                "pc_power": "homecontrol/pc/power",
                "pc_system": "homecontrol/pc/system"
            },
            "apps": [
                "chrome.exe",
                "firefox.exe",
                "msedge.exe",
                "spotify.exe",
                "discord.exe",
                "vlc.exe",
                "winamp.exe",
                "foobar2000.exe",
                "musicbee.exe",
                "steam.exe"
            ],
            "settings": {
                "update_rate_limit": 0.1,
                "sync_interval": 2.0,
                "reconnect_delay": 5.0,
                "debug": True,
                "enable_system_monitoring": True,
                "enable_tray": True,
                "monitor_interval": 5,
                "status_publish_interval": 60,
                "log_level": "INFO",
                "log_file": "volume_control.log",
                "max_log_size_mb": 10,
                "backup_log_count": 3
            },
            "power_management": {
                "detect_sleep_wake": True,
                "notify_esp32_on_sleep": True,
                "notify_esp32_on_wake": True,
                "idle_threshold_minutes": 30,
                "sleep_detection_method": "activity"
            },
            "diagnostics": {
                "enable_performance_monitoring": True,
                "collect_system_metrics": True,
                "publish_diagnostics": True,
                "diagnostic_interval": 300
            }
        }
    
    def setup_logging(self):
        """Setup logging directories and configuration"""
        try:
            # Create logs directory
            self.log_dir.mkdir(exist_ok=True)
            
            # Create logging configuration if it doesn't exist
            logging_config_file = self.script_dir / "logging_config.json"
            if logging_config_file.exists():
                print(f"✅ Logging configuration exists: {logging_config_file}")
            else:
                print("❌ Logging configuration not found")
                return False
            
            print(f"✅ Logging directory created: {self.log_dir}")
            return True
            
        except Exception as e:
            logger.error(f"Error setting up logging: {e}")
            return False
    
    def create_shortcuts(self):
        """Create desktop shortcuts"""
        try:
            desktop = Path.home() / "Desktop"
            
            # Create batch files for easy launching
            batch_files = [
                ("ESP32 Volume Control.bat", "python volume_control.py --tray"),
                ("ESP32 Volume Control (Console).bat", "python volume_control.py --no-tray"),
                ("ESP32 Diagnostics.bat", "python diagnostics.py"),
                ("ESP32 Service Install.bat", "python service_installer.py install"),
                ("ESP32 Service Remove.bat", "python service_installer.py remove")
            ]
            
            for filename, command in batch_files:
                batch_file = self.script_dir / filename
                with open(batch_file, 'w') as f:
                    f.write("@echo off\n")
                    f.write(f"cd /d \"{self.script_dir}\"\n")
                    f.write(f"{command}\n")
                    f.write("pause\n")
                
                # Copy to desktop if it exists
                if desktop.exists():
                    try:
                        shutil.copy2(batch_file, desktop / filename)
                    except Exception:
                        pass  # Don't fail if we can't copy to desktop
            
            print("✅ Batch files created")
            return True
            
        except Exception as e:
            logger.error(f"Error creating shortcuts: {e}")
            return False
    
    def install_service(self):
        """Install Windows service"""
        try:
            service_script = self.script_dir / "service_installer.py"
            
            if not service_script.exists():
                print("❌ Service installer not found")
                return False
            
            # Install service
            cmd = [sys.executable, str(service_script), "install"]
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode == 0:
                print("✅ Windows service installed successfully")
                
                # Ask if user wants to start the service
                start_service = input("Start the service now? (y/n): ").strip().lower()
                if start_service == 'y':
                    cmd = [sys.executable, str(service_script), "start"]
                    result = subprocess.run(cmd, capture_output=True, text=True)
                    if result.returncode == 0:
                        print("✅ Service started successfully")
                    else:
                        print(f"❌ Failed to start service: {result.stderr}")
                
                return True
            else:
                print(f"❌ Failed to install service: {result.stderr}")
                return False
                
        except Exception as e:
            logger.error(f"Error installing service: {e}")
            return False
    
    def test_installation(self):
        """Test the installation"""
        try:
            # Test imports
            print("Testing imports...")
            
            test_imports = [
                "pycaw.pycaw",
                "paho.mqtt.client",
                "psutil",
                "pystray",
                "PIL"
            ]
            
            failed_imports = []
            for module in test_imports:
                try:
                    __import__(module)
                    print(f"  ✅ {module}")
                except ImportError:
                    print(f"  ❌ {module}")
                    failed_imports.append(module)
            
            if failed_imports:
                print(f"❌ Some imports failed: {failed_imports}")
                print("You may need to install additional packages or run with --service flag")
                return False
            
            # Test configuration
            if self.config_file.exists():
                with open(self.config_file, 'r') as f:
                    _ = json.load(f)
                print("✅ Configuration file is valid JSON")
            else:
                print("❌ Configuration file not found")
                return False
            
            print("✅ Installation test passed")
            return True
            
        except Exception as e:
            logger.error(f"Error testing installation: {e}")
            return False
    
    def uninstall_system(self):
        """Uninstall the system"""
        try:
            print("Uninstalling ESP32 Volume Control PC Integration...")
            
            # Stop and remove service if installed
            service_script = self.script_dir / "service_installer.py"
            if service_script.exists():
                try:
                    cmd = [sys.executable, str(service_script), "stop"]
                    subprocess.run(cmd, capture_output=True)
                    
                    cmd = [sys.executable, str(service_script), "remove"]
                    result = subprocess.run(cmd, capture_output=True, text=True)
                    if result.returncode == 0:
                        print("✅ Windows service removed")
                    else:
                        print("ℹ️ Service was not installed or already removed")
                except Exception:
                    pass
            
            # Remove desktop shortcuts
            desktop = Path.home() / "Desktop"
            if desktop.exists():
                batch_files = [
                    "ESP32 Volume Control.bat",
                    "ESP32 Volume Control (Console).bat",
                    "ESP32 Diagnostics.bat",
                    "ESP32 Service Install.bat",
                    "ESP32 Service Remove.bat"
                ]
                
                for filename in batch_files:
                    shortcut = desktop / filename
                    if shortcut.exists():
                        shortcut.unlink()
                        print(f"✅ Removed desktop shortcut: {filename}")
            
            # Remove local batch files
            for filename in batch_files:
                batch_file = self.script_dir / filename
                if batch_file.exists():
                    batch_file.unlink()
            
            print("✅ Uninstallation completed")
            print("Note: Configuration files and logs were preserved")
            print("You can manually delete them if desired")
            
            return True
            
        except Exception as e:
            logger.error(f"Error during uninstallation: {e}")
            return False
    
    def print_usage_instructions(self, service_installed=False):
        """Print usage instructions"""
        print("\nUsage Instructions:")
        print("-" * 40)
        
        if service_installed:
            print("The system has been installed as a Windows service.")
            print("It will start automatically when Windows boots.")
            print("\nService Management:")
            print("  - Start service:   python service_installer.py start")
            print("  - Stop service:    python service_installer.py stop")
            print("  - Remove service:  python service_installer.py remove")
        else:
            print("Manual Operation:")
            print("  - With system tray: python volume_control.py --tray")
            print("  - Console mode:     python volume_control.py --no-tray")
        
        print("\nDiagnostics:")
        print("  - Run diagnostics:  python diagnostics.py")
        print("  - View logs:        Check volume_control.log")
        
        print("\nConfiguration:")
        print(f"  - Config file:      {self.config_file}")
        print(f"  - Log directory:    {self.log_dir}")
        
        print("\nDesktop shortcuts have been created for easy access.")

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="ESP32 Volume Control PC Integration Setup")
    parser.add_argument("--service", action="store_true", help="Install as Windows service")
    parser.add_argument("--config-only", action="store_true", help="Only create/update configuration")
    parser.add_argument("--uninstall", action="store_true", help="Uninstall the system")
    
    args = parser.parse_args()
    
    setup = PCIntegrationSetup()
    success = setup.run_setup(
        install_service=args.service,
        config_only=args.config_only,
        uninstall=args.uninstall
    )
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()