# Bone Sockets

Attach props to a character's skeleton — a sword in the hand, a scabbard on the back, a muzzle flash at a gun
barrel — and move them between attachment points from script.

A **socket** is a named point on a rig: a bone plus a constant position/rotation/scale offset, stored on the
`SkeletalMesh` asset. You author the placement once and every prop attached to that socket lands identically.

---

## Quick start

### 1. Attach a prop to a bone

Drag your prop (a `StaticMesh3D` with the sword mesh) under the character's `SkeletalMesh3D` in the outliner.
Then right-click the **skeletal mesh** node → **Attach Selected To Bone** → pick `hand_r` from the Bones
submenu (or type the name).

The sword now follows the hand through every animation.

### 2. Place it

Select the sword and use the translate/rotate gizmo to nudge it into the palm. Its Position / Rotation / Scale
in the Properties panel are now relative to the bone.

### 3. Promote the placement to a socket

Right-click the sword → **Create Socket From Selection**. This bakes the offset you just dialled in onto the
mesh asset as a named socket, re-points the sword at it, and zeroes the sword's own transform.

Now any other prop can use the same socket and land in exactly the same place. Save the mesh asset.

### 4. Attach other props to the socket

Select a prop parented under the character and pick the socket from the **Attach Socket** dropdown in the
Properties panel. Sockets are listed first, then every bone on the rig.

---

## Authoring sockets directly

Select the `SkeletalMesh` asset in the asset browser. The inspector has a **Sockets** section:

| Field | Meaning |
|---|---|
| Name | What scripts and the Attach Socket dropdown refer to. Must be unique; duplicates get a `_1` suffix. |
| Bone | Which bone the socket rides on. |
| Position / Rotation / Scale | The constant offset from that bone. Rotation is euler degrees. |

Sockets draw as red/green/blue axes in the viewport whenever a `SkeletalMesh3D` using that mesh is selected,
so you can see where they actually are.

A socket whose bone doesn't exist on the mesh is flagged in red in the inspector.

---

## Scripting

### Draw and sheathe

```lua
function Player:Start()
    -- Initial attach. Use AttachToSocket when the parent changes.
    self.sword:AttachToSocket(self.mesh, "Sheath_Back")
end

function Player:Draw()
    -- Moving between sockets on the SAME parent: use SetAttachSocket. It's a
    -- cheap in-place change, where AttachToSocket would reparent the node.
    self.sword:SetAttachSocket("WeaponHand_R")
end

function Player:Sheathe()
    self.sword:SetAttachSocket("Sheath_Back")
end
```

### Driving the swap from the animation

Rather than swapping the instant the animation starts, fire it partway through — when the hand actually
reaches the hilt:

```lua
function Player:Start()
    self.sword:AttachToSocket(self.mesh, "Sheath_Back")

    -- Gameplay notifies need time to keep advancing even when off-screen.
    self.mesh:SetAnimationUpdateMode(AnimationUpdateMode.AlwaysUpdateTime)

    self.mesh:AddAnimationNotify("DrawSword", 0.4, function()
        self.sword:SetAttachSocket("WeaponHand_R")
    end)

    self.mesh:AddAnimationNotify("SheatheSword", 0.6, function()
        self.sword:SetAttachSocket("Sheath_Back")
    end)
end

function Player:OnDrawPressed()
    self.mesh:PlayAnimation("DrawSword", false)
end
```

`AddAnimationNotify` fires once each time playback crosses that normalized point. It handles looping clips,
reverse playback (`SetAnimationSpeed(-1)`), and frames long enough to step over the threshold.

If you'd rather poll:

```lua
function Player:Tick(deltaTime)
    local t = self.mesh:GetAnimationNormalizedTime("DrawSword")
    if t >= 0.4 and not self.drawn then
        self.sword:SetAttachSocket("WeaponHand_R")
        self.drawn = true
    end
end
```

### Effects at a socket without attaching

`GetSocketPosition` / `GetSocketRotation` return world space, so you can spawn something at a socket without
parenting it there:

```lua
local pos = self.mesh:GetSocketPosition("Muzzle")
local rot = self.mesh:GetSocketRotation("Muzzle")
SpawnParticle(self.muzzleFlash, pos, rot)
```

Both accept a bare bone name too, so `GetSocketPosition("hand_r")` works without authoring a socket first.

---

## Gotchas

- **Off-screen characters don't fire notifies.** The default `AnimationUpdateMode.OnlyUpdateWhenRendered`
  skips the animation update entirely for a culled mesh, so its time never advances. An off-screen character
  will play its draw animation and the sword will stay on its back. Call
  `SetAnimationUpdateMode(AnimationUpdateMode.AlwaysUpdateTime)` for anything gameplay-critical — a warning is
  logged if you register a notify on a node that can't fire it.
- **`SetAttachSocket` vs `AttachToSocket`.** `AttachToSocket` reparents, which removes and re-adds the node to
  its parent's child list. For draw/sheathe on the same character, `SetAttachSocket` is the right call.
- **Non-uniform scale is dropped when a node has children.** `Node3D` forces uniform scale (using the X
  component) on any node that has children, to stop shear leaking down the hierarchy. A sword with a child
  trail effect will have its Y/Z scale ignored. Scale the mesh asset instead, or put the offset on the socket.
- **Sockets are keyed by bone name, not index.** That's deliberate — reimporting a mesh with a different bone
  order won't silently relocate your props. If a reimport renames or removes a bone, the affected sockets are
  logged as warnings.
- **Reimport keeps sockets.** They're editor-authored data with no counterpart in the source file, so they're
  carried across from the old asset rather than wiped.
- **Anim events vs notifies.** The older `Event_*` system needs a bone named `Event_Foo` authored in Blender
  and the rig re-exported, and only supports one handler per node. Notifies are registered at runtime and
  support any number of listeners. Both work; `SetAnimEventHandler` is unchanged.

---

## Related

- [Skeletal Animation](SkeletalAnimation.md) — multi-section meshes, animation assets, retargeting
- [Bone Mask](BoneMask.md) — per-bone masks and layered playback
- Lua reference: `Documentation/Lua/Nodes/3D/SkeletalMesh3D.md`, `Documentation/Lua/Nodes/3D/Node3D.md`
