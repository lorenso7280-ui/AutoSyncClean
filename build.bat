@echo off
setlocal
where cmake >nul 2>nul || (
  echo Chua tim thay CMake. Hay cai Visual Studio 2022 voi workload Desktop development with C++ va CMake.
  exit /b 1
)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1
cmake --build build --config Release
if errorlevel 1 exit /b 1
echo.
echo Da tao: build\Release\AutoSyncClean v.75 IPC DPI.exe
