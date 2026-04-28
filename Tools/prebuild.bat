@echo off
REM Master prebuild script for Windows.
REM Runs all prebuild steps needed before opening the solution.

setlocal

set SCRIPT_DIR=%~dp0
set REPO_ROOT=%SCRIPT_DIR%..

echo ============================================
echo  Polyphase Prebuild (Windows)
echo ============================================
echo.

REM --- libgit2 ---
echo [1/3] Building libgit2...
call "%SCRIPT_DIR%prebuild_libgit2.bat"
if %ERRORLEVEL% neq 0 (
    echo [FAILED] libgit2 prebuild failed.
    exit /b 1
)
echo.

REM --- Shaders ---
echo [2/3] Compiling shaders...
pushd "%REPO_ROOT%\Engine\Shaders\GLSL"
call compile.bat
popd
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Shader compilation failed.
    exit /b 1
)
echo.

REM --- Standalone embedded asset stubs ---
echo [3/3] Generating Standalone embedded asset stubs...
python "%SCRIPT_DIR%generate_embedded_stubs.py"
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Embedded asset stub generation failed.
    exit /b 1
)
echo.

echo ============================================
echo  Prebuild complete.
echo ============================================

endlocal
