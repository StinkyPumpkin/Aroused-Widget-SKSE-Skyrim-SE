@echo off
REM ArousedWidget 3.8.0+ (branch ng91): CommonLibSSE-NG built from the pinned submodule extern\CommonLibSSE-NG. See BUILD.md.
REM VS2022 Community (14.44) environment; the overlay triplet in cmake\ pins vcpkg deps to the same 14.44 toolset.
REM Configures build\ng\release on first run, then builds. When SKYRIM_MODS_FOLDER is set, POST_BUILD copies
REM ArousedWidget.dll into %SKYRIM_MODS_FOLDER%\Aroused Widget--Claude\SKSE\Plugins, so never run this while the game is running.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
set VCPKG_ROOT=C:\vcpkg
cd /d %~dp0
if not exist extern\CommonLibSSE-NG\cmake\CommonLibSSE.cmake (
    echo === extern\CommonLibSSE-NG missing: git submodule update --init --recursive --depth 1 ===
    exit /b 1
)
if not exist build\ng\release\build.ninja (
    echo === CONFIGURING ===
    cmake --preset release
    if errorlevel 1 ( echo === CONFIGURE FAILED === & exit /b 1 )
)
echo === BUILDING ===
cmake --build build\ng\release
if errorlevel 1 ( echo === BUILD FAILED === & exit /b 1 )
echo === BUILD SUCCEEDED ===
