# Terrain3D in Polyphase Engine

## Overview

`Terrain3D` is a heightmap-based terrain node that generates a renderable mesh grid from per-vertex height data. It supports multi-material painting via splatmap and texture atlas, procedural splatmap generation from elevation/slope rules, collision, NavMesh integration, heightmap import, grayscale brush masks, terrain chunk snapping, and baked texture export.

Key features:
- Configurable grid resolution and world-space dimensions
- Runtime height manipulation via C++ and Lua
- Import heightmap from grayscale textures
- 4-slot material system with texture atlas support
- Baked splatmap textures for smooth per-pixel material blending
- Procedural splatmap generation from elevation and slope rules (Auto-Generate + Mountain presets)
- Grayscale brush mask textures for custom brush shapes
- Stamp tools for quickly applying height/material from textures
- Automatic collision generation (Bullet physics)
- NavMesh triangle contribution for pathfinding
- Snap grid for aligning terrain chunks edge-to-edge
- Bake & Save system with companion .oct files for PIE and runtime
- Works on all platforms (Vulkan, GX, C3D) using MaterialLite

---

## Quick Start

### Creating a Terrain

1. **Right-click in the Scene hierarchy** > Add Node > 3D > Terrain3D, OR
2. **Right-click in the Viewport** > Terrain

A save dialog appears. Navigate to your project's **Assets** folder and type a base name (e.g., `MyTerrain`). Two companion files are created automatically:
- `MyTerrain_Splatmap.oct` — placeholder baked splatmap texture
- `MyTerrain_Heightmap.oct` — grayscale heightmap texture

The terrain spawns with collision enabled and atlas/material slots pre-configured.

### Basic Sculpting Workflow

1. Select the Terrain3D node
2. Switch to **Terrain Sculpt** mode (toolbar dropdown or "Open In Terrain Sculpt" in Properties)
3. Use **Raise/Lower/Flatten/Smooth** modes to sculpt the heightmap
4. Assign an **Atlas Texture** in the Properties inspector (Atlas Texturing section)
5. Set **Tiles X/Y** to match your atlas grid
6. Set tile indices for each slot in **Material Slot Tiles**
7. Switch to **Paint Mat** mode to paint materials
8. Click **Bake & Save Map** to export the blended texture
9. The baked map persists in PIE and runtime builds

---

## How It Works

### Heightmap Storage

- **Data format**: `std::vector<float>` with one height value per vertex, stored row-major (Z-major)
- **Default resolution**: 128x128 vertices
- **Default height scale**: 10.0 (matches Blender/DCC conventions)
- **Coordinate system**: Integer grid coordinates (x, z) starting from (0, 0)
- **Height values**: Raw floats multiplied by `mHeightScale` during mesh generation

### Mesh Generation Pipeline

1. When heights or splatmap are modified, the node is marked dirty
2. On the next tick, `RebuildMeshInternal()` is called
3. **Non-atlas mode**: A regular grid of shared vertices with simple UV mapping
4. **Atlas mode**: Per-triangle unique vertices (unshared) to prevent UV discontinuities at tile boundaries — same approach as Voxel3D
5. Per-vertex normals are computed via central differences on the heightmap
6. The mesh is uploaded to the GPU
7. Collision shape is rebuilt from the same triangle data

### Material System

#### Texture Atlas

The terrain uses a texture atlas (a single image containing a grid of material tiles) combined with a splatmap to determine which material appears where.

**Setup:**
1. Enable Atlas = checked
2. Set Atlas Texture to your atlas image (e.g., 512x256 with 64x64 tiles)
3. Set Tiles X/Y to match (e.g., 8x4)
4. Use Material Slots = checked
5. Assign tile indices to each slot in Material Slot Tiles

#### Splatmap

Each vertex stores 4 material weights packed as RGBA in vertex color:
- R channel = Slot 0 weight
- G channel = Slot 1 weight
- B channel = Slot 2 weight
- A channel = Slot 3 weight

Values are 0-255. When painting, weights are normalized so they sum to 1.0.

#### Baked Splatmap Texture

For smooth per-pixel material transitions (instead of per-triangle), enable **Bake Splatmap**. This generates a texture on the CPU where each pixel is the weighted blend of all 4 material tiles sampled from the atlas:

1. Check **Bake Splatmap** in the Atlas Texturing properties
2. Set **Bake Resolution** (512 default, increase for higher quality)
3. Set **Texture Tiling** (controls how many times each material tile repeats — e.g., 8 = 8 grid cells per repeat)
4. Click **Bake & Save Map** to export

The baked texture is saved as a `.oct` asset and assigned to the **Baked Map** property. It persists across PIE, runtime builds, and sessions.

#### Slot Tint Colors

Each material slot has a tint color that modulates the atlas tile during baking. Default is white (no tint). Set to gray (~0.5) if textures appear too bright with the Lit shading model.

### Brush Mask System

The editor supports grayscale texture brush masks for custom brush shapes:

- Any RGBA texture works (R channel is sampled)
- White (255) = full brush effect, Black (0) = no effect
- The mask is mapped to the brush radius area
- When no mask is set, a radial cosine falloff is used

**To assign a brush mask:**
1. Import a grayscale brush texture as a `.oct` asset
2. Drag it from the Assets panel onto the **"Drop Brush Mask Here"** button in the Terrain Sculpt panel

**Stamp tools** (appear when a brush mask is assigned):
- **Stamp as Height** — replaces the entire heightmap with the mask (white=high, black=low)
- **Add Height** — additively blends the mask onto existing heights using current brush Strength
- **Stamp as Splat** — stamps the mask as splatmap weight for the selected material slot, normalizes other slots, and auto-saves

---

## Editor Tools

### Entering Terrain Sculpt Mode

1. Select a Terrain3D node in the scene
2. Click **"Open In Terrain Sculpt"** in the Properties inspector, or
3. Select **"Terrain Sculpt"** from the editor mode dropdown in the toolbar

### Sculpt Modes

| Mode | Color | Description |
|------|-------|-------------|
| Raise | Green | Increase height under brush |
| Lower | Red | Decrease height under brush |
| Flatten | Yellow | Blend heights toward target value |
| Smooth | Blue | Average heights with neighbors |
| Paint Mat | Purple | Paint material slot weights |

### Brush Settings

- **Radius**: World-space brush radius (0.5 - 50)
- **Strength**: Effect intensity per frame (0.01 - 1.0)
- **Falloff**: Edge softness when no brush mask (0 = hard, 1 = soft cosine)
- **Brush Mask**: Optional grayscale texture for custom brush shape (drag from Assets panel)
- **Target Height**: Only for Flatten mode
- **Material Slot**: Only for Paint Material mode (0-3), with tile texture previews

### Material Slot Tiles

The **Material Slot Tiles** section (open by default) lets you assign which atlas tile each slot uses:
- 32px tile texture preview for each slot
- Input field with +/- buttons to set the tile index
- Changes take effect on next bake

### Auto-Generate Splatmap

Procedurally generate the splatmap from elevation and slope rules:

**Per-slot controls:**
- **Height Min/Max** — normalized elevation range (0=lowest point, 1=highest point on terrain)
- **Slope Min/Max** — angle in degrees (0=flat ground, 90=vertical cliff)
- **Blend** — smooth falloff at rule edges
- **Strength** — overall weight multiplier

**Default rules:**

| Slot | Preset | Height Range | Slope Range |
|------|--------|-------------|-------------|
| Slot 0 | Grass | 0% - 55% | 0 - 30 deg |
| Slot 1 | Rock | 0% - 100% | 30 - 90 deg |
| Slot 2 | Snow | 60% - 100% | 0 - 45 deg |
| Slot 3 | Dirt | 30% - 70% | 10 - 40 deg |

**Buttons:**
- **Generate Splatmap** — generates + auto-saves baked map to disk
- **Generate (Preview)** — generates + preview only (no save)

### Mountain Generate Splatmap

A simplified preset for mountain terrain with 4 named zones:

| Zone | Default Slot | Description |
|------|-------------|-------------|
| Ground | Slot 0 | Low elevation, flat areas (grass/dirt) |
| Mountain Side | Slot 1 | Steep slopes at any height (rock/cliffs) |
| Mountain Top | Slot 2 | High flat areas (alpine grass/gravel) |
| Peak | Slot 3 | Highest points (snow/ice) |

**Controls:**
- Each zone has a **Slot** selector to remap which atlas tile it uses
- **Ground Max Height**: elevation cutoff for lowlands
- **Mountain Side Min Slope**: angle threshold for steep slopes
- **Mountain Top Min Height**: where mountain tops begin
- **Peak Min Height**: where the very top begins
- **Blend**: shared edge softness

**Buttons:**
- **Generate Mountain** — generates + auto-saves
- **Preview Mountain** — generates + preview only

### Actions

- **Flatten All** — reset all heights to 0
- **Rebuild Mesh** — force immediate mesh rebuild
- **Bake Splatmap** — preview the baked blend texture (no save)
- **Bake & Save Map** — bake and save to disk (auto-overwrites if already saved)
- **Save Heightmap** — export heightmap as grayscale texture asset

### Debug Splatmap

Check **Debug Splatmap** to visualize raw splatmap weights as vertex color tints:
- Red = Slot 0
- Green = Slot 1
- Blue = Slot 2
- Alpha = Slot 3

Hover info shows per-vertex weight percentages.

### Warning Banner

A yellow warning appears when atlas + material slots are enabled but no Baked Map is saved. Click "Bake & Save Map" to resolve.

---

## C++ API

### Height Access

```cpp
void SetHeight(int32_t x, int32_t z, float height);
float GetHeight(int32_t x, int32_t z) const;
float GetHeightAtWorldPos(float worldX, float worldZ) const; // Bilinear interpolation
void SetHeightFromTexture(Texture* texture); // Import from grayscale image
void FlattenAll(float height = 0.0f);
void Resize(int32_t resX, int32_t resZ, float worldW, float worldD);
```

### Material Slots

```cpp
void SetMaterialSlot(int32_t slot, Material* mat); // 0-3, MaterialLite only
Material* GetMaterialSlot(int32_t slot) const;
void SetMaterialWeight(int32_t x, int32_t z, int32_t slot, float weight);
float GetMaterialWeight(int32_t x, int32_t z, int32_t slot) const;
```

### Baking & Export

```cpp
void BakeSplatmapTexture();                           // Runtime bake (preview)
void BakeAndSaveMap(const std::string& path = "");    // Bake + save to disk
void BakeAndSaveHeightmap(const std::string& path = ""); // Export heightmap
void GenerateSplatmapFromRules();                      // Procedural splatmap from rules
```

### Mesh Control

```cpp
void MarkDirty();      // Mark for rebuild on next tick
void RebuildMesh();    // Force immediate rebuild
bool IsDirty() const;  // Check if rebuild pending
```

### Accessors

```cpp
int32_t GetResolutionX() const;
int32_t GetResolutionZ() const;
float GetWorldWidth() const;
float GetWorldDepth() const;
uint32_t GetNumVertices() const;
uint32_t GetNumIndices() const;
const std::vector<VertexColor>& GetVertices() const;
const std::vector<IndexType>& GetIndices() const;
```

---

## Lua API

### Example Usage

```lua
-- Create a terrain
local terrain = world:SpawnNode("Terrain3D")
terrain:SetPosition(Vec(0, 0, 0))

-- Set some heights
for x = 0, 127 do
    for z = 0, 127 do
        local h = math.sin(x * 0.1) * math.cos(z * 0.1) * 2.0
        terrain:SetHeight(x, z, h)
    end
end

-- Import a heightmap texture
local hmap = LoadAsset("T_Heightmap")
terrain:SetHeightFromTexture(hmap)

-- Paint material slot 1 in a region
for x = 20, 40 do
    for z = 20, 40 do
        terrain:SetMaterialWeight(x, z, 1, 1.0)
        terrain:SetMaterialWeight(x, z, 0, 0.0)
    end
end

-- Force rebuild
terrain:RebuildMesh()
```

### Method Reference

| Method | Arguments | Returns | Description |
|--------|-----------|---------|-------------|
| `SetHeight` | `x, z, height` | - | Set height at grid position |
| `GetHeight` | `x, z` | `float` | Get height at grid position |
| `GetHeightAtWorldPos` | `worldX, worldZ` | `float` | Bilinear interpolated height |
| `SetHeightFromTexture` | `texture` | - | Import grayscale heightmap |
| `FlattenAll` | `[height]` | - | Set all heights (default 0) |
| `Resize` | `resX, resZ, worldW, worldD` | - | Resize grid (clears data) |
| `GetResolution` | - | `x, z` | Get grid resolution |
| `GetWorldSize` | - | `w, d` | Get world dimensions |
| `GetHeightScale` | - | `float` | Get height multiplier |
| `SetHeightScale` | `scale` | - | Set height multiplier |
| `MarkDirty` | - | - | Mark for rebuild |
| `RebuildMesh` | - | - | Force immediate rebuild |
| `IsDirty` | - | `bool` | Check if rebuild pending |
| `SetMaterialSlot` | `slot, material` | - | Set material (0-3) |
| `GetMaterialSlot` | `slot` | `material` | Get material at slot |
| `SetMaterialWeight` | `x, z, slot, weight` | - | Set per-vertex weight |
| `GetMaterialWeight` | `x, z, slot` | `float` | Get per-vertex weight |
| `SetSnapGridSize` | `size` | - | Set snap grid (0=off) |
| `GetSnapGridSize` | - | `float` | Get snap grid size |

---

## Platform-Specific Behavior

### Vulkan (Windows / Linux / Android)

- Full support with dedicated vertex/index buffer management
- Uses `VertexColor` vertex type with the `ForwardColor` shader pipeline
- Single draw call per Terrain3D node

### GameCube / Wii (GX)

- Full support via display list generation with automatic batching for meshes exceeding 65,535 indices
- Same vertex attribute layout as Voxel3D
- Per-element endian-safe serialization for cross-platform compatibility
- Consider smaller resolutions (64x64 or less) for memory constraints

### 3DS (C3D)

- Full support via double-buffered vertex/index data
- Same shader pipeline as static meshes
- Consider smaller resolutions for performance

### Material Compatibility

Terrain3D uses `MaterialLite` exclusively, ensuring compatibility across all platforms including Wii and 3DS.

---

## Serialization

Terrain data is embedded in scene files with versioned fields:

```
ASSET_VERSION_TERRAIN3D (23):
  - 2x int32: resolution (X, Z)
  - 3x float: world width, world depth, height scale
  - 1x float: snap grid size
  - Per-float heightmap data (endian-safe)
  - Per-uint32 splatmap data (endian-safe)
  - 4x AssetRef: material slot references
  - 1x AssetRef: heightmap texture reference

ASSET_VERSION_TERRAIN3D_MATSLOTS (24):
  - bool: use material slots

ASSET_VERSION_TERRAIN3D_ATLAS (25):
  - bool: enable atlas texturing
  - AssetRef: atlas texture
  - uint32: tiles X, tiles Y
  - float: texture tiling
  - Per-slot: int32 tile index + vec4 tint color

ASSET_VERSION_TERRAIN3D_BAKE (26):
  - bool: bake splatmap
  - uint32: bake resolution

ASSET_VERSION_TERRAIN3D_BAKEDMAP (27):
  - AssetRef: baked map texture
```

---

## Collision

Terrain3D generates a `btBvhTriangleMeshShape` from its mesh geometry:
- Built automatically when the mesh is rebuilt
- Enabled by default on spawn (collision group 1)
- Supports physics raycasting (used by the editor sculpt tools)
- Internal edge info generated for smoother physics behavior

---

## NavMesh Integration

Terrain3D nodes automatically contribute their triangles to the NavMesh system:
- `World::GatherNavTriangles()` includes Terrain3D triangles
- Triangles are filtered by NavMesh3D bounds (same as StaticMesh3D)
- Call `BuildNavigationData()` after terrain changes to update pathfinding

---

## Terrain Snapping

Multiple terrain chunks can be placed edge-to-edge:
- Set `Snap Grid` property to match `World Width` (e.g., 128.0)
- Position is automatically snapped to multiples of the grid size
- All terrain chunks with the same resolution and world size align their edges

---

## Companion Files

When a Terrain3D is created via the editor, companion `.oct` texture assets are generated:

| File | Purpose |
|------|---------|
| `{Name}_Splatmap.oct` | Baked splatmap texture (blended materials) |
| `{Name}_Heightmap.oct` | Grayscale heightmap export |

These files:
- Are saved to the user's chosen directory via save dialog
- Auto-overwrite on subsequent bakes (no repeated dialogs)
- Can be shared between projects
- Work in PIE and all runtime platforms

---

## Configurable Properties

### Terrain Section

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| Resolution X | Integer | 128 | Grid vertices along X |
| Resolution Z | Integer | 128 | Grid vertices along Z |
| World Width | Float | 128.0 | World-space extent X |
| World Depth | Float | 128.0 | World-space extent Z |
| Height Scale | Float | 10.0 | Height value multiplier |
| Snap Grid | Float | 0.0 | Position snap grid (0=off) |
| Use Material Slots | Bool | false | Enable splatmap vertex colors |
| Heightmap Texture | Texture | None | Source heightmap (auto-imports on set) |

### Atlas Texturing Section

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| Enable Atlas | Bool | false | Enable atlas-based texturing |
| Atlas Texture | Texture | None | Texture atlas image |
| Tiles X | Integer | 4 | Atlas grid columns |
| Tiles Y | Integer | 4 | Atlas grid rows |
| Texture Tiling | Float | 32.0 | Grid cells per texture repeat |
| Slot 0-3 Tile | Integer | 0,1,2,3 | Atlas tile index per slot |
| Slot 0-3 Tint | Color | White | Per-slot color multiplier |
| Bake Splatmap | Bool | false | Enable CPU-blended baked texture |
| Bake Resolution | Integer | 512 | Baked texture size (NxN) |
| Baked Map | Texture | None | Saved baked texture asset |

---

## Practical Guidance

### When to Use Terrain3D

**Good use cases:**
- Outdoor terrain and landscapes
- Height-based procedural generation
- Levels with large ground surfaces
- Multi-material terrain (grass, dirt, rock, snow)

**Consider alternatives when:**
- Cave interiors or overhangs (heightmaps can't represent overhangs)
- Very small detail areas (use StaticMesh3D instead)
- Voxel-based terrain (use Voxel3D)

### Recommended Workflow

1. Create Terrain3D via right-click menu (saves companion files)
2. Import a heightmap texture OR sculpt with Raise/Lower tools
3. Assign atlas texture, set tile grid dimensions
4. Set slot tile indices in Material Slot Tiles
5. Use **Mountain Generate** for quick procedural materials
6. Fine-tune with **Paint Mat** mode
7. Click **Bake & Save Map** to export final texture
8. Test in PIE

### Performance Considerations

- **Resolution**: 128x128 = 16K verts (shared) or ~97K verts (atlas mode). 64x64 recommended for GCN/3DS
- **Bake resolution**: Higher = better quality but more memory. 512 is good default, 1024 for detailed terrains
- **Atlas mode**: Uses 6x more vertices than non-atlas (per-triangle unshared). Disable atlas for performance-critical platforms
- **Rebuild frequency**: Batch height changes before the next tick
- **Draw calls**: Each Terrain3D node is one draw call

### Best Practices

1. **Batch modifications**: Set multiple heights before the next tick to avoid multiple rebuilds
2. **Use heightmap import**: More efficient than sculpting for complex terrain shapes
3. **Bake before shipping**: Always click Bake & Save Map so the texture persists in runtime
4. **Tint colors**: Set to ~0.5 gray if textures appear too bright with Lit shading
5. **Stamp tools**: Use brush mask stamps for quickly applying height/material patterns
6. **Auto-generate first, paint second**: Use Mountain Generate for the base, then fine-tune with manual painting

---

## Future Roadmap

Features planned for future phases:

1. **Custom terrain shader**: Per-pixel splatmap blending in the fragment shader (eliminates bake step)
2. **LOD / Geomipmapping**: Distance-based mesh simplification
3. **Chunked streaming**: Load/unload terrain chunks based on camera distance
4. **Edge stitching**: Automatic height averaging at shared edges between chunks
5. **Erosion tools**: Hydraulic and thermal erosion brushes
6. **Foliage painting**: Scatter instanced meshes based on splatmap weights
7. **Multi-layer atlas**: Support for more than 4 material slots via multiple atlases
