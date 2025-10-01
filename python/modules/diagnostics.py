"""
Diagnostics and logging system for volume control.
"""

import json
import time
import threading
import logging
import logging.handlers
import psutil
import os
from datetime import datetime

logger = logging.getLogger(__name__)


class DiagnosticLogger:
    """Enhanced logging with diagnostics, file rotation, and performance monitoring"""
    
    def __init__(self, config_manager):
        """
        Initialize diagnostic logger
        
        Args:
            config_manager: Configuration manager instance
        """
        self.config = config_manager
        self.performance_metrics = {}
        self.error_counts = {}
        self.diagnostics = {}
        self.start_time = time.time()
        self.lock = threading.Lock()
        
        # Get configuration
        settings = config_manager.get_settings()
        diagnostics_config = config_manager.get("diagnostics", {})
        
        self.enable_performance_monitoring = diagnostics_config.get("enable_performance_monitoring", True)
        self.collect_system_metrics = diagnostics_config.get("collect_system_metrics", True)
        self.diagnostic_interval = diagnostics_config.get("diagnostic_interval", 300)
        
        # Setup logging
        self.setup_logging()
        
        # Start diagnostic collection if enabled
        if self.collect_system_metrics:
            self._start_system_metrics_collection()
        
        logger.info("Diagnostic logger initialized")
    
    def setup_logging(self):
        """Setup enhanced logging with file rotation"""
        try:
            settings = self.config.get_settings()
            
            log_level = settings.get("log_level", "DEBUG")
            log_file = settings.get("log_file", "volume_control.log")
            max_size = settings.get("max_log_size_mb", 10) * 1024 * 1024
            backup_count = settings.get("backup_log_count", 3)
            debug_mode = settings.get("debug", True)
            
            # Create formatter
            formatter = logging.Formatter(
                '%(asctime)s - %(name)s - %(levelname)s - %(funcName)s:%(lineno)d - %(message)s',
                datefmt='%Y-%m-%d %H:%M:%S'
            )
            
            # Setup file handler with rotation
            file_handler = logging.handlers.RotatingFileHandler(
                log_file, 
                maxBytes=max_size, 
                backupCount=backup_count,
                encoding='utf-8'
            )
            file_handler.setFormatter(formatter)
            
            # Setup console handler
            console_handler = logging.StreamHandler()
            console_formatter = logging.Formatter(
                '%(asctime)s - %(levelname)s - %(message)s',
                datefmt='%H:%M:%S'
            )
            console_handler.setFormatter(console_formatter)
            
            # Configure root logger
            root_logger = logging.getLogger()
            
            # Set log level
            if debug_mode:
                log_level_obj = logging.DEBUG
            else:
                log_level_obj = getattr(logging, log_level.upper(), logging.INFO)
            
            root_logger.setLevel(log_level_obj)
            file_handler.setLevel(log_level_obj)
            console_handler.setLevel(log_level_obj)
            
            # Clear existing handlers and add new ones
            root_logger.handlers.clear()
            root_logger.addHandler(file_handler)
            root_logger.addHandler(console_handler)
            
            # Log startup information
            self.log_system_info()
            self.log_config_info()
            
            logger.info(f"Enhanced logging initialized - Level: {log_level}, File: {log_file}")
            
        except Exception as e:
            print(f"Error setting up enhanced logging: {e}")
            # Fallback to basic logging
            logging.basicConfig(
                level=logging.DEBUG,
                format='%(asctime)s - %(levelname)s - %(message)s'
            )
    
    def log_system_info(self):
        """Log system information at startup"""
        try:
            import platform
            
            system_info = {
                "platform": platform.platform(),
                "python_version": platform.python_version(),
                "cpu_count": psutil.cpu_count(),
                "memory_total_gb": psutil.virtual_memory().total / (1024**3),
                "disk_total_gb": psutil.disk_usage('/').total / (1024**3) if os.name != 'nt' else psutil.disk_usage('C:').total / (1024**3),
                "boot_time": datetime.fromtimestamp(psutil.boot_time()).isoformat()
            }
            
            logger.info(f"System Information: {json.dumps(system_info, indent=2)}")
            
        except Exception as e:
            logger.error(f"Error logging system info: {e}")
    
    def log_config_info(self):
        """Log configuration information at startup"""
        try:
            # Log sanitized configuration (without sensitive data)
            config_info = {
                "mqtt_broker": self.config.get("mqtt.broker", "Not configured"),
                "mqtt_port": self.config.get("mqtt.port", 1883),
                "monitored_apps_count": len(self.config.get_monitored_apps()),
                "debug_mode": self.config.get("settings.debug", False),
                "system_monitoring": self.config.get("settings.enable_system_monitoring", True),
                "tray_enabled": self.config.get("settings.enable_tray", True)
            }
            
            logger.info(f"Configuration: {json.dumps(config_info, indent=2)}")
            
        except Exception as e:
            logger.error(f"Error logging config info: {e}")
    
    def log_performance_metric(self, metric_name, value, unit=""):
        """Log performance metric"""
        if not self.enable_performance_monitoring:
            return
        
        try:
            with self.lock:
                timestamp = time.time()
                if metric_name not in self.performance_metrics:
                    self.performance_metrics[metric_name] = []
                
                self.performance_metrics[metric_name].append({
                    "timestamp": timestamp,
                    "value": value,
                    "unit": unit
                })
                
                # Keep only last 100 entries per metric
                if len(self.performance_metrics[metric_name]) > 100:
                    self.performance_metrics[metric_name] = self.performance_metrics[metric_name][-100:]
                
                logger.debug(f"Performance metric: {metric_name} = {value} {unit}")
                
        except Exception as e:
            logger.error(f"Error logging performance metric: {e}")
    
    def log_error_event(self, error_type, error_message, context=None):
        """Log error event with context"""
        try:
            with self.lock:
                timestamp = time.time()
                
                if error_type not in self.error_counts:
                    self.error_counts[error_type] = 0
                self.error_counts[error_type] += 1
                
                error_data = {
                    "timestamp": timestamp,
                    "error_type": error_type,
                    "message": str(error_message),
                    "count": self.error_counts[error_type],
                    "context": context or {}
                }
                
                logger.error(f"Error event: {json.dumps(error_data)}")
                
        except Exception as e:
            logger.error(f"Error logging error event: {e}")
    
    def log_diagnostic(self, category, data):
        """Log diagnostic information"""
        try:
            with self.lock:
                timestamp = time.time()
                if category not in self.diagnostics:
                    self.diagnostics[category] = []
                
                self.diagnostics[category].append({
                    "timestamp": timestamp,
                    "data": data
                })
                
                # Keep only last 100 entries per category
                if len(self.diagnostics[category]) > 100:
                    self.diagnostics[category] = self.diagnostics[category][-100:]
                
                logger.debug(f"Diagnostic logged - {category}: {data}")
                
        except Exception as e:
            logger.error(f"Error logging diagnostic: {e}")
    
    def get_diagnostic_summary(self):
        """Get comprehensive diagnostic summary"""
        try:
            with self.lock:
                uptime = time.time() - self.start_time
                
                # Get current process info
                process = psutil.Process()
                
                summary = {
                    "timestamp": time.time(),
                    "uptime_seconds": uptime,
                    "uptime_formatted": self._format_uptime(uptime),
                    "error_counts": self.error_counts.copy(),
                    "diagnostic_categories": list(self.diagnostics.keys()),
                    "total_diagnostic_entries": sum(len(entries) for entries in self.diagnostics.values()),
                    "performance_metrics": {},
                    "process_info": {
                        "memory_usage_mb": process.memory_info().rss / 1024 / 1024,
                        "cpu_percent": process.cpu_percent(),
                        "thread_count": threading.active_count(),
                        "open_files": len(process.open_files()) if hasattr(process, 'open_files') else 0
                    }
                }
                
                # Add latest performance metrics
                for metric_name, values in self.performance_metrics.items():
                    if values:
                        latest = values[-1]
                        # Calculate average for last 10 entries
                        recent_values = [entry["value"] for entry in values[-10:]]
                        avg_value = sum(recent_values) / len(recent_values)
                        
                        summary["performance_metrics"][metric_name] = {
                            "latest_value": latest["value"],
                            "average_recent": avg_value,
                            "unit": latest["unit"],
                            "sample_count": len(values)
                        }
                
                # Add system metrics if enabled
                if self.collect_system_metrics:
                    summary["system_metrics"] = self._get_current_system_metrics()
                
                return summary
                
        except Exception as e:
            logger.error(f"Error getting diagnostic summary: {e}")
            return {"error": str(e)}
    
    def _get_current_system_metrics(self):
        """Get current system metrics"""
        try:
            memory = psutil.virtual_memory()
            disk = psutil.disk_usage('C:' if os.name == 'nt' else '/')
            network = psutil.net_io_counters()
            
            return {
                "cpu_percent": psutil.cpu_percent(interval=1),
                "memory_percent": memory.percent,
                "memory_available_gb": memory.available / (1024**3),
                "disk_percent": disk.percent,
                "disk_free_gb": disk.free / (1024**3),
                "network_bytes_sent": network.bytes_sent,
                "network_bytes_recv": network.bytes_recv,
                "process_count": len(psutil.pids())
            }
        except Exception as e:
            logger.error(f"Error getting system metrics: {e}")
            return {}
    
    def _format_uptime(self, seconds):
        """Format uptime in human readable format"""
        days = int(seconds // 86400)
        hours = int((seconds % 86400) // 3600)
        minutes = int((seconds % 3600) // 60)
        secs = int(seconds % 60)
        return f"{days}d {hours}h {minutes}m {secs}s"
    
    def _start_system_metrics_collection(self):
        """Start background system metrics collection"""
        def collect_metrics():
            while True:
                try:
                    # Collect system metrics
                    metrics = self._get_current_system_metrics()
                    for metric_name, value in metrics.items():
                        if isinstance(value, (int, float)):
                            unit = "%" if "percent" in metric_name else ("GB" if "gb" in metric_name else "")
                            self.log_performance_metric(f"system.{metric_name}", value, unit)
                    
                    time.sleep(self.diagnostic_interval)
                    
                except Exception as e:
                    logger.error(f"Error in system metrics collection: {e}")
                    time.sleep(60)  # Wait a minute before retrying
        
        metrics_thread = threading.Thread(target=collect_metrics, daemon=True)
        metrics_thread.start()
        logger.info("System metrics collection started")
    
    def rotate_logs(self):
        """Manually trigger log rotation"""
        try:
            for handler in logging.getLogger().handlers:
                if isinstance(handler, logging.handlers.RotatingFileHandler):
                    handler.doRollover()
                    logger.info("Log rotation triggered manually")
                    break
        except Exception as e:
            logger.error(f"Error rotating logs: {e}")
    
    def get_log_files(self):
        """Get list of log files"""
        try:
            log_file = self.config.get("settings.log_file", "volume_control.log")
            log_files = [log_file]
            
            # Add rotated log files
            backup_count = self.config.get("settings.backup_log_count", 3)
            for i in range(1, backup_count + 1):
                backup_file = f"{log_file}.{i}"
                if os.path.exists(backup_file):
                    log_files.append(backup_file)
            
            return log_files
        except Exception as e:
            logger.error(f"Error getting log files: {e}")
            return []
    
    def cleanup(self):
        """Cleanup diagnostic logger"""
        try:
            logger.info("Diagnostic logger shutting down")
            # Final diagnostic summary
            summary = self.get_diagnostic_summary()
            logger.info(f"Final diagnostic summary: {json.dumps(summary, indent=2)}")
        except Exception as e:
            logger.error(f"Error during diagnostic logger cleanup: {e}")