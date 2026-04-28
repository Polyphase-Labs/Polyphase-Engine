# SpriteAnimation

An asset that bundles a sequence of texture frames into a named animation clip. Used by [SpriteAnimator](../Nodes/SpriteAnimator.md), [AnimatedWidget](../Nodes/Widgets/AnimatedWidget.md), and [AnimatedSprite3D](../Nodes/3D/AnimatedSprite3D.md).

Inheritance:
* [Asset](Asset.md)

## Modes

A SpriteAnimation operates in one of two modes:

* **Discrete** (default): one Texture asset per frame. Each frame can be a different size. Highest authoring flexibility, but more textures = more memory and more GPU texture binds at runtime.
* **AtlasGrid**: a single atlas Texture sliced into a regular grid (cols × rows + margin/spacing). Each frame is a cell index. Memory-efficient — frames share one texture binding, and only UVs change per frame. Ideal for packed sprite sheets.

The mode is set via the **Mode** property on the asset. Switching modes preserves the inactive mode's data, so you can set up both and toggle between them while authoring.

## Visual atlas editor

For AtlasGrid mode, the asset inspector exposes an **Edit Atlas Frames…** button that opens a visual editor:

* Set the grid via cols / rows / margin / spacing fields.
* Click cells in the atlas image to append them to the playback order.
* Right-click cells to remove from the order.
* Each cell already in playback shows its index as a green badge.
* Use the per-frame ↑/↓/✕ buttons in the right panel to reorder or remove.
* Apply commits the grid + frame indices back to the asset and saves immediately.

Grid settings carry over between assets — when you open a fresh atlas-mode SpriteAnimation, the editor seeds cols/rows/margin/spacing from the previous Apply, so authoring multiple animations from the same sprite sheet doesn't require re-entering the grid every time.

---
### GetAnimationName
Get the clip name. This is what `PlayAnimation(name)` matches against. If empty, the asset's filename is used as the clip name when registered on a SpriteAnimator-style node.

Sig: `name = SpriteAnimation:GetAnimationName()`
 - Ret: `string name`

---
### SetAnimationName
Set the clip name.

Sig: `SpriteAnimation:SetAnimationName(name)`
 - Arg: `string name`

---
### GetMode
Get the frame source mode. 0 = Discrete, 1 = AtlasGrid.

Sig: `mode = SpriteAnimation:GetMode()`
 - Ret: `integer mode`

---
### SetMode
Set the frame source mode.

Sig: `SpriteAnimation:SetMode(mode)`
 - Arg: `integer mode` 0 = Discrete, 1 = AtlasGrid

---
### GetFps
Get the playback rate in frames per second.

Sig: `fps = SpriteAnimation:GetFps()`
 - Ret: `number fps`

---
### SetFps
Set the playback rate.

Sig: `SpriteAnimation:SetFps(fps)`
 - Arg: `number fps`

---
### GetLoop
Get the loop flag.

Sig: `loop = SpriteAnimation:GetLoop()`
 - Ret: `boolean loop`

---
### SetLoop
Set the loop flag.

Sig: `SpriteAnimation:SetLoop(loop)`
 - Arg: `boolean loop`

---
### GetFrameCount
Total frames in the animation. Equals the Frames array size in Discrete mode, or the Atlas Frame Indices size in AtlasGrid mode.

Sig: `count = SpriteAnimation:GetFrameCount()`
 - Ret: `integer count`

---
### AddFrame
Discrete-mode only. Append a Texture to the Frames list.

Sig: `SpriteAnimation:AddFrame(texture)`
 - Arg: `Texture texture`

---
### ClearFrames
Discrete-mode only. Remove all Frames.

Sig: `SpriteAnimation:ClearFrames()`

---
### GetAtlasTexture
Atlas-mode only. Get the atlas texture.

Sig: `tex = SpriteAnimation:GetAtlasTexture()`
 - Ret: `Texture tex`

---
### SetAtlasTexture
Atlas-mode only. Set the atlas texture.

Sig: `SpriteAnimation:SetAtlasTexture(texture)`
 - Arg: `Texture texture`

---
### SetAtlasGrid
Atlas-mode only. Set the grid dimensions (number of cell columns and rows in the atlas).

Sig: `SpriteAnimation:SetAtlasGrid(cols, rows)`
 - Arg: `integer cols`
 - Arg: `integer rows`

---
### SetAtlasMargin
Atlas-mode only. Set the pixel margin around the entire grid (left/right and top/bottom).

Sig: `SpriteAnimation:SetAtlasMargin(x, y)`
 - Arg: `integer x` Horizontal margin in pixels
 - Arg: `integer y` Vertical margin in pixels

---
### SetAtlasSpacing
Atlas-mode only. Set the pixel spacing between adjacent cells.

Sig: `SpriteAnimation:SetAtlasSpacing(x, y)`
 - Arg: `integer x` Horizontal spacing in pixels
 - Arg: `integer y` Vertical spacing in pixels

---

## Properties (inspector)

| Property | Mode | Description |
|----------|------|-------------|
| Animation Name | Both | Name used by `PlayAnimation(name)`. Defaults to asset filename if empty. |
| Mode | Both | Discrete or Atlas Grid. |
| FPS | Both | Playback frame rate. |
| Loop | Both | Whether the clip loops by default (overridable on the animator). |
| Frames | Discrete | Ordered list of Texture assets, one per frame. |
| Atlas Texture | Atlas | Single packed sprite-sheet texture. |
| Atlas Cols / Rows | Atlas | Grid dimensions. |
| Atlas Margin X / Y | Atlas | Pixel margin around the grid. |
| Atlas Spacing X / Y | Atlas | Pixel spacing between cells. |
| Atlas Frame Indices | Atlas | Ordered list of cell indices that make up the playback. |
| Edit Atlas Frames… | Atlas | Opens the visual atlas editor. |

---

## Examples

> **Note:** Use `:` (colon) for method calls. Most of these examples are for runtime modification of an existing SpriteAnimation asset; in normal authoring you'd configure a SpriteAnimation through the editor inspector instead.

### Loading and inspecting a SpriteAnimation asset

```lua
local anim = LoadAsset("Animations/Anim_Walk")

Log.Debug("Clip name: " .. anim:GetAnimationName())
Log.Debug("FPS: " .. anim:GetFps())
Log.Debug("Frames: " .. anim:GetFrameCount())
Log.Debug("Loops: " .. tostring(anim:GetLoop()))
Log.Debug("Mode: " .. anim:GetMode())  -- 0 = Discrete, 1 = AtlasGrid
```

### Tweaking playback parameters at runtime

```lua
function CutsceneScript:Start()
    local anim = LoadAsset("Animations/Anim_Boss_Idle")

    -- Slow down idle for dramatic effect
    anim:SetFps(6.0)
    anim:SetLoop(true)
end
```

### Adding frames to a Discrete-mode SpriteAnimation

```lua
function BuildAnimation:Start()
    local anim = LoadAsset("Animations/Anim_Pickup")

    -- Make sure we're in Discrete mode (0). Atlas mode is 1.
    anim:SetMode(0)
    anim:ClearFrames()

    anim:AddFrame(LoadAsset("Textures/T_Pickup_01"))
    anim:AddFrame(LoadAsset("Textures/T_Pickup_02"))
    anim:AddFrame(LoadAsset("Textures/T_Pickup_03"))
    anim:SetFps(8.0)
    anim:SetLoop(false)
end
```

### Configuring an Atlas-mode SpriteAnimation from script

```lua
function SetupAtlasAnim:Start()
    local anim = LoadAsset("Animations/Anim_AtlasWalk")

    anim:SetMode(1)  -- Atlas Grid
    anim:SetAtlasTexture(LoadAsset("Textures/T_CharacterSheet"))
    anim:SetAtlasGrid(8, 4)        -- 8 columns × 4 rows
    anim:SetAtlasMargin(0, 0)
    anim:SetAtlasSpacing(2, 2)     -- 2px between adjacent cells
    anim:SetFps(12.0)
    anim:SetLoop(true)

    -- The Atlas Frame Indices array still has to be set in the inspector
    -- (or via the visual atlas editor) — there's no Lua method to populate
    -- it directly in v1.
end
```

### Common pattern: register on an animator and play by name

The asset itself is rarely manipulated from script — the typical flow is to register it with a [SpriteAnimator](../Nodes/SpriteAnimator.md) / [AnimatedWidget](../Nodes/Widgets/AnimatedWidget.md) / [AnimatedSprite3D](../Nodes/3D/AnimatedSprite3D.md) and play by clip name:

```lua
function PlayerSetup:Start()
    -- Register a SpriteAnimation asset on the animator. Once added, the
    -- animator can play it by name (the SpriteAnimation's Animation Name
    -- field, or its asset filename if blank).
    self.animator:AddAnimation(LoadAsset("Animations/Anim_Walk"))
    self.animator:AddAnimation("Animations/Anim_Run")  -- by path also works

    self.animator:PlayAnimation("walk")
end
```

### Authoring tip — bulk import from selected textures

You don't usually need any of the above. In the editor, select the texture frames in the Asset Browser (click first, Shift-click last for range), right-click → **Create Animation Asset From Selected**. Frames are added in selection order, and the new SpriteAnimation is auto-named `Anim_<base>`.

---

## See Also

- [SpriteAnimator](../Nodes/SpriteAnimator.md)
- [AnimatedWidget](../Nodes/Widgets/AnimatedWidget.md)
- [AnimatedSprite3D](../Nodes/3D/AnimatedSprite3D.md)
