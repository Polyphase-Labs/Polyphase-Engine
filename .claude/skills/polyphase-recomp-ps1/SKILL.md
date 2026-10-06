---
name: polyphase-recomp-ps1
description: PlayStation 1 specifics for native ports of PS1 decomps (PsyQ SDK games built with GCC/maspsx) into Polyphase — replacing libgpu/libgs/libgte/libcd/libetc/libapi/libsnd/libspu/libmcrd/libpress with a software GTE, GPU, SPU, MDEC/STR and memory card, loading SLUS from the disc image, and the PS1 memory model. Use together with `polyphase-recomp` whenever the target is a PS1 game (SLUS/SCUS/SLES executables, .bin/.cue discs, PsyQ headers).
---

# PS1 recomp specifics

Reference implementation: `P:\Projects\Recomp\DigimonWorld\Code\DigimonWorld\Packages\com.recomp.digimonworld\Native` (DW). Copy its `port/` tree and `tools/` as a starting point; most files are game-agnostic. Read `Native/README.md` and the `dw-native-port-workflow` memory first. Load `polyphase-recomp` for the general method.

## Memory model (proven)

| Region | Address | How |
|---|---|---|
| Main RAM 2 MB | 0x80000000 | `VirtualAlloc` at boot; reserve through 0x80710000 |
| Game stack | 0x80400000–0x80700000 | inside the same reservation; `run_on_stack` swaps ESP + TEB bounds |
| Scratchpad 1 KB | 0x1F800000 | `VirtualAlloc` 64 KB there |
| exe image | 0x80800000 | `/BASE:0x80800000 /FIXED /DYNAMICBASE:NO /LARGEADDRESSAWARE`; keep it below 0x81000000 |

Ordering tables hold 24-bit links: `next = (tag & 0xFFFFFF) | 0x80000000`. Primitives must therefore live in PS1 RAM, which is where the game's packet buffers already are. Prims allocated inside the exe image (0x808xxxxx) would also decode correctly, but keep them in RAM.

## Boot

1. Open the `.bin` (2352-byte raw sectors; user data at +24 for Mode 2 Form 1). Walk ISO9660 yourself; `files.txt` from an extraction script is handy for lookups.
2. Read the PS-X EXE header: `t_addr` at 0x18, `t_size` at 0x1C, `b_addr` at 0x28, `b_size` at 0x2C. Copy the text to `t_addr` and zero the BSS.
3. Run `InitGeom`, snapshot overlay data, then call the renamed `main`.
4. Overlay `*_REL.BIN` files are still read into their PS1 addresses by the game. Their code is compiled natively, but embedded data (models, TIMs) is read from the loaded copy. Reset the overlay's native globals in the loader (see `polyphase-recomp`).
5. Symbol files (`config/<ver>/symbols*.txt`): data entries become absolute symbols plus `/alternatename` (`tools/gen_symbols.py`). Function entries are left out, because a missing function must be a link error.

## PsyQ replacement map (DW `port/guest`)

- **libgte** (`gte.c`, `libgte.c`, `include/inline_n.h`):
  - Full software COP2: RTPS/RTPT, NCLIP, AVSZ3/4, NC*/NCC*/NCD*, MVMVA, DPCS, INTPL, SQR/OP/GPF/GPL, with flags, UNR division and the SXY/SZ/RGB FIFOs.
  - Override the `gte_*` macro header, which is MIPS asm.
  - `rsin`/`rcos` come from host math, with 4096 = 360°.
- **libgpu** (`gpu.c`, `libgpu.c`):
  - 1024×512×16 VRAM; GP0 polys, lines, rects, fill, VRAM↔VRAM/CPU copies, E1–E6.
  - CLUT 4/8-bit textures, semi-transparency modes 0–3 (texels with bit 15 only), mask bit, texture window, 4×4 dither for gouraud.
  - Top-left fill rule: right and bottom edges excluded. Sample at integer pixel positions.
  - `DrawOTag` walks the 24-bit chain to 0xFFFFFF. Display output supports 15- and 24-bit (movies use 24-bit).
- **libgs** (`libgs.c`):
  - `GsSwapDispBuff` displays `PSDBASE[PSDIDX]`, toggles the index, then sets the draw environment to the other buffer (GPU offset mode = draw offset + `POSITION`).
  - `GsClearOt` uses ClearOTagR linking, `tag = org + (1<<length) - 1`. `GsSortClear` adds a fill at the tag.
  - **`GsSortPoly` copies the primitive** (callers pass stack primitives). `GsSortSprite` and `GsSortFastSprite` are DR_MODE + SPRT/POLY_FT4.
  - TMD: `GsMapModelingData` relocates the object table; `GsSortObject4` decodes each packet by header (mode/flag) with one generic routine. All `GsTMDfast*` and `GsTMDdiv*` wrappers call it.
  - OT index = AVSZ result >> shift. Default ZSF3 = 0x155 and ZSF4 = 0x100, matching the game's own `otz >> 2`.
  - Lighting: `GsSetFlatLight` stores the negated, normalised direction plus the colour matrix; `GsSetLightMatrix` = `LIGHTWS × lw`.
  - Do **not** define globals that are in the symbol file (`GsWSMATRIX`, `PSDIDX`, `GsOUT_PACKET_P`…); declare them `extern` so game and lib share the PS1 copy. Initialise `GsIDMATRIX` and friends in `GsInit3D` (they are BSS).
- **libcd** (`libetc.c`):
  - Synchronous `CdRead` from the image; `CdReadSync` always returns 0.
  - `CdIntToPos`/`CdPosToInt` use BCD with a 150-sector offset. `CdSearchFile` uses the ISO walk.
  - St*/DecDCT* are stubs because movies are played by the port.
- **libetc**:
  - `VSync(0)` presents and waits for the next vblank; `VSync(n)` waits until n vblanks since the last call; `VSync(-1)` returns the count. Audio rendering is driven from here.
  - `PadRead` returns pressed = 1 bits: L2 0x1, R2 0x2, L1 0x4, R1 0x8, △ 0x10, ○ 0x20, ✕ 0x40, □ 0x80, Select 0x100, Start 0x800, Up 0x1000, Right 0x2000, Down 0x4000, Left 0x8000. Pad 2 is in the high 16 bits.
  - Pace busy-wait loops that only call `PadRead` (pause screens).
- **libapi** (`libapi.c`): first-fit `InitHeap3`/`malloc3`/`free3` inside the region the game passes; `printf`/`sprintf` with its own formatter. memcpy/strcpy come from the CRT.
- **libmcrd** (`host/memcard.c`):
  - Each card is a folder of files; size = blocks × 8 KB.
  - Exist/Accept/ReadFile/WriteFile register a result that `MemCardSync` reports once.
  - Create/Delete/Format/GetDirentry are synchronous and return the error code.
  - New Game in many games creates the save file immediately, so this must work early.
- **libsnd / libspu** (`libsnd.c`): see the audio section below.
- **Movies** (`movie.c`): see the FMV section below.

## Audio (software SPU)

- **VAB** (`pBAV`):
  - 32-byte header (`ps` programs at +18, master volume at +24), then `ProgAtr[128]` × 16 bytes, then `VagAtr[ps × 16]` × 32 bytes, then 256 × u16 VAG sizes (>>3).
  - VAG n (1-based) starts at the sum of sizes 1..n-1. Programs with `tones > 0` take consecutive 16-tone blocks.
  - The VH stays in game memory ("sticky"); copy the VB in `SsVabTransBody`, because the source buffer gets reused.
- **ADPCM:** 16-byte blocks: shift = b0 & 15, filter = b0 >> 4 with f0 {0,60,115,98,122} and f1 {0,0,-52,-55,-60}. Flags: bit0 end, bit1 repeat, bit2 loop start. Interpolate (linear at minimum).
- **ADSR:**
  - Rate = (shift << 2) | step. Step value = (7 − step) << max(0, 11 − shift) every 1 << max(0, shift − 11) samples; exponential increase above 0x6000 is 4× slower; exponential decrease scales by the current level.
  - **Decay shift is the 4-bit field and release shift the 5-bit field as-is.** Multiplying them by 4 makes notes hang forever, which shows up as a horizontal line in the spectrogram.
  - Sustain level = (N + 1) × 0x800.
- **Pitch:** 0x1000 = 44.1 kHz at the tone's `center` note; `shift` is fine tune in 1/128 semitone.
- **SEQ** (`pQES`):
  - Big-endian resolution at +8 and tempo at +10 (3 bytes), events from +15, MIDI with running status.
  - Loops via NRPN: CC99 = 20 marks the start, CC99 = 30 marks the end, CC6 sets the count (127 = infinite). Meta FF 51 = tempo, FF 2F = end.
  - `SsSeqPlay(..., 0)` loops forever.
- **SFX:** `SsUtKeyOnV(voice, vab, prog, tone, note, fine, voll, volr)` on voices 0–23. `SpuGetKeyStatus` returns 1 (on), 2 (released but sounding) or 0.
- Run SEQ voices in their own pool so they never collide with the game's explicit voices. Mix ×0.5 and add a small Schroeder reverb when `SpuSetReverb(ON)` is called.

## FMV (STR + MDEC + XA)

Read raw sectors. Subheader at +16: submode bit 2 = audio. Video sector data at +24:

| Offset | Field |
|---|---|
| +0 | 0x0160 |
| +2 | 0x8001 |
| +4 | chunk index |
| +6 | chunk count |
| +8 | frame number |
| +12 | frame bytes |
| +16 | width |
| +18 | height |
| +32 | 2016 bytes of payload per chunk |

- **Frame header:** u16 codes, 0x3800, qscale, version (2 or 3).
- **Bitstream:** 16-bit little-endian words, read MSB first.
- **Macroblocks:** 16×16, **column-major**. Blocks in the order Cr, Cb, Y0–Y3.
- **DC:** v2 is 10-bit raw. v3 uses MPEG-1 DC-size VLCs (luma "100,00,01,101,110,1110…", chroma "00,01,10,110,…") with per-component predictors stepping ×4.
- **AC:** MPEG-1 table B.14 plus sign bit. EOB = `10`; escape = `000001` + 6-bit run + 10-bit signed level.
- **Dequant:** DC × 2; AC = (level × q[n] × qscale + 4) / 8 with the PSX zigzag-order quant table. Float IDCT, Y + 128, BT.601 conversion to RGB.
- **XA:** 18 groups × 128 bytes. Unit u's param is at group[4 + u]; the nibble is at group[16 + s×4 + u/2] >> ((u & 1) × 4). Filters {0,60,115,98}/{0,0,-52,-55}; stereo alternates units L/R; 37.8 or 18.9 kHz, resampled to 44.1 kHz.
- **Playback:** pace video at 2 vblanks per frame (30 fps) and push audio as audio sectors arrive. Start skips the movie after about 30 frames. Present directly with `port_present`, bypassing VRAM.

## PS1-specific traps seen so far

- NULL dereferences through `ENTITY_TABLE[i]` and similar are everywhere in the original. The low-address fault handler covers them, but only if the code is built with `-fno-delete-null-pointer-checks`.
- An inlined `addFileReadRequest(..., NULL)` copied `*loc` from NULL. Without the flag above, clang dropped the rest of the function.
- `const` objects written at run time (DW `BTL_COMMAND_SHOUT`): link `.rdata` RW.
- MIPS `div` by zero doesn't trap: battle scripts divide by zero.
- Strings declared exactly as long as their text, relying on the zero word that follows them.
- Japanese text is CP932 in the original build.
- 24-bit TIMs and movies need the display's 24-bit mode. `GsGetTimInfo`/`OpenTIM`/`ReadTIM` parse the flags word (bit 3 = CLUT present).
- The title screen may have no music. Check with `--debug-sndlog` before chasing a bug.
