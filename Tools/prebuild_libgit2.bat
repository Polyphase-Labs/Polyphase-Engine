@echo off
REM Prebuild libgit2 static libraries for the Visual Studio (MSBuild) build path.
REM Run this once after cloning or updating the libgit2 submodule.
REM Produces git2.lib (Release) and git2d.lib (Debug) in Engine\External\lib\Lib\
REM (this path is referenced as $(SolutionDir)Engine/External/lib/Lib/ by the
REM  Standalone and Engine vcxproj link search paths).
REM
REM IMPORTANT: libgit2 is built with /GL (LTCG IL bytecode), so its compiler
REM version MUST match the cl.exe that MSBuild uses for the rest of the engine.
REM Otherwise the linker rejects git2.lib with C1047. To enforce that, this
REM script discovers MSBuild's VS install root and pins CMake to the same
REM install via CMAKE_GENERATOR_INSTANCE.

setlocal enabledelayedexpansion

set REPO_ROOT=%~dp0..
set LIBGIT2_DIR=%REPO_ROOT%\Engine\External\libgit2
set LIB_OUTPUT_DIR=%REPO_ROOT%\Engine\External\lib\Lib

set MSYS_NO_PATHCONV=1
set MSYS2_ARG_CONV_EXCL=*

REM Skip rebuild if both output libs already exist. Delete them (or pass
REM POLYPHASE_LIBGIT2_FORCE=1) to force a rebuild.
if "%POLYPHASE_LIBGIT2_FORCE%"=="" (
    if exist "%LIB_OUTPUT_DIR%\git2.lib" if exist "%LIB_OUTPUT_DIR%\git2d.lib" (
        echo [libgit2] git2.lib and git2d.lib already present, skipping rebuild.
        echo [libgit2]   %LIB_OUTPUT_DIR%\git2.lib
        echo [libgit2]   %LIB_OUTPUT_DIR%\git2d.lib
        echo [libgit2] Set POLYPHASE_LIBGIT2_FORCE=1 to force a rebuild.
        endlocal
        exit /b 0
    )
)

REM --- Locate MSBuild and derive its VS install root ---
REM MSBuild lives at <VS_ROOT>\MSBuild\Current\Bin\MSBuild.exe, so strip the
REM trailing path to get the install root. This is the install whose toolset
REM will compile the engine, so libgit2 must use the same install.
set MSBUILD_PATH=
for /f "delims=" %%I in ('where msbuild 2^>nul') do (
    if not defined MSBUILD_PATH set "MSBUILD_PATH=%%I"
)
if not defined MSBUILD_PATH (
    echo [libgit2] ERROR: msbuild not found on PATH.
    echo                  Run from a Visual Studio Developer Command Prompt,
    echo                  or add MSBuild to PATH.
    exit /b 1
)
echo [libgit2] MSBuild: %MSBUILD_PATH%

REM Strip "\MSBuild\Current\Bin\MSBuild.exe" to get the VS install root.
set "VS_INSTALL=%MSBUILD_PATH:\MSBuild\Current\Bin\MSBuild.exe=%"
if "%VS_INSTALL%"=="%MSBUILD_PATH%" (
    echo [libgit2] ERROR: Could not derive VS install root from MSBuild path.
    echo                  Expected ...\MSBuild\Current\Bin\MSBuild.exe layout.
    exit /b 1
)
echo [libgit2] VS install: %VS_INSTALL%

set "VS_CMAKE=%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%VS_CMAKE%" (
    echo [libgit2] ERROR: Bundled CMake not found at:
    echo                  %VS_CMAKE%
    echo                  Install the "C++ CMake tools for Windows" component
    echo                  in this VS install, or use a system CMake.
    exit /b 1
)
echo [libgit2] CMake: %VS_CMAKE%

REM --- Wipe any prior build dir so a stale CMakeCache.txt (which may pin a
REM     different VS install) is not reused. CMake doesn't reliably re-pick
REM     CMAKE_GENERATOR_INSTANCE on reconfigure. ---
if exist "%LIBGIT2_DIR%\build" (
    echo [libgit2] Removing stale build directory...
    rmdir /s /q "%LIBGIT2_DIR%\build"
)

pushd "%LIBGIT2_DIR%"

echo [libgit2] Configuring (pinned to %VS_INSTALL%)...
"%VS_CMAKE%" -B build -S . -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_GENERATOR_INSTANCE="%VS_INSTALL%" ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DBUILD_TESTS=OFF ^
    -DBUILD_CLI=OFF ^
    -DBUILD_EXAMPLES=OFF ^
    -DUSE_SSH=OFF ^
    -DUSE_BUNDLED_ZLIB=ON ^
    -DREGEX_BACKEND=builtin
if %ERRORLEVEL% neq 0 ( echo [libgit2] Configure failed. & popd & exit /b 1 )

echo [libgit2] Building Release...
"%VS_CMAKE%" --build build --config Release
if %ERRORLEVEL% neq 0 ( echo [libgit2] Release build failed. & popd & exit /b 1 )

echo [libgit2] Building Debug...
"%VS_CMAKE%" --build build --config Debug
if %ERRORLEVEL% neq 0 ( echo [libgit2] Debug build failed. & popd & exit /b 1 )

popd

if not exist "%LIB_OUTPUT_DIR%" mkdir "%LIB_OUTPUT_DIR%"
copy /Y "%LIBGIT2_DIR%\build\Release\git2.lib" "%LIB_OUTPUT_DIR%\git2.lib"
copy /Y "%LIBGIT2_DIR%\build\Debug\git2.lib" "%LIB_OUTPUT_DIR%\git2d.lib"

echo [libgit2] Done:
echo   %LIB_OUTPUT_DIR%\git2.lib   (Release)
echo   %LIB_OUTPUT_DIR%\git2d.lib  (Debug)

endlocal
