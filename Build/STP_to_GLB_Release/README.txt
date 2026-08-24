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

THRESHOLD (optional 6th argument, default 500)
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
