# AnimatedWidget

The AnimatedWidget is a Quad that plays sprite animations on itself. It combines the rendering of [Quad](../Lua/Nodes/Widgets/Quad.md) with a built-in sprite-animation player — drop one in, assign animations, and it animates without any extra glue script or sibling node.

For the underlying logical animator (no rendering of its own), see [SpriteAnimator](../Lua/Nodes/SpriteAnimator.md). For 3D scenes targeting a Material's texture parameters, see [AnimatedSprite3D](../Lua/Nodes/3D/AnimatedSprite3D.md).

## Features

- **Built-in playback**: same Play/Pause/Stop/PlayAnimation API as SpriteAnimator
- **Multiple named animations** per widget, asset-driven and/or runtime-built
- **Discrete or atlas mode** — driven by the assigned [SpriteAnimation](../Lua/Assets/SpriteAnimation.md) assets
- **Editor preview** — toggle a checkbox to advance the animation in edit mode
- **Targeted playback** via `AnimateTo` / `AnimateToProgress` with optional callback
- **Signals** for animation lifecycle so other nodes can react

## Basic Usage

1. Add an **AnimatedWidget** to a Canvas (it appears in the *Add Widget* menu).
2. Create one or more [SpriteAnimation](../Lua/Assets/SpriteAnimation.md) assets and drag them into the widget's **Animations** array.
3. Set **Default Animation** to the name of the clip you want playing on Start.
4. Tick **Auto Play**, hit Play.

No script required for basic playback. Use Lua only when you need to switch animations dynamically, react to events, or build animations at runtime.

## Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| Animations | Asset[] (SpriteAnimation) | empty | List of SpriteAnimation assets registered on this widget. Each asset's name (or filename) becomes its clip name. |
| Default Animation | String | "" | Name of the clip to play on `Start()` if Auto Play is on. |
| Auto Play | Bool | true | Start playing the default animation when the scene runs. |
| Loop Override | Bool | false | When on, forces all clips to loop regardless of the per-asset Loop flag. |
| Playback Speed | Float | 1.0 | Multiplier applied to dt — 2.0 = double speed, 0.5 = half. |
| Editor Preview | Bool | false | When on, the animation advances during EditorTick so you can preview without entering Play mode. |
| Editor Play | Bool | false | Synthetic button — toggling fires `Play()` / `Pause()`. |
| Editor Stop | Bool | false | Synthetic button — toggling fires `Stop()`. |

All [Quad properties](../Lua/Nodes/Widgets/Quad.md) (texture, color, UV scale/offset, corner radius, etc.) are also present and behave identically — the AnimatedWidget overwrites texture and UV scale/offset on each frame change. Manual edits to those properties at runtime are clobbered by the next frame advance.

## Signals

| Signal | Parameters | Description |
|--------|------------|-------------|
| OnAnimationStart | name (String) | Fired when `PlayAnimation` is called or AutoPlay kicks off the default. |
| OnAnimationEnd | name (String) | Fired when a non-looping clip reaches its last frame. |
| OnFrameChanged | frameIndex (Integer) | Fired every time the displayed frame changes. |

```lua
function MyScript:Start()
    self.widget:ConnectSignal("OnAnimationEnd", self, function(s, animName)
        if animName == "explode" then
            s.widget.Owner:DestroyDeferred()
        end
    end)
end
```

## Common Patterns

### One-shot effect that despawns when done

```lua
function HitFX:Start()
    self.fx:PlayAnimation("hit")
    self.fx:ConnectSignal("OnAnimationEnd", self, function(s)
        s.Owner:DestroyDeferred()
    end)
end
```

### Switch animations on input

```lua
function CharacterUI:Tick(deltaTime)
    if Input.IsKeyPressed(Key.W) then
        self.portrait:PlayAnimation("walk")
    elseif Input.IsKeyPressed(Key.A) then
        self.portrait:PlayAnimation("attack")
    end
end
```

### Hold on a specific frame

```lua
self.widget:Pause()
self.widget:SetFrame(0)  -- frozen on first frame
```

### Play partway, then pause and run logic

```lua
-- Play to frame 6, pause, then trigger a hit
self.widget:AnimateTo(6, true, function()
    Player:DealDamage()
end)
```

### Build animations from script (no asset needed)

```lua
function RuntimeBuilder:Start()
    self.widget:CreateAnimation("idle")
    self.widget:AddImage("idle", "Textures/Idle_01")
    self.widget:AddImage("idle", "Textures/Idle_02")
    self.widget:AddImage("idle", "Textures/Idle_03")
    self.widget:PlayAnimation("idle")
end
```

### Drive a progress UI

```lua
function ProgressMirror:Tick(deltaTime)
    self.bar:SetValue(self.widget:GetProgress())
end
```

## Editor Preview

Toggle **Editor Preview** to ✓, then toggle **Editor Play** to advance frames in edit mode. The widget animates in the scene viewport without entering Play mode, useful for verifying frame timing and atlas cell selection without committing to a full play session.

For atlas-mode animations, the visual editor (**Edit Atlas Frames…** button on the SpriteAnimation asset) lets you click cells to add to the playback order, with grid carry-over between assets so you don't have to re-enter cols/rows when authoring a series.

## See Also

- [SpriteAnimator](../Lua/Nodes/SpriteAnimator.md) — the standalone logical animator (no rendering)
- [AnimatedSprite3D](../Lua/Nodes/3D/AnimatedSprite3D.md) — 3D variant that drives a Material
- [SpriteAnimation](../Lua/Assets/SpriteAnimation.md) — the asset that holds the frames
- [Quad](../Lua/Nodes/Widgets/Quad.md) — the underlying widget
- [UI Animation guide](../Development/UI/Animation.md) — manual tweens, timelines, and crossfades
