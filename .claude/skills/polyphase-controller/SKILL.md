---
name: polyphase-controller
description: Drive the running Polyphase editor over its REST controller server. Use this when the user wants to author or modify a scene programmatically while the editor is open — create scenes, spawn nodes, set transforms and properties, attach Lua scripts, fill in script-exposed fields, and start/stop play-in-editor. Triggers on requests like "spawn a cube at origin", "add a directional light", "attach the player script to the pawn", "build a level with N enemies", or "play the scene".
---

# Polyphase Controller Skill

Author Polyphase scenes by talking to the running editor's REST controller server.
The editor exposes a small HTTP API at `http://localhost:7890` (default port).
Every call is queued onto the editor's main thread and completes synchronously, so a
sequence of HTTP calls behaves like a script of editor actions.

The source of truth for every endpoint is
`Engine/Source/Editor/ControllerServer/ControllerServerRoutes.cpp`. The architecture
overview lives in `.llm/ControlServer.md`. This skill is the agent-facing playbook.

## When to use

Reach for this skill whenever the user wants the **editor** to do scene-authoring work
without clicking around the UI: creating scenes, populating them with nodes, placing nodes,
attaching scripts, configuring properties, or driving play-in-editor. If the user is
writing engine code, building widgets, or working on something that doesn't involve the
running editor's REST server, this skill does not apply.

## Prerequisites

1. **Editor is running** in a project. The controller server is **EDITOR-only**.
2. **Server enabled** in `Preferences → Network → Controller Server Enabled` (defaults to OFF).
3. **Default port** is `7890` (configurable in the same Preferences pane).
4. **Smoke test** before doing real work:
   ```bash
   curl http://localhost:7890/api/scene
   ```
   A successful response is HTTP 200 with `{"scene": "...", "playing": false, "paused": false}`.
   Connection refused = server not enabled or wrong port.

## Mental model — three things every recipe relies on

1. **Errors come back as HTTP 200** with `{"error": "..."}` in the body. The status code
   is *not* a reliable failure signal; check for the `error` key in the JSON response.
   The only `404` is the catchall for unknown URLs.
2. **Nodes are looked up by name**, not by hierarchy path. Names should be unique within
   the scene — the lookup walks the world and returns the first match. The default Scene
   root node is named `Root`.
3. **Commands are queued to the main thread** (`ControllerServer::QueueCommand`) and the
   HTTP response only returns once the command has executed. No polling needed; sequential
   `curl`/`requests` calls run in order.

Wire formats to memorise:
- `position`, `rotation`, `scale` are JSON arrays `[x, y, z]` (rotation is degrees).
- `Color` (when read) is `[r, g, b, a]` floats in 0..1.
- `Vector2D` (when read) is `[x, y]`.
- `Asset` properties (when read) are the asset's name as a plain string.

## Quickstart — author a scene from scratch

The canonical flow: create a scene, add nodes, place them, attach a script, fill its
fields, save. Every step shows curl first; a Python equivalent follows the recipe block.

```bash
# 1. Create + open a fresh 3D scene with a default Camera3D at (0,0,5).
curl -X POST http://localhost:7890/api/scene/new \
     -H "Content-Type: application/json" \
     -d '{"name":"SC_MyLevel","type":"3D","createCamera":true,"open":true}'

# 2. Discover the hierarchy (root will be named "Root").
curl http://localhost:7890/api/scene/hierarchy

# 3. Spawn a directional light under Root.
curl -X POST http://localhost:7890/api/nodes \
     -H "Content-Type: application/json" \
     -d '{"type":"DirectionalLight3D","name":"Sun","parent":"Root"}'

# 4. Place it (rotate -45° around X to point downward).
curl -X PUT http://localhost:7890/api/nodes/Sun/transform \
     -H "Content-Type: application/json" \
     -d '{"position":[0,10,0],"rotation":[-45,0,0]}'

# 5. Spawn the player and attach a Lua script.
curl -X POST http://localhost:7890/api/nodes \
     -H "Content-Type: application/json" \
     -d '{"type":"StaticMesh3D","name":"Player","parent":"Root"}'
curl -X PUT http://localhost:7890/api/nodes/Player/properties \
     -H "Content-Type: application/json" \
     -d '{"name":"Script","value":"PlayerController"}'

# 6. Configure a script-exposed field (e.g. a Speed number defined in the Lua table).
curl -X PUT http://localhost:7890/api/nodes/Player/script-properties \
     -H "Content-Type: application/json" \
     -d '{"name":"Speed","value":12.5}'

# 7. Save.
curl -X POST http://localhost:7890/api/scene/save
```

Python equivalent of step 1 (use `requests` / `httpx` for the rest by pattern):

```python
import requests
BASE = "http://localhost:7890"
r = requests.post(f"{BASE}/api/scene/new", json={
    "name": "SC_MyLevel", "type": "3D", "createCamera": True, "open": True,
})
data = r.json()
if "error" in data:
    raise RuntimeError(data["error"])
```

**Pick your transport by task complexity:** curl one-liners are fine for small fixed
sequences and demos; switch to a Python script when the agent needs to loop, branch,
parse hierarchy responses, or batch many calls.

## Recipes

### Discover the scene
```bash
curl http://localhost:7890/api/scene/hierarchy
```
Returns a recursive tree:
```json
{
  "name": "Root",
  "type": "Node3D",
  "active": true,
  "visible": true,
  "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1],
  "children": [ { "name": "Camera3D", "type": "Camera3D", ... } ]
}
```
Walk this once at the start of any non-trivial recipe to learn what already exists.
The 3D transform fields are only present on `Node3D`-derived nodes.

### Create and place a 3D node
```bash
curl -X POST http://localhost:7890/api/nodes \
     -H "Content-Type: application/json" \
     -d '{"type":"PointLight3D","name":"Lamp","parent":"Root"}'
curl -X PUT http://localhost:7890/api/nodes/Lamp/transform \
     -H "Content-Type: application/json" \
     -d '{"position":[3,2,0]}'
```
`POST /api/nodes` only accepts `type`, `name`, `parent` — initial transform/properties
require separate follow-up calls. Omit `parent` and the node lands wherever
`EXE_SpawnNode` defaults (typically the current selection or world root); always pass
`parent` for deterministic placement.

### Attach a Lua script
```bash
curl -X PUT http://localhost:7890/api/nodes/Player/properties \
     -H "Content-Type: application/json" \
     -d '{"name":"Script","value":"PlayerController"}'
```
The script-attachment property is a `String` property literally named `"Script"` on
`Node` (registered in `Engine/Source/Engine/Nodes/Node.cpp:591`). The value is the
script asset's name as it appears in the asset browser (no `.lua` extension, no path
prefix — e.g. `"PlayerController"`, not `"Scripts/PlayerController.lua"`).

Reminder for the agent authoring the Lua: Polyphase scripts are **tables** with
`:Start()` and `:Tick(dt)` methods, not free functions. Returning the table from the
script file is what exposes its fields to the editor.

### Set script-exposed properties
First enumerate, then set:
```bash
curl http://localhost:7890/api/nodes/Player/script-properties
curl -X PUT http://localhost:7890/api/nodes/Player/script-properties \
     -H "Content-Type: application/json" \
     -d '{"name":"Speed","value":12.5}'
```
Script properties are the fields defined inside the Lua table (e.g. `Speed = 0.0`).
The `GET` returns `{"properties": []}` (empty array) if no script is attached or the
script has no exposed fields. Settable types: Integer, Float, Bool, String, Vector,
Vector2D, Color, Asset (and asset subtypes — Material, Scene, etc.).

**Persistence**: PUT routes through the same `EXE_EditProperty` action the editor's
inspector uses, so the C++ property cache *and* the Lua-side value both stay in sync.
Earlier versions of this endpoint called `Script::SetField` directly, which only
updated Lua and left the cache stale — REST-set string/asset values would survive
in-memory but get clobbered on scene save/reload. That's fixed.

### Set reflected (engine-side) properties
First enumerate to learn the names + types:
```bash
curl http://localhost:7890/api/nodes/Lamp/properties
```
Returns `{"properties": [{"name":"...", "type": <int>, "value": ...}, ...]}`. The
`type` is the integer DatumType code (see reference below). Then set:
```bash
curl -X PUT http://localhost:7890/api/nodes/Lamp/properties \
     -H "Content-Type: application/json" \
     -d '{"name":"Intensity","value":3.5}'
```

### Reparent a node
```bash
curl -X PUT http://localhost:7890/api/scene/hierarchy \
     -H "Content-Type: application/json" \
     -d '{"node":"Lamp","parent":"Player","index":0}'
```
`index` is optional (defaults to append).

### Visibility, deletion, play mode
```bash
curl -X PUT http://localhost:7890/api/nodes/Lamp/visibility \
     -H "Content-Type: application/json" -d '{"visible":false}'
curl -X POST http://localhost:7890/api/nodes/Lamp/delete
curl -X POST http://localhost:7890/api/play/start
curl -X POST http://localhost:7890/api/play/pause
curl -X POST http://localhost:7890/api/play/resume
curl -X POST http://localhost:7890/api/play/stop
```
Note that delete is `POST /api/nodes/<name>/delete` (not the HTTP DELETE verb).

### Import an asset from disk
```bash
curl -X POST http://localhost:7890/api/assets/import \
     -H "Content-Type: application/json" \
     -d '{"path":"C:/Models/cube.fbx"}'
```
Returns `{"success": true, "name": "<imported asset name>", "type": "<asset type>"}`.
The path is an absolute disk path; the importer infers the asset type from the file.

### Assign a static mesh / material override to a node
"Static Mesh" and "Material Override" are just `Asset` properties — once you know the
asset's name, set them like any other property. The same PUT route handles both.

```bash
# List available static meshes so you know what names to pass.
curl 'http://localhost:7890/api/assets?type=StaticMesh'

# Set the StaticMesh3D's mesh.
curl -X PUT http://localhost:7890/api/nodes/Coin/properties \
     -H "Content-Type: application/json" \
     -d '{"name":"Static Mesh","value":"coin-gold"}'

# Set its material override (a Material asset, not a node property).
curl -X PUT http://localhost:7890/api/nodes/Coin/properties \
     -H "Content-Type: application/json" \
     -d '{"name":"Material Override","value":"M_Gold"}'

# Clear the override (revert to the mesh's default material).
curl -X PUT http://localhost:7890/api/nodes/Coin/properties \
     -H "Content-Type: application/json" \
     -d '{"name":"Material Override","value":""}'
```

Pass `""` (empty string) as `value` to clear an asset slot.

### List, create, and edit assets
```bash
# Everything project-side (engine assets hidden by default).
curl http://localhost:7890/api/assets

# Just textures whose name contains "background".
curl 'http://localhost:7890/api/assets?type=Texture&prefix=background'

# Create a new MaterialLite asset under <Project>/Materials.
curl -X POST http://localhost:7890/api/assets \
     -H "Content-Type: application/json" \
     -d '{"type":"MaterialLite","name":"M_Background","directory":"Materials"}'

# Configure it (Color is [r,g,b,a] in 0..1, Texture 0 takes a Texture asset name).
curl -X PUT http://localhost:7890/api/assets/M_Background/properties \
     -H "Content-Type: application/json" \
     -d '{"name":"Color","value":[1.0,1.0,1.0,1.0]}'
curl -X PUT http://localhost:7890/api/assets/M_Background/properties \
     -H "Content-Type: application/json" \
     -d '{"name":"Texture 0","value":"background_color_hills"}'
curl -X PUT http://localhost:7890/api/assets/M_Background/properties \
     -H "Content-Type: application/json" \
     -d '{"name":"Shading Model","value":0}'

# Persist to disk.
curl -X POST http://localhost:7890/api/assets/M_Background/save
```

This is the full "build a runtime material from a texture, apply it to a plane" flow —
exactly what `BackgroundPlane.lua`-style scripts used to be the only way to do. It
replaces 30 lines of `AssetManager.CreateAndRegisterAsset` + `mat:SetTexture` Lua.

Common asset class names for `?type=` and the `POST /api/assets` body:
`StaticMesh`, `SkeletalMesh`, `Texture`, `Material`, `MaterialLite`, `MaterialBase`,
`Font`, `SoundWave`, `ParticleSystem`, `Scene`, `TileSet`, `TileMap`, `Timeline`,
`NodeGraphAsset`. (Class names match `DEFINE_FACTORY` registrations — case-sensitive.)

### Read the editor debug log (errors, warnings, prints)
The same buffer the editor's Debug Log panel shows is exposed over REST. Use this
after a failed action to see *why* the editor refused, or after `play/start` to read
Lua `Log.Debug` output, runtime asserts, etc.

```bash
# Tail the most recent 200 lines (whatever's still in the ring).
curl http://localhost:7890/api/log

# Errors only.
curl 'http://localhost:7890/api/log?minSeverity=2'

# Polling: pass the previous response's nextSeq as `since` to get only new entries.
curl 'http://localhost:7890/api/log?since=1234&limit=500'
```

Response shape:
```json
{
  "entries": [
    {"seq": 1234, "severity": 2, "severityName": "Error",
     "timestamp": 12.45, "message": "Node not found: Foo"}
  ],
  "nextSeq": 1234,
  "dropped": false
}
```

Notes:
- Buffer holds the most recent **2048 entries** (matches the Debug Log panel cap).
  If the editor logged more than 2048 lines since your last poll, `dropped` is `true`
  and you missed entries between `since` and the first returned `seq`.
- `seq` is monotonic across the editor session; `0` is reserved (no entry has it).
- `severity` integers: `0=Debug`, `1=Warning`, `2=Error`. `minSeverity=2` filters to
  errors only; `minSeverity=1` returns warnings + errors.
- `timestamp` is seconds since engine start (matches the panel's `[hh:mm:ss]` clock).
- Polling cadence: cheap (`QueueCommand` + a copy of up to `limit` entries). Once a
  second is fine; tighter polling won't break anything but rarely buys signal.

Recipe — run something, then read what it logged:
```python
import requests, time
BASE = "http://localhost:7890"

# Mark current high-water mark before doing the action.
mark = requests.get(f"{BASE}/api/log?limit=1").json()["nextSeq"]

# Do the thing that might log.
requests.post(f"{BASE}/api/play/start")
time.sleep(0.5)  # give the editor a tick to surface log output

# Pull only what was logged after `mark`.
new_logs = requests.get(f"{BASE}/api/log", params={"since": mark, "limit": 1000}).json()
for e in new_logs["entries"]:
    print(f"[{e['severityName']}] {e['message']}")
```

### Look at the Game Preview viewport (screenshot)
Returns the same image the editor's "Screenshot" button captures, as a base64 PNG
inline in JSON. Useful for visually verifying a scene change actually looks right —
not just that the API call returned `{"success": true}`.

```bash
# Native resolution (whatever Game Preview is currently configured for).
curl -s http://localhost:7890/api/screenshot -o /tmp/scene.json

# Downscaled to 640px wide (preserves aspect; ~10x smaller payload at 1080p).
curl -s 'http://localhost:7890/api/screenshot?width=640' -o /tmp/scene.json
```

Decode in Python (and view the image directly):
```python
import base64, requests, pathlib
BASE = "http://localhost:7890"

shot = requests.get(f"{BASE}/api/screenshot", params={"width": 1024}).json()
if "error" in shot:
    raise RuntimeError(shot["error"])

png_bytes = base64.b64decode(shot["data"])
out = pathlib.Path("scene.png")
out.write_bytes(png_bytes)
print(f"saved {shot['width']}x{shot['height']} → {out} ({len(png_bytes)} bytes)")
```

Notes:
- **Captures the Game Preview viewport only** — no imgui chrome, inspector, or
  hierarchy panel. The Game Preview panel must be enabled in the editor (the eye
  toggle in its panel header). If it's disabled or hasn't rendered yet you'll get
  `{"error": "Game Preview not currently rendered (enable it in the editor)"}`.
- **3D-camera scenes only.** Game Preview renders the world through a `Camera3D`,
  so for pure UI/widget authoring it won't show your work outside play mode — use
  `/api/screenshot/editor` instead (below).
- **Vulkan-only.** Other graphics backends return an error.
- `width` is optional and only downscales (won't upscale past native). Aspect ratio
  is preserved. A 1080p frame is a few MB base64; passing `width=640` or `width=512`
  is plenty for "did the cube spawn where I expected" verification.
- The capture happens synchronously on the main thread — the response only returns
  after readback is done, so a follow-up call is guaranteed to see the latest frame.
- Payload is base64 PNG, not a binary `image/png` response. This keeps the wire
  format consistent with every other endpoint (JSON in, JSON out) and makes it
  trivial to embed alongside other fields if needed later.

### Look at the entire editor window (UI screenshot)
Captures the swapchain after the next render — same image you'd see if you took a
screenshot of the editor window with the OS, including all imgui panels (inspector,
hierarchy, debug log) plus whatever the editor is rendering in the main viewport.

Use this when:
- Authoring a `Widget`/`Canvas` UI scene (Game Preview won't show widgets without a
  3D camera, so `/api/screenshot` is useless for pure UI work).
- Debugging a 2D scene that has no 3D camera attached.
- Verifying inspector / hierarchy panel state after a property edit.
- "Show me what the user is looking at right now."

```bash
# Full editor at native resolution.
curl http://localhost:7890/api/screenshot/editor -o /tmp/editor.json

# 1024px wide preview.
curl 'http://localhost:7890/api/screenshot/editor?width=1024' -o /tmp/editor.json
```

Same response shape as `/api/screenshot`: `{format, width, height, data}` with
base64 PNG in `data`. Decode the same way:

```python
import base64, requests, pathlib
shot = requests.get("http://localhost:7890/api/screenshot/editor",
                    params={"width": 1024}).json()
if "error" in shot:
    raise RuntimeError(shot["error"])
pathlib.Path("editor.png").write_bytes(base64.b64decode(shot["data"]))
```

Notes:
- **Latency: ~one render frame (~16 ms).** The capture is deferred via a promise
  fulfilled by the renderer's post-render hook just before `vkQueuePresent`. The
  request blocks until then. If the editor isn't actively rendering (window
  minimized, hidden behind other windows on some compositors), the route times out
  after 2s with `{"error": "Editor screenshot timed out (no render frame within 2s)"}`.
- **Vulkan-only.** Returns an error on other graphics backends.
- Captures the **whole editor window** at native resolution — that's typically
  1080p+ or higher on multi-monitor setups, so payloads can be sizeable. Always
  pass `width` (e.g. `1024` or `1280`) for routine viewing.
- Picks the actual swapchain format (handles both BGRA and RGBA surface formats).

## Endpoint reference

All endpoints return HTTP 200 on success with JSON; errors are HTTP 200 with `{"error": "..."}`.

### Scene
| Method | Path | Body | Returns on success |
|---|---|---|---|
| GET | `/api/scene` | — | `{scene, playing, paused}` |
| POST | `/api/scene/new` | `{name, type:"3D"\|"2D", createCamera, open}` | `{success, scene}` |
| POST | `/api/scene/open` | `{name}` | `{success, scene}` |
| POST | `/api/scene/save` | — | `{success}` |
| GET | `/api/scene/hierarchy` | — | recursive tree |
| PUT | `/api/scene/hierarchy` | `{node, parent, index?}` | `{success}` |

### Nodes
| Method | Path | Body | Returns on success |
|---|---|---|---|
| GET | `/api/nodes/<name>` | — | NodeJSON |
| POST | `/api/nodes` | `{type, name?, parent?}` | NodeJSON |
| POST | `/api/nodes/<name>/delete` | — | `{success}` |
| PUT | `/api/nodes/<name>/transform` | `{position?, rotation?, scale?}` | NodeJSON |
| PUT | `/api/nodes/<name>/move` | `{position}` | NodeJSON |
| PUT | `/api/nodes/<name>/rotate` | `{rotation}` (degrees) | NodeJSON |
| PUT | `/api/nodes/<name>/scale` | `{scale}` | NodeJSON |
| PUT | `/api/nodes/<name>/visibility` | `{visible}` | `{success, visible}` |

NodeJSON: `{name, type, active, visible, position?, rotation?, scale?}`. Transform fields
present only for `Node3D`-derived nodes. The `transform`/`move`/`rotate`/`scale`
endpoints are 3D-only; calling them on a 2D widget or pure `Node` returns an error.

### Properties
| Method | Path | Body | Returns on success |
|---|---|---|---|
| GET | `/api/nodes/<name>/properties` | — | `{properties: [{name, type, value}]}` |
| PUT | `/api/nodes/<name>/properties` | `{name, value}` | `{success}` |
| GET | `/api/nodes/<name>/script-properties` | — | `{properties: [...]}` |
| PUT | `/api/nodes/<name>/script-properties` | `{name, value}` | `{success}` |

### Play / Assets
| Method | Path | Body | Returns on success |
|---|---|---|---|
| POST | `/api/play/start` | — | `{success, playing}` |
| POST | `/api/play/stop` | — | `{success, playing}` |
| POST | `/api/play/pause` | — | `{success, paused}` |
| POST | `/api/play/resume` | — | `{success, paused}` |
| POST | `/api/assets/import` | `{path}` | `{success, name, type}` |

### Diagnostics
| Method | Path | Query | Returns on success |
|---|---|---|---|
| GET | `/api/log` | `since`, `limit`, `minSeverity` | `{entries: [...], nextSeq, dropped}` |
| GET | `/api/screenshot` | `width` | `{format, width, height, data}` (PNG, base64) — Game Preview viewport only |
| GET | `/api/screenshot/editor` | `width` | `{format, width, height, data}` (PNG, base64) — full editor window (incl. UI chrome) |

## Property types and JSON wire format

The integer `type` code in `GET .../properties` responses is the `DatumType` enum index
from `Engine/Source/Engine/Datum.h:27`. Settable types and wire format:

| Code | DatumType | JSON value | Settable via PUT? |
|---|---|---|---|
| 0 | Integer | number | yes |
| 1 | Float | number | yes |
| 2 | Bool | boolean | yes |
| 3 | String | string | yes |
| 4 | Vector2D | `[x, y]` | yes |
| 5 | Vector | `[x, y, z]` | yes |
| 6 | Color | `[r, g, b, a]` (0–1 floats) | yes |
| 7 | Asset | asset name (string, `""` to clear) | yes |
| 8 | Byte | number | yes |
| 11 | Short | number | yes |
| — | Material / Scene / TileSet / TileMap / Timeline / NodeGraphAsset (asset subtypes) | asset name (string) | yes |

Asset values are looked up by name from `AssetManager`; pass `""` to clear an asset
slot. The same JSON shape works on `/api/nodes/<n>/properties`,
`/api/nodes/<n>/script-properties`, and `/api/assets/<n>/properties`.

## Common node types you can pass to `POST /api/nodes`

3D: `Node3D`, `StaticMesh3D`, `SkeletalMesh3D`, `Camera3D`, `PointLight3D`,
`DirectionalLight3D`, `Audio3D`, `Particle3D`, `Box3D`, `Sphere3D`, `Capsule3D`,
`InstancedMesh3D`, `TextMesh3D`, `TileMap2D`, `Voxel3D`, `Terrain3D`, `Spline3D`,
`NavMesh3D`.

UI / 2D: `Widget`, `Quad`, `Text`, `Canvas`, `Button`, `CheckBox`, `InputField`,
`LineEdit`, `ProgressBar`, `Slider`, `SpinBox`, `Window`, `ComboBox`, `ListViewWidget`,
`ScrollContainer`.

Other: `TimelinePlayer`, `NodeGraphPlayer`, `SpriteAnimator`.

The class name is case-sensitive and must match a `DEFINE_NODE(...)` registration.

## Known limitations

- `PUT .../properties` only edits index 0 of an array property. Multi-index array
  edits aren't yet exposed — for those, do it in-editor or via Lua.
- `POST /api/nodes` ignores all body fields except `type` / `name` / `parent`. Initial
  transform and property values require follow-up `PUT` calls.
- `POST /api/scene/save` saves to the currently-open scene asset only — no save-as.
- `POST /api/scene/new` only supports built-in `2D` and `3D` scene types. Plugin-registered
  scene types must be created through the editor's File → New Scene menu.
- 2D widgets are not addressable by `transform`/`move`/`rotate`/`scale` (those endpoints
  are 3D-only). Use `PUT .../properties` to set widget position/size if needed.

## Troubleshooting

- **Connection refused** — controller server is OFF in Preferences, or the editor is not
  running. Open `Preferences → Network` and toggle it on.
- **`{"error": "Node not found: X"}`** — name typo, the node was deleted, or the name is
  not unique (the lookup returns the first match). `GET /api/scene/hierarchy` to verify.
- **`{"error": "Unsupported property type for set"}`** — see *Known limitations*. Set the
  property via the in-editor inspector or from Lua instead.
- **`{"error": "Node is not a 3D node"}`** — you used `transform`/`move`/`rotate`/`scale`
  on a non-`Node3D` node; use `PUT .../properties` for those node types.

## See also

- `.llm/ControlServer.md` — architecture overview, threading model, addon hook system.
- `Engine/Source/Editor/ControllerServer/ControllerServerRoutes.cpp` — the source of truth
  for every endpoint's wire format and validation rules.
- `Engine/Source/Engine/Datum.h` — full `DatumType` enum, in case a property's `type`
  code falls outside the table above (subtype codes for Widget/Node3D/asset variants).
