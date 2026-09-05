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

Frustum culling is separate and always on: it drops anything outside the view cone. Occlusion
culling handles the rest, things that are in view but behind something. The Stats overlay's
`Culling` mode and the free-fly camera HUD show both counts.

## Quick start

1. Select the walls, floors, and large props that should **block** the view and tick
   **Primitive → Occlusion → Occluder Static**.
2. Select the meshes that should be **hidden** when they are behind an occluder and tick
   **Occludee Static**. `World → Mark Selected Occluder + Occludee` sets both on the selection.
3. Add one or more **Occlusion Area** nodes (`Spawn Node → 3D → Environment`) covering the volume the
   camera can reach. Without them the bake volume is the union of all occludee bounds, which is
   usually far bigger than needed (a tall perimeter wall alone can push it past the cell limit).
4. In the scene asset's properties enable **Occlusion Culling**, then set **Occlusion Cell Size**,
   **Bake Quality**, **Dynamic Objects** and the **Console Budget KB** (see below).
5. `World → Bake Occlusion Culling`. A cancellable progress modal shows the cell count and progress.
   Re-bakes after moving props are incremental (see below).
6. Save the scene. The baked data is stored inside the scene asset and is cooked for every platform.

### What to flag

Occluders want to be **big and few**; occludees want to be **small and many**. An occludee is only
culled when *all* of it is hidden from the camera's cell, so an object that spans the level is
almost never culled.

| Object | Occluder | Occludee |
|---|---|---|
| Floor / terrain | yes | no |
| Perimeter walls, mountains, distant scenery | yes | no, unless split into segments |
| Interior walls, buildings, big props | yes | yes |
| Pillars, crates, small props | optional | yes |
| Foliage, glass, particles, anything not opaque | no | yes if the mesh is opaque, otherwise leave off |
| Doors and anything that moves | no | yes (culled by the dynamic table) |
| Skybox | never | never |

The bake logs a warning listing every occludee that spans more than 8 cells. Split those into
segments, or untick Occludee Static and keep them as occluders.

For interiors use a cell size smaller than a corridor width (2 units is typical), one Occlusion Area
per floor covering only walkable space, and make ceilings occluders too so cells cannot see over
walls into other rooms. Open doorways need no setup; they are simply holes in the geometry.

### Seeing the result

- `View → Occlusion Preview` (with `View → Bounds` on) draws in the editor viewport: the bake volume
  in cyan, the **game camera's** current cell in green, every occludee's box green (visible from that
  cell) or red (culled), and, for selected occludees, a line from the cell coloured by the result.
  The editor viewport itself is never culled; the Game Preview and Second Screen Preview panels are,
  each with its own camera (both 3DS screens are handled independently).
- The Stats overlay `Culling` mode shows frustum- and occlusion-culled draw counts. Counts are summed
  over every view rendered that frame.
- The scripts folder of the occlusion demo project has a `FreeFlyCam.lua` with an on-screen HUD and
  F1/F2 toggles for the two culling passes.

## How it works

- The bake gathers world-space triangles from every visible **Occluder Static** `StaticMesh3D`,
  `InstancedMesh3D` (every instance), `Terrain3D` and `Voxel3D` with an opaque material, and builds a
  BVH over them.
- The bake volume is split into uniform cells. For every cell a handful of sample points (skipping
  points inside solid geometry) cast rays to sample points on every occludee: box corners and face
  centres, a stride of mesh vertices, and for objects larger than two cells a grid of points across
  each face. The first unblocked ray marks the occludee visible from that cell.
- Identical per-cell bitsets are shared, and cell indices are stored as 16-bit values whenever fewer
  than 65,535 unique sets exist.
- **Dynamic objects.** When *Occlusion Dynamic Objects* is on, the bake also records which coarse
  cells (blocks of 4–16 cells per axis, chosen so there are about 512 of them) are visible from each
  cell. At runtime an occludee that has no baked slot (spawned, animated, or moved since the bake)
  is tested by its current bounds against that table. It is coarser than the per-object table but
  it costs nothing per frame beyond a few bit tests.
- **Instanced meshes.** Consoles unroll `InstancedMesh3D` into one draw per unroll cell on load. The
  bake records one occludee per unroll cell in addition to the whole node, so on consoles the cells
  cull individually (a forest behind a hill drops the hidden trees, not all or nothing).
- At runtime `Renderer::OcclusionCull` runs before frustum culling, so the frustum math is only
  spent on draws that survive the bit test. Culled draws receive the same "not in view" treatment as
  frustum-culled ones: skeletal meshes and particles follow their *Always Update* settings. Shadow
  casters are never occlusion-culled.
- Occludees and occluders are matched to the baked data by their world-space bounds when the scene
  is instantiated, with a tolerance of 10% of a cell or 1% of the object's size. An occludee that
  moves afterwards switches to the dynamic table. If an **occluder** moves or disappears the data is
  flagged stale, culling is disabled, and the editor shows a toast with a **Re-bake** button.
- **Incremental bakes.** If the grid and every occluder are unchanged since the last bake, the bits
  of occludees that already existed are copied and only new or moved occludees are ray-cast. The
  dynamic table is reused as well. Moving an occluder, changing the cell size, or changing the bake
  volume forces a full bake.

| Bake Quality | Samples per cell | Targets per occludee |
|--------------|------------------|----------------------|
| Low | 4 | 9 (+ face grid up to 6×6 per face on large objects) |
| Medium | 9 | 15 + 8 mesh vertices (+ face grid) |
| High | 17 | 15 + 24 mesh vertices (+ face grid up to 8×8) |

## Memory

Everything below is loaded into RAM with the scene on every platform. The bake log and the load-time
log (`Occlusion: resolved …`) print the actual sizes.

| Table | Size |
|---|---|
| Occludee / occluder entries | 24 bytes each |
| Static cell table | cells × 2 bytes (× 4 if there are ≥ 65,535 unique sets) |
| Static set pool | unique sets × ⌈occludees / 32⌉ × 4 bytes |
| Dynamic cell table | cells × 2 bytes |
| Dynamic set pool | unique coarse sets × ⌈coarse cells / 32⌉ × 4 bytes |

Worked example, a 230 × 60 × 220 unit level at 4-unit cells: 58 × 15 × 55 = 47,850 cells. With 30
occludees and 400 unique sets the static table is about 96 KB + 1.6 KB; the dynamic table adds
about 96 KB + (unique coarse sets × 32 bytes). Halving the cell size multiplies the cell count by 8.

Controls:

- **Occlusion Cell Size** is the main lever. Cells ÷ cell size³.
- **Occlusion Area** nodes bound the grid; the bake refuses more than 262,144 cells and warns above
  65,536.
- **Occlusion Dynamic Objects** off skips the second table entirely (roughly halves the data).
- **Occlusion Console Budget KB** (default 512, 0 = unlimited): when a scene is cooked for a console
  target (GameCube, Wii, 3DS, and addon consoles that report themselves as consoles) and its data is
  larger than this, the cook writes an empty table and logs a warning. Desktop builds and the editor
  copy always keep the full data. Use it to opt a scene out on the smallest targets without
  changing the scene itself.

## Limitations

- Moving an occluder invalidates the bake; only occludees may move freely.
- Cell size is a trade-off: smaller cells give tighter culling but more cells to bake and store.
- Translucent, additive, and masked materials never occlude.
- Dynamic occludees use the coarse table, so they are culled less aggressively than static ones.
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

REST (editor controller): `POST /api/occlusion/bake`, `POST /api/occlusion/clear`,
`GET /api/occlusion/status`, `PUT /api/occlusion/preview` with `{"enabled": true}`.
