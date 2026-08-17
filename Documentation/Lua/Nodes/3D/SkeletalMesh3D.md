# SkeletalMesh3D

A node that renders a skeletal mesh. Multiple animations can be played at the same time.

Inheritance:
* [Node](../Node.md)
* [Node3D](Node3D.md)
* [Primitive3D](Primitive3D.md)
* [Mesh3D](Mesh3D.md)

---

### SetSkeletalMesh
Set the skeletal mesh asset.

Sig: `SkeletalMesh3D:SetSkeletalMesh(mesh)`
 - Arg: `SkeletalMesh mesh` The skeletal mesh asset
---
### GetSkeletalMesh
Get the skeletal mesh asset.

Sig: `mesh = SkeletalMesh3D:GetSkeletalMesh()`
 - Ret: `SkeletalMesh mesh` The skeletal mesh asset
---
### PlayAnimation
Play an animation contained within the skeletal mesh asset. Multiple animations can be played at the same time by placing them into different slots. There are 8 available animation slots (0 through 7). Animations in higher slots will be processed after earlier slots. Blending between animations can be accomplished by adjusting the weight parameter.

Note: You can update an animation (for instance, change its speed) by calling PlayAnimation() a second time with the same animation name and updated options.

Sig: `SkeletalMesh3D:PlayAnimation(animName, slot=-1, loop=false, speed=1, weight=1)`
 - Arg: `string animName` Name of the animation to play
 - Arg: `integer slot` Animation slot (0 - 7, or -1 to place at next available slot)
 - Arg: `boolean loop` Should the animation loop once reaching the end
 - Arg: `number speed` Speed multiplier. 1 = normal speed.
 - Arg: `number weight` How much weight should be given to the animation. (1 = default)

Old signature kept for backwards compatibility:

Sig: `SkeletalMesh3D:PlayAnimation(animName, loop=false, speed=1, weight=1, slot=-1)`

---
### StopAnimation
Stop a specific animation.

Sig: `SkeletalMesh3D:StopAnimation(animName, cancelQueued=false)`
 - Arg: `string animName` Name of the animation to cancel
 - Arg: `boolean cancelQueued` Whether to remove the animation from play queue if it hasn't been played yet.
---
### StopAllAnimations
Stop all animations.

Sig: `SkeletalMesh3D:StopAllAnimations(cancelQueued=false)`
 - Arg: `boolean cancelQueued` Whether to remove all queued animations as well.
---
### IsAnimationPlaying
Check if an animation is playing.

Sig: `playing = SkeletalMesh3D:IsAnimationPlaying(animName)`
 - Arg: `string animName` Name of animation
 - Ret: `boolean playing` Is animation playing
---
### QueueAnimation
Queue an animation to be played. A target animation name can be provided to wait on, otherwise the queued animation will be played after the animation in the highest slot finishes.

Sig: `SkeletalMesh3D:QueueAnimation(animName, dependentAnimName=nil, slot=-1, loop=false, speed=1, weight=1)`
 - Arg: `string animName` Name of animation to queue
 - Arg: `string dependentAnimName` Name of dependent anim to wait on
 - Arg: `integer slot` Animation slot (0 - 7, or -1 to place at next available slot)
 - Arg: `boolean loop` Whether to loop
 - Arg: `number speed` Speed multiplier
 - Arg: `number weight` Animation blending weight (0 - 1)

 Old signature kept for backwards compatibility:

 Sig: `SkeletalMesh3D:QueueAnimation(animName, loop, dependentAnimName=nil, speed=1, weight=1, slot=-1)`

---
### CancelQueuedAnimation
Cancel a queued animation by name.

Sig: `SkeletalMesh3D:CancelQueuedAnimation(animName)`
 - Arg: `string animName` Name of animation to cancel
---
### CancelAllQueuedAnimations
Cancel all queued animations.

Sig: `SkeletalMesh3D:CancelAllQueuedAnimations()`

---
### SetInheritPose
Whether this skeletal mesh node should inherit its pose from its parent node (assuming its parent node is also a SkeletalMesh3D node). This is useful for things like having clothing or accessories animate when the base body mesh moves on a character for instance.

Sig: `SkeletalMesh3D:SetInheritPose(inheritPose)`
 - Arg: `boolean inheritPose` Whether to inherit parent pose
---
### IsInheritPoseEnabled
Check if this node is set to inherit its parent's pose.

Sig: `inheritPose = SkeletalMesh3D:IsInheritPoseEnabled()`
 - Ret: `boolean inheritPose` Whether pose is inherited
---
### ResetAnimation
Reset all animations to their beginning frame.

Sig: `SkeletalMesh3D:ResetAnimation()`

---
### GetAnimationSpeed
Get this node's animation playback speed. This playback speed multiplier is applied to ALL animations uniformly.

Sig: `speed = SkeletalMesh3D:GetAnimationSpeed()`
 - Ret: `number speed` Animation playback speed
---
### SetAnimationSpeed
Set this node's animation playback speed. This playback speed multiplier is applied to ALL animations uniformly.

Sig: `SkeletalMesh3D:SetAnimationSpeed(speed)`
 - Arg: `number speed` Animation playback speed
---
### SetAnimationPaused
Use to pause and unpause this node from animating.

Sig: `SkeletalMesh3D:SetAnimationPaused(paused)`
 - Arg: `boolean paused` Whether to pause
---
### IsAnimationPaused
Check if this node's animation is paused. (for all animations).

Sig: `paused = SkeletalMesh3D:IsAnimationPaused()`
 - Ret: `boolean paused` Whether node animation is paused
---
### GetBonePosition
Get a bone's world space position.

Sig: `position = SkeletalMesh3D:GetBonePosition(boneName)`
 - Arg: `string boneName` Bone name to query
 - Ret: `Vector position` Bone world space position
---
### GetBoneRotation
Get a bone's (local?) rotation as euler angles. I don't think this works yet.
TODO: Fix GetBoneRotation().

Sig: `rotEuler = SkeletalMesh3D:GetBoneRotation(boneName)`
 - Arg: `string boneName` Bone name to query
 - Ret: `Vector rotEuler` Bone rotation as euler angles
---
### GetBoneScale
Get a bone's scale (in local space?). I don't think this works yet.
TODO: Fix GetBoneScale().

Sig: `scale = SkeletalMesh3D:GetBoneScale(boneName)`
 - Arg: `string boneName` Bone name to query
 - Ret: `Vector scale` Scale of bone
---
### SetBonePosition
Set a bone's world space position. Not yet implemented.
TODO: Implement SetBonePosition().

Sig: `SkeletalMesh3D:SetBonePosition(boneName, position)`
 - Arg: `string boneName` Name of bone to adjust
 - Arg: `Vector position` World space position to place bone
---
### SetBoneRotation
Set a bone's local space rotation from euler angles. Not yet implemented.
TODO: Implement SetBoneRotation().

Sig: `SkeletalMesh3D:SetBoneRotation(boneName, rotEuler)`
 - Arg: `string boneName` Name of bone to adjust
 - Arg: `Vector rotEuler` Rotation in euler angles
---
### SetBoneScale
Set a bone's local space scale. Not yet implemented.
TODO: Implement SetBoneScale().

Sig: `SkeletalMesh3D:SetBoneScale(boneName, scale)`
 - Arg: `string boneName` Name of bone to adjust
 - Arg: `Vector scale` Bone scale
---
### GetNumBones
Get the number of bones this mesh node is using.

Sig: `numBones = SkeletalMesh3D:GetNumBones()`
 - Ret: `integer numBones` The number of bones in the mesh asset
---
### GetNumSockets
Get the number of named sockets on the assigned SkeletalMesh asset.

Sig: `numSockets = SkeletalMesh3D:GetNumSockets()`
 - Ret: `integer numSockets` Number of sockets
---
### GetSocketName
Get the name of a socket by index.

Sig: `name = SkeletalMesh3D:GetSocketName(index)`
 - Arg: `integer index` 1-based socket index
 - Ret: `string name` Socket name, or nil if the index is out of range
---
### GetSocketPosition
Get the world-space position of a named socket. Falls back to treating the name as a bone name if no such socket exists.

Use this to spawn effects at a socket without reparenting anything to it — muzzle flashes, hit sparks, footstep dust.

Sig: `position = SkeletalMesh3D:GetSocketPosition(socketName)`
 - Arg: `string socketName` Name of the socket or bone
 - Ret: `Vector position` World-space position
---
### GetSocketRotation
Get the world-space euler rotation of a named socket.

Sig: `rotation = SkeletalMesh3D:GetSocketRotation(socketName)`
 - Arg: `string socketName` Name of the socket or bone
 - Ret: `Vector rotation` World-space euler rotation in degrees
---
### GetAnimationTime
Get the current playback time of an animation, in seconds. Returns -1 if that animation isn't playing.

Sig: `time = SkeletalMesh3D:GetAnimationTime(animName)`
 - Arg: `string animName` Name of the animation
 - Ret: `number time` Playback time in seconds, or -1
---
### GetAnimationNormalizedTime
Get how far through an animation playback currently is, from 0 to 1. Returns -1 if that animation isn't playing.

Sig: `t = SkeletalMesh3D:GetAnimationNormalizedTime(animName)`
 - Arg: `string animName` Name of the animation
 - Ret: `number t` Progress from 0 to 1, or -1
---
### SetAnimationTime
Scrub a playing animation to a specific time in seconds.

Sig: `SkeletalMesh3D:SetAnimationTime(animName, seconds)`
 - Arg: `string animName` Name of the animation
 - Arg: `number seconds` Time to scrub to
---
### SetAnimationNormalizedTime
Scrub a playing animation to a normalized (0 to 1) point in its timeline.

Sig: `SkeletalMesh3D:SetAnimationNormalizedTime(animName, t)`
 - Arg: `string animName` Name of the animation
 - Arg: `number t` Progress from 0 to 1
---
### AddAnimationNotify
Register a function to be called each time playback of an animation crosses a normalized point in its timeline. Returns a handle you can pass to `RemoveAnimationNotify`.

This is the runtime-authored counterpart to the `Event_*` anim-event system: it needs no DCC changes, so gameplay beats can be retimed from script. It handles looping clips, reverse playback, and frames long enough to step past the threshold.

```lua
function Player:Start()
    self.mesh:AddAnimationNotify("DrawSword", 0.4, function()
        self.sword:SetAttachSocket("WeaponHand_R")
    end)
end
```

**Important:** notifies only fire while the animation's time is advancing. Under the default `AnimationUpdateMode.OnlyUpdateWhenRendered` an off-screen character never advances its animation, so the notify never fires. For gameplay-critical notifies call `SetAnimationUpdateMode(AnimationUpdateMode.AlwaysUpdateTime)`. A warning is logged if you register a notify on a node that can't fire it.

Sig: `handle = SkeletalMesh3D:AddAnimationNotify(animName, normalizedTime, callback)`
 - Arg: `string animName` Name of the animation to watch
 - Arg: `number normalizedTime` Point in the clip from 0 to 1
 - Arg: `function callback` Function called when playback crosses that point
 - Ret: `integer handle` Handle for removal, or -1 if registration failed
---
### RemoveAnimationNotify
Remove a previously registered animation notify.

Sig: `SkeletalMesh3D:RemoveAnimationNotify(handle)`
 - Arg: `integer handle` Handle returned by AddAnimationNotify
---
### ClearAnimationNotifies
Remove all animation notifies, or just those registered for one animation.

Sig: `SkeletalMesh3D:ClearAnimationNotifies(animName)`
 - Arg: `string animName` Optional. If omitted, every notify on this node is removed.
---
### SetAnimEventHandler
Set an animation event handler. An animation event can be setup by creating a bone with the name "Event_MyAnimEvent". When any animation event is triggered on this node, the given function will be called so that you can handle it.

Events will be triggered wherever there is a keyframe on the timeline (I don't think it matters if it's position, rotation, or scale). Make sure your asset is not using Sampled frames when exporting the animation.

Sig: `SkeletalMesh3D:SetAnimEventHandler(handlerFunc)`
 - Arg: `function handlerFunc` Anim event function handler that will be called when an anim event is triggered/played.
---
### SetBoundsRadiusOverride
Override the bounds radius. Because animations can cause vertices to extend past the default pose bounds, this function lets you override the radius to avoid erroneous frustum culling. Setting the bounds radius to 0 will use the mesh default radius.

Sig: `SkeletalMesh3D:SetBoundsRadiusOverride(radius)`
 - Arg: `number radius` Override radius
---
### GetBoundsRadiusOverride
Get the overridden bounds radius.

Sig: `radius = SkeletalMesh3D:GetBoundsRadiusOverride()`
 - Ret: `number radius` Override radius
---
### GetNumMaterialSlots
Number of per-section material slots on the assigned mesh. Equals `SkeletalMesh:GetNumSections()` when a mesh is assigned, or 1 (legacy single-material fallback) for older meshes.

Sig: `count = SkeletalMesh3D:GetNumMaterialSlots()`
 - Ret: `integer count` Number of slots
---
### GetMaterialSlot
Resolve the material that will actually render the given section. Walks the lookup chain: component override → section's own material → legacy `mMaterialOverride` → asset-default `mMaterial` → renderer default. Accepts either a slot index or a section name.

Sig: `material = SkeletalMesh3D:GetMaterialSlot(slotOrName)`
 - Arg: `integer|string slotOrName` Slot index (Lua 1-indexed) or section name
 - Ret: `Material material` Resolved material (never `nil` if a mesh is assigned)
---
### SetMaterialSlot
Override one section's material on this instance only. Doesn't dirty the SkeletalMesh asset — the override lives on the node. Pass `nil` to clear the override and fall back through the lookup chain.

Sig: `SkeletalMesh3D:SetMaterialSlot(slotOrName, material)`
 - Arg: `integer|string slotOrName` Slot index (Lua 1-indexed) or section name
 - Arg: `Material material` New material (or `nil` to clear)
---
### FindMaterialSlot
Look up a section's slot index by name.

Sig: `slot = SkeletalMesh3D:FindMaterialSlot(name)`
 - Arg: `string name` Section name
 - Ret: `integer slot` Slot index (Lua 1-indexed), or 0 if not found
---

## External animation clips

`SkeletalMesh3D` accepts a list of [`SkeletalAnimationAsset`](../../Assets/SkeletalAnimationAsset.md) references on its **Animation Assets** vector property (editable in the inspector or via scripts attached to scenes loaded from disk). At runtime, `PlayAnimation(name)` resolves names in this order:

1. Animations embedded in the assigned `SkeletalMesh`
2. The mesh's legacy `mAnimationLookupMesh` chain
3. External `Animation Assets` on this node — channel bone names are resolved into the target mesh's bone indices once per `(asset, mesh)` pair and cached

So a retargeted Mixamo clip on a custom character "just works" with `node:PlayAnimation("Walk", true)` — same play handle as if the clip were embedded.

The cache auto-invalidates when `SetSkeletalMesh` is called or when the Animation Assets property changes. There's no Lua API for the asset list itself yet (edit it through the inspector); the runtime resolution path is what scripts see.
