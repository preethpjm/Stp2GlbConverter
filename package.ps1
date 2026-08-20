$ErrorActionPreference = "Stop"

# --- Adjust these two paths for your machine ---
$ReleaseDir = "C:\Users\Preeth\Downloads\STP_to_GLB_Release"
$ZipPath    = "C:\Users\Preeth\Downloads\STP_to_GLB_Converter.zip"
$BuildDir   = "build\bin\Release"

Write-Host "Building..."
cmake --build build --config Release

if (-not (Test-Path $ReleaseDir)) {
    New-Item -ItemType Directory -Path $ReleaseDir | Out-Null
}

Write-Host "Copying exe + DLLs to release folder..."
# CMakeLists.txt's post-build step already copies every OpenCASCADE DLL next
# to the exe in $BuildDir, so copying the whole folder picks up everything.
Copy-Item "$BuildDir\*" $ReleaseDir -Force -Recurse

Write-Host "Writing convert.bat..."
$ConvertBat = @'
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
'@
Set-Content -Path "$ReleaseDir\convert.bat" -Value $ConvertBat -Encoding ASCII

Write-Host "Writing README.txt..."
$Readme = @'
STP to GLB Converter
====================

HOW TO USE
----------

Option 1 - Drag and Drop (easiest):
  Drag your .stp file onto convert.bat
  The .glb and .json will be saved in the same folder as your .stp file.

Option 2 - Command line:
  convert.bat "path\to\file.stp"
  convert.bat "path\to\file.stp" "output.glb" "assembly.json"
  convert.bat "path\to\file.stp" "output.glb" "assembly.json" flat

MODE (optional 4th argument)
-----------------------------
  auto      - (default) auto-detect whether the source has a real assembly
              hierarchy and use it if so, otherwise fall back to splitting
              by geometry/color.
  hierarchy - force the hierarchy path even if detection says otherwise.
  flat      - force the geometry-split path even if a real hierarchy exists.

QUALITY (optional 5th argument)
--------------------------------
  fast      - coarser mesh, faster conversion. Use for large/complex assemblies.
  balanced  - (default) unchanged from original quality.
  precise   - finer mesh, slower. Use when visual fidelity matters more than speed.

THRESHOLD (optional 6th argument, default 150)
------------------------------------------------
  Face-count above which a leaf part is checked for internal CATIA
  breadcrumb structure and split into sub-parts if found. Leave at default
  unless a specific file's face-count logs suggest otherwise.

OUTPUT FILES
------------
  .glb   - 3D mesh, open in Windows 3D Viewer, Blender, online at gltf-viewer.donmccurdy.com
  .json  - Bill of Materials and assembly tree

NOTES
-----
  All required DLLs are included. No installation needed.
  Keep all files in this folder together - do not move just the exe.
  If launching on a machine without the Visual C++ runtime installed,
  run vc_redist.x64.exe from this folder first (one-time, per machine).
'@
Set-Content -Path "$ReleaseDir\README.txt" -Value $Readme -Encoding ASCII

if (-not (Test-Path "$ReleaseDir\vc_redist.x64.exe")) {
    Write-Warning "vc_redist.x64.exe not found in $ReleaseDir. Download it from https://aka.ms/vs/17/release/vc_redist.x64.exe and place it there before zipping, so machines without the VC++ runtime can still run the exe."
}

Write-Host "Creating ZIP..."
Compress-Archive -Path "$ReleaseDir\*" -DestinationPath $ZipPath -Force

Write-Host "Done! ZIP is at $ZipPath"