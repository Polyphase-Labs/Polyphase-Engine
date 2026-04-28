@echo off
REM Prebuild libgit2 static libraries for the Visual Studio (MSBuild) build path.
REM Run this once after cloning or updating the libgit2 submodule.
REM Produces git2.lib (Release) and git2d.lib (Debug) in External\lib\Lib\

setlocal

set REPO_ROOT=%~dp0..
set LIBGIT2_DIR=%REPO_ROOT%\Engine\External\libgit2
set LIB_OUTPUT_DIR=%REPO_ROOT%\External\lib\Lib

set MSYS_NO_PATHCONV=1
set MSYS2_ARG_CONV_EXCL=*

REM Find the VS-bundled CMake
set VS_CMAKE=
set "VS_BASE=C:\Program Files\Microsoft Visual Studio\2022"
for %%E in (Community Professional Enterprise) do (
    if exist "%VS_BASE%\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        if not defined VS_CMAKE (
            set "VS_CMAKE=%VS_BASE%\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        )
    )
)
if not defined VS_CMAKE (
    echo [libgit2] ERROR: Could not find Visual Studio 2022 CMake.
    exit /b 1
)
echo [libgit2] Using CMake: %VS_CMAKE%

pushd "%LIBGIT2_DIR%"

echo [libgit2] Configuring...
"%VS_CMAKE%" -B build -S . -G "Visual Studio 17 2022" -A x64 ^
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
