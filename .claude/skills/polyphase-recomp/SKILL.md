---
name: polyphase-recomp
description: Port a decompiled console game (a C decomp repo plus the user's own disc/ROM) to native code that runs standalone and inside the Polyphase editor through a `com.recomp.<game>` native addon. Use when the user wants to "recompile", "port", "get running natively" or "put in Polyphase" a decomp (N64, PS1, GBA, GameCube…), wire a recompiled game into a Polyphase project, or debug a native port (crashes, rendering, audio, input, saves). For PlayStation 1 specifics also load `polyphase-recomp-ps1`.
---

# Polyphase recomp — native ports of decompiled games

Playbook distilled from two finished ports:

- **Super Smash Bros. 64**: `P:\Projects\Recomp\SuperSmashBros\Code\SuperSmashBros\Packages\com.recomp.ssmb64` (in-process 64-bit static lib, libultra/GBI/audio HLE).
- **Digimon World (PS1)**: `P:\Projects\Recomp\DigimonWorld\Code\DigimonWorld\Packages\com.recomp.digimonworld` (32-bit child process, PsyQ replacement, software GPU/SPU/MDEC). The most complete reference; read its `Native/README.md` first.

Per-port notes also live in auto-memory (`ssb64-native-port-workflow`, `dw-native-port-workflow`). When this skill and the code disagree, the code wins.

## Layout convention

```
P:\Projects\Recomp\<Game>\Code\
  <decomp>\                     the decomp repo (edits only under #ifdef PORT, never committed unless asked)
  <PolyphaseProject>\Packages\com.recomp.<id>\
    package.json  Source\       thin C++ addon glue (editor's addon builder globs Source/ only)
    Native\                     everything else: CMakeLists.txt, build.ps1, port\guest, port\host, tools\
    Bin\ or Lib\                published build output
```

Keep the port out of `Source/`: the editor's addon builder is a single `cl.exe` pass over `Source/*.cpp` with a shared include path, and decomp headers (`string.h`, `stdlib.h`…) would shadow the system ones.

## Step 0 — choose the memory model (the decision that shapes everything)

Console code assumes 32-bit pointers and fixed addresses. Pick one:

| Model | When | Cost |
|---|---|---|
| **32-bit process, console RAM at its real address** (DW) | Target addresses are free in a 32-bit Windows process (PS1 0x80000000, GBA 0x02000000/0x03000000/0x06000000…), game uses fixed addresses, pointer-in-int tricks, raw data blobs with embedded pointers | Must run as a child process of the 64-bit editor (shared memory for frames/input); needs `/LARGEADDRESSAWARE` for 0x80000000+ |
| **64-bit in-process, native pointers** (SSB) | Assets can be compiled from decomp C (relocData), addresses are virtual/segmented | Every pointer-in-u32, file-offset arithmetic and packed struct must be fixed; lots of generated tooling |

Default to the 32-bit fixed-address model whenever the console addresses fit: it needs far fewer source edits.

For the 32-bit model:
- Link the exe at a fixed base inside or just past the console window (DW: `/BASE:0x80800000 /FIXED /DYNAMICBASE:NO /LARGEADDRESSAWARE`) and `VirtualAlloc` the console RAM at its real address at startup.
- Run the game on its own stack inside that window: swap ESP plus the TEB `StackBase`/`StackLimit`. Fixed addresses and 24/32-bit links in data then just work.
- Load the original executable image to its load address. Data the decomp leaves to the linker (symbol-file addresses) then has real contents. Emit those symbols as absolute COFF symbols plus `/alternatename` (via an asm `.drectve` section, since `#pragma comment(linker)` is ignored on the GNU target).

## Step 1 — toolchain (Windows, VS 2022 clang)

- clang from `VC\Tools\Llvm\x64\bin`, CMake + Ninja from VS. Import `vcvarsall` (`x64_x86` for 32-bit, `x64` for the addon) inside PowerShell. Call `%SystemRoot%\System32\cmd.exe` by full path: devkitPro puts a `cmd` shim on PATH.
- In Git Bash use `MSYS_NO_PATHCONV=1` or linker flags such as `/BASE` get path-mangled. Heredocs in the Bash tool halve backslashes: write C/Python containing escapes with Write/Edit instead.
- Game code: `-ffreestanding -nostdlibinc -fno-builtin -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks -Wno-everything -g -gcodeview`, force-include a `port_prelude.h`, `-DPORT=1`, rename the game's `main` (`-Dmain=dw_main`).
- **GCC struct layout:** consoles were built with GCC/agbcc/IDO, and MSVC bitfield layout differs (`unsigned p:24; unsigned char n:8` becomes 8 bytes). Compile game code with `--target=i686-pc-windows-gnu -mno-ms-bitfields -mno-stack-arg-probe`. `-mno-ms-bitfields` is ignored on the MSVC target. The GNU target then needs `__divdi3`/`__moddi3`/`__udivdi3`/`__umoddi3`: provide them from a host (MSVC-target) file.
- Host code (Win32, CRT) is a separate object library on the MSVC target. Keep the host/guest interface to plain C types in one header (`port_host.h`).
- Non-ASCII string literals: decomps keep Japanese text as UTF-8 and build with `-fexec-charset=CP932/SJIS`, which clang can't do. Generate escaped copies at configure time (`tools/sjis_sources.py` in DW).
- `#pragma clang section` produces a blank section name on COFF, and `llvm-objcopy --rename-section` is unsupported for COFF. To place an object's data in a named section, compile it to asm and rewrite `.data` (DW `tools/ovl_launcher.py`, a `C_COMPILER_LAUNCHER`), or use MSVC `#pragma data_seg` on the MSVC target (SSB).

## Step 2 — get it linking

1. List sources from the decomp's makefile/config for one version (US). Exclude crt0/startup asm and replace it with a `boot.c`.
2. Compile everything with `-k 0` and fix compile errors by category. Inline asm almost always means the SDK's GTE/COP macros: override the header in `port/include` (it comes first on the include path).
3. Link and collect the undefined symbols: that list *is* the SDK replacement work list. Group it by library and write `port/guest/lib*.c` one library at a time. Stub audio and movies first, then come back.

## Step 3 — headless runner first, editor last

Give the exe a test mode before touching graphics fidelity. DW's `dw.exe` supports:

```
--headless --frames N --dump DIR --every N --dump-from F
--script "F:HEXBITS[:DUR],..."      scripted pad input by vblank
--dump-vram --trace-gpu F --wav out.wav
--debug-warp MAP,EXIT,F --debug-battle ENT,F --debug-nomovie 1   game-specific test hooks
```

- Dump PPM frames and build contact sheets (PIL) to look at many frames at once with Read. VRAM dumps show whether a problem is in drawing or in display (text missing from VRAM vs. present but not drawn).
- Headless vblanks advance as fast as the game presents (DW: about 600 fps), so a 40,000-frame soak takes seconds.
- Fuzz: random input scripts across several seeds in parallel, plus warps to every map id. Map ids whose files are missing from the disc crash legitimately.
- Judge audio without ears: record a WAV and render a spectrogram. Harmonic lines mean music; a grey broadband floor or a stuck horizontal line means a decode or envelope bug.
- Symbolize crashes with `llvm-symbolizer --obj=dw.exe 0xADDR </dev/null` (without `</dev/null` it hangs on stdin). `llvm-objdump -d --start-address` shows the faulting instruction.

## Step 4 — the console-behaviour safety net (host side)

A vectored exception handler restricted to the game thread:

- **NULL / low-address accesses** (legal on PS1/N64, where RAM is mirrored at 0):
  1. Retry with the base register moved into the RAM mirror (pick the register whose value sits just below the fault address), single-step, then restore the register unless the instruction overwrote it.
  2. If that fails, decode the instruction (`x86_emulate.c`) and do the load/store on the mirror.
  3. Needs `-fno-delete-null-pointer-checks`, or clang turns `*(T*)0` into unreachable code and execution falls into garbage.
- **Integer divide by zero / INT_MIN/-1:** give the R3000/ARM result and skip the instruction.
- **Writes to const data:** link `.rdata` RW (`-Xlinker /SECTION:.rdata,RW`; `-Wl,` splits on the comma).
- Log every fixup site once and symbolize them later: they are usually real original-game bugs (`ENTITY_TABLE[i]->x` on NULL) and fine to leave.

## Step 5 — recurring porting bugs (check these first)

- **Overlay globals:** consoles reload an overlay's `.data` from disc every time. Snapshot each overlay's data section at boot and restore it in the overlay loader. Battle #2 breaks without this.
- **Data adjacency:** `char s[16] = "16 chars exactly"` relies on the zero word that follows in the original layout. Fix under `#ifdef PORT`.
- **Libraries that copy:** some SDK calls copy the caller's stack primitive (`GsSortPoly`). Implement them as copies, never as links to the caller's memory.
- **K&R / mismatched prototypes and implicit declarations:** on x86-32 cdecl these are mostly harmless. In a 64-bit model, generate prototypes (SSB `gen_implicit_protos.py`).
- **Rasterizer fill rules:** test a textured quad at 1:1 scale. A missing first column or an extra column at the right edge means the top-left rule is inverted.
- **Async loads:** keep CD/DMA loads synchronous but keep the completion APIs returning "done". Never return "busy" forever.
- **Frame pacing:** the game's `VSync(n)` decides logic rate (DW field = `VSync(3)` = 20 fps). Pace host vblanks at 60 Hz and present at VSync time.
- **Audio clock:** generate samples from the vblank clock (735 per frame at 44.1 kHz). Never let a stall produce a burst; resync instead.

## Step 6 — Polyphase integration

The addon (`package.json` `native` block; see the `polyphase-addon` skill) adds one node, `<Game>Player : Node3D`:

- `Create()` makes a `FullStretch` `Quad` child. Hold the frame texture as an `AssetRef` to `NewTransientAsset<Texture>()` (Quad drops textures the AssetManager doesn't know), `Init` + `Create`, then `UpdatePixels` each new frame.
- Input: `sAPI->IsKeyDown` for the keyboard plus `INP_IsGamepad*`, mapped to the console pad bits.
- **Child-process model (32-bit games):** the node creates a named file mapping and launches `game.exe --shm NAME --disc ... --saves ... --log ...`. Use `CREATE_NO_WINDOW` and a job object with `KILL_ON_JOB_CLOSE`. The child writes frames to double-buffered slots plus a serial counter, reads pad bits, and plays its own audio with waveOut. `Destroy()` and `OnUnload` set a quit command, wait, then terminate. Share a fixed-size-types header (`port_shm.h`) between both sides and test it from Python (`mmap` with `tagname`) before the editor ever loads it.
- **In-process model (64-bit):** a static lib in `Lib/` linked via `nativePerPlatform.Windows.extraLibs`, built `/Zl` so it links into both `/MD` and `/MDd`. Step the game per Tick at a fixed 60 Hz accumulator.
- Paths are properties resolved against `GetEngineState()->mProjectDirectory`. Defaults: `Packages/com.recomp.<id>/Bin/<game>.exe`, `../<decomp>/<disc>`, `Saves/<Game>`.
- `build.ps1` publishes into `Bin/` or `Lib/`. Keep an addon compile check script (`tools/check_addon.ps1`: package `CMakeLists.txt` + Ninja + `cl`) so Source changes are verified without the editor.
- Never drive a running editor (controller server, scene edits) without asking. The user adds the node to the scene.

## Working style the user expects

- Long autonomous runs ("get it playable by morning"): keep going, test headless, send a proactive contact sheet when something visible lands, and keep a memory note per port up to date.
- Decomp edits only under `#ifdef PORT`, minimal, no commits unless asked. Report every decomp file touched.
- Use the scratchpad for test output, not the repos. Clean test save folders out of `Bin/`.
