@echo off
echo Starting ESP32 Controller Monitor...

REM Check if virtual environment exists
if not exist venv (
    echo Virtual environment not found! Please run setup_env.bat first.
    exit /b 1
)

REM Activate virtual environment and run the application
call venv\Scripts\activate.bat
python ui.py

if %ERRORLEVEL% NEQ 0 (
    echo Application exited with error code %ERRORLEVEL%
    pause
)

exit /b 0
