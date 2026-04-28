# AnimatedSprite3D

A logical 3D node that drives a connected Material's texture parameters from a sprite animation. Doesn't render anything itself — instead, every frame it writes the current animation frame into one or more of the target Material's texture slots, so any mesh using that Material picks up the animation through normal rendering.

For widget rendering, see [AnimatedWidget](Widgets/AnimatedWidget.md). For the standalone logical animator with no Material targeting, see [SpriteAnimator](../SpriteAnimator.md).

Inheritance:
* [Node](../Node.md)
* [Node3D](Node3D.md)

## Material targeting

`AnimatedSprite3D` auto-detects the type of Material it's writing into:

* **MaterialLite** — uses indexed texture slots (0..3). The Diffuse / Alpha / Emission Slot integer properties pick which slots get the animated texture. For atlas-mode clips, the per-frame UV scale/offset is also pushed into one of MaterialLite's two UV maps (configurable via *Atlas UV Map*).
* **Custom Material** — uses named shader parameters (`SetTextureParameter`). The Diffuse / Alpha / Emission Param Name strings pick which parameters to write. For atlas-mode clips, the UV rect is pushed as a vec4 parameter (default name `AnimUVRect`).

The same animation can drive multiple slots simultaneously — useful for setups where the diffuse and emission share the same animated mask.

---
### Play
Resume playback. Falls back to the Default Animation if none is selected.

Sig: `AnimatedSprite3D:Play()`

---
### Pause
Pause playback at the current frame.

Sig: `AnimatedSprite3D:Pause()`

---
### Stop
Stop playback and reset to frame 0. Cancels any pending `AnimateTo`.

Sig: `AnimatedSprite3D:Stop()`

---
### PlayAnimation
Switch to and start playing the named clip. Emits `OnAnimationStart`.

Sig: `AnimatedSprite3D:PlayAnimation(name)`
 - Arg: `string name` Animation name

---
### IsPlaying
Check if currently playing.

Sig: `playing = AnimatedSprite3D:IsPlaying()`
 - Ret: `boolean playing`

---
### SetFrame
Jump to a specific frame. Clamped to `[0, frameCount-1]`. Doesn't change play/pause state. Cancels any pending `AnimateTo`.

Sig: `AnimatedSprite3D:SetFrame(frameIndex)`
 - Arg: `integer frameIndex` Target frame

---
### AnimateTo
Play forward until reaching `targetFrame`, then optionally pause and call `onFinished`. If already at target, fires the callback immediately. Implicitly Plays.

Sig: `AnimatedSprite3D:AnimateTo(targetFrame, pauseOnFinished, onFinished)`
 - Arg: `integer targetFrame` Frame to stop at
 - Arg: `boolean pauseOnFinished` (optional, default `true`)
 - Arg: `function onFinished` (optional) Callback when target is reached

---
### AnimateToProgress
Animate to a target specified as 0..1 progress.

Sig: `AnimatedSprite3D:AnimateToProgress(progress, pauseOnFinished, onFinished)`
 - Arg: `number progress` Target progress in `[0, 1]`
 - Arg: `boolean pauseOnFinished` (optional, default `true`)
 - Arg: `function onFinished` (optional) Callback when target is reached

---
### CancelAnimateTo
Cancel a pending AnimateTo without affecting play/pause state.

Sig: `AnimatedSprite3D:CancelAnimateTo()`

---
### SetSpeed / GetSpeed
Get or set the playback speed multiplier.

Sig: `AnimatedSprite3D:SetSpeed(speed)`
 - Arg: `number speed` Speed multiplier

Sig: `speed = AnimatedSprite3D:GetSpeed()`
 - Ret: `number speed`

---
### SetAutoPlay / GetAutoPlay
Get or set the auto-play flag (plays the default animation on `Start()`).

Sig: `AnimatedSprite3D:SetAutoPlay(autoPlay)`
 - Arg: `boolean autoPlay`

Sig: `autoPlay = AnimatedSprite3D:GetAutoPlay()`
 - Ret: `boolean autoPlay`

---
### SetLoopOverride / GetLoopOverride
When on, forces all animations to loop regardless of the per-asset Loop flag.

Sig: `AnimatedSprite3D:SetLoopOverride(loop)`
 - Arg: `boolean loop`

Sig: `loop = AnimatedSprite3D:GetLoopOverride()`
 - Ret: `boolean loop`

---
### SetDefaultAnimation / GetDefaultAnimation
Get or set the name of the animation that plays on `Start()`.

Sig: `AnimatedSprite3D:SetDefaultAnimation(name)`
 - Arg: `string name`

Sig: `name = AnimatedSprite3D:GetDefaultAnimation()`
 - Ret: `string name`

---
### AddAnimation
Register a SpriteAnimation asset (or by path) so it can be played by name.

Sig (variant 1): `AnimatedSprite3D:AddAnimation(asset)`
 - Arg: `SpriteAnimation asset` SpriteAnimation asset

Sig (variant 2): `AnimatedSprite3D:AddAnimation(path)`
 - Arg: `string path` Path to a SpriteAnimation asset

---
### CreateAnimation
Create a script-built animation by name. Always Discrete mode.

Sig (variant 1): `AnimatedSprite3D:CreateAnimation(name)`
 - Arg: `string name`

Sig (variant 2): `AnimatedSprite3D:CreateAnimation(name, frames)`
 - Arg: `string name`
 - Arg: `table frames` Array of Texture assets

---
### AddImage
Append a frame to a script-built animation.

Sig (variant 1): `AnimatedSprite3D:AddImage(name, texture)`
 - Arg: `string name`
 - Arg: `Texture texture`

Sig (variant 2): `AnimatedSprite3D:AddImage(name, path)`
 - Arg: `string name`
 - Arg: `string path` Path to a Texture asset

---
### AddImages
Append multiple frames to a script-built animation.

Sig: `AnimatedSprite3D:AddImages(name, paths)`
 - Arg: `string name`
 - Arg: `table paths` Array of texture asset paths

---
### RemoveAnimation
Remove a registered animation by name.

Sig: `AnimatedSprite3D:RemoveAnimation(name)`
 - Arg: `string name`

---
### HasAnimation
Check if an animation is registered.

Sig: `has = AnimatedSprite3D:HasAnimation(name)`
 - Arg: `string name`
 - Ret: `boolean has`

---
### GetCurrentTexture
Get the current frame's texture (or atlas texture for atlas mode).

Sig: `tex = AnimatedSprite3D:GetCurrentTexture()`
 - Ret: `Texture tex`

---
### GetCurrentAnimationName
Get the currently-playing animation name.

Sig: `name = AnimatedSprite3D:GetCurrentAnimationName()`
 - Ret: `string name`

---
### GetCurrentFrameIndex
Get the current frame index.

Sig: `idx = AnimatedSprite3D:GetCurrentFrameIndex()`
 - Ret: `integer idx`

---
### GetProgress
Get smooth playback progress in `[0, 1]`. Sub-frame interpolated. Snaps to 1.0 at the end of a non-looping clip.

Sig: `p = AnimatedSprite3D:GetProgress()`
 - Ret: `number p`

---
### SetMaterial
Assign the target Material asset that the animation will drive.

Sig: `AnimatedSprite3D:SetMaterial(material)`
 - Arg: `Material material` Material or MaterialLite asset

---
### GetMaterial
Get the target Material.

Sig: `mat = AnimatedSprite3D:GetMaterial()`
 - Ret: `Material mat`

---
### SetAffectDiffuse / GetAffectDiffuse
Set/get whether the animation drives the diffuse (base color) texture slot.

Sig: `AnimatedSprite3D:SetAffectDiffuse(enable)`
 - Arg: `boolean enable`

Sig: `enable = AnimatedSprite3D:GetAffectDiffuse()`
 - Ret: `boolean enable`

---
### SetAffectAlpha / GetAffectAlpha
Set/get whether the animation drives the alpha mask texture slot.

Sig: `AnimatedSprite3D:SetAffectAlpha(enable)`
 - Arg: `boolean enable`

Sig: `enable = AnimatedSprite3D:GetAffectAlpha()`
 - Ret: `boolean enable`

---
### SetAffectEmission / GetAffectEmission
Set/get whether the animation drives the emission texture slot.

Sig: `AnimatedSprite3D:SetAffectEmission(enable)`
 - Arg: `boolean enable`

Sig: `enable = AnimatedSprite3D:GetAffectEmission()`
 - Ret: `boolean enable`

---

## Signals

| Signal | Parameters | Description |
|--------|------------|-------------|
| OnAnimationStart | name (String) | Fired when a new clip starts. |
| OnAnimationEnd | name (String) | Fired when a non-looping clip ends. |
| OnFrameChanged | frameIndex (Integer) | Fired on every frame change. |

## Note: Materials are shared assets

Two meshes referencing the same Material asset will both show the animation. To animate only one mesh:

* Duplicate the Material in the Asset Browser, assign the duplicate to the mesh you want animated, and target the duplicate from AnimatedSprite3D, OR
* Use a MaterialInstance with a per-instance texture override.

## Param-name / slot defaults

For **MaterialLite**, defaults are: Diffuse Slot = 0, Alpha Slot = 1, Emission Slot = 2, Atlas UV Map = 0. Override via the inspector to match your MaterialLite layout.

For **custom Materials**, defaults are: Diffuse Param = `DiffuseMap`, Alpha Param = `AlphaMap`, Emission Param = `EmissionMap`, UV Rect Param = `AnimUVRect`. Override to match your shader's parameter names.
