@echo off
echo === LilyGo GPS Tracker - Flash Tool ===
echo.
echo Step 1/2: Uploading firmware...
python -m platformio run --target upload --upload-port COM3
if errorlevel 1 (
    echo.
    echo ERROR: Firmware upload failed. Check that the board is connected to COM3.
    pause
    exit /b 1
)

echo.
echo Step 2/2: Uploading config (data/config.json)...
python -m platformio run --target uploadfs --upload-port COM3
if errorlevel 1 (
    echo.
    echo ERROR: Config upload failed.
    pause
    exit /b 1
)

echo.
echo === Done! Board is running with updated firmware and config. ===
pause
