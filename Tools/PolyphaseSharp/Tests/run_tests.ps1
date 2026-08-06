# PolyphaseSharp end-to-end test:
#   1. build the tool
#   2. transpile Tests/Sample/Scripts/CSharp
#   3. run the generated Lua under a Lua 5.3 interpreter with engine stubs
#
# The Lua interpreter is built once from the engine's vendored External/Lua
# (same luaconf.h, so LUA_32BITS numeric semantics match every shipping platform).
#
# Usage: pwsh Tools/PolyphaseSharp/Tests/run_tests.ps1

$ErrorActionPreference = "Stop"
$testsDir = $PSScriptRoot
$toolDir = Split-Path $testsDir -Parent
$repoRoot = Split-Path (Split-Path $toolDir -Parent) -Parent
$workDir = Join-Path $testsDir "work"

# ---- 1. build tool ----
dotnet build (Join-Path $toolDir "PolyphaseSharp\PolyphaseSharp.csproj") -c Release --nologo -v q
if ($LASTEXITCODE -ne 0) { throw "tool build failed" }
$toolDll = Join-Path $toolDir "PolyphaseSharp\bin\Release\net8.0\polyphasesharp.dll"

# ---- 2. transpile a fresh copy of the sample ----
if (Test-Path $workDir) { Remove-Item -Recurse -Force $workDir }
New-Item -ItemType Directory -Force (Join-Path $workDir "Scripts") | Out-Null
Copy-Item -Recurse (Join-Path $testsDir "Sample\Scripts\CSharp") (Join-Path $workDir "Scripts\CSharp")

dotnet $toolDll --scripts (Join-Path $workDir "Scripts\CSharp")
if ($LASTEXITCODE -ne 0) { throw "transpile failed" }

# ---- 3. build the Lua interpreter (cached) ----
$luaExe = Join-Path $testsDir "lua\lua.exe"
if (-not (Test-Path $luaExe)) {
    Write-Host "building Lua 5.3 test interpreter from External/Lua..."
    $luaSrc = Join-Path $repoRoot "External\Lua"
    $luaBuild = Join-Path $testsDir "lua"
    New-Item -ItemType Directory -Force $luaBuild | Out-Null
    Copy-Item (Join-Path $testsDir "lua_main.c") (Join-Path $luaBuild "main.c")

    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -property installationPath
    $vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
    # lua.c/luac.c excluded: the engine disables the standalone REPL (#if 0);
    # main.c is our minimal driver.
    $cfiles = (Get-ChildItem (Join-Path $luaSrc "*.c") |
        Where-Object { $_.Name -notin @("lua.c", "luac.c") } |
        ForEach-Object { '"' + $_.FullName + '"' }) -join " "
    $cmd = "call `"$vcvars`" >nul && cd /d `"$luaBuild`" && cl /nologo /O2 /MD /I`"$luaSrc`" main.c $cfiles /Fe:lua.exe"
    & "$env:SystemRoot\System32\cmd.exe" /c $cmd | Out-Null
    if (-not (Test-Path $luaExe)) { throw "lua.exe build failed" }
}

# ---- 4. run harness ----
Push-Location $workDir
try {
    & $luaExe (Join-Path $testsDir "harness.lua") "Scripts"
    if ($LASTEXITCODE -ne 0) { throw "harness failed" }
}
finally { Pop-Location }

Write-Host "ALL TESTS PASSED"
