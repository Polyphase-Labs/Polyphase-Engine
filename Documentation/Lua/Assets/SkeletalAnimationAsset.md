# SkeletalAnimationAsset

A reusable bone-animation clip, decoupled from any particular mesh. Channels are keyed by bone NAME so the same asset can bind to multiple compatible rigs. Created by extracting clips from a `SkeletalMesh` (right-click → Extract Animations…), importing animation-only files (Import Asset → Import Animations), or as the output of a retarget bake.

See [Skeletal Animation Pipeline](../../Development/SkeletalAnimation.md) for the authoring workflow.

Inheritance:
* [Asset](Asset.md)

---
### GetClipName
The display name used by `SkeletalMesh3D:PlayAnimation`. Defaults to the source clip name; preserved across retarget bakes so the same play handle works on both source and target rigs.

Sig: `name = SkeletalAnimationAsset:GetClipName()`
 - Ret: `string name` Clip name
---
### GetDuration
Duration of the clip in **ticks** (raw timeline units — divide by `GetTicksPerSecond` for seconds).

Sig: `ticks = SkeletalAnimationAsset:GetDuration()`
 - Ret: `number ticks` Duration in ticks
---
### GetDurationSeconds
Duration of the clip in seconds.

Sig: `seconds = SkeletalAnimationAsset:GetDurationSeconds()`
 - Ret: `number seconds` Duration in seconds
---
### GetTicksPerSecond
Tick-rate metadata. GLB exports out of Blender hard-code 1000 to work around Assimp's broken tickrate handling.

Sig: `tps = SkeletalAnimationAsset:GetTicksPerSecond()`
 - Ret: `number tps` Ticks per second
---
### GetSourceRigName
Name of the rig this clip was authored against. Set at extraction / import / retarget time. Used by tooling for diagnostics; runtime doesn't read it.

Sig: `name = SkeletalAnimationAsset:GetSourceRigName()`
 - Ret: `string name` Source rig name
---
### GetNumChannels
Number of bone channels the clip animates. Bones not in this list keep their bind-pose local transform at runtime.

Sig: `num = SkeletalAnimationAsset:GetNumChannels()`
 - Ret: `integer num` Number of channels
---
### GetChannelBoneName
Get the bone name a channel is keyed on. Combined with the target mesh's bone list, this is what lets the runtime bind a clip to a particular `SkeletalMesh3D`.

Sig: `name = SkeletalAnimationAsset:GetChannelBoneName(index)`
 - Arg: `integer index` Channel index (Lua 1-indexed)
 - Ret: `string name` Bone name (`nil` if index out of range)
---
