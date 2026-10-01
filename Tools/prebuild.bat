@echo off
REM Master prebuild script for Windows.
REM Runs all prebuild steps needed before opening the solution.

setlocal

set "SCRIPT_DIR=%~dp0"
REM Resolve REPO_ROOT to a normalized absolute path (no trailing "..") so that
REM downstream cd/pushd calls work reliably regardless of how this script was
REM invoked. %~f1 trick: use a for-loop to canonicalize.
for %%I in ("%SCRIPT_DIR%..") do set "REPO_ROOT=%%~fI"
set "SHADER_DIR=%REPO_ROOT%\Engine\Shaders\GLSL"

echo ============================================
echo  Polyphase Prebuild (Windows)
echo ============================================
echo.

REM --- libgit2 ---
echo [1/4] Building libgit2...
call "%SCRIPT_DIR%prebuild_libgit2.bat"
if %ERRORLEVEL% neq 0 (
    echo [FAILED] libgit2 prebuild failed.
    exit /b 1
)
echo.

REM --- Shaders ---
REM compile.bat uses relative paths (.\src, .\bin), so cwd MUST be the shader
REM directory when it runs. Use cd /d with explicit error checks rather than
REM pushd/popd so any failure is loud.
echo [2/4] Compiling shaders...
if not exist "%SHADER_DIR%\compile.bat" (
    echo [FAILED] compile.bat not found at: %SHADER_DIR%\compile.bat
    exit /b 1
)
cd /d "%SHADER_DIR%"
if errorlevel 1 (
    echo [FAILED] Could not change to shader directory: %SHADER_DIR%
    exit /b 1
)
call "%SHADER_DIR%\compile.bat"
if errorlevel 1 (
    echo [FAILED] Shader compilation failed.
    cd /d "%REPO_ROOT%"
    exit /b 1
)
cd /d "%REPO_ROOT%"
echo.

REM --- Standalone embedded asset stubs ---
echo [3/4] Generating Standalone embedded asset stubs...
python "%SCRIPT_DIR%generate_embedded_stubs.py"
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Embedded asset stub generation failed.
    exit /b 1
)
echo.

REM --- Scripting API from the Lua bindings (C# reference assembly + LuaLS stubs) ---
echo [4/4] Generating scripting API from Lua bindings...
python "%SCRIPT_DIR%generate_csharp_api.py"
if %ERRORLEVEL% neq 0 (
    echo [FAILED] C# API generation failed.
    exit /b 1
)
python "%SCRIPT_DIR%generate_lua_stubs.py"
if %ERRORLEVEL% neq 0 (
    echo [FAILED] LuaLS stub generation failed.
    exit /b 1
)
echo.

echo ============================================
echo  Prebuild complete.
echo ============================================

endlocal
