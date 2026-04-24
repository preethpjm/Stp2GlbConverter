Write-Host "Building..."
cmake --build build --config Release

Write-Host "Copying exe to release folder..."
Copy-Item "build\bin\Release\cad-step-to-glb.exe" `
  "C:\Users\Preeth\Downloads\STP_to_GLB_Release\" -Force

Write-Host "Creating ZIP..."
Compress-Archive `
  -Path "C:\Users\Preeth\Downloads\STP_to_GLB_Release\*" `
  -DestinationPath "C:\Users\Preeth\Downloads\STP_to_GLB_Converter.zip" `
  -Force

Write-Host "Done! ZIP is at C:\Users\Preeth\Downloads\STP_to_GLB_Converter.zip"