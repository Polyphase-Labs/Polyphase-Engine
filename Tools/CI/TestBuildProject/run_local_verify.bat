@echo off
REM One-shot local replica of the verify-test-project CI matrix for game-build
REM verification. Assumes the Polyphase editor is already built - does NOT
REM rebuild it. Clones the configured test project, runs the headless game
REM build for each requested platform, and cleans up.
REM
REM Usage:
REM   Tools\CI\TestBuildProject\run_local_verify.bat [options]
REM
REM Options:
REM   --platforms <list>     Comma list of: Windows,Wii,GameCube,3DS  (default: all four)
REM                          NOTE: Linux is intentionally NOT a default on Windows --
REM                          cross-compiling a Linux ELF requires a Linux toolchain
REM                          that isn't part of devkitPro. Run on a Linux box (or in
REM                          WSL via run_local_verify.sh) for the Linux target.
REM   --repo <url>           Test project git URL  (default: https://github.com/mholtkamp/octo-bombers)
REM   --subdir <name>        Project subdir inside clone  (default: Bomber)
REM   --dest <dir>           Where to clone   (default: .\test-project)
REM   --editor <path>        Editor binary path. If omitted, the script auto-locates
REM                          one in this order:
REM                            1. Polyphase.exe at the solution root (post-build copy)
REM                            2. Standalone\Build\Windows\x64\DebugEditor\Polyphase.exe
REM                            3. Standalone\Build\Windows\x64\ReleaseEditor\Polyphase.exe
REM                          DebugEditor is preferred over ReleaseEditor when both
REM                          exist because dev iteration usually targets DebugEditor
REM                          and you want the freshest binary.
REM   --skip-cleanup         Leave the clone in place for inspection after the run
REM
REM Note: console toolchains (devkitPro for Wii/GameCube/3DS) must already be
REM installed. The .bat does not auto-install them on Windows because devkitPro
REM on Windows requires msys2 setup that's outside this script's scope.
REM
REM Either DebugEditor or ReleaseEditor (x64) configurations work — both end up
REM headless-compatible. Build whichever you'd normally use, then run this.

setlocal EnableExtensions EnableDelayedExpansion

REM Anchor at the repo root. This .bat lives at Tools\CI\TestBuildProject\, so
REM go three levels up.
pushd "%~dp0\..\..\.." >nul

set "PLATFORMS=Windows,Wii,GameCube,3DS"
set "REPO=https://github.com/mholtkamp/octo-bombers"
set "SUBDIR=Bomber"
set "DEST=.\test-project"
REM EDITOR resolved later (after arg parsing) so --editor takes precedence.
set "EDITOR="
set "SKIP_CLEANUP=0"

:parse
if "%~1"=="" goto after_parse
if /I "%~1"=="--platforms"     (set "PLATFORMS=%~2" & shift & shift & goto parse)
if /I "%~1"=="--repo"          (set "REPO=%~2"      & shift & shift & goto parse)
if /I "%~1"=="--subdir"        (set "SUBDIR=%~2"    & shift & shift & goto parse)
if /I "%~1"=="--dest"          (set "DEST=%~2"      & shift & shift & goto parse)
if /I "%~1"=="--editor"        (set "EDITOR=%~2"    & shift & shift & goto parse)
if /I "%~1"=="--skip-cleanup"  (set "SKIP_CLEANUP=1"& shift & goto parse)
if /I "%~1"=="-h"              goto help
if /I "%~1"=="--help"          goto help
echo Unknown argument: %~1 1>&2
popd >nul & exit /b 2

:help
findstr /B "REM " "%~f0"
popd >nul & exit /b 0

:after_parse

REM --- 1. Resolve / verify the editor binary -----------------------------------
REM Search order (only if --editor wasn't supplied):
REM   1. Polyphase.exe at solution root (post-build copy)
REM   2. DebugEditor build output (preferred for dev iteration)
REM   3. ReleaseEditor build output
if not defined EDITOR (
  if exist "Polyphase.exe" (
    set "EDITOR=Polyphase.exe"
  ) else if exist "Standalone\Build\Windows\x64\DebugEditor\Polyphase.exe" (
    set "EDITOR=Standalone\Build\Windows\x64\DebugEditor\Polyphase.exe"
  ) else if exist "Standalone\Build\Windows\x64\ReleaseEditor\Polyphase.exe" (
    set "EDITOR=Standalone\Build\Windows\x64\ReleaseEditor\Polyphase.exe"
  )
)

if not defined EDITOR (
  echo ERROR: no editor binary found. Looked in: 1>&2
  echo          Polyphase.exe ^(solution root^) 1>&2
  echo          Standalone\Build\Windows\x64\DebugEditor\Polyphase.exe 1>&2
  echo          Standalone\Build\Windows\x64\ReleaseEditor\Polyphase.exe 1>&2
  echo        Build either DebugEditor or ReleaseEditor ^(x64^) in Visual Studio, 1>&2
  echo        or pass --editor ^<path^> to point at an existing binary. 1>&2
  popd >nul & exit /b 1
)
if not exist "%EDITOR%" (
  echo ERROR: editor binary not found: %EDITOR% 1>&2
  popd >nul & exit /b 1
)
echo Using editor: %EDITOR%

REM --- 2. Clone the test project ----------------------------------------------
echo ==^> Cloning %REPO% -^> %DEST%
call "Tools\CI\TestBuildProject\clone_test_project.bat" "%REPO%" "%DEST%"
if errorlevel 1 (
  echo ERROR: clone failed. 1>&2
  popd >nul & exit /b 1
)

set "PROJECT_DIR=%DEST%\%SUBDIR%"
if not exist "%PROJECT_DIR%\" (
  echo ERROR: project directory not found: %PROJECT_DIR% 1>&2
  echo        Check --subdir or the repo's layout. 1>&2
  popd >nul & exit /b 1
)

REM --- 3. Run each platform ---------------------------------------------------
set "FAILED="
for %%P in (%PLATFORMS:,= %) do (
  echo.
  echo ================================================================
  echo   %%P
  echo ================================================================
  call "Tools\CI\TestBuildProject\verify_project_build.bat" "%EDITOR%" "%PROJECT_DIR%" %%P
  if errorlevel 1 (
    echo   -^> %%P FAILED
    if defined FAILED (set "FAILED=!FAILED! %%P") else (set "FAILED=%%P")
  ) else (
    echo   -^> %%P PASSED
  )
)

REM --- 4. Cleanup -------------------------------------------------------------
if "%SKIP_CLEANUP%"=="0" (
  echo.
  echo ==^> Cleaning up clone
  call "Tools\CI\TestBuildProject\cleanup_test_project.bat" "%DEST%"
) else (
  echo.
  echo Clone preserved at: %DEST%
)

REM --- 5. Final summary -------------------------------------------------------
echo.
echo ================================================================
if not defined FAILED (
  echo   All requested platforms passed: %PLATFORMS%
  popd >nul & endlocal & exit /b 0
) else (
  echo   FAILED platforms: !FAILED!
  popd >nul & endlocal & exit /b 1
)
