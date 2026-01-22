# Building

## Requirements
- Visual Studio (MSVC) with Desktop C++ workload
- CMake
- vcpkg installed at C:\vcpkg

## Build (Release)
cmake --preset msvc-release
cmake --build --preset release

## Run
.\build\Release\gameoflife.exe
