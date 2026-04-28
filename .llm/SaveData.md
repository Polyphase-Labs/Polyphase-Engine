# SaveData Key-Value Save System

High-level key-value persistence layer built on top of the engine's existing
`Stream` + `SYS_WriteSave`/`SYS_ReadSave` infrastructure. Provides a
PlayerPrefs-style API to Lua scripts for saving and loading typed data by
string key, with named save slot support.

---

## 1. Overview

**One global in-memory store.** A static `std::unordered_map<std::string, Datum>`
holds all key-value pairs. Lua scripts set/get values by key, then call
`Save(slotName)` to flush to disk or `Load(slotName)` to replace the map
from disk.

**Platform dispatch.** `Save` and `Load` call `SYS_WriteSave`/`SYS_ReadSave`,
which are implemented per-platform:

| Platform   | Backend |
|------------|---------|
| Windows    | `fopen` to `ProjectDir/Saves/<slotName>` |
| Linux      | `fopen` to `ProjectDir/Saves/<slotName>` |
| Android    | `fopen` to `ProjectDir/Saves/<slotName>` |
| GameCube   | `CARD_Open`/`CARD_Read`/`CARD_Write` on Slot A |
| Wii        | `fopen` to SD card `ProjectDir/Saves/<slotName>` |
| 3DS        | `fopen` to `ProjectDir/Saves/<slotName>` |

---

## 2. Source Files

| File | Purpose |
|------|---------|
| `Engine/Source/LuaBindings/SaveData_Lua.h` | Header — struct with static Lua C functions + `Bind()` |
| `Engine/Source/LuaBindings/SaveData_Lua.cpp` | Implementation — storage map, serialization, Lua binding |

Registered in `Engine/Source/LuaBindings/LuaBindings.cpp` via
`SaveData_Lua::Bind()`, placed after `System_Lua::Bind()`.

---

## 3. Storage Design

```cpp
static std::unordered_map<std::string, Datum> sSaveData;
```

Each value is a `Datum` object (from `Engine/Source/Engine/Datum.h`). Datum
natively supports Integer, Float, Bool, String, Vector2D (vec2), Vector (vec3),
Color (vec4), and has built-in `WriteStream`/`ReadStream` serialization.

Only these types are exposed through the Lua API:
- `SetInt`/`GetInt` → `DatumType::Integer`
- `SetFloat`/`GetFloat` → `DatumType::Float`
- `SetBool`/`GetBool` → `DatumType::Bool`
- `SetString`/`GetString` → `DatumType::String`
- `SetVector`/`GetVector` → `DatumType::Vector` (vec3)
- `SetVector2D`/`GetVector2D` → `DatumType::Vector2D` (vec2)
- `SetColor`/`GetColor` → `DatumType::Color` (vec4)

Asset, Node, Table, and Function datum types are NOT supported in save data
(they don't make sense for persistence).

---

## 4. Serialization Format

Binary format written via `Stream`:

```
[uint32: version]          -- SAVE_DATA_VERSION (currently 1)
[uint32: entry count]
Per entry:
  [string: key]            -- Stream::WriteString (length-prefixed)
  [Datum::WriteStream()]   -- [uint8 type][uint8 count][typed data...]
```

- `Datum::WriteStream(stream, net=false)` handles type/count/data encoding.
- `Datum::ReadStream(stream, version, net=false, external=false)` decodes.
- The version field is passed to `Datum::ReadStream` for future-proofing.

---

## 5. Lua API

Global table `SaveData`. All functions are static C functions registered via
`REGISTER_TABLE_FUNC`.

### Setters

```lua
SaveData.SetInt(key, value)        -- int32
SaveData.SetFloat(key, value)      -- float
SaveData.SetBool(key, value)       -- bool
SaveData.SetString(key, value)     -- string
SaveData.SetVector(key, vec)       -- vec3
SaveData.SetVector2D(key, vec)     -- vec2
SaveData.SetColor(key, vec)        -- vec4
```

### Getters (with default)

```lua
val = SaveData.GetInt(key, default)
val = SaveData.GetFloat(key, default)
val = SaveData.GetBool(key, default)
val = SaveData.GetString(key, default)
val = SaveData.GetVector(key, default)
val = SaveData.GetVector2D(key, default)
val = SaveData.GetColor(key, default)
```

Getters are type-safe: if the key exists but holds a different DatumType, the
default is returned. The default argument is optional; if omitted, zero/false/""
is used.

### Save Slots

```lua
SaveData.Save(slotName)            -- flush to disk
SaveData.Load(slotName)            -- clear + load from disk
SaveData.DoesSaveExist(slotName)   -- check file exists
SaveData.DeleteSave(slotName)      -- delete file
```

`Load` clears the in-memory map before reading. If the file doesn't exist, a
warning is logged and the map is left untouched.

### Key Management

```lua
SaveData.HasKey(key)     -- bool
SaveData.DeleteKey(key)  -- erase one key from memory
SaveData.DeleteAll()     -- clear entire in-memory map
```

---

## 6. Gotchas

- **Load replaces everything.** `SaveData.Load("slot1")` clears the entire
  in-memory map first. If you need data from two slots simultaneously, load
  one, copy what you need into local variables, then load the other.

- **Type mismatch returns default.** `SaveData.SetFloat("x", 1.0)` then
  `SaveData.GetInt("x", 0)` returns `0`, not `1`. The getter checks
  `DatumType` exactly.

- **No auto-save.** Setting values only changes the in-memory map. Forgetting
  to call `Save()` means data is lost when the game exits.

- **Slot names are filenames.** On filesystem platforms, the slot name becomes
  the filename verbatim. Avoid `/`, `\`, and other path-separator characters.

- **GameCube sector alignment.** `SYS_WriteSave` on GameCube pads the stream
  to sector boundaries via the CARD API. The SaveData format doesn't need to
  worry about this — it's handled in `System_Dolphin.cpp`.

- **Version forward-compat.** If `version > SAVE_DATA_VERSION`, the load is
  rejected with an error log. Bump `SAVE_DATA_VERSION` if you change the
  binary format (not needed for adding new DatumTypes — Datum handles that
  internally via its own type byte).
