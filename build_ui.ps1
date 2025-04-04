# Build script for UI only
# First, ensure we're in the correct directory
$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptPath

# Configure environment 
$VCPKG_ROOT = "C:\vcpkg"
$env:Path = "$VCPKG_ROOT;$env:Path"

# Call Visual Studio developer shell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64

# Create build directory if it doesn't exist
if (-not (Test-Path "build_ui")) {
    New-Item -Path "build_ui" -ItemType Directory
}

# Navigate to build directory
Set-Location "build_ui"

# Configure with CMake, focusing on UI only
cmake -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
    -DBUILD_CLI=OFF `
    -DBUILD_UI=ON `
    -DCMAKE_BUILD_TYPE=RelWithDebInfo `
    ..

# Build just the UI target
cmake --build . --target miner_ui

# Return to the original directory
Set-Location $scriptPath

Write-Host "UI Build complete. Executable is at: build_ui\src\ui\miner_ui.exe" -ForegroundColor Green
