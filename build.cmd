@echo off
REM ============================================================
REM  SMPD build — Release x64 only
REM  cmake --preset x64-Release
REM  cmake --build --preset x64-Release
REM  Output: out\build\x64-Release\out\Release\winmm.dll
REM  Requires MASM (ml64, included with the C++ Build Tools).
REM ============================================================
setlocal
set "ROOT=%~dp0"
where cmake >nul 2>&1
if errorlevel 1 (
  echo [ERROR] cmake not found. Install CMake 3.21+ and Ninja, or open the folder in Visual Studio 2026.
  pause
  exit /b 1
)
cmake --preset x64-Release
if errorlevel 1 pause & exit /b 1
cmake --build --preset x64-Release
if errorlevel 1 pause & exit /b 1
echo.
echo [OK] winmm.dll in out\build\x64-Release\out\Release\
echo Copy winmm.dll into ^<Steam^>\ next to LumaCore (dwmapi/xinput1_4/LumaCore.dll stay untouched).
endlocal
