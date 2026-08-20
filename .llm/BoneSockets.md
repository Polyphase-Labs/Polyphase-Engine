# Bone Sockets

Named attachment points on a skeleton (bone + constant offset), plus the runtime API to move props between
them and to drive those moves from animation playback position.

Designer guide: `Documentation/Development/BoneSockets.md`.

---

## 1. Files

| File | Purpose |
|---|---|
| `Engine/Source/Engine/Assets/SkeletalMesh.h/.cpp` | `MeshSocket` struct + `mSockets` + accessors (`GetNumSockets`, `GetSocket`, `GetSocketMutable`, `FindSocketIndex`, `AddSocket`, `RemoveSocket`, `GetSocketLocalMatrix`). Versioned socket block in `LoadStream`/`SaveStream`. |
| `Engine/Source/Engine/Nodes/3D/Node3d.h/.cpp` | `mAttachSocket` (serialized name) + lazily-resolved `mParentBoneIndex`/`mAttachSocketIndex` cache. `AttachToSocket`, `SetAttachSocket`, `GetAttachSocket`, `ResolveAttachSocket`, `InvalidateAttachSocket`. Socket offset applied in `GetParentTransform`. `DrawCustomProperty` socket dropdown. Socket axes in `OnDrawGizmosSelected`. **Three transform bug fixes — see §5.** |
| `Engine/Source/Engine/Nodes/3D/SkeletalMesh3d.h/.cpp` | `GetBoneWorldMatrix` (consolidates three duplicated bone-space derivations). Socket world queries. Animation time getters/setters. `AnimationNotify` + `AddAnimationNotify`/`RemoveAnimationNotify`/`ClearAnimationNotifies`/`DetectTriggeredNotifies`/`DispatchPendingNotifies`. |
| `Engine/Source/Engine/Assets/Scene.h/.cpp` | `SceneNodeDef::mParentBone` widened `int8_t` → `int32_t` behind the version gate. Instantiate prefers the `Attach Socket` property over the legacy index. |
| `Engine/Source/Engine/Asset.h` | `ASSET_VERSION_BONE_SOCKETS = 39`; `ASSET_VERSION_CURRENT` bumped to 39. |
| `Engine/Source/Editor/EditorImgui.cpp` | Socket rows in the SkeletalMesh asset inspector. "Create Socket From Selection" on the node context menu. |
| `Engine/Source/Editor/ActionManager.cpp` | Carries sockets across a mesh reimport (they have no source-file counterpart, so they'd otherwise be wiped). |
| `Engine/Source/LuaBindings/Node3d_Lua.h/.cpp` | `AttachToSocket`, `SetAttachSocket`, `GetAttachSocket`. |
| `Engine/Source/LuaBindings/SkeletalMesh3d_Lua.h/.cpp` | Socket queries, animation time, notify registration. |

No new source files, so no vcxproj / filters / `Makefile_Linux` changes and no `FORCE_LINK_CALL`.

## 2. Asset versions

`ASSET_VERSION_BONE_SOCKETS = 39` gates two independent things:

- The socket block in `SkeletalMesh::LoadStream` (mirrors the section block's shape). Meshes below v39 load
  with zero sockets; `SaveStream` always writes the block, so resaving promotes them.
- The width of `SceneNodeDef::mParentBone` in `Scene::LoadStream` — `ReadInt32` at v39+, `ReadInt8` below.

## 3. Data model

```cpp
struct MeshSocket
{
    std::string mName;      // "WeaponHand_R"
    std::string mBoneName;  // "hand_r"
    glm::vec3 mPosition, mRotation /*euler degrees*/, mScale;
};
```

Sockets live on the `SkeletalMesh` **asset** (authored once per rig); `Node3D` stores only the socket **name**.
Name-keyed rather than index-keyed for the same reason as `BoneMaskAsset` — a reimport that reorders bones
must not silently relocate props.

`Node3D::mAttachSocket` names either a socket or a bare bone. Sockets win on a name collision.

## 4. Resolution and the transform chain

`mAttachSocket` is authoritative and serialized (as a `DatumType::String` property, which *is* the node
serialization path — `Node::SaveStream` is an empty hook). `mParentBoneIndex` and `mAttachSocketIndex` are a
lazily-rebuilt cache:

```
ResolveAttachSocket()   // no-op once mAttachResolved
  socket name -> mAttachSocketIndex, and its bone -> mParentBoneIndex
  else bone name       -> mParentBoneIndex
```

Invalidated by `SetAttachSocket`, `SetParent`, and `Attach`. Lazy resolution is what makes scene load work:
`Scene::Instantiate` applies properties *before* adding the child, and `Attach()` clears the attachment, so an
eagerly-resolved index would be destroyed. It also tolerates the parent's mesh not being assigned yet.

The whole attachment funnels through one function:

```cpp
// Node3D::GetParentTransform()
transform = parentWorld;
if (mParentBoneIndex != -1)
    transform = transform * boneMatrix * bone.mInvOffsetMatrix;
    if (socket) transform = transform * mesh->GetSocketLocalMatrix(socketIndex);
```

Everything downstream — `UpdateTransform`, `SetWorldPosition`, `SetWorldScale`, dirty propagation,
`UpdateAttachedChildren` — inherits socket support for free because it all reads this.

## 5. Transform bugs fixed here (the reason the feature was unusable)

**`SetWorldScale` ignored the bone.** The gizmo writes through `SetTransform(worldMatrix)`, which back-solves
position, scale *and* rotation on every drag frame regardless of gizmo mode. `SetWorldScale` divided the
extracted world scale by only the *parent node's* scale, never the bone term. The bone matrix carries
`mInvRootTransform` — the FBX/glTF unit-conversion scale, 100× on Mixamo rigs — so each frame stored
`S_bone · S_local` as the new local scale and the next frame extracted `S_bone² · S_local`. **Every gizmo
frame multiplied the scale by the bone factor**, and because `SetTransform` calls all three setters it fired
on translate too. Fixed by using `Maths::ExtractScale(GetParentTransform())`, which is provably identical for
non-attached nodes (`GetParentTransform()` is then just the parent's world matrix).

**`SetWorldRotation` double-counted the parent rotation.** It multiplied `parent->GetWorldRotationQuat()` by
`GetBoneRotationQuat()`, but that getter already bakes in the mesh node's own world matrix. Invisible at
identity rotation, wrong as soon as the character turns. Now derives from `GetParentTransform()`.

**`GetParentTransform` returned identity on a subclass.** The `else if (GetType() == SkeletalMesh3D::
GetStaticType())` was an exact-type compare with no `else`, so any `SkeletalMesh3D` subclass — or a stale bone
index — teleported the node to the world origin. Now seeds from the parent transform and uses
`As<SkeletalMesh3D>()`.

Also: `SkeletalMesh3D::GetBoneTransform(int32_t)` returned an uninitialised `glm::mat4` on an out-of-range
index.

## 6. Animation notifies

```cpp
struct AnimationNotify { std::string mAnimName; float mNormalizedTime; ScriptFunc mCallback; int32_t mHandle; };
```

`DetectTriggeredNotifies` runs in `UpdateAnimation` per slot, immediately after the loop-wrap clamp. Two
deliberate differences from the adjacent legacy `DetectTriggeredAnimEvents`:

- **Placed outside the `if (weight > 0.0f)` guard.** The legacy call sits inside it, so a slot still fading in
  emits nothing — acceptable for a footstep, wrong for "put the sword in the hand".
- **Sign-agnostic interval test**, not four sign-branched clauses. The legacy predicate requires a strict
  speed sign, which is why timeline-scrubbed playback (`AnimationTrack` sets `mSpeed = 0` and drives `mTime`
  directly) emits no events at all. Testing `[min(prev,cur), max(prev,cur)]` also catches a frame spike that
  steps past the threshold.
- **Loop wrap handled properly.** Detected as `|curNorm - prevNorm| > 0.5` on a looping slot, then the covered
  interval is the two *outer* segments. The legacy clause fires only keys above `prevTickTime` and silently
  drops every key near t=0 on each loop iteration.

Dispatch is deferred to the end of `UpdateAnimation`, after `mHasAnimatedThisFrame` is latched, via a
`mPendingNotifies` **member** vector — not a function-local static like the legacy `sAnimEvents`, because
`UpdateAnimation` re-enters itself through the inherit-pose path and a callback can re-enter too.
`DispatchPendingNotifies` swaps the list out before firing so a callback may freely mutate the notify list or
`mActiveAnimations`.

## 7. Gotchas

- **`OnlyUpdateWhenRendered` silently disables notifies.** It's the default `AnimationUpdateMode`, and it skips
  `UpdateAnimation` entirely for a culled mesh, so time never advances. `WarnIfNotifiesCantFire` logs once per
  node when a notify is registered under that mode.
- **`SetAttachSocket` vs `AttachToSocket`.** `Attach()` does a full `RemoveChild`/`AddChild` round trip, which
  reorders siblings. Swapping sockets on the same parent must go through `SetAttachSocket`.
- **Uniform-scale clamp.** `UpdateTransform` overwrites scale with `vec3(mScale.x)` when the node has children
  (a deliberate anti-shear guard, left alone here). A socketed prop with a child effect loses its Y/Z scale.
- **Reimport.** `ActionManager` copies sockets from the old asset to the new one because reimport constructs a
  fresh asset and purges the old. Sockets whose bone vanished are logged.
- **Per-backend work: none.** Sockets and attachment are entirely upstream of `mBoneMatrices`, which every
  backend (Vulkan / GX / C3D / PSP / addon targets) consumes unchanged.

## 8. Related

- `.llm/SkeletalAnimation.md` — multi-section meshes, animation assets, retargeting
- `.llm/BoneMask.md` — the name-keyed resolution pattern this follows
- `.llm/NodeSystem.md` — Node3D transform pipeline
