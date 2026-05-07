@echo off
REM Clone (or refresh) an arbitrary git repository for CI-driven Polyphase
REM project testing, and drop a marker file so cleanup_test_project.bat can
REM verify the directory was created by us before deleting it.
REM
REM Usage:
REM   Tools\CI\clone_test_project.bat ^<git-url^> [dest-dir] [--ref ^<ref^>]
REM
REM Defaults:
REM   dest-dir = .\test-project
REM   ref      = (default branch via --depth 1)

setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" (
  echo Usage: %~nx0 ^<git-url^> [dest-dir] [--ref ^<ref^>] 1>&2
  exit /b 2
)

set "REMOTE=%~1"
shift
set "DEST=.\test-project"
set "REF="

:parse
if "%~1"=="" goto after_parse
if /I "%~1"=="--ref" (
  set "REF=%~2"
  shift
  shift
  goto parse
)
set "DEST=%~1"
shift
goto parse
:after_parse

set "MARKER=.polyphase-ci-clone"

if exist "%DEST%\.git" (
  echo Clone already present at %DEST% - refreshing.
  git -C "%DEST%" remote set-url origin "%REMOTE%" || exit /b 1
  if defined REF (
    git -C "%DEST%" fetch --depth 1 origin "%REF%" || exit /b 1
  ) else (
    git -C "%DEST%" fetch --depth 1 origin HEAD || exit /b 1
  )
  git -C "%DEST%" reset --hard FETCH_HEAD || exit /b 1
) else (
  echo Cloning %REMOTE% into %DEST%...
  if defined REF (
    git clone --depth 1 --branch "%REF%" "%REMOTE%" "%DEST%" || exit /b 1
  ) else (
    git clone --depth 1 "%REMOTE%" "%DEST%" || exit /b 1
  )
)

REM Drop / refresh the marker file.
for /f "tokens=* usebackq" %%T in (`powershell -NoProfile -Command "Get-Date -Format yyyy-MM-ddTHH:mm:ssZ"`) do set "STAMP=%%T"
> "%DEST%\%MARKER%" (
  echo created_at=!STAMP!
  echo remote=%REMOTE%
  if defined REF echo ref=!REF!
  echo script=Tools/CI/clone_test_project.bat
)

echo.
echo Test project ready at: %DEST%
endlocal & exit /b 0
