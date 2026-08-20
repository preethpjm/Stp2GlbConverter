@echo off
cd /d "%~dp0"

if "%~1"=="" (
    echo.
    echo  STP to GLB Converter
    echo  --------------------
        echo  Usage:        convert.bat input.stp [output.glb] [assembly.json] [auto^|hierarchy^|flat] [fast^|balanced^|precise] [threshold]
    echo  Drag and drop: drag your .stp file onto convert.bat
    echo.
    pause
    exit /b 1
)

set INPUT=%~1
set GLB=%~2
set JSON=%~3
set MODE=%~4
set QUALITY=%~5
set THRESHOLD=%~6

if "%GLB%"==""       set GLB=%~dpn1.glb
if "%JSON%"==""      set JSON=%~dpn1_assembly.json
if "%MODE%"==""      set MODE=auto
if "%QUALITY%"==""   set QUALITY=balanced
if "%THRESHOLD%"=="" set THRESHOLD=150

echo.
echo  Input     : %INPUT%
echo  GLB       : %GLB%
echo  JSON      : %JSON%
echo  Mode      : %MODE%       (auto / hierarchy / flat)
echo  Quality   : %QUALITY%    (fast / balanced / precise)
echo  Threshold : %THRESHOLD%  (breadcrumb-split face count)
echo.

"%~dp0cad-step-to-glb.exe" "%INPUT%" "%GLB%" "%JSON%" "%MODE%" "%QUALITY%" "%THRESHOLD%"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo  ERROR: Conversion failed. Check the output above for details.
) else (
    echo.
    echo  Conversion successful!
    echo  GLB  saved to: %GLB%
    echo  JSON saved to: %JSON%
)

echo.
pause
