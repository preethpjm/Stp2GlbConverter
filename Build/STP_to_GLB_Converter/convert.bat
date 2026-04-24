@echo off
setlocal

:: Get the folder where this bat file lives
set SCRIPT_DIR=%~dp0

echo === STEP to GLB Converter ===
echo.

if "%~1"=="" (
    echo Drag and drop a .stp file onto this bat file
    echo OR
    echo Usage: convert.bat input.stp output.glb
    echo.
    pause
    exit
)

set INPUT=%~1
set OUTPUT=%~2
if "%OUTPUT%"=="" set OUTPUT=%~n1.glb
set BOM=%~n1_bom.json

echo Input:  %INPUT%
echo Output: %OUTPUT%
echo BOM:    %BOM%
echo.

:: Run exe from same folder as this bat file
"%SCRIPT_DIR%cad-step-to-glb.exe" "%INPUT%" "%OUTPUT%" "%BOM%"

if %ERRORLEVEL%==0 (
    echo.
    echo SUCCESS! Files created:
    echo   %OUTPUT%
    echo   %BOM%
) else (
    echo.
    echo ERROR: Conversion failed!
)

echo.
pause