# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.22.1)

# Declares and names the project.

project("standalone")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Cap parallel compile/link jobs so ninja doesn't OOM the host. spirv_cross
# and a handful of engine TUs peak around 1.5-2 GB per clang invocation; at
# default (CPU-count) parallelism on a 16-thread host that's 30 GB+ of working
# set and the host page-thrashes or clang dies on SIGSEGV. Mirrors the PSP
# build's -j4 cap (see project_psp_make_j_oom memory). JOB_POOLS works only
# with the Ninja generator, which is what Android Gradle uses.
set_property(GLOBAL PROPERTY JOB_POOLS compile_pool=4 link_pool=2)
set(CMAKE_JOB_POOL_COMPILE compile_pool)
set(CMAKE_JOB_POOL_LINK link_pool)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

include(AndroidNdkModules)
android_ndk_import_module_native_app_glue()

add_definitions(-DPLATFORM_ANDROID=1 -DAPI_VULKAN=1 -DVK_USE_PLATFORM_ANDROID_KHR=1 -DEDITOR=0)

file(GLOB SrcLua "../../../../../../External/Lua/*.c")
file(GLOB SrcVorbis "../../../../../../External/Vorbis/*.c")
file(GLOB SrcSpirvCross "../../../../../../External/Android/spirv_cross/*.cpp")
file(GLOB SrcBullet
        "../../../../../../External/Bullet/BulletCollision/BroadphaseCollision/*.cpp"
        "../../../../../../External/Bullet/BulletCollision/CollisionDispatch/*.cpp"
        "../../../../../../External/Bullet/BulletCollision/CollisionShapes/*.cpp"
        "../../../../../../External/Bullet/BulletCollision/Gimpact/*.cpp"
        "../../../../../../External/Bullet/BulletCollision/NarrowPhaseCollision/*.cpp"
        "../../../../../../External/Bullet/BulletDynamics/Character/*.cpp"
        "../../../../../../External/Bullet/BulletDynamics/ConstraintSolver/*.cpp"
        "../../../../../../External/Bullet/BulletDynamics/Dynamics/*.cpp"
        "../../../../../../External/Bullet/BulletDynamics/Featherstone/*.cpp"
        "../../../../../../External/Bullet/BulletDynamics/MLCPSolvers/*.cpp"
        "../../../../../../External/Bullet/BulletDynamics/Vehicle/*.cpp"
        "../../../../../../External/Bullet/LinearMath/*.cpp")
file(GLOB SrcRecast "../../../../../../External/recastnavigation/Recast/Source/*.cpp")
file(GLOB SrcDetour "../../../../../../External/recastnavigation/Detour/Source/*.cpp")
file(GLOB SrcIrrXML "../../../../../../External/IrrXML/*.cpp")
file(GLOB_RECURSE SrcEngine "../../../../../../Engine/Source/*.cpp")
list(FILTER SrcEngine EXCLUDE REGEX ".*/Engine/Source/Editor/.*")
file(GLOB_RECURSE SrcStandalone "../../../../../../Standalone/Source/*.cpp")

# Pull in the per-project generated EmbeddedScripts.cpp if the project has been
# packaged at least once. Scripts compile into libstandalone.so; assets are
# left out of EmbeddedAssets.cpp on Android (served from AAssetManager instead)
# so the .so doesn't balloon by ~16 MB.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../Standalone/Generated/EmbeddedScripts.cpp")
    list(APPEND SrcStandalone "../../../../../../Standalone/Generated/EmbeddedScripts.cpp")
endif()

# ── Engine addons ────────────────────────────────────────────────────────────
# ActionManager::InjectNativeAddonsIntoCmake writes Standalone/Generated/
# AddonInject.cmake every time we package for Android. It sets:
#   POLYPHASE_ADDON_SOURCE_DIRS   absolute dirs to glob .cpp/.c from
#   POLYPHASE_ADDON_INCLUDE_DIRS  absolute dirs for include_directories()
#   POLYPHASE_ADDON_DEFINES       -D values (FOO=1 BAR=2 ...)
#   POLYPHASE_ADDON_LIB_DIRS      link_directories() entries
#   POLYPHASE_ADDON_LIBS          libs to pass to target_link_libraries
#   POLYPHASE_ADDON_REGISTRAR     absolute path to AddonPlugins.cpp
# Lists are EMPTY when the project has no engine addons — include() is safe.
set(SrcAddons "")
set(POLYPHASE_ADDON_INJECT_CMAKE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../Standalone/Generated/AddonInject.cmake")
if(EXISTS "${POLYPHASE_ADDON_INJECT_CMAKE}")
    include("${POLYPHASE_ADDON_INJECT_CMAKE}")

    foreach(addonDir IN LISTS POLYPHASE_ADDON_SOURCE_DIRS)
        file(GLOB addonCpps "${addonDir}/*.cpp" "${addonDir}/*.c")
        list(APPEND SrcAddons ${addonCpps})
    endforeach()

    foreach(addonInc IN LISTS POLYPHASE_ADDON_INCLUDE_DIRS)
        include_directories("${addonInc}")
    endforeach()

    foreach(addonDef IN LISTS POLYPHASE_ADDON_DEFINES)
        add_compile_definitions(${addonDef})
    endforeach()

    if(POLYPHASE_ADDON_LIB_DIRS)
        link_directories(${POLYPHASE_ADDON_LIB_DIRS})
    endif()

    if(POLYPHASE_ADDON_REGISTRAR AND EXISTS "${POLYPHASE_ADDON_REGISTRAR}")
        list(APPEND SrcStandalone "${POLYPHASE_ADDON_REGISTRAR}")
    endif()
endif()

#
# Include path ORDER MATTERS — Windows is case-insensitive so an addon's
# `#include "Stream.h"` will match `External/Assimp/contrib/rapidjson/stream.h`
# (rapidjson's forward-declaration-only stream class) instead of the engine's
# `Engine/Source/Engine/Stream.h` if rapidjson is on -I first. Symptom:
# clang emits `-Wnonportable-include-path` warning ("Stream.h vs stream.h"),
# then fails downstream with `member access into incomplete type 'Stream'`
# because the wrong file got included.
#
# Engine include dirs must come first so engine headers shadow same-cased
# External ones.
#
include_directories(
        "../../../../../../Engine/Source/"
        "../../../../../../Engine/Source/Engine/"
        "../../../../../../Engine/External/PolyVox/include/")

include_directories("../../../../../../External/Lua/")
include_directories("../../../../../../External/Vorbis/")
include_directories("../../../../../../External/Bullet/")
include_directories("../../../../../../External/Assimp/contrib/rapidjson/")
include_directories("../../../../../../External/recastnavigation/Recast/Include/")
include_directories("../../../../../../External/recastnavigation/Detour/Include/")
include_directories(
        "../../../../../../External/"
        "../../../../../../External/Android/"
        "../../../../../../External/Bullet/")
include_directories("../../../../../../Standalone/Source")

add_library( # Sets the name of the library.
        standalone

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        ${SrcLua} ${SrcVorbis} ${SrcSpirvCross} ${SrcBullet}
        ${SrcRecast} ${SrcDetour} ${SrcIrrXML}
        ${SrcEngine} ${SrcStandalone}
        ${SrcAddons})

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)

find_library( # Sets the name of the path variable.
        vulkan-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        vulkan)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        standalone
        android
        OpenSLES
        native_app_glue
        ${vulkan-lib}
        ${log-lib}
        ${POLYPHASE_ADDON_LIBS})