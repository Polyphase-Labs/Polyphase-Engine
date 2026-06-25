# HumanoidAvatarAsset

A description of how a rig's bones map to the standard humanoid bone slots. One avatar per rig; pair two avatars (source + target) to retarget animations between them.

The full authoring workflow — auto-mapping, validation, the **Retarget…** modal — lives in the asset inspector. See [Skeletal Animation Pipeline](../../Development/SkeletalAnimation.md) for the end-to-end guide.

Inheritance:
* [Asset](Asset.md)

---

## Humanoid slot enum

Twenty-one slots covering a standard humanoid skeleton:

```
Hips, Spine, Chest, Neck, Head,
LeftShoulder,  LeftUpperArm,  LeftLowerArm,  LeftHand,
RightShoulder, RightUpperArm, RightLowerArm, RightHand,
LeftUpperLeg,  LeftLowerLeg,  LeftFoot,  LeftToes,
RightUpperLeg, RightLowerLeg, RightFoot, RightToes
```

Rigs without all 21 slots (a stripped robot with no Spine/Chest/Neck/Shoulder, for instance) leave those slots empty — retargeting just skips them. The arms/legs/head animate; the missing joints stay rigid. That's a property of the target rig, not the math.

---

## No Lua API yet

HumanoidAvatarAsset doesn't currently expose Lua bindings. The whole authoring loop is editor-driven (Create Asset → Humanoid Avatar, Auto-map button, manual slot text entry), and the retarget bake is also editor-side (right-click any SkeletalAnimationAsset → Retarget…).

Once baked, the retargeted clip is a regular [SkeletalAnimationAsset](SkeletalAnimationAsset.md) you can `LoadAsset()` and assign to a [SkeletalMesh3D](../Nodes/3D/SkeletalMesh3D.md)'s **Animation Assets** vector.

If you need runtime avatar inspection (e.g. UI that shows which bone a humanoid slot maps to), open an issue — bindings are easy to add but haven't had a real use case yet.
