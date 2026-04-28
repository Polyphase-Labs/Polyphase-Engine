# SpriteAnimator

A logical sprite-animation player. Holds a registry of named animations and advances them on Tick, exposing the current frame's texture and atlas UVs for binding to other nodes (Quad, Material, etc).

For combined render-and-animate widgets, use [AnimatedWidget](Widgets/AnimatedWidget.md). For 3D Material targeting, use [AnimatedSprite3D](3D/AnimatedSprite3D.md). SpriteAnimator is the lowest-level option — useful when you want manual control over how the current frame is consumed (e.g. driving custom shader parameters, multiple Quads, etc).

Inheritance:
* [Node](Node.md)

## Binding to a Quad (script glue)

```lua
function QuadBinder:Start()
    self.anim:ConnectSignal("OnFrameChanged", self, function(s)
        s.quad:SetTexture(s.anim:GetCurrentTexture())
        local sx, sy = s.anim:GetCurrentUVScale()
        local ox, oy = s.anim:GetCurrentUVOffset()
        s.quad:SetUvScale(Vec(sx, sy))
        s.quad:SetUvOffset(Vec(ox, oy))
    end)
end
```

The `GetCurrentUVScale` / `GetCurrentUVOffset` outputs are pre-encoded for Quad's `(baseUV + offset) * scale` formula. Discrete-mode clips return scale=(1,1) and offset=(0,0), so the same code works for both modes.

---
### Play
Resume playback. Falls back to Default Animation if none is selected.

Sig: `SpriteAnimator:Play()`

---
### Pause
Pause at the current frame.

Sig: `SpriteAnimator:Pause()`

---
### Stop
Stop and reset to frame 0. Cancels any pending `AnimateTo`.

Sig: `SpriteAnimator:Stop()`

---
### PlayAnimation
Switch to and start playing the named animation. Emits `OnAnimationStart`.

Sig: `SpriteAnimator:PlayAnimation(name)`
 - Arg: `string name` Animation name

---
### IsPlaying
Check if currently playing.

Sig: `playing = SpriteAnimator:IsPlaying()`
 - Ret: `boolean playing`

---
### SetFrame
Jump to a specific frame. Clamped. Doesn't change play/pause. Cancels `AnimateTo`.

Sig: `SpriteAnimator:SetFrame(frameIndex)`
 - Arg: `integer frameIndex`

---
### AnimateTo
Play forward to `targetFrame`, then optionally pause and call `onFinished`. Fires immediately if already at target.

Sig: `SpriteAnimator:AnimateTo(targetFrame, pauseOnFinished, onFinished)`
 - Arg: `integer targetFrame`
 - Arg: `boolean pauseOnFinished` (optional, default `true`)
 - Arg: `function onFinished` (optional)

---
### AnimateToProgress
Animate to a target specified as 0..1 progress.

Sig: `SpriteAnimator:AnimateToProgress(progress, pauseOnFinished, onFinished)`
 - Arg: `number progress` Target progress in `[0, 1]`
 - Arg: `boolean pauseOnFinished` (optional, default `true`)
 - Arg: `function onFinished` (optional)

---
### CancelAnimateTo
Cancel a pending `AnimateTo`.

Sig: `SpriteAnimator:CancelAnimateTo()`

---
### SetSpeed / GetSpeed
Get/set playback speed multiplier.

Sig: `SpriteAnimator:SetSpeed(speed)`
 - Arg: `number speed`

Sig: `speed = SpriteAnimator:GetSpeed()`
 - Ret: `number speed`

---
### SetAutoPlay / GetAutoPlay
Whether the default animation plays on `Start()`.

Sig: `SpriteAnimator:SetAutoPlay(autoPlay)` / `autoPlay = SpriteAnimator:GetAutoPlay()`
 - Arg/Ret: `boolean autoPlay`

---
### SetLoopOverride / GetLoopOverride
Force all animations to loop regardless of per-asset Loop flag.

Sig: `SpriteAnimator:SetLoopOverride(loop)` / `loop = SpriteAnimator:GetLoopOverride()`
 - Arg/Ret: `boolean loop`

---
### SetDefaultAnimation / GetDefaultAnimation
The animation that plays on `Start()` (when Auto Play is on).

Sig: `SpriteAnimator:SetDefaultAnimation(name)` / `name = SpriteAnimator:GetDefaultAnimation()`
 - Arg/Ret: `string name`

---
### AddAnimation
Register a SpriteAnimation asset (or by path).

Sig (variant 1): `SpriteAnimator:AddAnimation(asset)`
 - Arg: `SpriteAnimation asset`

Sig (variant 2): `SpriteAnimator:AddAnimation(path)`
 - Arg: `string path`

---
### CreateAnimation
Create a script-built animation. Always Discrete mode.

Sig (variant 1): `SpriteAnimator:CreateAnimation(name)`
 - Arg: `string name`

Sig (variant 2): `SpriteAnimator:CreateAnimation(name, frames)`
 - Arg: `string name`
 - Arg: `table frames` Array of Texture assets

---
### AddImage
Append a frame to a script-built animation.

Sig (variant 1): `SpriteAnimator:AddImage(name, texture)`
Sig (variant 2): `SpriteAnimator:AddImage(name, path)`
 - Arg: `string name`
 - Arg: `Texture texture` OR `string path`

---
### AddImages
Append multiple frame paths.

Sig: `SpriteAnimator:AddImages(name, paths)`
 - Arg: `string name`
 - Arg: `table paths` Array of texture asset paths

---
### RemoveAnimation
Remove a registered animation. Stops playback if it was active.

Sig: `SpriteAnimator:RemoveAnimation(name)`
 - Arg: `string name`

---
### HasAnimation
Check if an animation is registered.

Sig: `has = SpriteAnimator:HasAnimation(name)`
 - Arg: `string name`
 - Ret: `boolean has`

---
### GetCurrentTexture
Current frame's texture. For atlas mode, this is the atlas texture.

Sig: `tex = SpriteAnimator:GetCurrentTexture()`
 - Ret: `Texture tex`

---
### GetCurrentUVScale
UV scale to feed into Quad's `SetUvScale`. Returns 2 numbers (sx, sy). For discrete mode: (1, 1). For atlas mode: (uv1 - uv0).

Sig: `sx, sy = SpriteAnimator:GetCurrentUVScale()`
 - Ret: `number sx`
 - Ret: `number sy`

---
### GetCurrentUVOffset
UV offset to feed into Quad's `SetUvOffset`. Returns 2 numbers (ox, oy). Encoded for Quad's `(baseUV + offset) * scale` formula — pass directly to `SetUvOffset`.

Sig: `ox, oy = SpriteAnimator:GetCurrentUVOffset()`
 - Ret: `number ox`
 - Ret: `number oy`

---
### GetCurrentUVRect
Raw UV rectangle (u0, v0, u1, v1) — useful for material vec4 parameters where the shader handles the UV transform itself.

Sig: `u0, v0, u1, v1 = SpriteAnimator:GetCurrentUVRect()`
 - Ret: `number u0, v0, u1, v1`

---
### GetCurrentAnimationName
Current animation name (empty string if none).

Sig: `name = SpriteAnimator:GetCurrentAnimationName()`
 - Ret: `string name`

---
### GetCurrentFrameIndex
Current frame index.

Sig: `idx = SpriteAnimator:GetCurrentFrameIndex()`
 - Ret: `integer idx`

---
### GetProgress
Smooth playback progress in `[0, 1]`. Sub-frame interpolated. Snaps to 1.0 at end of non-looping clip.

Sig: `p = SpriteAnimator:GetProgress()`
 - Ret: `number p`

---

## Signals

| Signal | Parameters | Description |
|--------|------------|-------------|
| OnAnimationStart | name (String) | Fired when a new clip starts. |
| OnAnimationEnd | name (String) | Fired when a non-looping clip ends. |
| OnFrameChanged | frameIndex (Integer) | Fired on every frame change. |

---

## Examples

> **Note:** Always use `:` for method calls (`anim:Play()`), not `.` (`anim.Play()`). The colon form auto-passes the node as the first argument; the dot form does not, and ConnectSignal in particular will fail with a confusing nil-index error if you use `.`.

### Connecting to OnAnimationEnd

```lua
MyScript = {}

function MyScript:GatherProperties()
    return { { name = "anim", type = DatumType.Node } }
end

function MyScript:Start()
    -- Use ':' (colon), not '.' — this is the most common gotcha
    self.anim:ConnectSignal("OnAnimationEnd", self, MyScript.OnAnimEnded)
end

function MyScript:OnAnimEnded(animName)
    Log.Debug("Animation finished: " .. animName)
end
```

### Driving a Quad from a SpriteAnimator

```lua
QuadBinder = {}

function QuadBinder:GatherProperties()
    return {
        { name = "anim", type = DatumType.Node },
        { name = "quad", type = DatumType.Quad },
    }
end

function QuadBinder:Start()
    self.anim:ConnectSignal("OnFrameChanged", self, QuadBinder.Refresh)
    self:Refresh()  -- apply the initial frame so the quad isn't blank
end

function QuadBinder:Refresh()
    local tex = self.anim:GetCurrentTexture()
    if tex ~= nil then self.quad:SetTexture(tex) end

    -- UV scale/offset are no-ops in discrete mode, only matter for atlas mode
    local sx, sy = self.anim:GetCurrentUVScale()
    local ox, oy = self.anim:GetCurrentUVOffset()
    self.quad:SetUvScale(Vec(sx, sy))
    self.quad:SetUvOffset(Vec(ox, oy))
end
```

### Building animations at runtime (no asset needed)

```lua
function RuntimeBuilder:Start()
    -- Variant 1: empty + AddImage one-by-one
    self.anim:CreateAnimation("idle")
    self.anim:AddImage("idle", "Textures/Idle_01")
    self.anim:AddImage("idle", "Textures/Idle_02")

    -- Variant 2: bulk add via AddImages (string paths)
    self.anim:AddImages("walk", {
        "Textures/Walk_01",
        "Textures/Walk_02",
        "Textures/Walk_03",
    })

    -- Variant 3: pre-load + CreateAnimation with table of textures
    local hitFrames = {
        LoadAsset("Textures/Hit_01"),
        LoadAsset("Textures/Hit_02"),
    }
    self.anim:CreateAnimation("hit", hitFrames)

    self.anim:PlayAnimation("idle")
end
```

### Pause on a specific frame

```lua
self.anim:Pause()
self.anim:SetFrame(0)  -- frozen on first frame
```

### Animate to a frame, then trigger logic

```lua
-- Wind-up then deal damage on the impact frame, then return to idle
function Attack:Run()
    self.anim:PlayAnimation("attack")
    self.anim:AnimateTo(8, false, function()
        Player:DealDamage()
    end)
end
```

### Drive a progress bar

```lua
function ProgressMirror:Tick(deltaTime)
    self.bar:SetValue(self.anim:GetProgress())
end
```

### Simple state machine

```lua
function Character:Tick(deltaTime)
    if Input.IsKeyJustDown(Key.W) and self.state ~= "walk" then
        self.state = "walk"
        self.anim:PlayAnimation("walk")
    elseif Input.IsKeyJustDown(Key.Space) and self.state ~= "jump" then
        self.state = "jump"
        self.anim:PlayAnimation("jump")
        -- One-shot: when jump ends, go back to idle
        self.anim:ConnectSignal("OnAnimationEnd", self, function(s, name)
            if name == "jump" then
                s.state = "idle"
                s.anim:PlayAnimation("idle")
            end
        end)
    end
end
```
