# Frequently Asked Questions


# Black Screen - Built Game

## Log Files
1. Go to `Edit > App Settings > Runtime > Log To File` and enable it.
2. Build your game again and run it.
3. After the game crashes, go to the `./Polyphase.log` file in your game's directory and open it with a text editor.
4. Look for any error messages or warnings that might indicate the cause of the black screen.

## 	Check Project Directory Structure vs Project Name
You project directory that the `{ProjectName}.oct` file is in must be named the same as the project name. For example, if your project is named "MyGame", the directory should be named "MyGame" and contain the `MyGame.oct` file. If there is a mismatch, the game will not load at all after a successful build.

## `Polyphase.log` is empty while the game is running
The log file is opened line-buffered and flushed per write, so once logging is enabled every complete line lands on disk immediately. If the log is still empty after the game has clearly produced output, check:
- `Config.ini` has both `Logging=1` **and** `LogToFile=1`. `Logging=0` compiles-in but silences all `LogDebug/Warning/Error` calls.
- The log is written to the game's working directory (the folder containing the `.exe`), not the project directory. It is named `{ProjectName}.log`, falling back to `Polyphase.log` if the project name is not yet set at init.


# Recovering from a hard crash (BSOD / power loss) during a build

If Windows crashes while Visual Studio or our packager was mid-write, a handful of files may be truncated or padded with null bytes. Symptoms are confusing because the filesystem still lists the file and Windows Explorer shows a plausible size — it's the contents that are garbage.

## "Root element is missing" when opening the solution
Standalone's build path rewrites `Standalone/Standalone.vcxproj` in place to inject native-addon sources. If the machine crashed during that write, the vcxproj is likely truncated and ends with null bytes, which the XML parser rejects.

**Fix:** restore from the `.orig` backup the injection leaves behind:

```
copy /Y Standalone\Standalone.vcxproj.orig Standalone\Standalone.vcxproj
```

You can verify with `tail -c 200 Standalone\Standalone.vcxproj | od -c` — a healthy file ends in `</Project>\n`, a corrupted one ends in a long run of `\0 \0 \0`.

## "Engine.lib is not a valid Win32 application" on F5
Two separate causes produce similar-sounding errors:

1. **Startup project got flipped to `Engine`.** Engine's output is a `.lib`, not a launchable `.exe`, so `CreateProcess` fails with `ERROR_BAD_EXE_FORMAT (193)`. Solution Explorer → right-click **Standalone** → **Set as Startup Project** (its name goes bold).
2. **`Engine.lib` was mid-link when the crash hit and its COFF archive header is garbage.** The linker for the next build reports `LNK1107: invalid or corrupt file`. Delete the stale artifacts and rebuild:
   ```
   del /Q Engine\Build\Windows\x64\DebugEditor\Engine.lib
   del /Q Engine\Build\Windows\x64\DebugEditor\Engine.pdb
   rmdir /S /Q Engine\Intermediate\Windows\x64\DebugEditor
   ```
   Replace `DebugEditor` with whichever config the crash was in (usually `Release` if it happened during shipped-build packaging). A full rebuild of that config is ~2 min.

## BSOD during packaging (link.exe + `MiQueryAddressState`)
Bugcheck `0x0000000A IRQL_NOT_LESS_OR_EQUAL` faulting in `nt!MiQueryAddressState` while `link.exe` is running is a **kernel-side issue**, not a project bug. It has been reported on Windows 11 24H2/25H2 with VBS / HVCI enabled under heavy LTCG links. Mitigations, in order of effectiveness:

1. **Turn off HVCI** (Settings → Privacy & Security → Windows Security → Device Security → Core Isolation → Memory Integrity = Off → reboot). Most direct fix; re-test.
2. **Disable `WholeProgramOptimization` (LTCG)** in `Release|x64` and `ReleaseSteam|x64` of `Standalone.vcxproj` and `Engine.vcxproj`. LTCG forces link.exe to hold every TU's IR in memory at once, which is what stresses `MiQueryAddressState`. You lose ~2-5% runtime perf on the engine's own C++ (invisible in a frame budget dominated by Vulkan/scripts).
3. **Defender (or other AV) exclusions** for the repo root and the MSVC intermediate dirs. Real-time scan of thousands of `.obj`/`.pdb` writes during a link aggravates the MM path.
4. Make sure Windows Update is current — Microsoft has been pushing MM/hypervisor fixes in this area monthly.

## "Force Rebuild" still produced a stale build
`Build → Windows` with **Force Rebuild** checked wipes the following before invoking the linker, in the active config (`Release` or `ReleaseSteam`):
- `Standalone/Intermediate/Windows/x64/{config}/Standalone/`
- `Standalone/Build/Windows/x64/{config}/Polyphase.{exe,ilk,pdb}`
- `Engine/Build/Windows/x64/{config}/Engine.{lib,pdb}`

The Standalone wipe is necessary because MSBuild's own `.tlog`-based up-to-date check otherwise decides "nothing to do" even after the addon injection has changed the project. The `Engine.lib` delete forces MSBuild to re-lib Engine (otherwise `devenv /Build` skips it and hands the linker a stale `Engine.lib`). The `Engine.pdb` delete sidesteps the most common failure mode: every `.cpp` in Engine failing with `error C1033: cannot open program database 'Engine.pdb'` because the PDB is locked by a leaked `mspdbsrv.exe` or an active debug session of `Polyphase.exe` in Visual Studio. We do **not** wipe Engine intermediates — the .obj files survive, so re-link is fast (seconds) and only changed `.cpp` files recompile.

If `Engine.pdb` itself can't be deleted (lock is real, not stale), the packager aborts with `ERROR: Could not delete ... Engine.pdb (file is locked)` instead of letting devenv spin up a doomed compile. Stop debugging in Visual Studio (or close `devenv.exe`) and retry.

If the packaged `.exe` still looks stale after a Force Rebuild run, check:
- The **editor** (`Polyphase.exe` in `Standalone/Build/Windows/x64/DebugEditor`) is newer than your latest `ActionManager.cpp` edit. Older editor → running old build logic.
- The packager log shows `[BUILD] needCompile=1`. If you see `needCompile=0 … Reusing pre-compiled game executable.`, Force Rebuild wasn't actually honored — this indicates an older editor build.