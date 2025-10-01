#!/usr/bin/env python3
"""
Main entry point for the ESP32 Volume Control application.
"""

import sys
import argparse
import logging
from modules.app import VolumeControlApp
from modules import __version__

# Set up basic logging before application starts
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    datefmt="%H:%M:%S",
)

logger = logging.getLogger(__name__)


def print_banner():
    """Print application banner"""
    banner = f"""
╔═════════════════════════════════════════════════════════╗
║                ESP32 Home Automation                    ║
║              Enhanced PC Volume Control                 ║
║                    Version {__version__}                        ║
╚═════════════════════════════════════════════════════════╝

Features:
• Bidirectional volume control with ESP32
• Per-application volume control  
• MQTT integration with auto-reconnection
• System monitoring and power event detection
• Sleep/wake detection and notification
• System tray integration (optional)
• Configurable settings and validation

Author: DJ Kruger
"""
    print(banner)


def main():
    """Main entry point"""
    # Parse command line arguments
    parser = argparse.ArgumentParser(
        description="ESP32 Home Automation - Enhanced PC Volume Control",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          # Run with default settings
  %(prog)s --config my_config.json  # Use custom configuration file
  %(prog)s --no-tray                # Run without system tray
  %(prog)s --debug                  # Enable debug logging
  %(prog)s --tray --debug           # Force tray with debug logging

Configuration:
  The application uses a JSON configuration file for settings.
  If no config file exists, a default one will be created.
  
  Default config file: volume_control_config.json
        """,
    )

    parser.add_argument(
        "--config",
        "-c",
        default="volume_control_config.json",
        help="Configuration file path (default: volume_control_config.json)",
    )

    parser.add_argument(
        "--tray",
        action="store_true",
        help="Force enable system tray interface (if available)",
    )

    parser.add_argument(
        "--no-tray", action="store_true", help="Force disable system tray interface"
    )

    parser.add_argument("--debug", action="store_true", help="Enable debug logging")

    parser.add_argument(
        "--quiet",
        "-q",
        action="store_true",
        help="Suppress banner and reduce console output",
    )

    parser.add_argument(
        "--version", action="version", version=f"ESP32 Volume Control v{__version__}"
    )

    # Parse arguments
    args = parser.parse_args()

    # Validate argument combinations
    if args.tray and args.no_tray:
        parser.error("Cannot specify both --tray and --no-tray")

    # Print banner unless quiet mode
    if not args.quiet:
        print_banner()

    # Initialize application
    try:
        logger.info("Initializing ESP32 Volume Control Application...")

        # Create application instance
        app = VolumeControlApp(config_file=args.config)

        # Initialize components first so we can access config_manager
        if not app.initialize_components():
            logger.error("Failed to initialize application components")
            return 1

        # Override configuration based on command line arguments
        if args.debug:
            # Enable debug mode in configuration
            app.config_manager.set("settings.debug", True)
            app.config_manager.set("settings.log_level", "DEBUG")
            logger.info("Debug mode enabled via command line")

        if args.tray:
            app.config_manager.set("settings.enable_tray", True)
            logger.info("System tray force enabled via command line")
        elif args.no_tray:
            app.config_manager.set("settings.enable_tray", False)
            logger.info("System tray disabled via command line")

        # Determine tray setting
        enable_tray = not args.no_tray
        if args.tray:
            enable_tray = True

        # Log startup information
        logger.info(f"Configuration file: {args.config}")
        logger.info(f"System tray: {'enabled' if enable_tray else 'disabled'}")
        logger.info(f"Debug mode: {'enabled' if args.debug else 'disabled'}")

        # Start the application
        logger.info("Starting application...")
        success = app.start(enable_tray=enable_tray)

        if success:
            logger.info("Application started successfully")
            return 0
        else:
            logger.error("Application failed to start")
            return 1

    except KeyboardInterrupt:
        logger.info("Application interrupted by user")
        return 0

    except FileNotFoundError as e:
        logger.error(f"Configuration file not found: {e}")
        logger.info("A default configuration file will be created on next run")
        return 1

    except PermissionError as e:
        logger.error(f"Permission denied: {e}")
        logger.info("Try running as administrator or check file permissions")
        return 1

    except ImportError as e:
        logger.error(f"Missing required dependency: {e}")
        logger.info("Please install required packages: pip install -r requirements.txt")
        return 1

    except Exception as e:
        logger.error(f"Fatal error: {e}")
        if args.debug:
            import traceback

            logger.error(f"Traceback: {traceback.format_exc()}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
