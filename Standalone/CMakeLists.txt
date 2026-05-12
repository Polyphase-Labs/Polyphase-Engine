set(TARGET EngineEditor)

file(GLOB SRC
    Source/*.cpp
)

add_executable(${TARGET}
    ${SRC}
)

# --- W1: Link selection -------------------------------------------------------
# Three modes:
#   1. Default (both options OFF): static engine, addons resolve symbols against
#      the editor exe (ENABLE_EXPORTS TRUE -> -rdynamic on ELF).
#   2. POLYPHASE_DESKTOP_SHARED_RUNTIME ON: link against the locally-built
#      EngineGameRuntime SHARED target. Addons resolve against the DLL/SO.
#   3. POLYPHASE_USE_PREBUILT_RUNTIME ON: link against an IMPORTED SHARED target
#      pointing at a vendored runtime under <Project>/Vendor/PolyphaseRuntime/.
if(POLYPHASE_USE_PREBUILT_RUNTIME)
    # Default prebuilt-runtime location is <repo>/../Vendor/PolyphaseRuntime/...
    # User projects override via -DPOLYPHASE_PREBUILT_RUNTIME_DIR=<path>.
    set(POLYPHASE_PREBUILT_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/../Vendor/PolyphaseRuntime"
        CACHE PATH "Directory containing vendored PolyphaseRuntime binaries.")

    if(NOT CMAKE_BUILD_TYPE)
        set(_POLY_RUNTIME_CONFIG "Release")
    else()
        set(_POLY_RUNTIME_CONFIG "${CMAKE_BUILD_TYPE}")
    endif()
    set(_POLY_RUNTIME_LAYOUT "${POLYPHASE_PREBUILT_RUNTIME_DIR}/${PLATFORM_NAME}-x64-${_POLY_RUNTIME_CONFIG}")

    add_library(PolyphaseRuntimeImported SHARED IMPORTED)
    if(PLATFORM_NAME STREQUAL "Linux")
        set_target_properties(PolyphaseRuntimeImported PROPERTIES
            IMPORTED_LOCATION "${_POLY_RUNTIME_LAYOUT}/libPolyphaseGame.so"
        )
    elseif(PLATFORM_NAME STREQUAL "Windows")
        set_target_properties(PolyphaseRuntimeImported PROPERTIES
            IMPORTED_LOCATION "${_POLY_RUNTIME_LAYOUT}/PolyphaseGame.dll"
            IMPORTED_IMPLIB   "${_POLY_RUNTIME_LAYOUT}/PolyphaseGame.lib"
        )
    endif()

    target_link_libraries(${TARGET}
        PolyphaseRuntimeImported
    )
    # In shared-runtime mode, addons must resolve engine symbols from the DLL
    # rather than the editor exe, so ENABLE_EXPORTS is disabled here.
    set_target_properties(${TARGET} PROPERTIES ENABLE_EXPORTS FALSE)
elseif(POLYPHASE_DESKTOP_SHARED_RUNTIME)
    target_link_libraries(${TARGET}
        EngineGameRuntime
    )
    set_target_properties(${TARGET} PROPERTIES ENABLE_EXPORTS FALSE)
else()
    target_link_libraries(${TARGET}
        EngineEditorLib
    )
    # Native addons (e.g. com.polyphase.formats.video) are loaded via dlopen at
    # runtime and resolve engine symbols against the executable. ENABLE_EXPORTS
    # adds -rdynamic on ELF platforms so those symbols stay visible to dlopen.
    set_target_properties(${TARGET} PROPERTIES ENABLE_EXPORTS TRUE)
endif()
