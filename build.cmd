@echo off
rem Dovakarn's build of InventoryReforged.dll: the mod's own preset (Ninja, RelWithDebInfo, x64-windows-static-md,
rem CommonLibSSE-NG 7.2.0 from the overlay port), Visual Studio 2026's compiler, a vcpkg checkout at or after the
rem preset's builtin-baseline (C:\tools\irb\vcpkg)
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set VCPKG_ROOT=C:\tools\irb\vcpkg
set PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
cd /d C:\tools\irb\plugin\equipment-panel || exit /b 1
cmake --preset skyrim-17 || exit /b 1
cmake --build build || exit /b 1
echo BUILD OK
