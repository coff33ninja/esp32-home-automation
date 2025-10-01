@echo off
cd /d %~dp0
echo ESP32 Home Automation - PC Volume Control
echo ==========================================
echo.

REM Check if uv is installed
uv --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: uv is not installed or not in PATH.
    echo Please install uv from https://github.com/astral-sh/uv
    pause
    exit /b 1
)

REM Check if virtual environment exists, creating it with Python 3.11 if not.
if not exist ".venv" (
    echo Creating virtual environment with Python 3.11...
    uv venv --python 3.11
    if errorlevel 1 (
        echo ERROR: Failed to create virtual environment.
        echo Please ensure Python 3.11 is installed and discoverable by uv.
        echo uv does not install Python versions. You may need a tool like pyenv-win.
        pause
        exit /b 1
    )
)

REM Activate virtual environment
echo Activating virtual environment...
call .venv\Scripts\activate.bat

REM Install/update requirements
echo Installing/updating requirements...
uv pip install -r requirements.txt

REM Start the volume control script
echo.
echo Starting ESP32 Volume Control...
echo Press Ctrl+C to stop
echo.

python main.py --tray

REM Deactivate virtual environment
deactivate

echo.
echo Volume control stopped.
pause
