# STEP to GLB Converter

Converts STEP/STP CAD files to GLB format with full assembly hierarchy and BOM JSON output.

## Requirements

- Windows 10/11 64-bit
- [Visual Studio 2022](https://visualstudio.microsoft.com/vs/) with **Desktop development with C++** workload
- [Git](https://git-scm.com/download/win)

## Setup & Build

### 1. Clone
```bash
git clone https://github.com/yourusername/stp-to-glb-converter.git
cd stp-to-glb-converter
```

### 2. Set up vcpkg
```powershell
cd C:\Users\YOUR_USERNAME\Downloads
git clone https://github.com/microsoft/vcpkg
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install opencascade nlohmann-json --triplet x64-windows
```
> First install takes 45-90 minutes. Grab a coffee.

### 3. Download TinyGLTF dependencies
Run these from inside the project folder:
```powershell
Invoke-WebRequest -Uri "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp" -OutFile "third_party\tinygltf\json.hpp"

Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" -OutFile "third_party\tinygltf\stb_image.h"

Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h" -OutFile "third_party\tinygltf\stb_image_write.h"
```

### 4. Configure
Open **Developer PowerShell for VS 2022** from Start Menu, then:
```powershell
cmake -B build -S . `
  -DCMAKE_TOOLCHAIN_FILE="C:/Users/YOUR_USERNAME/Downloads/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_BUILD_TYPE=Release
```

### 5. Build
```powershell
cmake --build build --config Release
```

Executable will be at `build\bin\Release\cad-step-to-glb.exe`

## Usage

```powershell
.\build\bin\Release\cad-step-to-glb.exe "input.stp" "output.glb" "bom.json"
```

## Common Issues

**`cmake` not found** — Use Developer PowerShell for VS 2022, not regular PowerShell.

**vcpkg can't find Visual Studio** — Open Visual Studio Installer → Modify → check Desktop development with C++.

**Red squiggles in VS Code** — Cosmetic only, does not affect building. See `.vscode/c_cpp_properties.json` to fix IntelliSense.
