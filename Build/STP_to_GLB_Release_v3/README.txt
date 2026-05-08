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

OUTPUT FILES
------------
  .glb   - 3D mesh, open in Windows 3D Viewer, Blender, online at gltf-viewer.donmccurdy.com
  .json  - Bill of Materials and assembly tree

NOTES
-----
  All required DLLs are included. No installation needed.
  Keep all files in this folder together - do not move just the exe.
