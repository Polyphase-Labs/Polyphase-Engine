# Bone-Mask Animation Layering

Layer animations across multiple slots with per-bone masks, smooth weight fades, and an additive blend mode. Lets a character run with their legs while attacking, aiming, reacting, or breathing with their upper body.

Designer guide: `Documentation/Development/BoneMask.md`.

---

## 1. Files

| File | Purpose |
|---|---|
| `Engine/Source/Engine/Assets/BoneMaskAsset.h/.cpp` | Name-keyed subtree spec (`mIncludeRoots`, `mExcludeRoots`, `mSelfOnly`) that resolves to a per-bone bitset against any `SkeletalMesh`. Cached against the last queried mesh; invalidated on prop change or component mesh swap. |
| `Engine/Source/Engine/Assets/SkeletalMesh.h/.cpp` | `GatherDescendants`, `GatherSubtreeBoneSet`, decomposed bind-pose accessors (`GetBindPosePos/Rot/Scale`) used by the blend loop. Bones in `mBones` are DFS-ordered (parent index strictly less than child indices) — `GatherDescendants` exploits this for a single forward pass. |
| `Engine/Source/Engine/Nodes/3D/SkeletalMesh3d.h/.cpp` | `ActiveAnimation` extended with `mBoneMask`, `mWeightTarget`, `mWeightFadeRate`, `mLayerMode`. New API: `PlayAnimationMasked`, `PlayAnimationAdditive`, `SetSlotMask`, `SetSlotWeight`, `FadeSlotWeight`, `SetSlotLayerMode`. Blend loop refactored to per-bone seed-from-bind + mask gating + `AnimLayerMode` branch. |
| `Engine/Source/Engine/AssetRef.h` | `BoneMaskRef` typedef. |
| `Engine/Source/Engine/Asset.h` | `ASSET_VERSION_BONE_MASK = 37`; `ASSET_VERSION_CURRENT` bumped to 37. |
| `Engine/Source/Editor/BoneMaskEditor/BoneMaskEditor.h/.cpp` | `#if EDITOR` panel: hierarchical bone tree (left), live preview viewport (right) with offscreen RT, orbit camera, animation scrubber, and ImGui screen-space bone wireframe overlay drawn in green for included bones and grey for excluded. Reuses AnimationBrowser's RT pattern. |
| `Engine/Source/Engine/Renderer.cpp` | `GetBoneMaskEditor()->Render()` hook alongside `AnimationBrowser`. |
| `Engine/Source/Editor/EditorImgui.cpp` | BoneMaskAsset double-click and asset-context-menu wiring opens the editor. "Bone Mask" entry in **Create Asset** popup. |
| `Engine/Source/Editor/EditorMainMenu.cpp` | View → "Bone Mask Editor" toggle. |
| `Engine/Source/Editor/EditorState.h` | `mShowBoneMaskEditor` panel flag. |
| `Engine/Source/LuaBindings/BoneMaskAsset_Lua.h/.cpp` | Read/write Lua surface on the asset (target mesh, include/exclude root arrays, self-only flag). |
| `Engine/Source/LuaBindings/SkeletalMesh3d_Lua.h/.cpp` | `PlayAnimationMasked`, `PlayAnimationAdditive`, `SetSlotMask`, `SetSlotWeight`, `FadeSlotWeight`, `SetSlotLayerMode`. Registers global `AnimLayerMode` enum table (`Replace = 0`, `Additive = 1`). |
| `Engine/Source/LuaBindings/LuaBindings.cpp` | `BoneMaskAsset_Lua::Bind()` registered alongside `SkeletalMesh_Lua`. |
| `Engine/Source/Engine/Engine.cpp` | `FORCE_LINK_CALL(BoneMaskAsset)`. |

## 2. Asset versions

`ASSET_VERSION_BONE_MASK = 37` gates the entire `BoneMaskAsset` body on load. The whole field block is wrapped:

```cpp
if (stream.GetAssetVersion() >= ASSET_VERSION_BONE_MASK) { ... }
```

Older assets that get a type-id match before this version exist only theoretically — `BoneMaskAsset` was introduced at this version. The gate is for forward-compat: a downstream load that bumps to `38` and reads a v37 file still works.

## 3. Resolution model

`BoneMaskAsset` is **name-keyed**, not index-keyed. Saved to disk it contains:

```cpp
SkeletalMeshRef          mTargetMesh;       // editor convenience only
std::vector<std::string> mIncludeRoots;     // subtree roots to include
std::vector<std::string> mExcludeRoots;     // subtree roots to subtract
bool                     mSelfOnly = false; // include only the named bones, not descendants
```

`Resolve(const SkeletalMesh* mesh)` returns a `std::vector<uint8_t>` bitset sized to `mesh->GetNumBones()`. Cache holds last resolved bitset + mesh pointer; identity comparison invalidates. `BoneMaskAsset::HandlePropChange` invalidates the cache on any property edit. `SkeletalMesh3D::InvalidateAnimationBindings` invalidates the cache on every active-slot mask when the component swaps meshes.

The resolve walks `mIncludeRoots` (or self-only) into a scratch bitset using `SkeletalMesh::GatherDescendants`, then walks `mExcludeRoots` and AND-NOTs them out. Unknown bone names log a warning once and are skipped.

## 4. Blend-loop refactor (`SkeletalMesh3D::UpdateAnimation`)

The pre-existing loop had a `bonesUpdated` global flag — the first slot wrote through, subsequent slots `glm::mix`'d. That breaks if the first slot is masked: masked-OUT bones in slot 0 never get any pose data.

Replaced with **per-bone seed-from-bind**:

```cpp
static std::vector<uint8_t> sBoneSeeded;
sBoneSeeded.assign(numBones, 0);

for each ActiveAnimation slot in mActiveAnimations:
    if (slot.mWeight <= 0) continue;
    const auto& mask = slot.mBoneMask ? slot.mBoneMask->Resolve(mesh) : kEmptyMask;
    const bool masked = (mask.size() == numBones);

    for each channel in anim->mChannels:
        int b = channel.mBoneIndex;
        if (masked && !mask[b]) continue;   // masked-out, skip

        sample (P, R, S) at slot.mTime;

        if (!sBoneSeeded[b]) {
            sDecompTransforms[b] = mesh->GetBindPose(b);   // seed once per bone per frame
            sBoneSeeded[b] = 1;
        }

        if (slot.mLayerMode == AnimLayerMode::Additive) {
            // accum + (clip - bind) * weight
            accum.P += (P - bindP) * w;
            accum.R = slerp(identity, R * inverse(bindR), w) * accum.R;
            accum.S += (S - bindS) * w;
        } else {
            // Replace: standard weighted lerp
            accum.P = mix(accum.P, P, w);
            accum.R = slerp(accum.R, R, w);
            accum.S = mix(accum.S, S, w);
        }
```

**Backward-compat proof:** with a single full-body slot at `w=1` in Replace mode, every bone seeds from bind and then `mix(bind, anim, 1.0) == anim` — bit-identical to the prior "first writer wins" path.

`SkeletalMesh3D::Tick` does **not** drive pose evaluation; the actual call site is `Renderer::HandleCullResult` (`Renderer.cpp`), which decides whether to run a full `UpdateAnimation(dt, true)`, advance time only, or skip entirely based on `mAnimationUpdateMode` and frustum visibility.

## 5. Weight fade

`ActiveAnimation::mWeightTarget` (default -1, disabled) and `mWeightFadeRate` drive linear weight easing each tick. When `|mWeight - mWeightTarget| < 0.0001f` the slot snaps to target. If the target was `0.0`, `animFinished = true` and the existing queued-anim finalize path erases the slot — same removal site as non-looping clips that play out.

`FadeSlotWeight(slot, target, seconds)` sets `mWeightFadeRate = |target - mWeight| / seconds` so the fade always completes in `seconds`. `seconds <= 0` snaps immediately.

## 6. Layer modes (`AnimLayerMode`)

```cpp
enum class AnimLayerMode { Replace, Additive, Count };
```

Default per slot is `Replace`. `PlayAnimationAdditive` is a shortcut that calls `PlayAnimation` then sets `mLayerMode = Additive` and attaches the mask. `SetSlotLayerMode(slot, mode)` retunes an already-playing slot.

**Authoring constraint for Additive clips**: the clip must be a delta-from-rest animation (start frame matches bind pose). A normal full-pose clip in Additive mode double-transforms and looks broken. Replace mode is the right tool for masked attack / aim / dance clips; Additive is for breathing, recoil, hit reactions, leans, additive idles.

## 7. Per-platform considerations

Bone masking and additive blending are entirely upstream of `SkeletalMesh::FinalizeBoneTransforms`. Every backend consumes the resulting `mBoneMatrices` buffer unchanged:

- **Vulkan** — GPU skinning from `VertexSkinned` (`VulkanUtils.cpp::DrawSkeletalMeshComp`).
- **GX (Wii / GameCube)** — `GX_LoadPosMtxImm` per bone (`Graphics_GX.cpp`).
- **C3D (3DS)** — uniform array; CPU fallback when bone count exceeds GPU limit (`Graphics_C3D.cpp`).
- **PSP CPU skinning** — repacks `mBoneMatrices` into `psp::StaticVertex` per frame.
- Any addon-target backend (OG Xbox, Dreamcast, custom hardware) — same buffer.

**Zero per-backend code changes.** The new `BoneMaskAsset.cpp` falls under `Engine/Source/Engine/Assets/` and is picked up by every build system via existing globs (Linux Makefile, CMake, engine-addon injection for addon-target builds). Only the Windows vcxproj and editor vcxproj filters need explicit additions.

## 8. Editor panel (`BoneMaskEditor`)

Two-pane layout inside one ImGui dock:

- **Left** — hierarchical bone tree built from `Bone::mParentIndex`. Tri-state coloring: green (in include list), red (in exclude list), light green (resolved-included by parent), grey (resolved-excluded). Right-click row → Include subtree / Exclude subtree / Clear.
- **Right** — header (Target Mesh picker + Self Only + "Included: N / M bones" summary), animation dropdown / play-pause / scrubber / grid toggle / reset view, and the 3D viewport.

3D viewport spawns its own private `World` with `Camera3D`, `DirectionalLight3D`, and a `SkeletalMesh3D` preview node — same pattern as `AnimationBrowser`. Renders via `Renderer::RenderSecondScreen` to a Vulkan offscreen RT. Orbit (left-drag), pan (right/middle-drag), zoom (wheel). `BoneMaskEditor::Render` is called from `Renderer.cpp` alongside `AnimationBrowser::Render`.

Bone wireframe overlay is drawn as a **screen-space ImGui drawlist** on top of the rendered image, not as world-space debug lines — engine debug lines share the depth buffer and get fully occluded by the skinned mesh body. Each bone-to-parent segment is projected through the camera's `GetViewProjectionMatrix()` to NDC, mapped into the image rect, and drawn via `ImGui::GetWindowDrawList()->AddLine`. The Vulkan projection already bakes in the Y-flip, so the NDC-to-pixel math is `py = imgMin.y + (ndc.y * 0.5 + 0.5) * imgH` — no extra inversion.

## 9. Lua surface

```lua
-- Asset
mask:GetTargetMesh()                 mask:SetTargetMesh(skel)
mask:GetSelfOnly()                   mask:SetSelfOnly(bool)
mask:GetNumIncludeRoots()            mask:GetIncludeRoot(i)
mask:AddIncludeRoot(name)            mask:RemoveIncludeRoot(name)
mask:GetNumExcludeRoots()            mask:GetExcludeRoot(i)
mask:AddExcludeRoot(name)            mask:RemoveExcludeRoot(name)

-- Node
mesh:PlayAnimationMasked(name, slot, mask, loop, speed?, weight?)
mesh:PlayAnimationAdditive(name, slot, mask, loop, speed?, weight?)
mesh:SetSlotMask(slot, mask)
mesh:SetSlotWeight(slot, weight)
mesh:SetSlotLayerMode(slot, AnimLayerMode.Replace | AnimLayerMode.Additive)
mesh:FadeSlotWeight(slot, target, seconds)

-- Enum table
AnimLayerMode.Replace   -- 0
AnimLayerMode.Additive  -- 1
```

`PlayAnimation` keeps its prior signature unchanged. `PlayAnimationMasked` and `PlayAnimationAdditive` delegate to it then patch `mBoneMask` and `mLayerMode` on the resulting slot.

## 10. Gotchas

- **Slot conflicts**: a script that calls `PlayAnimation` every tick on slot N (e.g. an Idle/Run blend) will overwrite any masked/additive slot you placed there. Use a free slot (3+ in most setups).
- **Default `loop=true` is rare for layered slots**: most attack / aim / hit-reaction layers should use `loop=false` so the slot self-removes when the clip ends.
- **Mask with empty include and empty exclude**: resolves to an all-zero bitset (mask is valid, so masking is on, but no bones are in scope) — the slot drives nothing. Add at least one include root or clear the mask.
- **Additive on a full-pose clip**: double-transforms. Use Replace mode for full-pose attack clips with a mask; Additive is only for delta-from-rest clips.
- **Bone names must match across rigs for retargeting**: masks are name-keyed and resolve fresh against any target mesh, but a name typo or rig-rename silently drops that bone from the mask.
