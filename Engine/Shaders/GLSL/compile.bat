@echo off
setlocal EnableDelayedExpansion
rem Compiles every GLSL shader in src\ to SPIR-V in bin\ using glslc from the
rem Vulkan SDK. Output files keep the source name (bin\Forward.vert is SPIR-V).
rem
rem Non-interactive on purpose: the editor streams this script's output into
rem its build window, so a "pause" here would hang the packaging build with
rem no key to press. Failures are reported and the script exits non-zero.

if not exist ".\bin" mkdir .\bin

set "GLSLC="
if defined VULKAN_SDK (
  if exist "%VULKAN_SDK%\Bin\glslc.exe" set "GLSLC=%VULKAN_SDK%\Bin\glslc.exe"
)
if not defined GLSLC (
  for /f "delims=" %%p in ('where glslc 2^>nul') do (
    if not defined GLSLC set "GLSLC=%%p"
  )
)
if not defined GLSLC (
  echo ERROR: glslc not found. Install the LunarG Vulkan SDK and set VULKAN_SDK
  echo        ^(https://vulkan.lunarg.com/sdk/home^), or put glslc.exe on PATH.
  exit /b 1
)
echo Using %GLSLC%

set FAILED=0
for %%f in (.\src\*) do (
  if /I NOT "%%~xf" == ".glsl" (
    echo:
    echo %%~nxf
    "%GLSLC%" "%%f" -O -g -fpreserve-bindings -o ".\bin\%%~nxf"
    if errorlevel 1 (
      echo ERROR: failed to compile %%~nxf
      set FAILED=1
    ) else (
      echo Compile Successful
    )
  )
)

if "%FAILED%"=="1" (
  echo Shader compilation FAILED.
  exit /b 1
)
echo Compilation successful!
exit /b 0
