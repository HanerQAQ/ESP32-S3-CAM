@echo off
cd /d "%~dp0"
echo ========================================================
echo  ColorTool v1.0 PyInstaller Packaging Script
echo ========================================================
echo.
echo Select packaging mode:
echo   [1] Single Portable File (--onefile, portable .exe)
echo   [2] Fast Directory Package (--onedir, instant startup)
echo.
set /p MODE="Enter choice [1 or 2] (default: 1): "
if "%MODE%"=="" set MODE=1

if "%MODE%"=="2" (
    echo [INFO] Building Directory Package (--onedir)...
    python -m PyInstaller --noconfirm --clean --onedir --windowed --icon "vision_control/assets/app_icon.ico" --add-data "vision_control/assets;vision_control/assets" --name "ColorTool_v1.0" main.py
    if errorlevel 1 goto build_err
    echo.
    echo ========================================================
    echo [SUCCESS] Package generated at: dist\ColorTool_v1.0\
    echo ========================================================
) else (
    echo [INFO] Building Single Executable (--onefile)...
    python -m PyInstaller --noconfirm --clean --onefile --windowed --icon "vision_control/assets/app_icon.ico" --add-data "vision_control/assets;vision_control/assets" --name "ColorTool_v1.0" main.py
    if errorlevel 1 goto build_err
    copy /y "dist\ColorTool_v1.0.exe" "ColorTool_v1.0.exe" >nul
    echo.
    echo ========================================================
    echo [SUCCESS] Standalone executable generated: ColorTool_v1.0.exe
    echo ========================================================
)
pause
exit /b 0

:build_err
echo.
echo [ERROR] PyInstaller build failed! Please check python dependencies.
pause
exit /b 1
