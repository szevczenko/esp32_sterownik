#!/bin/bash
echo "Starting ESP32 Controller Monitor..."

# Check if virtual environment exists
if [ ! -d "venv" ]; then
    echo "Virtual environment not found! Please run setup_env.sh first."
    exit 1
fi

# Activate virtual environment and run the application
source venv/bin/activate
python ui.py

if [ $? -ne 0 ]; then
    echo "Application exited with error code $?"
    read -p "Press Enter to continue..." 
fi

exit 0
