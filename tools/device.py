import requests
import time
import threading
from dataclasses import dataclass
from typing import Dict, List, Union, Tuple, Optional, Callable, Iterable

@dataclass
class Parameter:
    name: str
    min_value: int
    max_value: int
    default_value: int
    display_name: str
    current_value: int = None
    
    def __repr__(self):
        return f"{self.display_name} ({self.name}): {self.current_value} [{self.min_value}-{self.max_value}]"

# Dictionary containing all parameters defined in project_parameters.h
# Removed "PARAM_" prefix and sorted by name
PARAMETERS_DICT = {
    "AUTO_MODE": {"min_value": 0, "max_value": 1, "default_value": 1, "display_name": "auto_mode"},
    "CLOSE_SERVO_REGULATION": {"min_value": 0, "max_value": 99, "default_value": 50, "display_name": "close_servo_regulation"},
    "CLOSE_SERVO_REGULATION_FLAG": {"min_value": 0, "max_value": 1, "default_value": 0, "display_name": "close_servo_regulation_flag"},
    "CORRECTION_FACTOR": {"min_value": 0, "max_value": 200, "default_value": 100, "display_name": "correction_factor"},
    "CURRENT_MOTOR": {"min_value": 0, "max_value": 0xFFFF, "default_value": 0, "display_name": "current_motor"},
    "DISTANCE_HM": {"min_value": 0, "max_value": 0xffff, "default_value": 0, "display_name": "distance_hm"},
    "ERROR_MOTOR": {"min_value": 0, "max_value": 1, "default_value": 1, "display_name": "error_motor"},
    "ERROR_MOTOR_CALIBRATION": {"min_value": 0, "max_value": 99, "default_value": 50, "display_name": "error_motor_calibration"},
    "ERROR_SERVO": {"min_value": 0, "max_value": 1, "default_value": 1, "display_name": "error_servo"},
    "ERROR_SERVO_CALIBRATION": {"min_value": 0, "max_value": 99, "default_value": 20, "display_name": "error_servo_calibration"},
    "GRAIN_PER_HECTARE": {"min_value": 0, "max_value": 250, "default_value": 50, "display_name": "grain_per_hectare"},
    "HIGH_OF_MACHINE_CM": {"min_value": 0, "max_value": 1000, "default_value": 50, "display_name": "hight_of_machine"},
    "LANGUAGE": {"min_value": 0, "max_value": 3, "default_value": 0, "display_name": "language"},
    "LOW_LEVEL_SILOS": {"min_value": 0, "max_value": 1, "default_value": 0, "display_name": "low_level_silos"},
    "MACHINE_ERRORS": {"min_value": 0, "max_value": 0xFFFF, "default_value": 0, "display_name": "machine_errors"},
    "MOTOR": {"min_value": 0, "max_value": 100, "default_value": 0, "display_name": "motor"},
    "MOTOR2": {"min_value": 0, "max_value": 100, "default_value": 0, "display_name": "motor_2"},
    "MOTOR_IS_ON": {"min_value": 0, "max_value": 1, "default_value": 0, "display_name": "motor_is_on"},
    "MOTOR_MAX_CALIBRATION": {"min_value": 0, "max_value": 100, "default_value": 100, "display_name": "motor_max_calibration"},
    "MOTOR_MIN_CALIBRATION": {"min_value": 0, "max_value": 100, "default_value": 20, "display_name": "motor_min_calibration"},
    "MOTOR_RPM_PER_100": {"min_value": 0, "max_value": 5000, "default_value": 0, "display_name": "motor_rpm"},
    "OPEN_SERVO_REGULATION": {"min_value": 0, "max_value": 99, "default_value": 50, "display_name": "open_servo_regulation"},
    "OPEN_SERVO_REGULATION_FLAG": {"min_value": 0, "max_value": 1, "default_value": 0, "display_name": "open_servo_regulation_flag"},
    "PERIOD": {"min_value": 0, "max_value": 180, "default_value": 10, "display_name": "period"},
    "RESET_DISTANCE": {"min_value": 0, "max_value": 1, "default_value": 0, "display_name": "reset_distance"},
    "SEEDING_IS_ACTIVE": {"min_value": 0, "max_value": 1, "default_value": 0, "display_name": "seeding_is_active"},
    "SEEDING_START_SPEED_KMH": {"min_value": 1, "max_value": 100, "default_value": 2, "display_name": "seeding_start_speed"},
    "SERVO": {"min_value": 0, "max_value": 100, "default_value": 0, "display_name": "servo"},
    "SERVO_IS_ON": {"min_value": 0, "max_value": 1, "default_value": 0, "display_name": "servo_is_on"},
    "SERVO_MINIMAL_OPEN": {"min_value": 0, "max_value": 99, "default_value": 5, "display_name": "servo_minimal_open"},
    "SERVO_MINIMAL_OPEN_CORRECTION": {"min_value": 0, "max_value": 100, "default_value": 5, "display_name": "servo_minimal_open_correction"},
    "SERVO_OPEN_DELAY_S": {"min_value": 0, "max_value": 50, "default_value": 5, "display_name": "servo_open_delay"},
    "SET_VELOCITY_KM_H": {"min_value": 0, "max_value": 200, "default_value": 45, "display_name": "set_velocity"},
    "SILOS_HEIGHT_CM": {"min_value": 0, "max_value": 300, "default_value": 60, "display_name": "silos_height"},
    "SILOS_LEVEL": {"min_value": 0, "max_value": 100, "default_value": 0, "display_name": "silos_level"},
    "SILOS_SENSOR_IS_CONNECTED": {"min_value": 0, "max_value": 1, "default_value": 0, "display_name": "silos_server_is_connected"},
    "SIZE_OF_GRAIN": {"min_value": 0, "max_value": 2, "default_value": 1, "display_name": "size_of_grain"},
    "START_SYSTEM": {"min_value": 0, "max_value": 1, "default_value": 0, "display_name": "start_system"},
    "TEMPERATURE": {"min_value": 0, "max_value": 0xFFFF, "default_value": 0, "display_name": "temperature"},
    "TRY_OPEN_CALIBRATION": {"min_value": 0, "max_value": 10, "default_value": 8, "display_name": "try_open_calibration"},
    "VELOCITY_HMS": {"min_value": 0, "max_value": 2000, "default_value": 450, "display_name": "velocity"},
    "VELOCITY_SENSOR_STATUS": {"min_value": 0, "max_value": 10, "default_value": 0, "display_name": "velocity_sensor_is_connected"},
    "VIBRO_DUTY_PWM": {"min_value": 50, "max_value": 100, "default_value": 50, "display_name": "vibro_duty_pwm"},
    "VIBRO_OFF_S": {"min_value": 0, "max_value": 100, "default_value": 0, "display_name": "vibro_off_s"},
    "VIBRO_ON_S": {"min_value": 0, "max_value": 100, "default_value": 0, "display_name": "vibro_on_s"},
    "VOLTAGE_ACCUM": {"min_value": 0, "max_value": 0xFFFF, "default_value": 0, "display_name": "voltage_accum"},
    "VOLTAGE_SERVO": {"min_value": 0, "max_value": 0xFFFF, "default_value": 0, "display_name": "voltage_servo"},
    "WORK_AREA": {"min_value": 0, "max_value": 100, "default_value": 50, "display_name": "work_area"},
    "WORKING_WIDTH_CM": {"min_value": 0, "max_value": 800, "default_value": 400, "display_name": "working_width"}
}

class Device:
    def __init__(self, ip="192.168.4.1", port=80):
        self.base_url = f"http://{ip}:{port}"
        self.parameters = {}
        self.connected = False
        self.update_lock = threading.Lock()
        self.update_thread = None
        self.parse_parameters()
    
    def parse_parameters(self):
        """Create parameters from the hardcoded dictionary"""
        for param_name, param_data in PARAMETERS_DICT.items():
            self.parameters[param_name] = Parameter(
                name=param_name,
                min_value=param_data["min_value"],
                max_value=param_data["max_value"],
                default_value=param_data["default_value"],
                display_name=param_data["display_name"],
                current_value=param_data["default_value"]
            )
    
    def connect(self) -> bool:
        """Test connection to the device with a ping"""
        try:
            response = requests.post(f"{self.base_url}/api/ping", timeout=2)
            self.connected = response.status_code == 200
            return self.connected
        except requests.exceptions.RequestException:
            self.connected = False
            return False
    
    def get_parameter(self, param_name: str) -> Optional[int]:
        """Get the value of a parameter from the device"""
        if param_name not in self.parameters:
            print(f"Unknown parameter: {param_name}")
            return None
        
        try:
            response = requests.get(
                f"{self.base_url}/api/parameter_u32/{self.parameters[param_name].display_name}", 
                timeout=2
            )
            
            if response.status_code == 200:
                value = int(response.text)
                self.parameters[param_name].current_value = value
                return value
            else:
                print(f"Failed to get parameter {param_name}: HTTP {response.status_code}")
                return None
        except requests.exceptions.RequestException as e:
            print(f"Connection error while getting {param_name}: {e}")
            self.connected = False
            return None
        except ValueError:
            print(f"Invalid response for {param_name}: {response.text}")
            return None
    
    def set_parameter(self, param_name: str, value: int) -> bool:
        """Set the value of a parameter on the device"""
        if param_name not in self.parameters:
            print(f"Unknown parameter: {param_name}")
            return False
        
        param = self.parameters[param_name]
        if value < param.min_value or value > param.max_value:
            print(f"Value {value} is out of range [{param.min_value}-{param.max_value}] for {param_name}")
            return False
        
        try:
            response = requests.post(
                f"{self.base_url}/api/parameter_u32/{param.display_name}", 
                data=str(value),
                timeout=2
            )
            
            if response.status_code == 200:
                param.current_value = value
                return True
            else:
                print(f"Failed to set parameter {param_name}: HTTP {response.status_code}")
                return False
        except requests.exceptions.RequestException as e:
            print(f"Connection error while setting {param_name}: {e}")
            self.connected = False
            return False
    
    def update_all_parameters(self) -> Dict[str, int]:
        """Update all parameter values from the device"""
        if not self.connected and not self.connect():
            return {}
        
        results = {}
        for param_name in self.parameters:
            value = self.get_parameter(param_name)
            if value is not None:
                results[param_name] = value
        
        return results
    
    def async_update_parameters(self, callback: Callable[[Dict[str, int]], None] = None):
        """Update parameters in a background thread to prevent UI freezing"""
        # Don't start another thread if one is already running
        if self.update_thread and self.update_thread.is_alive():
            return False
        
        def update_thread_func():
            with self.update_lock:
                results = self.update_all_parameters()
                if callback:
                    callback(results)
        
        self.update_thread = threading.Thread(target=update_thread_func, daemon=True)
        self.update_thread.start()
        return True
    
    def update_selected_parameters(self, param_names: Iterable[str]) -> Dict[str, int]:
        """Update only the specified parameter values from the device"""
        if not self.connected and not self.connect():
            return {}
        
        results = {}
        for param_name in param_names:
            if param_name in self.parameters:
                value = self.get_parameter(param_name)
                if value is not None:
                    results[param_name] = value
        
        return results
    
    def async_update_selected_parameters(self, param_names: Iterable[str], 
                                       callback: Callable[[Dict[str, int]], None] = None):
        """Update selected parameters in a background thread to prevent UI freezing"""
        # Don't start another thread if one is already running
        if self.update_thread and self.update_thread.is_alive():
            return False
        
        def update_thread_func():
            with self.update_lock:
                results = self.update_selected_parameters(param_names)
                if callback:
                    callback(results)
        
        self.update_thread = threading.Thread(target=update_thread_func, daemon=True)
        self.update_thread.start()
        return True
    
    def get_parameter_list(self) -> List[Parameter]:
        """Return a list of all parameters"""
        return list(self.parameters.values())
    
    def get_parameter_by_name(self, param_name: str) -> Optional[Parameter]:
        """Get a parameter object by its name"""
        return self.parameters.get(param_name)

if __name__ == "__main__":
    # Test the Device class
    device = Device()
    print(f"Found {len(device.parameters)} parameters")
    
    if device.connect():
        print("Connected to device")
        values = device.update_all_parameters()
        print(f"Updated {len(values)} parameters")
        
        # Print a few parameters
        for param_name in list(device.parameters.keys())[:5]:
            print(device.parameters[param_name])
    else:
        print("Failed to connect to device")
