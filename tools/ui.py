import sys
import os
import time
import csv
from datetime import datetime
from typing import Dict, List, Optional
import threading

from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                            QHBoxLayout, QTableWidget, QTableWidgetItem, QPushButton, 
                            QLabel, QSpinBox, QCheckBox, QFileDialog, QTabWidget,
                            QSplitter, QComboBox, QGroupBox, QMessageBox, QProgressBar,
                            QScrollArea, QFrame)
from PyQt5.QtCore import Qt, QTimer, pyqtSlot, pyqtSignal, QObject
from PyQt5.QtGui import QColor

import matplotlib
matplotlib.use('Qt5Agg')
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import matplotlib.pyplot as plt
import numpy as np

from device import Device, Parameter

class ParameterTableWidget(QTableWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setColumnCount(5)
        self.setHorizontalHeaderLabels(["Name", "Value", "Min", "Max", "Edit"])
        self.verticalHeader().setVisible(False)
        self.setSelectionBehavior(QTableWidget.SelectRows)
        self.setAlternatingRowColors(True)
        self.horizontalHeader().setStretchLastSection(True)
        self.cellChanged.connect(self.on_cell_changed)
        self._block_signals = False
    
    def update_parameters(self, parameters: List[Parameter]):
        self._block_signals = True
        self.setRowCount(len(parameters))
        
        for row, param in enumerate(parameters):
            # Name
            name_item = QTableWidgetItem(f"{param.display_name} ({param.name})")
            name_item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
            name_item.setData(Qt.UserRole, param.name)
            self.setItem(row, 0, name_item)
            
            # Current value
            value_item = QTableWidgetItem(str(param.current_value))
            value_item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
            self.setItem(row, 1, value_item)
            
            # Min value
            min_item = QTableWidgetItem(str(param.min_value))
            min_item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
            self.setItem(row, 2, min_item)
            
            # Max value
            max_item = QTableWidgetItem(str(param.max_value))
            max_item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
            self.setItem(row, 3, max_item)
            
            # Edit value
            spin_box = QSpinBox()
            spin_box.setRange(param.min_value, param.max_value)
            spin_box.setValue(param.current_value)
            spin_box.setProperty("param_name", param.name)
            spin_box.valueChanged.connect(self.on_value_changed)
            self.setCellWidget(row, 4, spin_box)
        
        self.resizeColumnsToContents()
        self._block_signals = False
    
    def on_cell_changed(self, row, column):
        if self._block_signals:
            return
    
    def on_value_changed(self):
        spin_box = self.sender()
        param_name = spin_box.property("param_name")
        new_value = spin_box.value()
        
        # Signal to parent
        if hasattr(self.parent(), "on_parameter_changed"):
            self.parent().on_parameter_changed(param_name, new_value)

class WatchlistTableWidget(QTableWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setColumnCount(3)
        self.setHorizontalHeaderLabels(["Name", "Value", "Remove"])
        self.verticalHeader().setVisible(False)
        self.setAlternatingRowColors(True)
        self.horizontalHeader().setStretchLastSection(True)
        self.watched_parameters = set()  # Set of parameter names being watched

    def add_parameter(self, param: Parameter):
        """Add a parameter to the watchlist if not already present"""
        if param.name in self.watched_parameters:
            return False
        
        # Add to watched set
        self.watched_parameters.add(param.name)
        
        # Add to table
        row = self.rowCount()
        self.insertRow(row)
        
        # Name
        name_item = QTableWidgetItem(f"{param.display_name} ({param.name})")
        name_item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
        name_item.setData(Qt.UserRole, param.name)
        self.setItem(row, 0, name_item)
        
        # Value
        value_item = QTableWidgetItem(str(param.current_value))
        value_item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
        self.setItem(row, 1, value_item)
        
        # Remove button
        remove_button = QPushButton("Remove")
        remove_button.setProperty("param_name", param.name)
        remove_button.clicked.connect(self.on_remove_clicked)
        self.setCellWidget(row, 2, remove_button)
        
        self.resizeColumnsToContents()
        return True
    
    def on_remove_clicked(self):
        """Remove a parameter from the watchlist"""
        button = self.sender()
        param_name = button.property("param_name")
        
        # Remove from set
        if param_name in self.watched_parameters:
            self.watched_parameters.remove(param_name)
        
        # Find and remove the row
        for row in range(self.rowCount()):
            if self.item(row, 0).data(Qt.UserRole) == param_name:
                self.removeRow(row)
                break
    
    def update_values(self, parameters: Dict[str, Parameter]):
        """Update the values of all watched parameters"""
        for row in range(self.rowCount()):
            param_name = self.item(row, 0).data(Qt.UserRole)
            if param_name in parameters:
                param = parameters[param_name]
                self.item(row, 1).setText(str(param.current_value))

class PlotWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.layout = QVBoxLayout(self)
        
        # Plot controls
        control_layout = QHBoxLayout()
        
        self.param_selector = QComboBox()
        self.param_selector.setMinimumWidth(250)  # Increase width to see full parameter names
        self.param_selector.currentIndexChanged.connect(self.update_plot)
        control_layout.addWidget(QLabel("Parameter:"))
        control_layout.addWidget(self.param_selector)
        
        self.time_window = QSpinBox()
        self.time_window.setRange(1, 3600)
        self.time_window.setValue(60)
        self.time_window.setSuffix(" sec")
        control_layout.addWidget(QLabel("Time Window:"))
        control_layout.addWidget(self.time_window)
        
        self.auto_scale = QCheckBox("Auto Scale")
        self.auto_scale.setChecked(True)
        control_layout.addWidget(self.auto_scale)
        
        self.clear_button = QPushButton("Clear")
        self.clear_button.clicked.connect(self.clear_data)
        control_layout.addWidget(self.clear_button)
        
        control_layout.addStretch()
        self.layout.addLayout(control_layout)
        
        # Matplotlib figure
        self.figure = Figure(figsize=(5, 4), dpi=100)
        self.canvas = FigureCanvas(self.figure)
        self.layout.addWidget(self.canvas)
        
        self.ax = self.figure.add_subplot(111)
        self.ax.set_xlabel('Time (s)')
        self.ax.set_ylabel('Value')
        self.ax.grid(True)
        
        # Data storage
        self.data = {}  # param_name -> (timestamps, values)
        self.start_time = time.time()
        self.selected_param = None  # Track the currently selected parameter
    
    def set_parameters(self, parameters: List[Parameter]):
        """Set the available parameters for plotting"""
        current_selection = self.param_selector.currentData()
        
        # Remember the current selection index
        self.param_selector.blockSignals(True)
        self.param_selector.clear()
        
        for param in parameters:
            self.param_selector.addItem(f"{param.display_name} ({param.name})", param.name)
        
        # Try to restore the previous selection
        if current_selection:
            index = self.param_selector.findData(current_selection)
            if index >= 0:
                self.param_selector.setCurrentIndex(index)
        elif self.param_selector.count() > 0:
            # Select the first item if nothing was selected before
            self.param_selector.setCurrentIndex(0)
            self.selected_param = self.param_selector.currentData()
        
        self.param_selector.blockSignals(False)
        
        # Trigger an initial plot update
        if self.param_selector.count() > 0:
            self.update_plot()
    
    def add_data_point(self, param_name, value):
        """Add a data point for the specified parameter"""
        current_time = time.time() - self.start_time
        
        if param_name not in self.data:
            self.data[param_name] = ([], [])
        
        timestamps, values = self.data[param_name]
        timestamps.append(current_time)
        values.append(value)
        
        # Limit data points to those in the time window
        time_window = self.time_window.value()
        cutoff_time = current_time - time_window
        
        i = 0
        while i < len(timestamps) and timestamps[i] < cutoff_time:
            i += 1
        
        if i > 0:
            self.data[param_name] = (timestamps[i:], values[i:])
        
        # If this is the selected parameter, update the plot
        if param_name == self.selected_param:
            self.update_plot()
    
    def update_plot(self):
        """Update the plot with the currently selected parameter data"""
        # Update the selected parameter based on the current selection
        self.selected_param = self.param_selector.currentData()
        
        if not self.selected_param or self.selected_param not in self.data:
            # Clear the plot if no valid parameter is selected
            self.ax.clear()
            self.ax.set_xlabel('Time (s)')
            self.ax.set_ylabel('Value')
            self.ax.grid(True)
            self.canvas.draw()
            return
        
        self.ax.clear()
        timestamps, values = self.data[self.selected_param]
        
        if not timestamps:
            # No data points yet
            self.ax.set_xlabel('Time (s)')
            self.ax.set_ylabel('Value')
            self.ax.set_title(self.param_selector.currentText())
            self.ax.grid(True)
            self.canvas.draw()
            return
        
        self.ax.plot(timestamps, values, 'b-')
        
        # Add grid and labels
        self.ax.grid(True)
        self.ax.set_xlabel('Time (s)')
        self.ax.set_ylabel('Value')
        self.ax.set_title(self.param_selector.currentText())
        
        # Set time range based on selected time window
        time_window = self.time_window.value()
        if timestamps:
            current_time = timestamps[-1]
            self.ax.set_xlim(max(0, current_time - time_window), max(time_window, current_time))
        
        # Auto scale Y-axis if enabled
        if not self.auto_scale.isChecked() and values:
            min_val = min(values)
            max_val = max(values)
            padding = (max_val - min_val) * 0.1 if max_val > min_val else 0.1
            self.ax.set_ylim(min_val - padding, max_val + padding)
        
        self.canvas.draw()
    
    def clear_data(self):
        """Clear the data for the currently selected parameter"""
        param_name = self.param_selector.currentData()
        if param_name in self.data:
            self.data[param_name] = ([], [])
            self.update_plot()

# Signal class for thread-safe UI updates
class UpdateSignals(QObject):
    update_complete = pyqtSignal(dict)

class ParameterCheckList(QWidget):
    """Widget that displays a list of checkable parameters"""
    def __init__(self, parent=None):
        super().__init__(parent)
        self.layout = QVBoxLayout(self)
        self.scroll_area = QScrollArea()
        self.scroll_area.setWidgetResizable(True)
        
        self.content_widget = QWidget()
        self.content_layout = QVBoxLayout(self.content_widget)
        self.content_layout.setAlignment(Qt.AlignTop)
        self.scroll_area.setWidget(self.content_widget)
        
        # Control buttons
        buttons_layout = QHBoxLayout()
        self.select_all_button = QPushButton("Select All")
        self.select_all_button.clicked.connect(self.select_all)
        buttons_layout.addWidget(self.select_all_button)
        
        self.clear_all_button = QPushButton("Clear All")
        self.clear_all_button.clicked.connect(self.clear_all)
        buttons_layout.addWidget(self.clear_all_button)
        
        buttons_layout.addStretch()
        self.layout.addLayout(buttons_layout)
        self.layout.addWidget(self.scroll_area)
        
        self.checkboxes = {}  # Dictionary to store parameter checkboxes
    
    def set_parameters(self, parameters: List[Parameter]):
        """Set the list of parameters and create checkboxes"""
        # Clear existing checkboxes
        for i in reversed(range(self.content_layout.count())):
            widget = self.content_layout.itemAt(i).widget()
            if widget:
                widget.deleteLater()
        
        self.checkboxes = {}
        
        # Add checkboxes for each parameter
        for param in parameters:
            checkbox = QCheckBox(f"{param.display_name} ({param.name})")
            checkbox.setChecked(True)  # Enable all by default
            checkbox.setProperty("param_name", param.name)
            self.checkboxes[param.name] = checkbox
            self.content_layout.addWidget(checkbox)
    
    def get_selected_parameters(self) -> List[str]:
        """Get a list of parameter names that are checked"""
        return [name for name, checkbox in self.checkboxes.items() if checkbox.isChecked()]
    
    def select_all(self):
        """Select all parameters"""
        for checkbox in self.checkboxes.values():
            checkbox.setChecked(True)
    
    def clear_all(self):
        """Clear all parameter selections"""
        for checkbox in self.checkboxes.values():
            checkbox.setChecked(False)

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP32 Controller Monitor")
        self.resize(1024, 768)
        
        # Device connection
        self.device = Device()
        self.connected = False
        
        # Signals for thread-safe updates
        self.signals = UpdateSignals()
        self.signals.update_complete.connect(self.on_update_complete)
        
        # Setup UI
        self.setup_ui()
        
        # Setup timer for periodic updates - increased to 1000ms to reduce load
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self.update_data)
        self.update_timer.start(1000)  # 1000 ms update interval
        self.auto_update_enabled = True
        
        # Status for update in progress
        self.update_in_progress = False
        
        # Data logging
        self.logging = False
        self.log_file = None
        self.log_writer = None
    
    def setup_ui(self):
        # Central widget and main layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        
        # Connection status and controls
        status_layout = QHBoxLayout()
        self.status_label = QLabel("Not Connected")
        self.connect_button = QPushButton("Connect")
        self.connect_button.clicked.connect(self.toggle_connection)
        status_layout.addWidget(self.status_label)
        status_layout.addWidget(self.connect_button)
        
        # Reading control buttons
        self.start_reading_button = QPushButton("Start Reading")
        self.start_reading_button.clicked.connect(self.start_reading)
        self.start_reading_button.setEnabled(False)
        status_layout.addWidget(self.start_reading_button)
        
        self.stop_reading_button = QPushButton("Stop Reading")
        self.stop_reading_button.clicked.connect(self.stop_reading)
        self.stop_reading_button.setEnabled(False)
        status_layout.addWidget(self.stop_reading_button)
        
        # Manual update button
        self.manual_update_button = QPushButton("Update Now")
        self.manual_update_button.clicked.connect(self.manual_update)
        self.manual_update_button.setEnabled(False)
        status_layout.addWidget(self.manual_update_button)
        
        # Logging controls
        self.log_button = QPushButton("Start Logging")
        self.log_button.clicked.connect(self.toggle_logging)
        status_layout.addWidget(self.log_button)
        
        # Add update status indicator
        self.update_indicator = QProgressBar()
        self.update_indicator.setRange(0, 0)  # Indeterminate progress
        self.update_indicator.setVisible(False)
        self.update_indicator.setMaximumWidth(100)
        status_layout.addWidget(self.update_indicator)
        
        status_layout.addStretch()
        
        main_layout.addLayout(status_layout)
        
        # Tab widget
        self.tab_widget = QTabWidget()
        
        # Parameters tab
        self.param_tab = QWidget()
        param_layout = QVBoxLayout(self.param_tab)
        self.param_table = ParameterTableWidget()
        param_layout.addWidget(self.param_table)
        self.tab_widget.addTab(self.param_tab, "Parameters")
        
        # Plot tab
        self.plot_tab = QWidget()
        plot_layout = QVBoxLayout(self.plot_tab)
        self.plot_widget = PlotWidget()
        plot_layout.addWidget(self.plot_widget)
        self.tab_widget.addTab(self.plot_tab, "Plot")
        
        # Watchlist tab
        self.watchlist_tab = QWidget()
        watchlist_layout = QVBoxLayout(self.watchlist_tab)
        
        # Parameter selection controls
        selection_group = QGroupBox("Add Parameters to Watchlist")
        selection_layout = QHBoxLayout(selection_group)
        
        self.param_dropdown = QComboBox()
        self.param_dropdown.setMinimumWidth(300)
        selection_layout.addWidget(QLabel("Parameter:"))
        selection_layout.addWidget(self.param_dropdown)
        
        self.add_param_button = QPushButton("Add to Watchlist")
        self.add_param_button.clicked.connect(self.add_to_watchlist)
        selection_layout.addWidget(self.add_param_button)
        
        selection_layout.addStretch()
        watchlist_layout.addWidget(selection_group)
        
        # Watchlist table
        self.watchlist_table = WatchlistTableWidget()
        watchlist_layout.addWidget(self.watchlist_table)
        
        self.tab_widget.addTab(self.watchlist_tab, "Watchlist")
        
        # Parameter Selection tab
        self.param_select_tab = QWidget()
        param_select_layout = QVBoxLayout(self.param_select_tab)
        
        # Instructions label
        instructions_label = QLabel("Select parameters to update. Only checked parameters will be read from the device.")
        instructions_label.setWordWrap(True)
        param_select_layout.addWidget(instructions_label)
        
        # Parameter selection list
        self.param_checklist = ParameterCheckList()
        param_select_layout.addWidget(self.param_checklist)
        
        self.tab_widget.addTab(self.param_select_tab, "Parameter Selection")
        
        main_layout.addWidget(self.tab_widget)
    
    def toggle_connection(self):
        if not self.connected:
            # Try to connect
            self.connect_button.setText("Connecting...")
            self.connect_button.setEnabled(False)
            
            if self.device.connect():
                self.connected = True
                self.status_label.setText("Connected")
                self.connect_button.setText("Disconnect")
                self.start_reading_button.setEnabled(True)
                self.stop_reading_button.setEnabled(True)
                self.manual_update_button.setEnabled(True)
                self.initialize_data()
            else:
                self.status_label.setText("Connection Failed")
                self.connect_button.setText("Connect")
                self.start_reading_button.setEnabled(False)
                self.stop_reading_button.setEnabled(False)
                self.manual_update_button.setEnabled(False)
            
            self.connect_button.setEnabled(True)
        else:
            # Disconnect
            self.connected = False
            self.status_label.setText("Not Connected")
            self.connect_button.setText("Connect")
            self.start_reading_button.setEnabled(False)
            self.stop_reading_button.setEnabled(False)
            self.manual_update_button.setEnabled(False)
            self.stop_reading()
    
    def initialize_data(self):
        # Show update in progress
        self.update_indicator.setVisible(True)
        self.update_in_progress = True
        
        # Get all parameters from device asynchronously
        self.device.async_update_parameters(self.signals.update_complete.emit)
        
        # Start update timer if auto-update is enabled
        if self.auto_update_enabled:
            self.update_timer.start()
    
    def start_reading(self):
        """Start automatic reading from the device"""
        if not self.connected:
            return
        
        self.auto_update_enabled = True
        self.update_timer.start()
        self.status_label.setText("Connected (Auto-Update)")
        self.start_reading_button.setEnabled(False)
        self.stop_reading_button.setEnabled(True)
    
    def stop_reading(self):
        """Stop automatic reading from the device"""
        self.auto_update_enabled = False
        self.update_timer.stop()
        if self.connected:
            self.status_label.setText("Connected (Manual)")
        self.start_reading_button.setEnabled(True)
        self.stop_reading_button.setEnabled(False)
    
    def manual_update(self):
        """Manually trigger an update"""
        if not self.connected or self.update_in_progress:
            return
        
        self.update_data()
    
    def add_to_watchlist(self):
        """Add the selected parameter to the watchlist"""
        if not self.connected:
            QMessageBox.warning(self, "Not Connected", "Connect to the device first.")
            return
        
        param_name = self.param_dropdown.currentData()
        if param_name:
            param = self.device.get_parameter_by_name(param_name)
            if param:
                if not self.watchlist_table.add_parameter(param):
                    QMessageBox.information(
                        self, 
                        "Already Added", 
                        f"Parameter {param.display_name} is already in the watchlist."
                    )
    
    def update_data(self):
        if not self.connected or self.update_in_progress:
            return
        
        # Get selected parameters
        selected_params = self.param_checklist.get_selected_parameters()
        if not selected_params:
            return  # Don't update if no parameters selected
        
        # Add the currently selected plot parameter to ensure it's always updated
        plot_param = self.plot_widget.selected_param
        if plot_param and plot_param not in selected_params:
            selected_params.append(plot_param)
        
        # Also include all watchlist parameters
        for param_name in self.watchlist_table.watched_parameters:
            if param_name not in selected_params:
                selected_params.append(param_name)
        
        # Show update in progress
        self.update_indicator.setVisible(True)
        self.update_in_progress = True
        
        # Use the device's async_update_selected_parameters method
        self.device.async_update_selected_parameters(selected_params, self.signals.update_complete.emit)
    
    @pyqtSlot(dict)
    def on_update_complete(self, updated_params):
        """Handle the completion of parameter updates from the background thread"""
        # Hide the update indicator
        self.update_indicator.setVisible(False)
        self.update_in_progress = False
        
        if not updated_params:
            # Connection lost
            if self.connected:
                self.connected = False
                self.status_label.setText("Connection Lost")
                self.connect_button.setText("Connect")
                self.start_reading_button.setEnabled(False)
                self.stop_reading_button.setEnabled(False)
                self.manual_update_button.setEnabled(False)
                self.update_timer.stop()
            return
        
        # Get the current parameter list
        param_list = self.device.get_parameter_list()
        
        # Update the parameter table
        self.param_table.update_parameters(param_list)
        
        # Update the parameter selection list if it's empty
        if not self.param_checklist.checkboxes:
            self.param_checklist.set_parameters(param_list)
        
        # Always update the plot parameter selector
        self.plot_widget.set_parameters(param_list)
        
        # Update the parameter dropdown for the watchlist if it's empty
        if self.param_dropdown.count() == 0:
            self.param_dropdown.clear()
            for param in param_list:
                self.param_dropdown.addItem(f"{param.display_name} ({param.name})", param.name)
        
        # Update the watchlist table
        self.watchlist_table.update_values(self.device.parameters)
        
        # Update the plot data
        for param_name, value in updated_params.items():
            self.plot_widget.add_data_point(param_name, value)
        
        # Log data if enabled
        if self.logging and self.log_writer:
            self.log_data(updated_params)
    
    def on_parameter_changed(self, param_name, new_value):
        if not self.connected:
            return
        
        success = self.device.set_parameter(param_name, new_value)
        if not success:
            QMessageBox.warning(
                self, 
                "Parameter Update Failed", 
                f"Failed to update parameter {param_name} to {new_value}"
            )
    
    def toggle_logging(self):
        if not self.logging:
            # Start logging
            file_path, _ = QFileDialog.getSaveFileName(
                self, 
                "Save Log File", 
                f"esp32_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv", 
                "CSV Files (*.csv)"
            )
            
            if file_path:
                try:
                    # Use UTF-8 encoding to handle all Unicode characters
                    self.log_file = open(file_path, 'w', newline='', encoding='utf-8')
                    self.log_writer = csv.writer(self.log_file)
                    
                    # Write header row
                    header = ['timestamp']
                    for param in self.device.get_parameter_list():
                        header.append(param.name)
                    self.log_writer.writerow(header)
                    
                    self.logging = True
                    self.log_button.setText("Stop Logging")
                except Exception as e:
                    QMessageBox.critical(
                        self, 
                        "Logging Error", 
                        f"Failed to start logging: {str(e)}"
                    )
        else:
            # Stop logging
            if self.log_file:
                self.log_file.close()
                self.log_file = None
                self.log_writer = None
            
            self.logging = False
            self.log_button.setText("Start Logging")
    
    def log_data(self, updated_params):
        if not self.log_writer:
            return
        
        timestamp = datetime.now().isoformat()
        row = [timestamp]
        
        for param in self.device.get_parameter_list():
            row.append(param.current_value)
        
        self.log_writer.writerow(row)
    
    def closeEvent(self, event):
        """Handle window close event to clean up resources"""
        if self.logging and self.log_file:
            self.log_file.close()
        
        # Stop the update timer
        self.update_timer.stop()
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
