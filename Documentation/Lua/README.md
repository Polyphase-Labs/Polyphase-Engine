# Overview

## Nodes

* [Node](Nodes/Node.md)
    * [SpriteAnimator](Nodes/SpriteAnimator.md)
    * [Node3D](Nodes/3D/Node3D.md)
        * [AnimatedSprite3D](Nodes/3D/AnimatedSprite3D.md)
        * [Audio3D](Nodes/3D/Audio3D.md)
        * [Camera3D](Nodes/3D/Camera3D.md)
        * [Light3D](Nodes/3D/Light3D.md)
            * [DirectionalLight3D](Nodes/3D/DirectionalLight3D.md)
            * [PointLight3D](Nodes/3D/PointLight3D.md)
        * [Primitive3D](Nodes/3D/Primitive3D.md)
            * [Box3D](Nodes/3D/Box3D.md)
            * [Capsule3D](Nodes/3D/Capsule3D.md)
            * [Mesh3D](Nodes/3D/Mesh3D.md)
                * [SkeletalMesh3D](Nodes/3D/SkeletalMesh3D.md)
                * [StaticMesh3D](Nodes/3D/StaticMesh3D.md)
                    * [InstancedMesh3D](Nodes/3D/InstancedMesh3D.md)
                    * [ShadowMesh3D](Nodes/3D/ShadowMesh3D.md)
                * [TextMesh3D](Nodes/3D/TextMesh3D.md)
            * [Particle3D](Nodes/3D/Particle3D.md)
            * [Sphere3D](Nodes/3D/Sphere3D.md)
    * [Widget](Nodes/Widgets/Widget.md)
        * [Poly](Nodes/Widgets/Poly.md)
            * [PolyRect](Nodes/Widgets/PolyRect.md)
        * [Quad](Nodes/Widgets/Quad.md)
            * [AnimatedWidget](Nodes/Widgets/AnimatedWidget.md)
        * [Text](Nodes/Widgets/Text.md)
        * [Button](Nodes/Widgets/Button.md)

## Assets

* [Asset](Assets/Asset.md)
    * [Font](Assets/Font.md)
    * [Material](Assets/Material.md)
        * [MaterialBase](Assets/MaterialBase.md)
        * [MaterialInstance](Assets/MaterialInstance.md)
        * [MaterialLite](Assets/MaterialLite.md)
    * [ParticleSystem](Assets/ParticleSystem.md)
        * [ParticleSystemInstance](Assets/ParticleSystemInstance.md)
    * [Scene](Assets/Scene.md)
    * [SkeletalMesh](Assets/SkeletalMesh.md)
    * [SoundWave](Assets/SoundWave.md)
    * [SpriteAnimation](Assets/SpriteAnimation.md)
    * [StaticMesh](Assets/StaticMesh.md)
    * [Texture](Assets/Texture.md)

## Systems

* [AssetManager](Systems/AssetManager.md)
* [Audio](Systems/Audio.md)
* [Debugger](Systems/Debugger.md)
* [Engine](Systems/Engine.md)
* [Input](Systems/Input.md)
* [Log](Systems/Log.md)
* [Math](Systems/Math.md)
* [Network](Systems/Network.md)
* [Renderer](Systems/Renderer.md)
* [Script](Systems/Script.md)
* [System](Systems/System.md)
* [TimerManager](Systems/TimerManager.md)
* [SignalBus](Systems/SignalBus.md)

## Networking

* [Setting Up a Multiplayer Game](Networking/Multiplayer.md)
* [Http](Networking/Http.md)

## Misc

* [Enums](Misc/Enums.md)
* [Globals](Misc/Globals.md)
* [Property](Misc/Property.md)
* [Rect](Misc/Rect.md)
* [Stream](Misc/Stream.md)
* [Vector](Misc/Vector.md)
* [World](Misc/World.md)

## In-editor Lua Debugger

The editor ships with a built-in Lua breakpoint debugger — no external IDE
required. It's editor-only (zero footprint in packaged builds) and lets you
pause script execution, inspect the call stack, and view locals / upvalues
without leaving Polyphase.

### Setting a breakpoint

There are two ways to set a breakpoint, depending on where you're editing:

**1. From the in-engine Script Editor.** Open a `.lua` file in the Script
Editor dock, place the cursor on the line you want to break on, and press
`F9`. Press `F9` again to clear it. The current set of breakpoints is also
listed in the **Lua Debugger** dock (under the *Breakpoints* tab) where you
can remove them individually or clear them all.

**2. From your own editor (VS Code, Vim, etc.).** Add a call to
`Debugger.Break()` directly in the `.lua` source:

```lua
function Goblin:OnDamage(amount)
    self.hp = self.hp - amount
    if self.hp < 0 then
        Debugger.Break("hp went negative")
    end
end
```

`Debugger.Break` is a **silent no-op in shipping builds**, so leaving these
calls in the source is safe — you can ship a packaged game without removing
them. See [Systems/Debugger.md](Systems/Debugger.md) for the full API
reference (`Debugger.Break`, `Debugger.IsAttached`).

### When a breakpoint hits

The engine pauses, all scripts stop ticking, and the **Lua Debugger** dock
panel populates with:

* **Pause status header** — file:line, plus the optional message from
  `Debugger.Break("...")`.
* **Call Stack** tab — every Lua frame in the paused thread, click to
  inspect.
* **Locals** tab — locals + upvalues for the selected stack frame, with
  shallow `{k=v, ...}` summaries for tables.
* **Breakpoints** tab — manage all currently-set breakpoints.

Click **Continue (F5)** to resume. The engine arms a one-shot skip at the
break location so the script can step past its own breakpoint instead of
immediately re-trapping on the next Tick.

### Default keybindings

| Action | Shortcut |
|---|---|
| Toggle breakpoint at cursor (in Script Editor) | `F9` |
| Continue (only while paused) | `F5` |

These are remappable in the editor's hotkey settings under the *Lua
Debugger* category.

### Limitations (v1)

* **Continue-only.** Live step-over / step-into / step-out are not in v1 —
  they require an editor frame-pump that's tracked for v2. For now, set a
  fresh breakpoint or `Debugger.Break()` on the next line of interest.
* **Coroutines.** The debugger hook is installed on the main Lua thread.
  Scripts using `coroutine.create` won't trap inside the coroutine body
  unless the hook is re-installed inside it.
* **LuaPanda coexistence.** Lua only allows one line hook at a time. If
  LuaPanda is loaded at startup (via `OCT_LUA_DEBUGGING` on Windows), the
  in-engine debugger logs a warning and **replaces** LuaPanda's hook so F9
  / `Debugger.Break` work. Switch at runtime via the **Active** checkbox at
  the top of the Lua Debugger panel: uncheck to uninstall our hook and
  re-arm LuaPanda's so VS Code can attach; check again to take back over.
  No editor restart needed.

---

## Debugging with LuaPanda

*If you don't need VS Code integration, see the simpler
[In-editor Lua Debugger](#in-editor-lua-debugger) above.*

Polyphase integrates [LuaPanda](https://github.com/Tencent/LuaPanda) for breakpoint debugging of Lua scripts from VSCode.

### Requirements

* Windows build compiled with the `OCT_LUA_DEBUGGING` flag (enabled in `Engine/Source/Engine/Engine.cpp`).
* VSCode with the LuaPanda extension installed.
* LuaSocket, which ships with the engine in `External/LuaSocket/`.

### Install the VSCode extension

In VSCode press `Ctrl+Shift+P` → **Extensions: Install from VSIX…** and pick one of:

* `External/LuaPanda/vsix/luapanda-3.3.0.vsix` — recommended for general use.
* `External/LuaPanda/vsix/luapanda-3.1.1-for-unlua.vsix` — only for the UnLua variant.

### Configure `launch.json`

Open the engine repo root as your VSCode workspace folder, then add this entry to the `configurations` array in `.vscode/launch.json`:

```json
{
    "name": "LuaPanda",
    "type": "lua",
    "request": "launch",
    "cwd": "${workspaceFolder}",
    "luaFileExtension": "lua",
    "connectionPort": 8818,
    "stopOnEntry": false,
    "useCHook": false,
    "autoPathMode": true
}
```

Set `useCHook` to `true` later for lower per-line overhead if the native hook library is available on your machine.

### How it connects

On startup the engine runs `Engine/Scripts/StartLuaPanda.lua`, which requires `LuaPanda.lua` and calls `LuaPanda.start("127.0.0.1", 8818)`. The engine opens a TCP client on port **8818** and waits. VSCode attaches when you press F5 on the LuaPanda configuration.

### Workflow

1. Launch the engine first.
2. In VSCode open **Run and Debug** (`Ctrl+Shift+D`), pick **LuaPanda**, press `F5`.
3. Open a `.lua` file and click in the gutter to set a breakpoint.
4. Trigger the code path in the engine — execution halts and Variables, Call Stack, and Watch panes populate.
5. Use `F5` continue, `F10` step over, `F11` step in, `Shift+F11` step out.

### Troubleshooting

* **Hollow/grey breakpoints** indicate a path mismatch. Ensure `cwd` in `launch.json` points at the folder whose subtree contains the `.lua` files the engine loads.
* Run `LuaPanda.testBreakpoint()` from a Lua console to compare what `debug.getinfo` reports against what VSCode sent.
* Scripts loaded via `loadstring` / `load` cannot be debugged — they have no source mapping.
* Coroutines need `LuaPanda.changeCoroutineHookState()` called inside them to be debuggable.
