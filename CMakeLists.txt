cmake_minimum_required(VERSION 3.21)
# Renamed 2026-08-11: ArousedWidgetClaude -> ArousedWidget for Nexus release.
# DLL name and SKSE-registered plugin name both follow the project name.
# iHUDClaude's ArousedWidgetBridge kReceiver must match this name.
project(ArousedWidget VERSION 0.3.2 LANGUAGES CXX)

# Output to MO2 mods folder
if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/Aroused Widget--Claude")
endif()

find_package(CommonLibSSE CONFIG REQUIRED)
add_commonlibsse_plugin(${PROJECT_NAME} SOURCES
    src/plugin.cpp
    src/WidgetController.cpp
    src/ArousalReader.cpp
    src/JsonStore.cpp
    src/Settings.cpp
    src/HudUI.cpp
    src/SettingsUI.cpp
    src/Visibility.cpp
    src/HotkeyHandler.cpp
    src/iHUDBridge.cpp
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE PCH.h)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

if(DEFINED OUTPUT_FOLDER)
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")
    set(TEX_FOLDER "${OUTPUT_FOLDER}/Interface/HUDWidgets")

    message(STATUS "SKSE plugin output: ${DLL_FOLDER}")
    message(STATUS "Texture root      : ${TEX_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
            TARGET "${PROJECT_NAME}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            VERBATIM
        )
    endif()
endif()
