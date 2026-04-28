# AnimatedWidget

A Quad widget that plays sprite animations on itself. Combines the rendering of Quad with a built-in sprite-animation player — no sibling SpriteAnimator + script glue required.

For the standalone logical animator, see [SpriteAnimator](../SpriteAnimator.md). For the 3D variant that drives a Material, see [AnimatedSprite3D](../3D/AnimatedSprite3D.md).

Inheritance:
* [Node](../Node.md)
* [Widget](Widget.md)
* [Quad](Quad.md)

All Quad methods (`SetTexture`, `SetColor`, `SetUvScale`, etc.) are available, but `SetTexture` / `SetUvScale` / `SetUvOffset` are overwritten on each frame change.

---
### Play
Resume playback of the current animation. If no animation is currently selected, falls back to the Default Animation.

Sig: `AnimatedWidget:Play()`

---
### Pause
Pause playback at the current frame. The current frame stays visible.

Sig: `AnimatedWidget:Pause()`

---
### Stop
Stop playback and reset to frame 0. Cancels any pending `AnimateTo`.

Sig: `AnimatedWidget:Stop()`

---
### PlayAnimation
Switch to and start playing the named animation. Cancels any pending `AnimateTo`. Emits the `OnAnimationStart` signal.

Sig: `AnimatedWidget:PlayAnimation(name)`
 - Arg: `string name` Animation name (matches the Animation Name field on a SpriteAnimation asset, or a name passed to `CreateAnimation`)

---
### IsPlaying
Check whether the widget is currently advancing frames.

Sig: `playing = AnimatedWidget:IsPlaying()`
 - Ret: `boolean playing` True if playing

---
### SetFrame
Jump to a specific frame in the current animation. Clamps to `[0, frameCount-1]`. Does not change the play/pause state. Cancels any pending `AnimateTo`.

Sig: `AnimatedWidget:SetFrame(frameIndex)`
 - Arg: `integer frameIndex` Target frame index

---
### AnimateTo
Play forward until reaching `targetFrame`, then optionally pause and call `onFinished`. If already at the target, the callback fires immediately. Implicitly calls `Play()` so it works even if paused.

Sig: `AnimatedWidget:AnimateTo(targetFrame, pauseOnFinished, onFinished)`
 - Arg: `integer targetFrame` Frame to stop at
 - Arg: `boolean pauseOnFinished` (optional, default `true`) Pause when target is reached
 - Arg: `function onFinished` (optional) Callback invoked when target is reached

---
### AnimateToProgress
Like `AnimateTo` but specifies the target as a 0..1 progress value. Convenience wrapper that converts to a frame index internally.

Sig: `AnimatedWidget:AnimateToProgress(progress, pauseOnFinished, onFinished)`
 - Arg: `number progress` Target progress in `[0, 1]`
 - Arg: `boolean pauseOnFinished` (optional, default `true`) Pause when target is reached
 - Arg: `function onFinished` (optional) Callback invoked when target is reached

---
### CancelAnimateTo
Cancel a pending `AnimateTo` target without affecting play/pause state. The callback won't fire.

Sig: `AnimatedWidget:CancelAnimateTo()`

---
### SetSpeed
Set the playback speed multiplier. 1.0 = normal, 2.0 = double speed, 0.5 = half. Negative values are not supported.

Sig: `AnimatedWidget:SetSpeed(speed)`
 - Arg: `number speed` Speed multiplier

---
### GetSpeed
Get the current playback speed multiplier.

Sig: `speed = AnimatedWidget:GetSpeed()`
 - Ret: `number speed` Speed multiplier

---
### SetAutoPlay
Set whether the default animation plays automatically on `Start()`.

Sig: `AnimatedWidget:SetAutoPlay(autoPlay)`
 - Arg: `boolean autoPlay` Auto play

---
### GetAutoPlay
Get the auto-play flag.

Sig: `autoPlay = AnimatedWidget:GetAutoPlay()`
 - Ret: `boolean autoPlay` Auto play

---
### SetLoopOverride
When set to true, all animations loop regardless of the per-asset Loop flag.

Sig: `AnimatedWidget:SetLoopOverride(loop)`
 - Arg: `boolean loop` Loop override

---
### GetLoopOverride
Get the loop override flag.

Sig: `loop = AnimatedWidget:GetLoopOverride()`
 - Ret: `boolean loop` Loop override

---
### SetDefaultAnimation
Set the name of the animation to play on `Start()` (when Auto Play is on).

Sig: `AnimatedWidget:SetDefaultAnimation(name)`
 - Arg: `string name` Animation name

---
### GetDefaultAnimation
Get the default animation name.

Sig: `name = AnimatedWidget:GetDefaultAnimation()`
 - Ret: `string name` Animation name

---
### AddAnimation
Register a SpriteAnimation asset (or asset by path) so it can be played by name.

Sig (variant 1): `AnimatedWidget:AddAnimation(asset)`
 - Arg: `SpriteAnimation asset` SpriteAnimation asset

Sig (variant 2): `AnimatedWidget:AddAnimation(path)`
 - Arg: `string path` Path to a SpriteAnimation asset

---
### CreateAnimation
Create a script-built animation entry by name. Optionally pass a list of textures as the initial frames. Script-built entries are always Discrete-mode (one texture per frame).

Sig (variant 1): `AnimatedWidget:CreateAnimation(name)`
 - Arg: `string name` Animation name

Sig (variant 2): `AnimatedWidget:CreateAnimation(name, frames)`
 - Arg: `string name` Animation name
 - Arg: `table frames` Array of Texture assets

---
### AddImage
Append a frame to a script-built animation. The animation is auto-created if it doesn't exist yet. Cannot be used on asset-driven entries.

Sig (variant 1): `AnimatedWidget:AddImage(name, texture)`
 - Arg: `string name` Animation name
 - Arg: `Texture texture` Texture asset

Sig (variant 2): `AnimatedWidget:AddImage(name, path)`
 - Arg: `string name` Animation name
 - Arg: `string path` Path to a Texture asset

---
### AddImages
Append multiple frames at once.

Sig: `AnimatedWidget:AddImages(name, paths)`
 - Arg: `string name` Animation name
 - Arg: `table paths` Array of texture asset paths (strings)

---
### RemoveAnimation
Remove a registered animation by name. If it's the currently-playing clip, playback stops.

Sig: `AnimatedWidget:RemoveAnimation(name)`
 - Arg: `string name` Animation name

---
### HasAnimation
Check if an animation is registered.

Sig: `has = AnimatedWidget:HasAnimation(name)`
 - Arg: `string name` Animation name
 - Ret: `boolean has` True if registered

---
### GetCurrentTexture
Get the current frame's texture. For atlas-mode clips, this is the atlas texture (UV scale/offset is what changes per frame).

Sig: `tex = AnimatedWidget:GetCurrentTexture()`
 - Ret: `Texture tex` Current frame texture (or nil)

---
### GetCurrentAnimationName
Get the name of the currently-playing animation. Empty string if none.

Sig: `name = AnimatedWidget:GetCurrentAnimationName()`
 - Ret: `string name` Animation name

---
### GetCurrentFrameIndex
Get the current frame index within the active animation.

Sig: `idx = AnimatedWidget:GetCurrentFrameIndex()`
 - Ret: `integer idx` Frame index

---
### GetProgress
Get smooth playback progress in `[0, 1]` across the current clip. Sub-frame interpolated. Snaps to 1.0 at the end of a non-looping clip.

Sig: `p = AnimatedWidget:GetProgress()`
 - Ret: `number p` Progress in `[0, 1]`

---

## Signals

| Signal | Parameters | Description |
|--------|------------|-------------|
| OnAnimationStart | name (String) | Fired when a new clip starts via `PlayAnimation` or auto-play. |
| OnAnimationEnd | name (String) | Fired when a non-looping clip reaches its last frame. |
| OnFrameChanged | frameIndex (Integer) | Fired every time the displayed frame changes. |
