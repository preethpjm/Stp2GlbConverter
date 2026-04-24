=== STEP to GLB Converter ===

REQUIREMENTS:
- Windows 10 or 11 (64-bit)
- Visual C++ Redistributable 2022
  Download: https://aka.ms/vs/17/release/vc_redist.x64.exe

HOW TO USE:

Option 1 - Drag and Drop (Easiest):
  Just drag your .stp file onto convert.bat
  The output .glb file will appear in the same folder as your .stp file

Option 2 - Command Line:
  Open CMD in this folder and run:
  convert.bat "path\to\your\file.stp" "output.glb"

OUTPUT FILES:
  - output.glb      : 3D model (open in Windows 3D Viewer, Blender, etc)
  - output_bom.json : Bill of materials + assembly tree

VIEW YOUR GLB FILE:
  - Online:  https://gltf.report  (drag and drop)
  - Windows: 3D Viewer app (built into Windows)
  - Blender: File > Import > glTF 2.0
