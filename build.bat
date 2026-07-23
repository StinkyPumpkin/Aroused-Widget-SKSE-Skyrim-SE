@echo off
REM Build ArousedWidgetClaude SKSE DLL (renamed from HUDWidgets 2026-05-03)
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64
set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
cd /d C:\dev\HUDWidgets-Claude

if not exist build\release (
    echo === CONFIGURING ===
    cmake --preset release
    if errorlevel 1 (
        echo === CONFIGURE FAILED ===
        exit /b 1
    )
)

echo === BUILDING ===
cmake --build build/release
if errorlevel 1 (
    echo === BUILD FAILED ===
    exit /b 1
)

echo === BUILD SUCCEEDED ===
if exist build\release\ArousedWidgetClaude.dll (
    if not exist "X:\MODDINGSSE\modorganizer2\mods\Aroused Widget--Claude\SKSE\Plugins" mkdir "X:\MODDINGSSE\modorganizer2\mods\Aroused Widget--Claude\SKSE\Plugins"
    copy /y build\release\ArousedWidgetClaude.dll "X:\MODDINGSSE\modorganizer2\mods\Aroused Widget--Claude\SKSE\Plugins\ArousedWidgetClaude.dll"
    echo === DEPLOYED TO MO2 ===
) else (
    echo === DLL NOT FOUND ===
    exit /b 1
)
