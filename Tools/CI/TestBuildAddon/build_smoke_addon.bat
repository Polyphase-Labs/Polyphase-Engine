@echo off
:: CI smoke-test addon build.
::
:: Compiles Tools/CI/TestBuildAddon/com.polyphase.smoke.material/Source against
:: the freshly-staged dist/Editor/ tree. Mirrors the cl.exe invocation in
:: NativeAddonManager::GenerateBuildScript (Engine/Source/Editor/Addons/
:: NativeAddonManager.cpp ~line 1808) so the link surface is identical to
:: what a real installed-editor addon build would use.
::
:: Fails (exit 1) if any engine symbol the fixture references is missing
:: from Polyphase.lib's export table — i.e. someone removed a POLYPHASE_API
:: from a class addons depend on.
::
:: Usage:
::   Tools\CI\TestBuildAddon\build_smoke_addon.bat <dist-dir>
::
:: <dist-dir> defaults to dist\Editor (matches Installers/stage_distribution.py).

setlocal

set "DIST_DIR=%~1"
if "%DIST_DIR%"=="" set "DIST_DIR=dist\Editor"

if not exist "%DIST_DIR%\Polyphase.lib" (
    echo ERROR: %DIST_DIR%\Polyphase.lib not found.
    echo        Run Installers\stage_distribution.py --platform windows first.
    exit /b 2
)
if not exist "%DIST_DIR%\Lua.lib" (
    echo ERROR: %DIST_DIR%\Lua.lib not found.
    exit /b 2
)

:: Find Visual Studio (same probe NativeAddonManager uses)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)
if not defined VS_PATH (
    echo ERROR: Visual Studio with VC++ toolchain not found.
    exit /b 2
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

if not defined VULKAN_SDK (
    echo ERROR: VULKAN_SDK environment variable is not set.
    exit /b 2
)

set "ADDON_ROOT=%~dp0com.polyphase.smoke.material"
set "OUT_DIR=%~dp0Build\Windows"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo Building CI smoke addon against %DIST_DIR%

cl.exe /nologo /EHsc /std:c++17 /O2 /LD /MD ^
    /DEDITOR=1 /DLUA_ENABLED=1 /DGLM_FORCE_RADIANS ^
    /DPLATFORM_WINDOWS=1 /DAPI_VULKAN=1 /DNOMINMAX ^
    /DOCTAVE_PLUGIN_EXPORT /DPOLYPHASE_SMOKE_MATERIAL_EXPORT ^
    /I"%DIST_DIR%\Engine\Source" ^
    /I"%DIST_DIR%\Engine\Source\Engine" ^
    /I"%DIST_DIR%\Engine\Source\Plugins" ^
    /I"%DIST_DIR%\External" ^
    /I"%DIST_DIR%\External\Lua" ^
    /I"%DIST_DIR%\External\glm" ^
    /I"%DIST_DIR%\External\Imgui" ^
    /I"%DIST_DIR%\External\ImGuizmo" ^
    /I"%DIST_DIR%\External\Bullet" ^
    /I"%DIST_DIR%\External\Vorbis" ^
    /I"%DIST_DIR%\External\Assimp" ^
    /I"%ADDON_ROOT%\Source" ^
    /I"%VULKAN_SDK%\Include" ^
    "%ADDON_ROOT%\Source\SmokeMaterial.cpp" ^
    /Fe"%OUT_DIR%\com.polyphase.smoke.material.dll" ^
    /Fo"%OUT_DIR%\\" ^
    /link /DLL ^
    /LIBPATH:"%DIST_DIR%" ^
    Polyphase.lib Lua.lib

:: Capture cl.exe's exit code before any other command can clobber it. Then
:: compare via an explicit string comparison — using `if %ERRORLEVEL% neq 0 (`
:: inside a parenthesized block trips cmd's parser when echoed messages
:: contain `(` / `)`, producing a useless "`. was unexpected at this time.`"
:: instead of the actual link errors. Pull the check out of any nested block
:: and avoid parens in the diagnostic text so the error surfaces cleanly.
set "CL_EXIT=%ERRORLEVEL%"

if not "%CL_EXIT%"=="0" goto :smoke_failed

echo [OK] CI smoke addon linked against %DIST_DIR%\Polyphase.lib
exit /b 0

:smoke_failed
echo.
echo [FAIL] CI smoke addon build failed with exit %CL_EXIT%.
echo        An engine symbol consumed by the fixture is missing from
echo        %DIST_DIR%\Polyphase.lib. Most likely cause: a class addons
echo        depend on lost its POLYPHASE_API annotation. Check the LNK2001
echo        symbol names above against the list in
echo        Documentation/Development/NativeAddon/NativeAddon.md
echo        under the "Engine API surface available to addons" section.
exit /b 1
