$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"

# Create a temporary batch file to run vcvarsall and then run our commands
$tempFile = New-TemporaryFile
$batchContent = @"
call "$vcvarsall" x64
set PATH=%PATH%;%CD%\ninja-build
rmdir /s /q build
mkdir build
cd build
"C:\Program Files\CMake\bin\cmake.exe" -G "Ninja" ^
    -DCMAKE_MAKE_PROGRAM="%CD%\..\ninja-build\ninja.exe" ^
    -DCMAKE_C_COMPILER="cl.exe" ^
    -DCMAKE_CXX_COMPILER="cl.exe" ^
    -DCMAKE_CUDA_COMPILER="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/bin/nvcc.exe" ^
    ..
"%CD%\..\ninja-build\ninja.exe"
"@
$batchContent | Out-File -FilePath "$tempFile.bat" -Encoding ASCII

# Run the batch file
cmd /c "$tempFile.bat"

# Clean up
Remove-Item "$tempFile.bat"
