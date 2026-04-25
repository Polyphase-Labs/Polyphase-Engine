set(TARGET EngineEditor)

file(GLOB SRC
    Source/*.cpp
)

add_executable(${TARGET}
    ${SRC}
)
target_link_libraries(${TARGET}
    EngineEditorLib
)

# Native addons (e.g. com.polyphase.formats.video) are loaded via dlopen at
# runtime and resolve engine symbols against the executable. ENABLE_EXPORTS
# adds -rdynamic on ELF platforms so those symbols stay visible to dlopen.
set_target_properties(${TARGET} PROPERTIES ENABLE_EXPORTS TRUE)
