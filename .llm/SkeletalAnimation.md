# Skeletal Animation Subsystem

Three layered systems, usable independently or together:

1. **Multi-section `SkeletalMesh`** — per-section materials on a shared bone/vertex/index buffer.
2. **`SkeletalAnimationAsset`** — bone-name-keyed clip asset, decoupled from any mesh.
3. **`HumanoidAvatarAsset` + retargeting** — Mecanim-style humanoid avatar + bake-time tier-1 (name-remap) and tier-2 (world-space, reference-pose-aware) retarget.

Designer guide: `Documentation/Development/SkeletalAnimation.md`. Design doc: `.dev/animation/skeletalanimationmultimesh.md` (the 5-PR plan all of this implements).

---

## 1. Files

| File | Purpose |
|---|---|
| `Engine/Source/Engine/Assets/SkeletalMesh.h/.cpp` | `SkeletalMeshSection` struct + `mSections` + accessors. `CreateCombined` rewritten to build bone-superset skeleton + emit one section per source primitive. Versioned `LoadStream`/`SaveStream` with legacy fallback. |
| `Engine/Source/Engine/Assets/SkeletalAnimationAsset.h/.cpp` | `SkeletalAnimationAsset`, `SkeletalAnimationChannel` (bone-name keyed), `CopyFromEmbedded`, `ParseAnimationsFromFile` (animation-only Assimp parser), `Retarget` (both tiers). |
| `Engine/Source/Engine/Assets/HumanoidAvatarAsset.h/.cpp` | `HumanoidBone` enum (21 slots), 21-slot bone-name table, `AutoMap` alias scanner, `Validate`, `GetReferenceLocalBind`. |
| `Engine/Source/Engine/Nodes/3D/SkeletalMesh3d.h/.cpp` | `mSectionMaterialOverrides`, `Get/SetMaterialSlot`, `FindMaterialSlot`, `mAnimationAssets` vector property, `FindAnimation` + lazy `mBoundExternalAnims` cache with empty-track backfill. |
| `Engine/Source/Engine/AssetRef.h` | `SkeletalAnimationRef`, `HumanoidAvatarRef` typedefs. |
| `Engine/Source/Engine/Asset.h` | `ASSET_VERSION_SKELETAL_MESH_SECTIONS = 36`; `ASSET_VERSION_CURRENT` bumped to 36. |
| `Engine/Source/Graphics/Vulkan/VulkanUtils.cpp` | `DrawSkeletalMeshComp` loops sections. Shadow pass stays single full-mesh depth draw. |
| `Engine/Source/Graphics/GX/Graphics_GX.cpp` | `GFX_DrawSkeletalMeshComp` loops sections — per-section `BindMaterial` + `GX_Begin`/`GX_End` over the section's index range. |
| `Engine/Source/Graphics/C3D/Graphics_C3D.cpp` | `GFX_DrawSkeletalMeshComp` loops sections. Ordering is critical: `BindMaterial` → matrix uniforms → UV uniforms → `C3D_DrawElements`. Inverting that order leaves the first section's draw on stale GPU state and section 0 renders black. |
| `Engine/Source/Editor/ActionManager.h/.cpp` | Three modal pipelines: `BeginExtractSkeletalAnimations`/`ExtractSkeletalAnimations`/`DrawExtractSkeletalAnimationsModal`, `BeginImportAnimations`/`ImportAnimations`/`DrawImportAnimationsModal`, `BeginRetargetAnimation`/`RetargetAnimation`/`DrawRetargetAnimationModal`. |
| `Engine/Source/Editor/EditorImgui.cpp` | Right-click entries on SkeletalMesh stubs (Extract Animations…) and SkeletalAnimationAsset stubs (Retarget…). Asset-browser "Import Animations" entry. Inspector panels for SkeletalMesh sections, SkeletalAnimationAsset channels, HumanoidAvatarAsset slots. Create Asset → Humanoid Avatar menu entry. Three `Draw*Modal` hooks at end-of-frame. |
| `Engine/Source/LuaBindings/SkeletalMesh_Lua.h/.cpp` | Section accessors (`GetNumSections`, `GetSectionName`, `Get/SetSectionMaterial`, `FindSectionIndex`). |
| `Engine/Source/LuaBindings/SkeletalMesh3d_Lua.h/.cpp` | Material slot accessors (`GetNumMaterialSlots`, `Get/SetMaterialSlot`, `FindMaterialSlot`). Slot args accept integer index or string section name. |
| `Engine/Source/LuaBindings/SkeletalAnimationAsset_Lua.h/.cpp` | Read-only clip accessors (clip name, duration, ticks, source rig, channel introspection). |
| `Engine/Source/LuaBindings/LuaBindings.cpp` | `SkeletalAnimationAsset_Lua::Bind()` registered alongside `SkeletalMesh_Lua`. |
| `Engine/Source/Engine/Engine.cpp` | `FORCE_LINK_CALL(SkeletalAnimationAsset)`, `FORCE_LINK_CALL(HumanoidAvatarAsset)`. |

## 2. Asset versions

`ASSET_VERSION_SKELETAL_MESH_SECTIONS = 36` gates the section block in `SkeletalMesh::LoadStream`/`SaveStream`. Older assets (v<36) load with one implicit `"Default"` section spanning the whole index range, materialed by their `mMaterial`. The save path always writes the section block, so resaving an old asset promotes it. `ASSET_VERSION_CURRENT` bumped to 36.

`SkeletalAnimationAsset` and `HumanoidAvatarAsset` use unconditional `Asset::SaveStream`/`LoadStream` (no version-gated fields yet); their on-disk layout is the same for every engine version that ships them.

## 3. Multi-section data model

```cpp
struct SkeletalMeshSection
{
    std::string mName;
    uint32_t mFirstIndex = 0;
    uint32_t mIndexCount = 0;
    uint32_t mBaseVertex = 0;   // present but not currently used by backends — indices are written absolute
    uint32_t mVertexCount = 0;
    MaterialRef mMaterial;
};
```

Vertices and indices live on the existing `mVertices` / `mIndices` arrays — sections are just **windows** into them. `CreateCombined` writes vertex indices with `+ vOffset` baked in so each section's indices are absolute (not relative to `mBaseVertex`), which keeps the backend draw path one-call-per-section without offset arithmetic.

Material resolution chain (5 fallbacks deep) is documented inline in `SkeletalMesh3D::GetMaterialSlot`.

## 4. `CreateCombined` bone-superset walk

Replaces the prior "first skinned primitive is canonical, drop weights for missing bones" path. Algorithm:

1. Iterate every render mesh. Collect bone-name set from every skinned primitive. Register `Event_*` bones into a separate list (parsed during `SetupAnimations`, stripped after).
2. Cache offset matrices keyed by bone name from any primitive that owns them.
3. DFS-walk `scene.mRootNode`. Emit a `Bone` for every node whose name is in the gathered set, preserving parent indices. DFS order guarantees parent-before-child, which `FinalizeBoneTransforms` / `InitBindPose` depend on.
4. For each render mesh, build per-vertex bone-influence arrays mapping its local aiBone names back into the now-merged skeleton via `FindBoneIndex`.
5. Append vertices/indices into one buffer; emit one `SkeletalMeshSection` per primitive.

Fixes a long-standing import bug where weights on accessories whose bones only existed on a secondary primitive were silently dropped at import. The new walk treats every skinned primitive's bones as first-class.

## 5. `SkeletalAnimationAsset`

```cpp
struct SkeletalAnimationChannel
{
    std::string mBoneName;            // canonical key — runtime resolves to target mesh bone index
    int32_t     mSourceBoneIndex;     // metadata only; index into mSourceBoneNames
    std::vector<PositionKey> mPositionKeys;
    std::vector<RotationKey> mRotationKeys;
    std::vector<ScaleKey>    mScaleKeys;
};

class SkeletalAnimationAsset : public Asset
{
    std::string mClipName;            // what PlayAnimation() matches against
    float       mDuration, mTicksPerSecond;
    std::vector<SkeletalAnimationChannel> mChannels;
    std::vector<AnimEventTrack>           mEventTracks;

    // Source-rig metadata for retarget consumers.
    std::string              mSourceRigName;
    std::vector<std::string> mSourceBoneNames;
    std::vector<int32_t>     mSourceParentIndices;
    std::vector<glm::mat4>   mSourceBindPose;
};
```

Two construction paths:

- `CopyFromEmbedded(const Animation&, const SkeletalMesh*)` — translates an embedded `Animation` (bone-index-keyed `Channel`s) into name-keyed channels, capturing source-rig metadata from the mesh. Used by the Extract Animations modal.
- `ParseAnimationsFromFile(path, outAssets, outNames)` — static helper. Runs Assimp without requiring a render mesh; walks `aiNode` hierarchy for source-bone metadata. Mirrors the embedded-mesh importer's `Event_*` track convention and redundant-key stripping. Used by the Import Animations modal.

## 6. `SkeletalMesh3D::FindAnimation` resolution + bind cache

```
SkeletalMesh3D::FindAnimation(name) → const Animation*
  1. mesh->GetAnimation(name)                 // embedded; walks mAnimationLookupMesh chain too
  2. lazy-rebuild mBoundExternalAnims if !mAnimBindingsValid
  3. linear search mBoundExternalAnims for matching mName
```

`mBoundExternalAnims` is a `std::vector<Animation>` (the runtime `Animation` struct with bone-index-keyed `Channel`s). Built once per `(asset list, target mesh)` pair from `mAnimationAssets`. For each external `SkeletalAnimationAsset`:

- Resolve each `SkeletalAnimationChannel::mBoneName` to a target-mesh bone index via `mesh->FindBoneIndex(name)`.
- Channels whose source bone isn't on the target rig are silently dropped (filter — not an error).
- **Defensive backfill**: any channel with empty `mPositionKeys` / `mRotationKeys` / `mScaleKeys` gets a single bind-pose key inserted at t=0. The runtime's `Find{Position,Rotation,Scale}Index` helpers `OCT_ASSERT(size > 0)`; without backfill, malformed retargeted assets (older bakes, hand-edited assets) would crash playback.

Cache invalidated by `InvalidateAnimationBindings()`, which is called from `SetSkeletalMesh` and from `HandlePropChange` on the `"Animation Assets"` property.

## 7. `HumanoidAvatarAsset`

```cpp
enum class HumanoidBone : uint8_t
{
    Hips, Spine, Chest, Neck, Head,
    LeftShoulder,  LeftUpperArm,  LeftLowerArm,  LeftHand,
    RightShoulder, RightUpperArm, RightLowerArm, RightHand,
    LeftUpperLeg,  LeftLowerLeg,  LeftFoot,  LeftToes,
    RightUpperLeg, RightLowerLeg, RightFoot, RightToes,
    Count
};

class HumanoidAvatarAsset
{
    SkeletalMeshRef mReferenceMesh;
    std::vector<std::string> mBoneNames;   // size == HumanoidBone::Count
};
```

**Auto-mapping** (`AutoMap(overwriteAll)`): normalizes every reference-mesh bone name (lowercase, strip `[' '_-.:]`, strip common rig prefixes `mixamorig` / `rig` / `armature` / `def` / `mch` / `ctrl` / `skl` / `bone`), then scans an alias table per slot for the first match. The alias table covers:

- Mixamo: `leftarm`, `leftupperarm`, etc.
- ARP / Auto-Rig Pro: `c_arm_fk.l` → `arml`
- Rigify: `DEF-upper_arm.L` → `upperarml`
- Blender `.L`/`.R` suffix style: `UpperArm.L` → `upperarml`
- Common variants

First match wins per slot; existing non-empty mappings are kept unless `overwriteAll=true`.

**Validation** (`Validate(outMissing, outUnknownBones)`): empty slots are recorded as missing (not errors — optional slots like Toes are commonly absent); slots whose name doesn't resolve to a bone on the reference mesh are recorded as unknown (red, surfaced in the inspector).

**Reference local bind** (`GetReferenceLocalBind(slot)`): the bone's bind-pose local-to-parent transform, derived from `mesh->GetBindPoseMatrix(idx)` and the parent's bind matrix. Identity if the slot is unmapped or there's no reference mesh.

## 8. Retarget pipeline

`SkeletalAnimationAsset::Retarget(srcClip, srcAvatar, dstAvatar, mode, outDiag)` — static factory returning a populated `SkeletalAnimationAsset` (caller decides whether to register). Two modes:

### Tier 1 — `RetargetMode::NameRemap`

Per humanoid slot, look up the source clip's channel by `srcAvatar.GetBoneName(slot)`. Copy its `mPositionKeys` / `mRotationKeys` / `mScaleKeys` verbatim into a new channel keyed by `dstAvatar.GetBoneName(slot)`. O(slots × keys). Works for rigs with identical proportions + bone-axis conventions.

### Tier 2 — `RetargetMode::ReferencePose` (world-space)

Requires both avatars to have a Reference Mesh (else falls back to NameRemap with a diagnostic). Algorithm:

1. **Precompute**:
   - `srcChanByBone[srcMesh bone idx]` — pointer to source channel for that bone, or `nullptr`.
   - `slotMaps` — `(slot, srcBoneIdx, dstBoneIdx, dstBoneName, outChannelIdx)` for slots that map on both sides AND have a source channel.
   - `dstSlotByBone[dstMesh bone idx]` — slotMaps index for that bone, or -1.
   - `src/dstLocalBindRot/Pos[i]` from `mesh->GetBindPoseMatrix(i)`.
   - `src/dstWorldBindRot[i]` from `decompose(bones[i].mInvOffsetMatrix).rotation` — bone's resting model-space rotation.
   - Union of all rotation timestamps across mapped slots' source channels.

2. **Per source rotation timestamp `t`**:
   - **Source walk** (parent-first, which is bone index order due to DFS at import time): sample each bone's channel rotation (or bind if no channel), accumulate `srcWorldRot[i] = srcWorldRot[parent] * localRot`.
   - **Target walk**: for each bone `i`:
     - If `dstSlotByBone[i] >= 0`: compute `worldDelta = srcWorldRot[srcIdx] * inverse(srcWorldBindRot[srcIdx])`, then `targetWorld = worldDelta * dstWorldBindRot[i]`, then `localRot = inverse(dstWorldRot[parent]) * targetWorld`. Emit as keyframe on the slot's output channel.
     - Else: `localRot = dstLocalBindRot[i]`.
     - Either way: `dstWorldRot[i] = dstWorldRot[parent] * localRot`.

3. **Hips position** (Hips slot only, if it has source position keys): per timestamp, sample source position, multiply by uniform `scaleY = dstLocalBindPos[hips].y / srcLocalBindPos[hips].y` (bind-Y ratio approximates character-height ratio). Output as Hips channel position keys.

4. **Backfill empty tracks**: per output channel, if `mPositionKeys` / `mRotationKeys` / `mScaleKeys` is empty, insert a single bind-pose key at t=0. Same defensive backfill as `FindAnimation`'s bind cache; protects against any track the retarget didn't fill (notably scale, which is never animated).

Bone-axis convention drops out of the math because everything is computed in a common (world) frame. This is what makes tier-2 robust to "arms come out backwards on a rig with different bone roll" — the failure mode tier-1 and naive local-bind retargeting can't solve.

Output clip's `mClipName` matches the source's so `PlayAnimation("Walk", true)` works on both rigs without per-clip code changes.

## 9. Editor modals

Three modal pipelines, all in `ActionManager`:

| Modal | Trigger | Entry point | Body |
|---|---|---|---|
| Extract Animations | right-click SkeletalMesh | `BeginExtractSkeletalAnimations` | `DrawExtractSkeletalAnimationsModal` |
| Import Animations | asset browser → Import Asset → Import Animations | `BeginImportAnimations` | `DrawImportAnimationsModal` |
| Retarget | right-click SkeletalAnimationAsset | `BeginRetargetAnimation` | `DrawRetargetAnimationModal` |

The Retarget modal uses `ImGui::Begin` (not `BeginPopupModal`) so the asset browser stays interactive for `AssetRefPicker` drag-drop. `SetNextWindowFocus` fires only on the first frame the modal opens via a `mRetargetModalJustOpened` latch — otherwise the modal steals focus back from any popup (combo, autocomplete) that opens inside it. This was the original "second avatar picker doesn't work" bug.

All three modals draw at end-of-frame from `EditorImgui.cpp`'s tail block alongside the existing build modal.

## 10. Gotchas

- **C3D draw ordering.** Per-section uniform/material uploads MUST be inside the loop in the order `BindMaterial` → matrix uniforms → UV uniforms → `C3D_DrawElements`. Hoisting uniforms above the first `BindMaterial` leaves section 0 on stale GPU state (only section 1 onwards renders). Caught with a "second slot renders, rest is black" repro on 3DS hardware.
- **Tier-2 requires Reference Mesh on both avatars.** Without it, the retarget silently downgrades to NameRemap and produces the same garbage tier-1 would have. The avatar status panel in the Retarget modal surfaces this so users see it pre-bake.
- **Clip name vs asset name.** `PlayAnimation` matches against `mClipName`, NOT the asset name. Pre-fix retarget bakes used to suffix the clip name with `_Retargeted` — re-bake or hand-edit those.
- **Bind cache backfill.** Both the per-mesh runtime bind cache (`SkeletalMesh3D::FindAnimation`) AND the retarget bake itself perform empty-track backfill. The runtime version is the safety net for malformed assets created by older code paths or by hand.
- **Cross-mesh resolution.** External `SkeletalAnimationAsset` channels are filtered by name against the assigned mesh. Channels referencing bones the mesh doesn't have are silently dropped. This is the right behavior (a Mixamo clip retargeted to a rig with no Spine just doesn't animate the chest), but it can hide a misconfiguration as "no animation". The inspector channel list helps diagnose.
- **Mixamo scale/orientation.** Mixamo FBX exports at 100x scale with a baked -90°X armature rotation. Blender's "Apply All Transforms" silently fails to propagate scale to animation keyframes. Recommended: FBX2glTF on the FBX directly. Alternatively, FBX export with `Apply Scalings = FBX All` checked (not the default "FBX Units Scale").
- **Stripped humanoids.** A rig without Spine/Chest/Neck/Shoulder leaves those slots empty in the avatar. Retargeting just skips them; arms/legs/head animate, missing joints stay at bind pose. Property of the target rig, not the retarget math.

## 11. Out of scope (PR3+ candidates)

- IK end-effector solving for hand-on-prop placement (tier-3 from the design doc).
- Twist bone distribution.
- Foot locking / animation masks / additive layers.
- `SkeletalAnimationLibraryAsset` — bundling many clips into one asset for easier organization.
- Per-clip retarget tuning (root-motion lock, per-axis hip scale, custom base/scale override).
- Runtime humanoid solver (everything is bake-time today; consoles only play baked clips).

## 12. Related

- Designer guide: `Documentation/Development/SkeletalAnimation.md`
- Lua refs: `Documentation/Lua/Assets/SkeletalMesh.md`, `SkeletalAnimationAsset.md`, `HumanoidAvatarAsset.md`, `Documentation/Lua/Nodes/3D/SkeletalMesh3D.md`
- Design doc: `.dev/animation/skeletalanimationmultimesh.md` — the 5-PR plan
- Asset system reference: `.llm/AssetSystem.md` (versioning, stub, ref counting)
- Graphics overview: `.llm/Graphics.md` (where the per-section draw loops live in the backend hierarchy)
