# Bone-Mask Animation Layering

Layer multiple animations on a single skeletal mesh, with each slot scoped to a subset of bones and blended in either Replace or Additive mode.

The classic motivating case: a character running with their legs while attacking, aiming, or reacting with their upper body — without authoring a custom "run-and-attack" clip for every combination.

---

## TL;DR

```lua
-- Lower body: full-body locomotion
mesh:PlayAnimation("Idle", true, 1, 1.0 - runAlpha, 0)
mesh:PlayAnimation("Run",  true, 1, runAlpha,       1)

-- Upper body: masked attack, plays once and self-removes
mesh:PlayAnimationMasked("Punch", 2, UpperBodyMask, false, 1.0, 1.0)

-- Hit reaction on top of everything, additive
mesh:PlayAnimationAdditive("HitReact_add", 3, UpperBodyMask, false, 1.0, 1.0)
```

Slot 0/1 drive every bone with weighted Idle↔Run blend. Slot 2 overwrites masked-in bones only (Replace mode). Slot 3 adds a delta on top of whatever's below (Additive mode).

---

## Authoring a BoneMaskAsset

1. **Right-click in the asset browser → New Asset → Bone Mask**, or use the View menu → **Bone Mask Editor**.
2. **Assign a Target Mesh** (the rig you want to author against). Masks are name-keyed, so any rig with matching bone names will use this mask correctly; the target mesh is for authoring preview only.
3. **Right-click bones in the tree** to Include or Exclude subtrees. Include adds the bone and all descendants; Exclude removes the bone and all its descendants from the resolved set.
4. **Self Only** flips both lists from "include this bone and descendants" to "include only this bone". Useful for finger or twist-bone tweaks.
5. **Save** (top of panel, lights up when dirty).

The live 3D preview shows your target mesh with the resolved-included bones tinted bright green and excluded bones dim grey, drawn as a wireframe overlay on top of the rendered mesh. Pick an animation and hit Play to verify the mask follows the deforming pose in real time.

Typical mask presets:

| Mask | Include | Exclude |
|---|---|---|
| Upper Body | `Spine_01` (or root spine bone) | — |
| Upper Body, no head | `Spine_01` | `Head` |
| Lower Body | `Hips` | `Spine_01` |
| Left Arm | `LeftShoulder` | — |
| Both Arms | `LeftShoulder`, `RightShoulder` | — |
| Head only | `Neck` | — |

Bone names depend on your rig's exporter conventions (Mixamo's `mixamorig:Spine`, ARP's `c_spine_01`, Rigify's `DEF-spine`, etc). The mask stores the name verbatim and resolves at runtime.

## Playing masked animations

The blend system has **8 simultaneous slots** per `SkeletalMesh3D`. Each slot holds one clip plus its own weight, speed, mask, and layer mode.

```lua
-- Replace mode (default for PlayAnimationMasked):
-- masked-in bones are weighted-mixed with the slot's clip,
-- masked-out bones stay at whatever lower slots provided.
mesh:PlayAnimationMasked(clipName, slot, mask, loop, speed, weight)

-- Additive mode:
-- the slot's clip is treated as a DELTA FROM BIND POSE.
-- (clip - bind) * weight is added on top of the accumulator.
-- Use for breathing, recoil, hit reactions, leans — never for full-pose clips.
mesh:PlayAnimationAdditive(clipName, slot, mask, loop, speed, weight)
```

`loop = false` is usually correct for one-shot upper-body clips — the slot self-removes when the clip finishes, leaving the lower-body blend untouched.

`mask = nil` is equivalent to no mask (full body).

## Slot management at runtime

```lua
-- Swap or clear the mask on an already-playing slot
mesh:SetSlotMask(slot, mask)        -- nil to clear
mesh:SetSlotMask(slot, nil)

-- Snap the slot's weight
mesh:SetSlotWeight(slot, 0.5)

-- Fade weight over time. Slot self-removes when target == 0 and fade completes.
mesh:FadeSlotWeight(slot, 0.0, 0.25)    -- fade out over 0.25 s
mesh:FadeSlotWeight(slot, 1.0, 0.10)    -- fade in over 0.10 s

-- Change layer mode without re-issuing the play call
mesh:SetSlotLayerMode(slot, AnimLayerMode.Additive)
mesh:SetSlotLayerMode(slot, AnimLayerMode.Replace)
```

`AnimLayerMode` is a global Lua enum table: `AnimLayerMode.Replace == 0`, `AnimLayerMode.Additive == 1`.

## How the blend math works

For each slot in iteration order, for each animated bone:

1. If the slot has a mask and the bone is not in the resolved bitset → **skip**.
2. If this bone hasn't been touched yet this frame → **seed** `sDecompTransforms[bone]` from the mesh's decomposed bind pose.
3. **Replace mode**:
   ```
   accum.position = mix(accum.position, clip.position, weight)
   accum.rotation = slerp(accum.rotation, clip.rotation, weight)
   accum.scale    = mix(accum.scale,    clip.scale,    weight)
   ```
4. **Additive mode**:
   ```
   accum.position += (clip.position - bind.position) * weight
   accum.rotation  = slerp(identity, clip.rotation * inv(bind.rotation), weight) * accum.rotation
   accum.scale    += (clip.scale - bind.scale) * weight
   ```

Then the composed local matrices go through `FinalizeBoneTransforms` (the parent-chained world walk) and from there into every platform's skinning pipeline. **All masking and additive math happens before the chain step, so every backend — Vulkan, GX (Wii / GC), C3D (3DS), PSP CPU skinning, addon-target — works without per-platform changes.**

## Replace vs Additive — choosing the right mode

| Clip type | Mode | Example |
|---|---|---|
| Full-pose animation: every bone has explicit position/rotation/scale | **Replace** | Punch, Kick, Wave, Reload, Dance |
| Delta animation: starts at bind pose, deviates by some delta | **Additive** | Breathing oscillation, Recoil pulse, Hit flinch, Lean-into-turn, Aim offset |

The most common mistake: pointing an Additive slot at a normal full-pose clip. The math treats every bone's pose as a delta from bind, so the resulting pose double-transforms and looks broken. Use Replace mode for full-pose attacks with a mask; Additive is reserved for clips authored specifically as offsets (usually distributed with `_add` or `Additive` in the name).

## A complete character pattern

```lua
-- In Tick:
function Player:UpdateAnimation(dt)
    local runAlpha = Math.Clamp(self.speed / self.maxSpeed, 0, 1)

    -- Lower-body locomotion (slots 0+1, full body, weighted)
    self.mesh:PlayAnimation("Idle", true, 1, 1.0 - runAlpha, 0)
    self.mesh:PlayAnimation("Run",  true, 1, runAlpha,       1)
end

-- On attack (one-shot):
function Player:M_Attack()
    -- Masked Replace, no loop, self-removes when clip ends.
    self.mesh:PlayAnimationMasked("Attack", 2, self.upperBodyMask, false, 1.0, 1.0)
    self.attackTime = 0.4
end

-- On take-hit (one-shot additive):
function Player:OnHit()
    self.mesh:PlayAnimationAdditive("HitReact_add", 3, self.upperBodyMask, false, 2.0, 1.0)
end

-- On aim hold/release (smooth weight):
function Player:SetAiming(on)
    if on then
        self.mesh:PlayAnimationMasked("AimPose", 4, self.armsMask, true, 1.0, 0.0)
        self.mesh:FadeSlotWeight(4, 1.0, 0.15)
    else
        self.mesh:FadeSlotWeight(4, 0.0, 0.20)   -- slot self-removes
    end
end
```

## Gotchas

- **Slot collisions**: any per-tick `PlayAnimation(name, ..., slot=N)` will overwrite a masked or additive slot you placed on N. Use a free slot for layers (typically 2+ if your locomotion uses 0/1).
- **`PlayAnimation` with `loop = true` for layers is rarely correct.** Most masked layers are one-shot reactions and should be `false`.
- **Don't apply Additive to full-pose clips.** It will double-transform. Use Replace with a mask instead.
- **Empty mask resolves to zero bones in scope.** If a `BoneMaskAsset` has no includes and no excludes, the slot is valid but drives nothing. Add at least one include.
- **Mask names depend on your rig.** A mask authored against a Mixamo skeleton with bones named `mixamorig:Spine` will not resolve on a custom rig that names the same bone `Spine_01`. Re-author or use a HumanoidAvatarAsset bone-name remap.
- **`SkeletalMesh3D::Tick` does not drive the blend** — the renderer's frustum-cull pass does, gated by `AnimationUpdateMode`. Out-of-frustum characters skip the blend by default unless you change `mAnimationUpdateMode` on the node.

## Related

- [Skeletal Animation](SkeletalAnimation.md) — multi-section meshes, animation assets, retargeting
- [Bone Sockets](BoneSockets.md) — attaching props to bones, and animation-time notifies

## File reference

Internal architecture, blend-loop refactor proof, asset-version gate, per-platform notes: see `.llm/BoneMask.md`.
