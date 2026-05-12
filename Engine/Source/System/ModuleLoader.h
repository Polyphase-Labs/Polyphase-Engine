#pragma once

/**
 * @file ModuleLoader.h
 * @brief Cross-platform dynamic library loading utility.
 *
 * Available in EDITOR builds (for hot-loading native addons in the editor)
 * and in shipped Windows/Linux game builds (for loading addon DLLs/SOs that
 * sit alongside the game exe). Consoles (3DS, Wii, GCN, Dolphin, Android)
 * keep the source-compile-in addon model and do not expose this API.
 */

#if PLATFORM_WINDOWS || PLATFORM_LINUX

/**
 * @brief Load a dynamic library.
 *
 * @param path Path to the library file (.dll on Windows, .so on Linux)
 * @return Handle to loaded library, or nullptr on failure
 */
void* MOD_Load(const char* path);

/**
 * @brief Get a symbol (function/variable) from a loaded library.
 *
 * @param handle Library handle from MOD_Load
 * @param name Symbol name to find
 * @return Pointer to the symbol, or nullptr if not found
 */
void* MOD_Symbol(void* handle, const char* name);

/**
 * @brief Unload a dynamic library.
 *
 * @param handle Library handle from MOD_Load
 */
void MOD_Unload(void* handle);

/**
 * @brief Get the last error message.
 *
 * @return Error string, or empty string if no error
 */
const char* MOD_GetError();

#endif // PLATFORM_WINDOWS || PLATFORM_LINUX
