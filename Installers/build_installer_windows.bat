@echo off
REM =========================================================================
REM  build_installer_windows.bat
REM  Full build pipeline for Polyphase Engine Windows 64-bit installer.
REM
REM  This script performs the complete build process:
REM    1. Initialize git submodules
REM    2. Run master prebuild (libgit2, shaders, embedded asset stubs)
REM    3. Build Engine (ReleaseEditor x64)
REM    4. Stage distribution files
REM    5. Build Inno Setup installer
REM
REM  Prerequisites:
REM    - Visual Studio 2022 with MSBuild (or VS Developer Command Prompt)
REM      (the bundled CMake is used to build libgit2)
REM    - Vulkan SDK installed (VULKAN_SDK environment variable set)
REM    - Python 3 on PATH
REM    - Inno Setup installed (ISCC.exe on PATH or at default location)
REM
REM  Usage: Installers\build_installer_windows.bat
REM =========================================================================

setlocal enabledelayedexpansion

REM Navigate to repo root
cd /d "%~dp0.."

echo ============================================
echo  Polyphase Engine - Windows 64-bit Full Build
echo ============================================
echo.

REM --- Check prerequisites ---
echo Checking prerequisites...

if not defined VULKAN_SDK (
    echo ERROR: VULKAN_SDK environment variable not set.
    echo        Install the Vulkan SDK and set VULKAN_SDK.
    exit /b 1
)
echo   [OK] VULKAN_SDK = %VULKAN_SDK%

where python >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found on PATH.
    exit /b 1
)
echo   [OK] Python found

where msbuild >nul 2>&1
if errorlevel 1 (
    echo WARNING: MSBuild not found on PATH.
    echo          Run this script from a VS Developer Command Prompt,
    echo          or ensure MSBuild is in your PATH.
    echo.
)

REM --- Make sure Polyphase.exe isn't running ---
REM A running editor holds Polyphase.exe open, and the ReleaseEditor link
REM step then fails with LNK1104 "cannot open file ...\Polyphase.exe" after a
REM 17-minute build. Catch it up front instead.
REM
REM Uses PowerShell's Get-Process so the check is locale-independent and
REM doesn't depend on tasklist's "INFO: No tasks are running..." stderr
REM behavior, which produced false positives on some Windows builds.
REM Set POLYPHASE_SKIP_RUNNING_CHECK=1 to bypass.
if defined POLYPHASE_SKIP_RUNNING_CHECK goto :running_check_skipped

powershell -NoProfile -Command "if (Get-Process -Name 'Polyphase' -ErrorAction SilentlyContinue) { exit 1 } else { exit 0 }"
if errorlevel 1 (
    echo ERROR: Polyphase.exe is currently running.
    echo        The linker can't overwrite a running executable, so the engine
    echo        build would fail at the link step with LNK1104.
    echo        Close every running Polyphase editor instance and re-run.
    echo.
    echo        To force-close from this prompt:
    echo            taskkill /F /IM Polyphase.exe
    echo.
    echo        To bypass this check, only if you are sure nothing is running:
    echo            set POLYPHASE_SKIP_RUNNING_CHECK=1
    exit /b 1
)
echo   [OK] No running Polyphase.exe instances.
goto :running_check_done

:running_check_skipped
echo   [SKIP] Running-process check disabled by POLYPHASE_SKIP_RUNNING_CHECK.

:running_check_done

echo.

REM --- Step 1: Initialize submodules ---
echo [1/5] Initializing git submodules...
git submodule init -- External/bullet3 External/doxygen-awesome-css External/zep Engine/External/PolyVox Plugins/Blender/polyshade-gameengine-connect
if errorlevel 1 (
    echo ERROR: Submodule init failed.
    exit /b 1
)
git submodule update --recursive
if errorlevel 1 (
    echo ERROR: Submodule update failed.
    exit /b 1
)
echo   Submodules initialized.
echo.

REM --- Step 2: Master prebuild (libgit2, shaders, embedded asset stubs) ---
echo [2/5] Running prebuild (libgit2, shaders, embedded asset stubs)...
call Tools\prebuild.bat
if errorlevel 1 (
    echo ERROR: Prebuild failed.
    exit /b 1
)
echo   Prebuild complete.
echo.

REM --- Step 3: Build Engine ---
echo [3/5] Building Engine (ReleaseEditor x64)...
msbuild Polyphase.sln /p:Configuration=ReleaseEditor /p:Platform=x64 /m
if errorlevel 1 (
    echo ERROR: Engine build failed.
    exit /b 1
)
echo   Engine built successfully.

REM --- Verify import libraries exist ---
REM stage_distribution.py only WARNS when these are missing, which produces a
REM broken installer where native addons can't link (LNK1181: Polyphase.lib
REM not found). Fail the pipeline here instead so the issue is caught loudly.
echo   Verifying import libraries for native addon builds...
if not exist "Standalone\Build\Windows\x64\ReleaseEditor\Polyphase.lib" (
    echo ERROR: Polyphase.lib was not produced by the engine build.
    echo        Expected: Standalone\Build\Windows\x64\ReleaseEditor\Polyphase.lib
    echo        Without it, native addons cannot link against the installed editor.
    echo        Check that the Standalone project is part of the ReleaseEditor build.
    exit /b 1
)
if not exist "External\Lua\Build\Windows\x64\ReleaseEditor\Lua.lib" (
    echo ERROR: Lua.lib was not produced by the engine build.
    echo        Expected: External\Lua\Build\Windows\x64\ReleaseEditor\Lua.lib
    echo        Native addons that use Lua require this import library.
    exit /b 1
)
echo   [OK] Polyphase.lib and Lua.lib found.
echo.

REM --- Step 4: Stage distribution files ---
echo [4/5] Staging distribution files...
python Installers\stage_distribution.py --platform windows --verbose
if errorlevel 1 (
    echo ERROR: Staging failed.
    exit /b 1
)

REM --- Verify the import libraries actually made it into the distribution ---
REM stage_distribution.py defaults --output-dir to dist\Editor.
if not exist "dist\Editor\Polyphase.lib" (
    echo ERROR: Polyphase.lib is missing from the staged distribution.
    echo        Expected: dist\Editor\Polyphase.lib
    echo        The installer would ship without native-addon link support.
    exit /b 1
)
if not exist "dist\Editor\Lua.lib" (
    echo ERROR: Lua.lib is missing from the staged distribution.
    echo        Expected: dist\Editor\Lua.lib
    exit /b 1
)
echo   [OK] Import libraries staged into dist\Editor\.
echo.

REM --- Step 5: Build installer ---
echo [5/5] Building installer...

set "ISCC="

REM Check PATH first
where iscc >nul 2>&1
if not errorlevel 1 (
    set "ISCC=iscc"
    goto :found_iscc
)

REM Check default install locations
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    goto :found_iscc
)
if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe"
    goto :found_iscc
)

echo ERROR: Inno Setup compiler (ISCC.exe) not found.
echo        Install Inno Setup 6 from https://jrsoftware.org/isinfo.php
echo        or add ISCC.exe to your PATH.
exit /b 1

:found_iscc
"%ISCC%" Installers\Windows\PolyphaseSetup.iss
if errorlevel 1 (
    echo ERROR: Inno Setup compilation failed.
    exit /b 1
)

echo.
echo ============================================
echo  BUILD COMPLETE!
echo ============================================
echo.
echo  Installer: dist\PolyphaseSetup-*.exe
echo.
echo ============================================
