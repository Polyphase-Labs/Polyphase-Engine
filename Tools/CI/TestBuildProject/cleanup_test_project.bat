@echo off
REM Safely delete a directory previously created by clone_test_project.bat.
REM
REM Usage:
REM   Tools\CI\cleanup_test_project.bat ^<dir^> [--build-artifacts-only]
REM
REM Default: removes ^<dir^> entirely. With --build-artifacts-only, removes only
REM Packaged\ and Intermediate\ subdirectories under ^<dir^> so the next build
REM runs from a clean slate without re-cloning.
REM
REM Refuses to delete unless EVERY safety check passes:
REM   1. ^<dir^>\.polyphase-ci-clone marker exists.
REM   2. Resolved absolute path has 3+ components and isn't a drive root.
REM   3. ^<dir^>\.git\config (if present) references the marker's remote URL.

setlocal EnableExtensions EnableDelayedExpansion

set "DIR="
set "ARTIFACTS_ONLY=0"
:parse
if "%~1"=="" goto after_parse
if /I "%~1"=="--build-artifacts-only" (
  set "ARTIFACTS_ONLY=1"
  shift
  goto parse
)
if "%DIR%"=="" (
  set "DIR=%~1"
  shift
  goto parse
)
echo Unexpected extra argument: %~1 1>&2
exit /b 2
:after_parse

if "%DIR%"=="" (
  echo Usage: %~nx0 ^<dir^> [--build-artifacts-only] 1>&2
  exit /b 2
)

if not exist "%DIR%\" (
  echo Nothing to clean: %DIR% does not exist.
  exit /b 0
)

REM Resolve absolute path.
for %%I in ("%DIR%") do set "ABS=%%~fI"

REM Refuse drive roots like C:\ or D:\
if "%ABS:~3%"=="" (
  echo REFUSE: %ABS% is a drive root - refusing to delete. 1>&2
  exit /b 1
)

REM Require at least 3 path components beneath the drive root.
set "REL=%ABS:~3%"
set "COMPONENTS=0"
for %%S in ("%REL:\=" "%") do set /a COMPONENTS+=1
if %COMPONENTS% lss 3 (
  echo REFUSE: %ABS% has fewer than 3 path components - refusing to delete. 1>&2
  exit /b 1
)

REM Require marker file.
set "MARKER=%ABS%\.polyphase-ci-clone"
if not exist "%MARKER%" (
  echo REFUSE: marker file '%MARKER%' is missing - directory was not created by clone_test_project. 1>&2
  echo         If you really want to delete this directory, do it manually. 1>&2
  exit /b 1
)

REM Cross-check: if .git\config exists, its remote URL must match the marker's.
set "GIT_CONFIG=%ABS%\.git\config"
if exist "%GIT_CONFIG%" (
  set "EXPECTED_REMOTE="
  for /f "usebackq tokens=1,* delims==" %%A in ("%MARKER%") do (
    if /I "%%A"=="remote" if not defined EXPECTED_REMOTE set "EXPECTED_REMOTE=%%B"
  )
  if defined EXPECTED_REMOTE (
    findstr /C:"!EXPECTED_REMOTE!" "%GIT_CONFIG%" >nul 2>&1
    if errorlevel 1 (
      echo REFUSE: %GIT_CONFIG% does not reference '!EXPECTED_REMOTE!' from the marker - wrong directory? 1>&2
      exit /b 1
    )
  )
)

if "%ARTIFACTS_ONLY%"=="1" (
  set "REMOVED=0"
  for /f "delims=" %%D in ('dir /b /s /a:d "%ABS%\Packaged" "%ABS%\Intermediate" 2^>nul') do (
    rmdir /s /q "%%D" 2>nul
    echo Cleaned: %%D
    set /a REMOVED+=1
  )
  if !REMOVED!==0 echo No Packaged or Intermediate directories found under %ABS%.
  echo Build artifacts removed; clone preserved at %ABS%.
  endlocal & exit /b 0
)

rmdir /s /q "%ABS%"
echo Cleaned: %ABS%
endlocal & exit /b 0
