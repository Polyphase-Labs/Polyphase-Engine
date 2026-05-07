@echo off
REM Drive a headless Polyphase editor build of an external project for one
REM platform, then verify the expected artifact landed under
REM <project>\Packaged\<Platform>\.
REM
REM Usage:
REM   Tools\CI\verify_project_build.bat <editor-binary> <project-dir> <platform>
REM
REM <platform> in { Windows, Linux, Wii, GameCube, 3DS }
REM
REM Exit codes:
REM   0  build succeeded and artifact is present
REM   1  build failed or artifact missing/empty
REM   2  bad arguments / pre-flight failure

setlocal EnableExtensions EnableDelayedExpansion

if "%~3"=="" (
  echo Usage: %~nx0 ^<editor-binary^> ^<project-dir^> ^<platform^> 1>&2
  exit /b 2
)

set "EDITOR=%~1"
set "PROJECT_DIR=%~2"
set "PLATFORM=%~3"

REM Resolve absolute paths.
for %%I in ("%EDITOR%") do set "EDITOR_ABS=%%~fI"
for %%I in ("%PROJECT_DIR%") do set "PROJECT_ABS=%%~fI"

set "EXT="
if /I "%PLATFORM%"=="Windows"  set "EXT=.exe"
if /I "%PLATFORM%"=="Linux"    set "EXT=.elf"
if /I "%PLATFORM%"=="Wii"      set "EXT=.dol"
if /I "%PLATFORM%"=="GameCube" set "EXT=.dol"
if /I "%PLATFORM%"=="3DS"      set "EXT=.3dsx"
if "%EXT%"=="" (
  echo Unknown platform '%PLATFORM%' ^(expected: Windows, Linux, Wii, GameCube, 3DS^) 1>&2
  exit /b 2
)

if not exist "%EDITOR_ABS%" (
  echo ERROR: editor binary not found: %EDITOR_ABS% 1>&2
  exit /b 2
)
if not exist "%PROJECT_ABS%\" (
  echo ERROR: project directory not found: %PROJECT_ABS% 1>&2
  exit /b 2
)

set "FOUND_OCTP="
for %%F in ("%PROJECT_ABS%\*.octp") do set "FOUND_OCTP=%%F"
if "%FOUND_OCTP%"=="" (
  echo ERROR: no .octp project file found in %PROJECT_ABS% 1>&2
  exit /b 2
)

echo ==^> verify_project_build
echo     editor:   %EDITOR_ABS%
echo     project:  %PROJECT_ABS%
echo     platform: %PLATFORM%
echo     expect:   Packaged\%PLATFORM%\*%EXT%
echo.

set "LOG_FILE=%TEMP%\polyphase-verify-%RANDOM%-%RANDOM%.log"

REM Run the headless build, teeing output to a log we can dump on failure.
"%EDITOR_ABS%" -headless -project "%PROJECT_ABS%" -build %PLATFORM% embedded > "%LOG_FILE%" 2>&1
set "EDITOR_RC=%ERRORLEVEL%"
type "%LOG_FILE%"

if not "%EDITOR_RC%"=="0" (
  echo.
  echo FAIL: editor exited with code %EDITOR_RC% building %PLATFORM% 1>&2
  echo ----- last 200 lines of editor output ----- 1>&2
  powershell -NoProfile -Command "Get-Content -Tail 200 -LiteralPath '%LOG_FILE%'" 1>&2
  del /q "%LOG_FILE%" 2>nul
  exit /b 1
)
del /q "%LOG_FILE%" 2>nul

set "PACKAGED_DIR=%PROJECT_ABS%\Packaged\%PLATFORM%"
if not exist "%PACKAGED_DIR%\" (
  echo FAIL: %PACKAGED_DIR% was not created by the build. 1>&2
  exit /b 1
)

set "ARTIFACT="
for /f "delims=" %%F in ('dir /b /s "%PACKAGED_DIR%\*%EXT%" 2^>nul') do (
  if not defined ARTIFACT (
    for %%S in ("%%F") do if %%~zS gtr 0 set "ARTIFACT=%%F"
  )
)

if not defined ARTIFACT (
  echo FAIL: no non-empty *%EXT% artifact found in %PACKAGED_DIR% 1>&2
  echo ----- contents of %PACKAGED_DIR% ----- 1>&2
  dir /a /s "%PACKAGED_DIR%" 1>&2
  exit /b 1
)

for %%S in ("%ARTIFACT%") do set "ARTIFACT_SIZE=%%~zS"
echo.
echo OK: %ARTIFACT% (%ARTIFACT_SIZE% bytes)
endlocal & exit /b 0
