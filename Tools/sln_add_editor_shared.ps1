# One-shot helper: appends "DebugEditor Shared" / "ReleaseEditor Shared" project
# configuration mappings for the seven external sub-projects in Polyphase.sln.
# Each project's new mapping points at the same target its existing DebugEditor /
# ReleaseEditor maps to (external libs don't have a "Shared" variant of their own).
# Engine and Standalone were handled by hand because they need their *own*
# "Shared" target configs. Re-running this script is idempotent.

$ErrorActionPreference = 'Stop'

$slnPath = Join-Path $PSScriptRoot '..\Polyphase.sln'
$content = Get-Content $slnPath -Raw

$projects = @(
    # Tuple: GUID, DBG-x64, DBG-Android, DBG-x86, REL-x64, REL-Android, REL-x86.
    # Targets here mirror each project's existing DebugEditor/ReleaseEditor map.
    @('3F6A7D22-1E7C-4708-A88B-676674AFCA7A','DebugEditor|x64','DebugEditor|Android-arm64-v8a','DebugEditor|Win32','ReleaseEditor|x64','ReleaseEditor|Android-arm64-v8a','ReleaseEditor|Win32'),
    @('8BCD93F2-BE7E-48E0-9C7C-82E0640C1712','DebugEditor|x64','DebugEditor|Android-arm64-v8a','DebugEditor|Win32','ReleaseEditor|x64','ReleaseEditor|Android-arm64-v8a','ReleaseEditor|Win32'),
    @('843F284E-7382-4E28-9597-027F8145D55A','DebugEditor|x64','DebugEditor|Android-arm64-v8a','DebugEditor|Win32','ReleaseEditor|x64','ReleaseEditor|Android-arm64-v8a','ReleaseEditor|Win32'),
    @('62666E6C-3F33-45A2-88A6-7A4DAC19E439','Debug|x64','Debug|Android-arm64-v8a','Debug|Win32','Release|x64','Release|Android-arm64-v8a','Release|Win32'),
    @('76B204C0-0490-46FA-AD50-8030F398A5BA','Debug|x64','Debug|Android-arm64-v8a','Debug|Win32','Release|x64','Release|Android-arm64-v8a','Release|Win32'),
    @('9CC47ACB-DFDE-4FDA-ADAE-CE660F8DD450','Debug|x64','Debug|Android-arm64-v8a','Debug|Win32','Release|x64','Release|Android-arm64-v8a','Release|Win32'),
    @('DF4F0435-E38D-465D-9BAD-1382FE0E2624','Debug|x64','Debug|x64','Debug|Win32','Release|x64','Release|x64','Release|Win32')
)

foreach ($p in $projects) {
    $guid = $p[0]
    $dbgX64 = $p[1]; $dbgAnd = $p[2]; $dbgX86 = $p[3]
    $relX64 = $p[4]; $relAnd = $p[5]; $relX86 = $p[6]

    if ($content.Contains("{$guid}.DebugEditor Shared|x64.ActiveCfg")) {
        Write-Host "[$guid] already has DebugEditor Shared mappings - skipping"
        continue
    }

    $dbgAnchor = "`t`t{$guid}.DebugEditor|x86.Build.0 = $dbgX86"
    $dbgBlock  = "$dbgAnchor`r`n" +
                 "`t`t{$guid}.DebugEditor Shared|Android-arm64-v8a.ActiveCfg = $dbgAnd`r`n" +
                 "`t`t{$guid}.DebugEditor Shared|Android-arm64-v8a.Build.0 = $dbgAnd`r`n" +
                 "`t`t{$guid}.DebugEditor Shared|x64.ActiveCfg = $dbgX64`r`n" +
                 "`t`t{$guid}.DebugEditor Shared|x64.Build.0 = $dbgX64`r`n" +
                 "`t`t{$guid}.DebugEditor Shared|x86.ActiveCfg = $dbgX86`r`n" +
                 "`t`t{$guid}.DebugEditor Shared|x86.Build.0 = $dbgX86"

    $relAnchor = "`t`t{$guid}.ReleaseEditor|x86.Build.0 = $relX86"
    $relBlock  = "$relAnchor`r`n" +
                 "`t`t{$guid}.ReleaseEditor Shared|Android-arm64-v8a.ActiveCfg = $relAnd`r`n" +
                 "`t`t{$guid}.ReleaseEditor Shared|Android-arm64-v8a.Build.0 = $relAnd`r`n" +
                 "`t`t{$guid}.ReleaseEditor Shared|x64.ActiveCfg = $relX64`r`n" +
                 "`t`t{$guid}.ReleaseEditor Shared|x64.Build.0 = $relX64`r`n" +
                 "`t`t{$guid}.ReleaseEditor Shared|x86.ActiveCfg = $relX86`r`n" +
                 "`t`t{$guid}.ReleaseEditor Shared|x86.Build.0 = $relX86"

    if (-not $content.Contains($dbgAnchor)) { throw "DBG anchor not found for $guid : $dbgAnchor" }
    if (-not $content.Contains($relAnchor)) { throw "REL anchor not found for $guid : $relAnchor" }

    $content = $content.Replace($dbgAnchor, $dbgBlock)
    $content = $content.Replace($relAnchor, $relBlock)
    Write-Host "[$guid] inserted DebugEditor/ReleaseEditor Shared mappings"
}

Set-Content -Path $slnPath -Value $content -NoNewline
Write-Host "Done. Wrote $slnPath."
