@echo off
rem Dovakarn's build of InventoryReforged.dll (DOVAKARN-BUILD.md): the mod's own preset (Ninja, RelWithDebInfo,
rem x64-windows-static-md, CommonLibSSE-NG 7.2.0 from the overlay port) with Visual Studio's C++ compiler.
rem vcpkg: VCPKG_ROOT when it is set, otherwise a checkout in the folder vcpkg next to this file, cloned and
rem bootstrapped on the first build. Either must be at or after the preset's builtin-baseline.
setlocal
set "ROOT=%~dp0"
set "OWN_VCPKG=%VCPKG_ROOT%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo vswhere.exe was not found: install Visual Studio 2022 or 2026 with the C++ tools & exit /b 1)
set "VS="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (echo Visual Studio with the C++ tools was not found & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"

rem vcvars may point VCPKG_ROOT at the vcpkg inside Visual Studio, which can be older than the baseline
if defined OWN_VCPKG (set "VCPKG_ROOT=%OWN_VCPKG%") else (set "VCPKG_ROOT=%ROOT%vcpkg")
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
	if not exist "%VCPKG_ROOT%\.git" (git clone https://github.com/microsoft/vcpkg "%VCPKG_ROOT%" || exit /b 1)
	call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics || exit /b 1
)

cd /d "%ROOT%plugin\equipment-panel" || exit /b 1
cmake --preset skyrim-17 || exit /b 1
cmake --build build || exit /b 1
echo BUILD OK: %ROOT%plugin\equipment-panel\build\InventoryReforged.dll
