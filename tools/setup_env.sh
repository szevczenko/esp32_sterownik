#!/bin/bash
echo "Setting up Python virtual environment for ESP32 Controller Monitor..."

# Check if Python is installed
if ! command -v python3 &> /dev/null; then
    echo "Python 3 is not installed! Please install Python 3.7 or newer."
    exit 1
fi

# Create virtual environment if it doesn't exist
if [ ! -d "venv" ]; then
    echo "Creating virtual environment..."
    python3 -m venv venv
    if [ $? -ne 0 ]; then
        echo "Failed to create virtual environment!"
        exit 1
    fi
fi

# Activate virtual environment and install dependencies
echo "Installing dependencies..."
source venv/bin/activate
python -m pip install --upgrade pip
pip install -r requirements.txt

if [ $? -ne 0 ]; then
    echo "Failed to install dependencies!"
    exit 1
fi

echo "Virtual environment setup complete!"
echo ""
echo "To run the application:"
echo "  ./run_monitor.sh"
echo ""

exit 0
