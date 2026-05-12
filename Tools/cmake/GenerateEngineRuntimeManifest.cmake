# GenerateEngineRuntimeManifest.cmake
#
# Pure-CMake script. Generates `engine-runtime.json` (schema v1) next to a
# freshly-built engine shared library. Invoke as:
#
#   cmake -P GenerateEngineRuntimeManifest.cmake \
#         -DPOLY_TARGET_FILE=<path to .dll/.so>   \
#         -DPOLY_OUTPUT_FILE=<path to engine-runtime.json> \
#         [-DPOLY_TARGET_PLATFORM=Linux|Windows]  \
#         [-DPOLY_TARGET_ARCH=x64]                \
#         [-DPOLY_TARGET_CONFIG=Release|Debug]    \
#         [-DPOLY_ENGINE_SOURCE_DIR=<repo root>]  \
#         [-DPOLY_COMPILER_ID=MSVC|GCC|Clang]     \
#         [-DPOLY_COMPILER_VERSION=...]           \
#         [-DPOLY_COMPILER_CRT=...]               \
#         [-DPOLY_IMPORT_LIB=<path or empty>]     \
#         [-DPOLY_DEBUG_SYMBOLS=<path or empty>]
#
# Required: POLY_TARGET_FILE, POLY_OUTPUT_FILE.
# The script never invokes the EngineGameRuntime build target; W1 owns
# the POST_BUILD wire-up in a follow-up integration commit.

cmake_minimum_required(VERSION 3.10)

# ---------------------------------------------------------------------------
# Argument validation
# ---------------------------------------------------------------------------
if(NOT DEFINED POLY_TARGET_FILE OR POLY_TARGET_FILE STREQUAL "")
    message(FATAL_ERROR "POLY_TARGET_FILE is required")
endif()
if(NOT EXISTS "${POLY_TARGET_FILE}")
    message(FATAL_ERROR "POLY_TARGET_FILE does not exist: ${POLY_TARGET_FILE}")
endif()
if(NOT DEFINED POLY_OUTPUT_FILE OR POLY_OUTPUT_FILE STREQUAL "")
    message(FATAL_ERROR "POLY_OUTPUT_FILE is required")
endif()

# Default optional args.
if(NOT DEFINED POLY_TARGET_PLATFORM OR POLY_TARGET_PLATFORM STREQUAL "")
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(POLY_TARGET_PLATFORM "Windows")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(POLY_TARGET_PLATFORM "Linux")
    else()
        set(POLY_TARGET_PLATFORM "${CMAKE_SYSTEM_NAME}")
    endif()
endif()
if(NOT DEFINED POLY_TARGET_ARCH OR POLY_TARGET_ARCH STREQUAL "")
    set(POLY_TARGET_ARCH "x64")
endif()
if(NOT DEFINED POLY_TARGET_CONFIG OR POLY_TARGET_CONFIG STREQUAL "")
    set(POLY_TARGET_CONFIG "Release")
endif()
if(NOT DEFINED POLY_COMPILER_ID OR POLY_COMPILER_ID STREQUAL "")
    set(POLY_COMPILER_ID "unknown")
endif()
if(NOT DEFINED POLY_COMPILER_VERSION)
    set(POLY_COMPILER_VERSION "")
endif()
if(NOT DEFINED POLY_COMPILER_CRT)
    set(POLY_COMPILER_CRT "")
endif()
if(NOT DEFINED POLY_IMPORT_LIB)
    set(POLY_IMPORT_LIB "")
endif()
if(NOT DEFINED POLY_DEBUG_SYMBOLS)
    set(POLY_DEBUG_SYMBOLS "")
endif()

# ---------------------------------------------------------------------------
# Locate the engine source directory.
# ---------------------------------------------------------------------------
if(NOT DEFINED POLY_ENGINE_SOURCE_DIR OR POLY_ENGINE_SOURCE_DIR STREQUAL "")
    # Default to one level above this script's directory
    # (<repo>/Engine/Build/Generate...cmake -> <repo>).
    get_filename_component(_self_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
    get_filename_component(POLY_ENGINE_SOURCE_DIR "${_self_dir}/../.." ABSOLUTE)
endif()

set(_constants_h  "${POLY_ENGINE_SOURCE_DIR}/Engine/Source/Engine/Constants.h")
set(_abi_h        "${POLY_ENGINE_SOURCE_DIR}/Engine/Source/Engine/PolyphaseAbi.h")
set(_plugin_api_h "${POLY_ENGINE_SOURCE_DIR}/Engine/Source/Plugins/PolyphasePluginAPI.h")
set(_asset_h      "${POLY_ENGINE_SOURCE_DIR}/Engine/Source/Engine/Asset.h")
foreach(_f IN ITEMS "${_constants_h}" "${_abi_h}" "${_plugin_api_h}" "${_asset_h}")
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR "Engine source header not found: ${_f}\n"
                            "Pass -DPOLY_ENGINE_SOURCE_DIR=<repo root> if your tree layout differs.")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Helper: extract a `#define NAME <value>` from a header. <value> may be
# quoted (string) or a bare integer.
# ---------------------------------------------------------------------------
function(poly_extract_define out_var header_path macro_name)
    file(READ "${header_path}" _contents)
    # Match either: #define NAME "value"   or   #define NAME 123
    # The unquoted alternative needs to match bare identifiers, integers, and
    # version-ish tokens like "6.2.0-beta.4". CMake regex treats '-' and '.'
    # as literal inside a char class when escaped; '+' must be escaped to
    # mean literal-plus rather than the one-or-more quantifier.
    string(REGEX MATCH "#[ \t]*define[ \t]+${macro_name}[ \t]+(\"[^\"]*\"|[A-Za-z0-9_.\\+\\-]+)" _m "${_contents}")
    if(NOT _m)
        message(FATAL_ERROR "Could not find #define ${macro_name} in ${header_path}")
    endif()
    set(_val "${CMAKE_MATCH_1}")
    # Strip surrounding quotes if present.
    string(REGEX REPLACE "^\"(.*)\"$" "\\1" _val "${_val}")
    set(${out_var} "${_val}" PARENT_SCOPE)
endfunction()

poly_extract_define(POLY_VERSION_STRING      "${_constants_h}"  "POLYPHASE_VERSION_STRING")
poly_extract_define(POLY_VERSION_MAJOR       "${_constants_h}"  "POLYPHASE_VERSION")
poly_extract_define(POLY_ENGINE_ABI          "${_abi_h}"        "POLYPHASE_ENGINE_ABI")
poly_extract_define(POLY_PLUGIN_API_VERSION  "${_plugin_api_h}" "POLYPHASE_PLUGIN_API_VERSION")
poly_extract_define(POLY_ASSET_VERSION       "${_asset_h}"      "ASSET_VERSION_CURRENT")

# ---------------------------------------------------------------------------
# Compute file SHA256 and size.
# ---------------------------------------------------------------------------
file(SHA256 "${POLY_TARGET_FILE}" POLY_MODULE_SHA256)
file(SIZE   "${POLY_TARGET_FILE}" POLY_MODULE_SIZE)
get_filename_component(POLY_MODULE_NAME "${POLY_TARGET_FILE}" NAME)

# Optional companion artifacts — pass through their basenames only.
set(POLY_IMPORT_LIB_NAME "")
if(NOT POLY_IMPORT_LIB STREQUAL "")
    get_filename_component(POLY_IMPORT_LIB_NAME "${POLY_IMPORT_LIB}" NAME)
endif()
set(POLY_DEBUG_SYMBOLS_NAME "")
if(NOT POLY_DEBUG_SYMBOLS STREQUAL "")
    get_filename_component(POLY_DEBUG_SYMBOLS_NAME "${POLY_DEBUG_SYMBOLS}" NAME)
endif()

# ---------------------------------------------------------------------------
# Build hash via git, falling back to "local".
# ---------------------------------------------------------------------------
set(POLY_BUILD_HASH "local")
find_program(_git_exe git)
if(_git_exe)
    execute_process(
        COMMAND "${_git_exe}" rev-parse --short HEAD
        WORKING_DIRECTORY "${POLY_ENGINE_SOURCE_DIR}"
        OUTPUT_VARIABLE _git_sha
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _git_rc
        ERROR_QUIET
    )
    if(_git_rc EQUAL 0 AND NOT _git_sha STREQUAL "")
        set(POLY_BUILD_HASH "${_git_sha}")
    endif()
endif()

# ---------------------------------------------------------------------------
# ISO-8601 UTC timestamp.
# ---------------------------------------------------------------------------
string(TIMESTAMP POLY_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)

# ---------------------------------------------------------------------------
# JSON escape helper.
# ---------------------------------------------------------------------------
function(poly_json_escape in_str out_var)
    set(_s "${in_str}")
    string(REPLACE "\\" "\\\\" _s "${_s}")
    string(REPLACE "\"" "\\\"" _s "${_s}")
    set(${out_var} "${_s}" PARENT_SCOPE)
endfunction()

poly_json_escape("${POLY_VERSION_STRING}"      _e_version)
poly_json_escape("${POLY_BUILD_HASH}"          _e_buildHash)
poly_json_escape("${POLY_BUILD_TIMESTAMP}"     _e_buildTs)
poly_json_escape("${POLY_TARGET_PLATFORM}"     _e_platform)
poly_json_escape("${POLY_TARGET_ARCH}"         _e_arch)
poly_json_escape("${POLY_TARGET_CONFIG}"       _e_config)
poly_json_escape("${POLY_MODULE_NAME}"         _e_module)
poly_json_escape("${POLY_IMPORT_LIB_NAME}"     _e_importLib)
poly_json_escape("${POLY_DEBUG_SYMBOLS_NAME}"  _e_debugSyms)
poly_json_escape("${POLY_MODULE_SHA256}"       _e_sha256)
poly_json_escape("${POLY_COMPILER_ID}"         _e_compId)
poly_json_escape("${POLY_COMPILER_VERSION}"    _e_compVer)
poly_json_escape("${POLY_COMPILER_CRT}"        _e_compCrt)

# ---------------------------------------------------------------------------
# Emit the JSON. Layout matches the documented v1 schema, pretty-printed
# with 2-space indentation. Empty strings for optional binary fields
# (importLib / debugSymbols) follow the convention in the C++ header.
# ---------------------------------------------------------------------------
set(_json
"{
  \"schemaVersion\": 1,
  \"engine\": {
    \"name\": \"Polyphase\",
    \"version\": \"${_e_version}\",
    \"versionMajor\": ${POLY_VERSION_MAJOR},
    \"abi\": ${POLY_ENGINE_ABI},
    \"buildHash\": \"${_e_buildHash}\",
    \"buildTimestampUtc\": \"${_e_buildTs}\"
  },
  \"target\": {
    \"platform\": \"${_e_platform}\",
    \"arch\": \"${_e_arch}\",
    \"config\": \"${_e_config}\"
  },
  \"addonApiVersion\": ${POLY_PLUGIN_API_VERSION},
  \"assetVersion\": ${POLY_ASSET_VERSION},
  \"binary\": {
    \"module\": \"${_e_module}\",
    \"importLib\": \"${_e_importLib}\",
    \"debugSymbols\": \"${_e_debugSyms}\",
    \"moduleSha256\": \"${_e_sha256}\",
    \"moduleSize\": ${POLY_MODULE_SIZE}
  },
  \"requiredExports\": [
    \"PolyphaseRuntime_QueryInterface\",
    \"PolyphaseRuntime_GetAbi\",
    \"PolyphaseRuntime_GetVersionString\"
  ],
  \"compiler\": {
    \"id\": \"${_e_compId}\",
    \"version\": \"${_e_compVer}\",
    \"crt\": \"${_e_compCrt}\"
  }
}
")

get_filename_component(_out_dir "${POLY_OUTPUT_FILE}" DIRECTORY)
if(_out_dir AND NOT EXISTS "${_out_dir}")
    file(MAKE_DIRECTORY "${_out_dir}")
endif()
file(WRITE "${POLY_OUTPUT_FILE}" "${_json}")

message(STATUS "[manifest] wrote: ${POLY_OUTPUT_FILE}")
