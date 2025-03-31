# ESP32 Controller Monitor

This project contains both an ESP32 controller firmware and a Python-based monitoring tool for parameter visualization and control.

## Build the project (ESP32 firmware)
1. Install esp-idf 5.1.2 https://docs.espressif.com/projects/esp-idf/en/v5.1.2/esp32/get-started/index.html
2. Install Git
3. Clone the project:
```
git clone --recurse-submodules https://github.com/szevczenko/esp32_sterownik.git
```
4. Init submodules for hq_components:
```
git submodule update --init --recursive
```
5. Run ESP-IDF 5.1 CMD and enter to cloned folder.
6. build by running command:
```
idf.py build
```
7. flash esp32:
```
idf.py flash -p COM8
```

## ESP32 Controller Monitor Tool

The monitoring tool allows you to connect to your ESP32 device, view and modify parameters, create watchlists, and plot parameter values in real-time.

### Setup Environment

#### Windows

1. Make sure Python 3.7 or newer is installed on your system
2. Open Command Prompt and navigate to the tools directory:
```
cd c:\projekty\esp32_sterownik\tools
```
3. Run the setup script to create a virtual environment and install dependencies:
```
setup_env.bat
```
4. If successful, you'll see a message confirming the setup is complete

#### Linux/macOS

1. Make sure Python 3.7 or newer is installed on your system
2. Open a terminal and navigate to the tools directory:
```
chmod +x setup_env.sh
```
4. Run the setup script:
```
./setup_env.sh
```
5. If successful, you'll see a message confirming the setup is complete

### Running the Program

#### Windows

1. Navigate to the tools directory
2. Run the monitoring tool:
```
run_monitor.bat
```
#### Linux/macOS

1. Navigate to the tools directory
2. Make sure the run script is executable:
```
chmod +x run_monitor.sh
```
3. Run the monitoring tool:
```
./run_monitor.sh
```
### Using the Monitoring Tool

#### Connecting to the Device

1. Make sure your ESP32 device is powered on and accessible on your network
2. The default IP address is 192.168.4.1 (device's access point mode)
3. Click the "Connect" button to establish a connection with the device
4. Once connected, all parameters will be loaded automatically

#### Parameter Table

- The "Parameters" tab shows a complete list of all device parameters
- You can see the current value, min/max limits, and edit values directly
- Changes are sent to the device immediately when you modify a value

#### Watchlist

1. Go to the "Watchlist" tab to create a personalized list of important parameters
2. Select a parameter from the dropdown menu and click "Add to Watchlist"
3. Parameters in the watchlist are monitored and updated regardless of other settings
4. Click "Remove" to remove a parameter from the watchlist

#### Real-time Plotting

1. Go to the "Plot" tab to visualize parameter values over time
2. Select the parameter to plot from the dropdown menu
3. Adjust the time window to change how much historical data is shown
4. Toggle "Auto Scale" to automatically adjust the y-axis scale
5. Click "Clear" to reset the plot data

#### Parameter Selection

1. Go to the "Parameter Selection" tab to choose which parameters to monitor
2. Check/uncheck parameters to include/exclude them from updates
3. Use "Select All" or "Clear All" for quick selection
4. This helps reduce network traffic by only requesting necessary parameters

#### Data Logging

1. Click "Start Logging" to save parameter values to a CSV file
2. Choose a location to save the log file
3. All parameter values will be recorded with timestamps
4. Click "Stop Logging" to end the logging session

#### Auto-Update Controls

- Click "Start Reading" to enable automatic parameter updates
- Click "Stop Reading" to pause automatic updates
- Click "Update Now" to manually trigger an update when automatic updates are paused
- The update indicator shows when data is being fetched from the device