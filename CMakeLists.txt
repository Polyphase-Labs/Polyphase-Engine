# Version 3.7 needed for find_vulkan
cmake_minimum_required(VERSION 3.7)

project(polyphase)

add_subdirectory(External)

if(WIN32)
    set(PLATFORM_NAME "Windows")
elseif(ANDROID)
    set(PLATFORM_NAME "Android")
elseif(CMAKE_SYSTEM_NAME MATCHES "NintendoWii" OR
       CMAKE_SYSTEM_NAME MATCHES "NintendoGameCube")
    set(PLATFORM_NAME "Dolphin")
elseif(CMAKE_SYSTEM_NAME MATCHES "Nintendo3DS")
    set(PLATFORM_NAME "3ds")
elseif(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
    set(PLATFORM_NAME "Linux")
endif()

string(TOUPPER ${PLATFORM_NAME} PLATFORM_DEF)
add_compile_definitions(
    -DPLATFORM_${PLATFORM_DEF}
)

# --- W1: Desktop shared-runtime build mode -----------------------------------
# When POLYPHASE_DESKTOP_SHARED_RUNTIME is ON, the engine is additionally built
# as a SHARED library (libPolyphaseGame.so / PolyphaseGame.dll) on Windows/Linux
# only. Default behavior (static engine) is preserved when both options are OFF.
# Consoles must hard-error when this option is requested.
option(POLYPHASE_DESKTOP_SHARED_RUNTIME "Build engine as shared library (Win/Linux desktop)" OFF)
option(POLYPHASE_USE_PREBUILT_RUNTIME   "Link standalone against vendored runtime"            OFF)

if(POLYPHASE_DESKTOP_SHARED_RUNTIME AND NOT (PLATFORM_NAME STREQUAL "Windows" OR PLATFORM_NAME STREQUAL "Linux"))
    message(FATAL_ERROR "POLYPHASE_DESKTOP_SHARED_RUNTIME is desktop-only (Windows/Linux).")
endif()

add_subdirectory(Engine)

add_subdirectory(Standalone)
