# Skeletal Animation Pipeline

The skeletal animation system spans three layers, each addressing a separate problem:

1. **Multi-section `SkeletalMesh`** — multi-part characters where each part keeps its own material slot.
2. **`SkeletalAnimationAsset`** — first-class, reusable bone-animation clips that can be extracted from imported meshes or imported standalone.
3. **`HumanoidAvatarAsset` + retargeting** — Mecanim-style humanoid avatars and a bake-time retarget pipeline so a clip authored for one rig plays correctly on another.

You can use any layer independently. The combined end-to-end workflow is "Mixamo clip → my custom character" with no middleware.

---

## Multi-section SkeletalMesh

A `SkeletalMesh` now holds an array of named `SkeletalMeshSection`s on top of its shared vertex/index buffer. Each section is a contiguous index range with its own material assignment.

### Importing a multi-part character

In the **Import** options for a `.glb` / `.fbx` / `.gltf`, enable **As Single Object** ("combineMeshes" in the importer). The combined importer:

1. Collects every renderable primitive (skipping `UCX*`, `UBX*`, `USP*` collision helpers).
2. Builds the **union** of bone names across every skinned primitive into a single shared skeleton — preserving parent indices via a DFS walk of the source scene hierarchy.
3. Appends every primitive's vertices and indices into one buffer.
4. Emits one `SkeletalMeshSection` per source primitive, with `mName` taken from `aiMesh::mName`.

> **Fixed in this release:** previously the combined importer picked the first skinned primitive as canonical and silently dropped weights for bones that only existed on later primitives. Accessories and clothing that animated correctly in Blender would appear rigidly attached at runtime. The new bone-superset walk handles this correctly.

### In the inspector

Open the SkeletalMesh asset. The **Sections** panel lists every section with:

- Section name + triangle count
- A material picker per row (the section's own material)

The asset also keeps its legacy single `Material` slot, used as a fallback when a section has no material assigned.

### Per-instance material overrides

On the `SkeletalMesh3D` node, the **Section Material Overrides** vector property accepts one material per section. The runtime resolution chain per slot is:

1. Component override (`mSectionMaterialOverrides[i]`)
2. Section's own material (on the asset)
3. Legacy whole-mesh override (`mMaterialOverride`)
4. Asset-default `mMaterial`
5. Renderer default material

So you can swap your hero's tabard color in one scene without touching the asset.

### Lua

```lua
local mesh = LoadAsset("SK_Hero")
print(mesh:GetNumSections())            -- 4
print(mesh:GetSectionName(1))           -- "Body"   (Lua-1-indexed)
mesh:SetSectionMaterial(2, LoadAsset("M_Cape_Red"))

local node = world:SpawnBasicNode(BASIC_SKELETAL_MESH)
node:SetSkeletalMesh(mesh)
print(node:GetNumMaterialSlots())                       -- 4
node:SetMaterialSlot("Cape", LoadAsset("M_Cape_Blue"))  -- by section name
node:SetMaterialSlot(1, LoadAsset("M_Body_Tanned"))     -- by slot index
```

### Backend support

All three rendering backends loop sections, binding per-section material before each indexed draw range:

- **Vulkan** — `vkCmdDrawIndexed(firstIndex, indexCount)` per section
- **Wii / GameCube (GX)** — `GX_Begin / GX_End` per section with material bind
- **3DS (C3D)** — `C3D_DrawElements` per section; ordering is `BindMaterial` → matrix uniforms → UV uniforms → draw (C3D's deferred state cache requires this; getting it wrong renders only the second section onwards)

Shadow passes stay as a single full-mesh depth draw — material state doesn't matter for depth-only.

### Legacy asset compatibility

Assets saved before this release load with one implicit `"Default"` section spanning the whole index range, materialed by their existing `mMaterial`. Re-saving promotes them to the new format.

---

## SkeletalAnimationAsset

The `SkeletalAnimationAsset` asset type is a standalone, reusable bone-animation clip:

- Channels keyed by **bone name** (not bone index) so the same asset can bind to multiple rigs
- Position / rotation / scale keyframes per channel
- Event tracks (`AnimEventTrack`)
- Source-skeleton metadata: rig name, bone names, parent indices, bind pose matrices — used by the retarget pipeline

### Extracting clips from a SkeletalMesh

Right-click any `SkeletalMesh` asset → **Extract Animations…**. The modal lists every embedded clip with checkboxes:

- **Prefix** (default `SA_`)
- **Overwrite existing** — replaces any prior extract with the same name
- **Unique names** — appends `_1`, `_2` etc. on collision

**Extract Selected** / **Extract All** writes each chosen clip to a `SA_<MeshName>_<ClipName>` asset in the source mesh's directory. The new assets carry full source-rig metadata for later retargeting.

### Importing animation-only files

Mixamo, Adobe Mixamo, and similar services export `.fbx` / `.glb` files that contain animation channels but no render mesh. Asset browser → **Import Asset → Import Animations**:

1. File picker → choose the file
2. Modal shows every `aiAnimation` in the file with checkboxes
3. **Import Selected** / **Import All** generates standalone `SA_*` assets

The importer walks the scene node hierarchy to record source-bone metadata (rig name, bone names, parent indices) even when there's no skinned mesh present.

### Playing external clips on a SkeletalMesh3D

On a `SkeletalMesh3D` node, the **Animation Assets** vector property accepts any number of `SkeletalAnimationAsset` references. The runtime's `FindAnimation(name)` resolves in this order:

1. Embedded animations on the assigned SkeletalMesh
2. The mesh's `mAnimationLookupMesh` chain (legacy)
3. External `Animation Assets` on the node

So `PlayAnimation("Walk", true)` works regardless of whether `Walk` is embedded or external — and you can override an embedded animation by adding a SkeletalAnimationAsset with the same clip name.

```lua
local hero = world:SpawnBasicNode(BASIC_SKELETAL_MESH)
hero:SetSkeletalMesh(LoadAsset("SK_Hero"))

-- Add external clips (could be Mixamo retargets, hand-authored clips, etc.)
hero:AddAnimationAsset(LoadAsset("SA_Walk"))
hero:AddAnimationAsset(LoadAsset("SA_Run"))
hero:AddAnimationAsset(LoadAsset("SA_JumpStart"))

-- PlayAnimation resolves by clip name, regardless of which list it lives in.
hero:PlayAnimation("Walk", true)
```

### Inspector channel readout

When you select a SkeletalAnimationAsset, the inspector shows duration, source rig, channel count, and an expandable **Channel List** with bone name + key counts per channel. Use this to diagnose retarget bakes that come out empty — if you see zero channels, your avatar slot mappings didn't line up.

---

## Humanoid Avatars and Retargeting

A `HumanoidAvatarAsset` is a description of how a rig's bones map to the standard humanoid bone slots:

- 21 slots: Hips, Spine, Chest, Neck, Head, L/R Shoulder/UpperArm/LowerArm/Hand, L/R UpperLeg/LowerLeg/Foot/Toes
- A **Reference Mesh** pointer to the `SkeletalMesh` whose rig this avatar describes
- Per-slot bone name strings

With one avatar per rig you can bake any clip authored for one rig into a target-compatible clip for another.

### Creating an avatar

Asset browser → **Create Asset → Humanoid Avatar**. Open the new asset:

1. Set **Reference Mesh** to the SkeletalMesh whose rig this describes.
2. Click **Auto-map from Reference**. The auto-mapper recognizes:
   - **Mixamo**: `mixamorig:Hips`, `mixamorig:LeftArm`, etc. (`mixamorig:` prefix auto-stripped)
   - **ARP / Auto-Rig Pro**: `c_root.x`, `c_arm_fk.l`, etc.
   - **Rigify**: `DEF-upper_arm.L`, `MCH-spine.001`, etc.
   - **Blender suffix style**: `UpperArm.L`, `LowerLeg.R`, `Foot.L`
   - Common variants: `LeftArm`, `Left_Arm`, `leftarm`, `upperarml`, `arm_l`, etc.
3. Check the per-slot validation chips: green "ok" / yellow "unset" / red "not in mesh". Fix any red ones by clicking the slot's text input and typing the exact bone name.

A stripped humanoid (e.g. a robot with no Spine/Chest/Neck/Shoulder) leaves those slots empty — retargeting just skips them. The arms/legs/head animate; the missing joints stay rigid. That's a property of the target rig, not the math.

### Baking a retargeted clip

Right-click any `SkeletalAnimationAsset` → **Retarget…**.

The modal has:

- **Source Avatar** — the avatar for the rig the clip was authored against
- **Target Avatar** — the avatar for the rig you want the clip to play on
- **Output Name** — defaults to `<source>_Retargeted`
- **Mode**:
  - **Tier 1 — Name remap** — copies keyframes verbatim, just renames channels by slot. Use when source/target rigs have identical proportions and bone-axis conventions.
  - **Tier 2 — Reference-pose aware** — full world-space retarget through both rigs' bind poses. Use when proportions or bone-axis conventions differ. **Both avatars must have a Reference Mesh assigned.**
- **Avatar status panel** — lives status for each avatar (mesh + slot count), so you can see before baking whether the result is going to be useful.
- **Overwrite existing** / **Unique names** — usual asset-collision toggles.

**Bake** writes a new SA asset next to the source clip. Its clip name is preserved (not suffixed) so `PlayAnimation("Walk", true)` works on both source and target rigs.

### Tier-1 vs Tier-2

**Tier-1** is one frame of math per keyframe: rename the channel, copy the keys. Fast, lossless, works for any rig pair where:
- Source and target bind poses are nearly identical (both T-pose with same axis layout, just different bone names).
- Or the original clip was authored on a rig with the same skeleton structure.

**Tier-2** walks both rigs' full bone hierarchies per keyframe, computing world-space rotations. The math is:

```
worldDelta  = sourceWorldRot * inverse(sourceWorldBindRot)   // what the animation did, in world space
targetWorld = worldDelta * targetWorldBindRot                // apply same delta to target's rest
targetLocal = inverse(targetParentWorldRot) * targetWorld    // back to local space
```

Bone-axis conventions drop out of the math because everything is computed in a common frame. This fixes the classic "arms come out backwards" failure mode that local-bind retargeting can't solve.

Hips translation gets a uniform Y-ratio scale (`dstBindY / srcBindY`) so a tall Mixamo run on a stubby robot still strides correctly.

### What tier-2 can't do

- **IK-correct hand-on-prop placement** — needs end-effector solving (tier-3, not implemented).
- **Twist bone distribution** — clips with separate forearm-twist / upperarm-twist bones get all rotation on the main bone.
- **Generate motion for missing intermediate joints** — if your target rig has no Spine bone, chest rotation from the source is silently dropped. Add the joint to your rig if you need it.

---

## End-to-end: Mixamo to your character

The canonical workflow:

1. **Import your character** (any rig — does NOT need to match Mixamo's bone names).
2. **Create `HA_Hero`**, point Reference Mesh at your mesh, hit **Auto-map**. Fix any red slots.
3. **Import a Mixamo character + animation clips.** Use FBX2glTF on the Mixamo FBX first if you're hitting Mixamo's 100x scale / 90° rotation issues (see "Mixamo gotchas" below).
4. **Create `HA_Mixamo`**, point Reference Mesh at the Mixamo skeleton, auto-map.
5. **For each Mixamo clip, right-click → Retarget…** → source `HA_Mixamo`, target `HA_Hero`, mode **Tier 2**, bake.
6. **Drop the baked `SA_*_Retargeted` clips into your character node's `Animation Assets` vector** in the scene.
7. From Lua: `hero:PlayAnimation("Walk", true)`.

Done.

---

## Mixamo gotchas

Mixamo characters export at **100x scale** (centimeters) and often have a **-90° X rotation** baked into the armature parent transform. Blender's "Apply All Transforms" on an armature *looks* correct in viewport but frequently fails to propagate scale to animation F-curves — the export still ships at the wrong scale.

### Recommended workflow

Use **FBX2glTF** (Adobe / Facebook tool) on the Mixamo FBX directly — it normalizes scale to meters during conversion. Import the resulting `.glb`.

### Blender export checklist (if you must)

When exporting Mixamo from Blender:

```
File → Export → FBX (.fbx)

Transform:
  Scale:           1.00
  Apply Scalings:  FBX All           ← critical, not "FBX Units Scale"
  Forward:         -Z Forward
  Up:              Y Up
  ✓ Apply Unit
  ✓ Use Space Transform
  ☐ Apply Transform                  (leave OFF — Blender bug fuse)

Armature:
  ✓ Only Deform Bones
  ☐ Add Leaf Bones                   (uncheck — fake bones the retargeter ignores)

Animation:
  ✓ Baked Animation
  ✓ Key All Bones
  ☐ NLA Strips
```

`Apply Scalings = FBX All` is the one setting that bakes 100x scale into bone offsets and keyframes.

### Diagnosing wrong-orientation imports

| Imported character looks... | Means... |
|---|---|
| 100x bigger, upright | Scale not baked, rotation OK — fix `Apply Scalings = FBX All` |
| Correct size, lying on its back | Axis flip — fix `Up: Y Up` |
| Correct size, T-pose only | Animation keys not baked — fix `Key All Bones` + `Baked Animation`, uncheck `NLA Strips` |

---

## Troubleshooting

### "I retargeted but the animation doesn't play"

Open the retargeted SA asset's inspector. Check **Channels (N)**.

| N | Meaning | Fix |
|---|---|---|
| 0 | Neither avatar mapped any slots that resolve in the source clip | Re-check both avatars' validation; especially red "not in mesh" slots |
| > 0 but plays nothing | Clip name mismatch (you're calling `PlayAnimation("Walk")` but the clip is named `Walk_Retargeted`) | Edit the **Clip Name** field, or re-bake with the latest engine (new bakes preserve the clip name) |
| > 0 and plays but limbs wrong | Tier-1 on rigs with different bind poses or bone axes | Switch to Tier-2; ensure both avatars have a Reference Mesh assigned |

### "Tier-2 produces the same garbage as Tier-1"

One of the avatars has no Reference Mesh assigned, so the retargeter has no bind pose to project through and falls back to NameRemap with a diagnostic log line. Assign the Reference Mesh on both avatars.

### "Hips translation goes wild"

Most often this is the source clip having a Hips channel in cm-scale (Mixamo) and the target rig in m-scale, with the source avatar's reference mesh missing — so the Y-scale ratio falls back to 1.0. Set the source avatar's Reference Mesh.

### "Only one section of my multi-part character renders on 3DS"

Should be fixed in this release — see the C3D ordering note above. If you still see it, file a bug with the asset.

### "Per-section material override doesn't apply"

Override slots are addressed by **section index** (1-based in Lua, 0-based in C++). If the mesh has 3 sections but the override vector only has 2 entries, slot 2 falls through to the asset default. Use `SetMaterialSlot` with the section name to avoid index confusion.

---

## Related

- [Lua API: SkeletalMesh](../Lua/Assets/SkeletalMesh.md)
- [Lua API: SkeletalAnimationAsset](../Lua/Assets/SkeletalAnimationAsset.md)
- [Lua API: HumanoidAvatarAsset](../Lua/Assets/HumanoidAvatarAsset.md)
- [Lua API: SkeletalMesh3D](../Lua/Nodes/3D/SkeletalMesh3D.md)
- Design doc: `.dev/animation/skeletalanimationmultimesh.md`
