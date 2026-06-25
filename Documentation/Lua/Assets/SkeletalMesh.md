# SkeletalMesh

A mesh that can contain a bone hierarchy and animations.

Inheritance:
* [Asset](Asset.md)

---
### GetMaterial
Get the mesh's default material.

Sig: `material = SkeletalMesh:GetMaterial()`
 - Ret: `Material material` Default material
---
### SetMaterial
Set the mesh's default material.

Sig: `SkeletalMesh:SetMaterial(material)`
 - Arg: `Material material` Default material
---
### GetNumIndices
Get the number of indices.

Sig: `num = SkeletalMesh:GetNumIndices()`
 - Ret: `integer num` Num indices
---
### GetNumFaces
Get the number of faces.

Sig: `num = SkeletalMesh:GetNumFaces()`
 - Ret: `integer num` Num faces
---
### GetNumVertices
Get the number of vertices.

Sig: `num = SkeletalMesh:GetNumVertices()`
 - Ret: `integer num` Num vertices
---
### FindBoneIndex
Find a bone's index from its name.

Sig: `index = SkeletalMesh:FindBoneIndex(name)`
 - Arg: `string name` Bone name
 - Ret: `integer index` Bone index
---
### GetNumBones
Get the number of bones.

Sig: `num = SkeletalMesh:GetNumBones()`
 - Ret: `integer num` Num bones
---
### GetAnimationName
Get an animation name from its index.

Sig: `name = SkeletalMesh:GetAnimationName(index)`
 - Arg: `integer index` Animation index
 - Ret: `string name` Animation name
---
### GetNumAnimations
Get the number of animations.

Sig: `num = SkeletalMesh:GetNumAnimations()`
 - Ret: `integer num` Number of animations
---
### GetAnimationDuration
Get an animation's duration.

Sig: `duration = SkeletalMesh:GetAnimationDuration(name)`
 - Arg: `string name` Animation name
 - Ret: `number duration` Animation duration
---
### GetNumSections
Get the number of named mesh sections. Legacy single-material meshes report 1 (a `"Default"` section spanning the whole index range).

Sig: `num = SkeletalMesh:GetNumSections()`
 - Ret: `integer num` Number of sections
---
### GetSectionName
Get a section's name. Multi-part imports preserve the source primitive's name (e.g. `"Body"`, `"Cape"`).

Sig: `name = SkeletalMesh:GetSectionName(index)`
 - Arg: `integer index` Section index (Lua 1-indexed)
 - Ret: `string name` Section name (`nil` if index out of range)
---
### GetSectionMaterial
Get a section's material. Falls back to the mesh's default `mMaterial` when the section has no explicit material assigned.

Sig: `material = SkeletalMesh:GetSectionMaterial(index)`
 - Arg: `integer index` Section index (Lua 1-indexed)
 - Ret: `Material material` Resolved section material (`nil` if neither slot nor default is set)
---
### SetSectionMaterial
Assign a material to a section. Use `nil` to clear back to the default. Edits the **asset** — to override per-instance without dirtying the asset, use `SkeletalMesh3D:SetMaterialSlot` on the node.

Sig: `SkeletalMesh:SetSectionMaterial(index, material)`
 - Arg: `integer index` Section index (Lua 1-indexed)
 - Arg: `Material material` New material (or `nil` to clear)
---
### FindSectionIndex
Look up a section's index by name.

Sig: `index = SkeletalMesh:FindSectionIndex(name)`
 - Arg: `string name` Section name
 - Ret: `integer index` Section index (Lua 1-indexed), or 0 if not found
---
