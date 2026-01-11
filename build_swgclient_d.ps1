# Set up VS2013 environment
& "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" x86

# Build SwgClient project
& "C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" `
    "C:\titan\client\src\build\win32\SwgClient\SwgClient.vcxproj" `
    /p:Configuration=Debug /p:Platform=Win32