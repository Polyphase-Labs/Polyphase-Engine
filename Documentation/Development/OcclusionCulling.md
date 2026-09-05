# Occlusion Culling

Occlusion culling skips drawing meshes that are inside the camera frustum but completely hidden
behind other geometry. Polyphase uses a **baked potentially-visible-set (PVS)**, the same model as
Unity's occlusion culling: the editor precomputes visibility once, and the runtime does a single
cell lookup plus one bit test per draw. Nothing runs on the GPU, so the feature works identically on
every target.

| Platform | Runtime culling | Bake |
|----------|-----------------|------|
| Windows / Linux (Vulkan) | Yes | Yes (editor) |
| Wii / GameCube (GX) | Yes | – |
| 3DS (C3D) | Yes | – |
| Addon platforms (PSP, PS2, Web, …) | Yes | – |

## Quick start

1. Select the walls, floors, and large props that should **block** the view and tick
   **Primitive → Occlusion → Occluder Static**.
2. Select the meshes that should be **hidden** when they are behind an occluder and tick
   **Occludee Static**. Most static geometry is both. `World → Mark Selected Occluder + Occludee`
   sets both flags on the current selection.
3. Optionally add one or more **Occlusion Area** nodes (`Spawn Node → 3D → Environment`) covering the
   volume the camera can reach. Without them the bake volume is the union of all occludee bounds,
   which can be much larger than needed.
4. In the scene asset's properties enable **Occlusion Culling** and set a **Cell Size** (default
   4 units) and **Bake Quality**.
5. `World → Bake Occlusion Culling`. A cancellable progress modal shows the cell count and progress.
6. Save the scene. The baked data is stored inside the scene asset and is cooked for every platform.

To see the result in the editor viewport enable `View → Occlusion Preview` (and `View → Bounds` to
draw the bake volume in cyan, the camera's current cell in green, and every culled mesh in red).
The **Stats** overlay has a `Culling` display mode showing frustum- and occlusion-culled draw counts
(summed over every view rendered that frame, so the Game Preview panel counts separately from the viewport).

The editor's REST controller exposes the same workflow for tooling: `POST /api/occlusion/bake`,
`POST /api/occlusion/clear`, `GET /api/occlusion/status` (baked data summary, live counters, per-node
slots) and `PUT /api/occlusion/preview` with `{"enabled": true}`.

## How it works

- The bake gathers world-space triangles from every visible **Occluder Static** `StaticMesh3D`,
  `InstancedMesh3D` (every instance), `Terrain3D` and `Voxel3D` with an opaque material, and builds a
  BVH over them.
- The bake volume is split into uniform cells. For every cell a handful of sample points (skipping
  points that are inside solid geometry) cast rays to sample points on every occludee (AABB corners,
  face centers, and a stride of mesh vertices). The first unblocked ray marks the occludee visible
  from that cell.
- Identical per-cell bitsets are shared, so large grids with repetitive visibility stay small.
- At runtime `Renderer::OcclusionCull` runs right after frustum culling. It looks up the camera's cell
  and removes draws whose bit is clear. Shadow casters are never occlusion culled, and skeletal
  animation / particle simulation keep following the frustum result.
- Occludees are matched to the baked data by their world-space bounds when the scene is
  instantiated. An occludee that moves after the bake (in the editor or from a script) is treated as
  always visible. If an **occluder** is moved or deleted the data is flagged stale, a warning is
  logged, and culling is disabled until you re-bake.

| Bake Quality | Samples per cell | Targets per occludee |
|--------------|------------------|----------------------|
| Low | 4 | 9 |
| Medium | 9 | 15 + 8 mesh vertices |
| High | 17 | 15 + 24 mesh vertices |

## Limitations

- Static only. Moving objects are never culled, and moving an occluder invalidates the bake.
- Cell size is a trade-off: smaller cells give tighter culling but more cells to bake and store.
  The bake refuses more than 262,144 cells; use `OcclusionArea3D` nodes to bound large worlds.
- Translucent, additive, and masked materials never occlude.
- `InstancedMesh3D` is one occludee (its whole bounds), including the unrolled cells consoles create
  on load.
- Ray sampling is an approximation. A mesh seen only through a very small gap may pop; raise the
  bake quality or reduce the cell size.

## API

C++:

```cpp
Renderer::Get()->EnableOcclusionCulling(bool);
Renderer::Get()->IsOcclusionCullingEnabled();
Renderer::Get()->GetNumOcclusionCulled();
Renderer::Get()->GetNumFrustumCulled();
prim->EnableOccluder(bool);   prim->IsOccluder();
prim->EnableOccludee(bool);   prim->IsOccludee();
world->IsOcclusionCullingEnabled();
world->IsOcclusionDataStale();
```

Lua:

```lua
Renderer.EnableOcclusionCulling(false)
local n = Renderer.GetNumOcclusionCulled()
mesh:EnableOccludee(true)
if mesh:IsOccluder() then ... end
```
