# Shadows

Polyphase supports **dynamic directional shadows** rendered via shadow mapping. A single directional light casts depth into a 2048×2048 shadow map; lit materials sample that depth in their fragment shader to darken occluded pixels.

This page describes how to use shadows in a scene, what's supported, and how the system is wired internally.

---

## Platform support

| Platform | Cast | Receive | Notes |
|---|---|---|---|
| Windows (Vulkan) | ✅ | ✅ | Full support — GPU skinning, PCF, MaterialLite + MaterialBase receivers |
| Linux (Vulkan) | ✅ | ✅ | Same as Windows |
| Android (Vulkan) | ✅ | ✅ | CPU-skinning path for skeletal meshes (engine forces it) |
| GameCube / Wii (GX) | ❌ | ❌ | No shadow render path; `Cast Shadows` flag is silently ignored |
| 3DS (C3D) | ❌ | ❌ | Same as GX — Picasso shader pipeline has no shadow implementation |

Shadow rendering is **Vulkan-only by design**. The shadow pass, shadow-map descriptor, and shader-side shadow reception all live in `Engine/Source/Graphics/Vulkan/` (gated by `API_VULKAN`) and in `Engine/Shaders/GLSL/` (only consumed by Vulkan's shaderc compile path). Console backends never touch any of it and don't need conditional guards.

For platforms without dynamic shadows, use **baked lighting** instead — the engine has full bake support that works everywhere and looks better than shadow mapping for stationary geometry.

---

## Quick start

Three things must be true for a shadow to appear:

1. **A `DirectionalLight3D` exists in the scene.**
   - Its `Cast Shadows` property is **on** (default `true`).
   - It's actually pointing somewhere useful — by default, identity rotation points it horizontally along `-Z`. Aim it downward (e.g. rotate `-90°` on X) for typical "sun overhead" behavior.

2. **The caster mesh has `Cast Shadows = true`** on its `Primitive3D` properties (the property is inherited by every `*Mesh3D` node type).
   - **Default is `false`.** Easy to miss — toggle it explicitly in the inspector.

3. **The receiver mesh has a material that samples shadows.**
   - All shipped material paths sample shadows on Vulkan:
     - `MaterialLite` → `ForwardSpec.frag` (or `Forward.frag`) — sampled automatically.
     - `MaterialBase` with the bundled PBR shader (`PBRLit.glsl`) — sampled automatically.
     - Custom `MaterialBase` shaders inherit the `shadowSampler` declaration via `Common.glsl`; they sample shadows only if their author calls `SampleDirectionalShadow(worldPos)` (or rolls their own equivalent).

That's it. Toggle the flags, point the light downward, and you should see shadow patches under your casters on receiver geometry.

---

## What casts what

| Caster node | Casts shadows? |
|---|---|
| `StaticMesh3D` | ✅ |
| `SkeletalMesh3D` | ✅ (CPU- and GPU-skinned variants both wired) |
| `InstancedMesh3D` | ❌ (not yet wired in shadow pass; would need a similar short-circuit in `DrawInstancedMeshComp`) |
| `Particle3D` | ❌ |
| `TileMap2D` | ❌ |
| `Terrain3D` | ❌ |
| `Voxel3D` | ❌ |
| `TextMesh3D` | ❌ |
| `ShadowMesh3D` | Uses the engine's separate **simple shadow** pipeline (alpha-blended quads), not the depth-map system. |

Receiver support is symmetrical to the materials list above — any mesh whose material samples the shadow map receives shadows. There is no per-mesh "Receive Shadows" toggle for dynamic shadows; the property exists but only affects the legacy `ShadowMesh3D` blob system.

---

## Configuration

### `Constants.h`

```cpp
#define SHADOW_MAP_RESOLUTION 2048   // square shadow map
#define SHADOW_RANGE          50.0f  // XY ortho half-extent (units)
#define SHADOW_RANGE_Z        400.0f // along-light-axis half-extent (units)
```

`SHADOW_RANGE` controls how wide an area the shadow map covers. With the default 50, the visible shadow region is a 100×100 footprint **centered on the active camera**. Geometry more than 50 units from where the camera is looking is outside the ortho's XY range and won't appear in the shadow map.

`SHADOW_RANGE_Z` controls the near/far extents along the light direction. 400 units should accommodate any reasonable indoor or small outdoor scene. Bumping it dramatically (e.g. 10,000) costs depth precision and can produce shadow acne.

Increasing `SHADOW_RANGE` to cover a larger area is the easy knob for sprawling scenes, but it trades quality: the same 2048² shadow map now covers more world, so individual shadows get blockier.

### Per-shader bias

PCF + bias logic lives in:

- `Engine/Shaders/GLSL/src/Forward.frag` → `SampleDirShadow(worldPos)`
- `Engine/Shaders/GLSL/src/ForwardSpec.frag` → `SampleDirShadow(worldPos)`
- `<your project>/PBRLit.glsl` → `SampleDirectionalShadow(worldPos)`

Each uses a hardcoded bias of `0.0005` and a 3×3 PCF (9 taps). If shadows show acne (self-shadow speckling on lit surfaces), increase the bias. If shadows look "peter-panned" (detached from the caster), decrease it.

---

## How it follows the camera

The shadow ortho is **centered on the active camera's world position** each frame:

```
eye    = active_camera_world_position
target = eye + directional_light_forward
ortho  = orthoRH(-50, 50, -50, 50, -400, 400)
```

This means the shadow region "drags along" with the camera — useful for in-game where the player camera defines the play area, but also why shadows in the editor follow the editor viewport camera rather than your scene `Camera3D` until you enter Play / Game Preview.

For very large scenes, the proper solution is **cascaded shadow maps** (CSM): split the camera frustum into 2–4 depth ranges, each with its own shadow map. Not currently implemented.

---

## Architecture (engine internals)

For engine maintainers — what each piece does, where to find it, what to touch if you're extending the system.

### The shadow render pass

`Renderer::Render` (`Engine/Source/Engine/Renderer.cpp`) dispatches the shadow pass before the main forward pass:

```cpp
GFX_BeginRenderPass(RenderPassId::Shadows);
DirectionalLight3D* dirLight = /* first registered light */;
if (dirLight && dirLight->ShouldCastShadows())
{
    RenderDraws(mShadowDraws, PipelineConfig::Shadow);
}
GFX_EndRenderPass();
```

`mShadowDraws` is populated during gather: any `Primitive3D` with `mCastShadows == true` is added (no frustum culling — the shadow ortho's frustum differs from the camera's, so we can't reuse camera-frustum culling here).

### `mShadowViewProj`

`VulkanContext::UpdateGlobalUniformData` (`VulkanContext.cpp`) builds the shadow view-projection matrix each frame:

```cpp
glm::vec3 dir = firstDirLight->GetDirection();
glm::vec3 eye = activeCamera->GetWorldPosition();
glm::mat4 view = glm::lookAtRH(eye, eye + dir, /*up=*/...);
glm::mat4 proj = glm::orthoRH(-SHADOW_RANGE, SHADOW_RANGE,
                              -SHADOW_RANGE, SHADOW_RANGE,
                              -SHADOW_RANGE_Z, SHADOW_RANGE_Z);
mShadowViewProj = clip_y_flip * proj * view;   // y-flip for Vulkan NDC
```

The matrix is written into the global uniform buffer at the same time as the camera VP matrix, so shadow casters (`Shadow.vert` / `ShadowSkinned.vert`) and shadow receivers (`Forward.frag` / `ForwardSpec.frag` / `PBRLit.glsl`) all use the same `global.mShadowViewProj` and stay in lockstep.

The matrix is computed by iterating `world->GetLights()` rather than `world->FindNode<DirectionalLight3D>()` — this ensures we pick the same light instance the engine populates `global.mLights[]` with, otherwise scenes with multiple directional lights can produce shadow-vs-lighting mismatches.

### The shadow map image

`mShadowMapImage` is a `VK_FORMAT_D16_UNORM` depth attachment at 2048×2048 with a `CLAMP_TO_BORDER` sampler whose border color is opaque white (so out-of-bounds samples return `depth = 1.0` = far plane = "not in shadow"). It's bound to every material at `set=0, binding=1` as `shadowSampler`.

The image is kept in `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` outside the shadow pass. Each frame, `BeginRenderPass` for `RenderPassId::Shadows` transitions it to `VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL` for writing; `EndRenderPass` transitions it back.

### Cast-side pipelines

`PipelineConfig::Shadow` uses `Shadow.vert` + `Shadow.frag` (now an empty no-op fragment shader — depth is written by the rasterizer, no per-pixel work needed). The pipeline is fixed at `VertexType::Vertex`, but the per-mesh draw functions override the vertex type / shader to match the mesh's actual vertex format:

- `DrawStaticMeshComp` (in `VulkanUtils.cpp`) — sets `VertexType::Vertex` or `VertexType::VertexColor` based on `mesh->HasVertexColor()`. Without this, a `VertexColor` mesh (stride 44) would be misread at `Vertex` stride (40) and rasterize to garbage.
- `DrawSkeletalMeshComp` — for CPU-skinned meshes (pre-skinned positions in basic `Vertex` format), uses the default `Shadow.vert`. For GPU-skinned meshes, swaps in `ShadowSkinned.vert` and sets `VertexType::VertexSkinned`, so bone-indexed skinning happens on the GPU during the shadow pass.

Both draw functions check `GetVulkanContext()->GetCurrentRenderPassId() == RenderPassId::Shadows` and return early after the depth draw — they don't bind the mesh's material (Shadow.frag doesn't need it) and they don't call `BindMaterialResource` (which would overwrite the Shadow pipeline shaders with the material's forward shaders).

### Receive-side helper

Material fragment shaders sample the shadow map with `SampleDirectionalShadow` (PBR) or `SampleDirShadow` (Forward / ForwardSpec). Both implementations:

1. Project the fragment's world position through `global.mShadowViewProj` → clip → NDC → texture UV.
2. Bounds-check against `[0,1]^3` — out-of-frustum returns `1.0` (fully lit) so geometry past the shadow ortho extent doesn't suddenly turn black.
3. Sample a 3×3 PCF kernel for soft edges.
4. Compare each tap's depth against the receiver's projected depth + a 0.0005 bias.
5. Return the average of the 9 comparisons (0.0 → 1.0).

Visibility is then multiplied into the directional light's `radiance` term in the shading loop, so it dampens both direct diffuse and direct specular consistently.

### Adding a new caster mesh type

If you add a new mesh type and want it to cast shadows, follow the pattern in `DrawStaticMeshComp` / `DrawSkeletalMeshComp`:

```cpp
void DrawMyMeshComp(MyMesh3D* comp)
{
    // ... bind mesh vertex/index buffers ...

    if (GetVulkanContext()->GetCurrentRenderPassId() == RenderPassId::Shadows)
    {
        // Set the vertex type to match the mesh's actual stride.
        GetVulkanContext()->SetVertexType(MyVertexType);

        // If the mesh has a special vertex format requiring custom skinning /
        // displacement / etc., override the vertex shader:
        // GetVulkanContext()->SetVertexShader("MyShadow.vert");

        GetVulkanContext()->CommitPipeline();
        BindGeometryDescriptorSet(comp);  // set=1 mWorldMatrix
        vkCmdDrawIndexed(cb, /* index count */, 1, 0, 0, 0);
        return;
    }

    // ... normal forward draw path ...
}
```

Don't call `BindForwardVertexType`, `BindMaterialResource`, or `BindMaterialDescriptorSet` — those will trample the Shadow pipeline state and bind material descriptors the empty `Shadow.frag` doesn't need.

---

## Known limitations

- **One directional shadow caster.** Only the first registered `DirectionalLight3D` casts shadows. Multi-sun scenes get only one set of shadows.
- **No point-light shadows.** Would require cubemap shadow maps — not implemented.
- **No spot-light shadows.** Would require per-spot shadow maps — not implemented.
- **No cascaded shadow maps.** Single shadow map covering a ±50 unit area around the camera. Quality drops linearly with `SHADOW_RANGE`.
- **`Shadow.frag` is empty.** Alpha-masked materials (foliage, fences, windows) cast solid bounding-shape shadows instead of punched-out shadows. To fix, restore `Shadow.frag`'s alpha-test logic and bind a per-material descriptor set in the shadow short-circuit — currently skipped because each `MaterialBase` variant has its own set=2 layout and matching them all up wasn't worth the complexity.
- **No "Receive Shadows" toggle.** Every material that samples the shadow map receives shadows unconditionally. Disabling shadows on a per-receiver basis would require a uniform branch or a separate material variant.
- **Console backends ignore the cast flag.** Toggling `Cast Shadows` on a node has no visible effect on GX or C3D builds. Use baked lighting on those platforms.
- **Shadow box follows the active camera.** Outside the ±50 unit footprint, shadows fade out. Move the camera and shadows "track" with it. Cascaded shadow maps would fix this.

---

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| No shadows on anything | Caster's `Cast Shadows` is `false` (default); or DirectionalLight3D's `Cast Shadows` is off; or there's no DirectionalLight3D in the scene at all. |
| Shadows appear only when camera is close | `SHADOW_RANGE` is too small for your scene. Increase it (or rebuild with a larger value). |
| Shadows rotate with the editor camera but ignore the light's rotation | You're rotating a non-active light, or the light's matrix is stuck. The shadow matrix uses the first light in `world->GetLights()` — make sure that's the one you're rotating. |
| Shadow acne (speckling on lit surfaces) | Bias too low. Bump `0.0005` in the receiver shader's PCF loop. |
| Peter-panning (shadow detached from caster's feet) | Bias too high. Decrease it. |
| Static meshes cast, skeletal meshes don't | Skeletal mesh's `Cast Shadows` defaults to `false` — toggle it explicitly. |
| Shadows look correct on PBR materials but not on lite materials | Editor was never restarted after a `Forward.frag` / `ForwardSpec.frag` change — global shaders compile once at startup. |
