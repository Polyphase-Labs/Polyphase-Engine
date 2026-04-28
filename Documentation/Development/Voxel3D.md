# Voxel3D in Polyphase Engine

## Overview

`Voxel3D` is a runtime-editable voxel mesh node that allows creation of block-based terrain and structures. It uses PolyVox for voxel storage and cubic mesh extraction, generating renderable geometry from a 3D grid of voxel values.

Key features:
- Runtime voxel manipulation via C++ and Lua
- Automatic mesh generation from voxel data
- Scene persistence with RLE compression
- Dirty-flag based rebuild system for efficiency

---

## How It Works

### Voxel Storage

`Voxel3D` uses PolyVox's `RawVolume<uint8_t>` for voxel storage:
- **Voxel values**: 0 = air (empty), 1-255 = solid material IDs
- **Default dimensions**: 32x32x32 voxels
- **Coordinate system**: Integer coordinates (x, y, z) starting from (0, 0, 0)

### Mesh Extraction Pipeline

1. When voxels are modified, the node is marked dirty
2. On the next tick, `RebuildMeshInternal()` is called
3. PolyVox's `extractCubicMesh()` generates a blocky mesh from the volume
4. Vertices are converted to engine `VertexColor` format with per-face normals
5. Material IDs are mapped to colors (see Material ID to Color mapping below)
6. The mesh is uploaded to the GPU

### Material ID to Color Mapping

| ID | Color | Description |
|----|-------|-------------|
| 0 | Transparent | Air (not rendered) |
| 1 | Light Gray | Stone/Default |
| 2 | Brown | Dirt |
| 3 | Green | Grass |
| 4 | Blue | Water |
| 5 | Red | - |
| 6 | Yellow | - |
| 7 | Dark Gray | - |
| 8+ | Auto-generated | HSV-based rainbow |

---

## C++ API

### Voxel Access

```cpp
// Set a single voxel
void SetVoxel(int32_t x, int32_t y, int32_t z, VoxelType value);

// Get a voxel value (returns 0 if out of bounds)
VoxelType GetVoxel(int32_t x, int32_t y, int32_t z) const;

// Fill the entire volume
void Fill(VoxelType value);

// Fill a box region (coordinates are clamped to volume bounds)
void FillRegion(int32_t x0, int32_t y0, int32_t z0,
                int32_t x1, int32_t y1, int32_t z1, VoxelType value);
```

### Mesh Control

```cpp
// Mark mesh for rebuild on next tick
void MarkDirty();

// Force immediate mesh rebuild
void RebuildMesh();

// Check if rebuild is pending
bool IsDirty() const;
```

### Dimensions

```cpp
// Get current dimensions
glm::ivec3 GetDimensions() const;

// Set dimensions (recreates volume, clears data)
void SetDimensions(glm::ivec3 dims);
```

---

## Lua API

### Example Usage

```lua
-- Create a voxel node
local voxel = world:SpawnNode("Voxel3D")
voxel:SetPosition(Vec(0, 0, 0))

-- Fill with stone
voxel:Fill(1)

-- Carve out a sphere of air
local cx, cy, cz = 16, 16, 16
local radius = 8
for x = cx - radius, cx + radius do
    for y = cy - radius, cy + radius do
        for z = cz - radius, cz + radius do
            local dx, dy, dz = x - cx, y - cy, z - cz
            if dx*dx + dy*dy + dz*dz < radius*radius then
                voxel:SetVoxel(x, y, z, 0)
            end
        end
    end
end

-- Force rebuild (optional - happens automatically on tick)
voxel:RebuildMesh()
```

### Method Reference

| Method | Arguments | Returns | Description |
|--------|-----------|---------|-------------|
| `SetVoxel` | `x, y, z, value` | - | Set voxel at position |
| `GetVoxel` | `x, y, z` | `int` | Get voxel value |
| `Fill` | `value` | - | Fill entire volume |
| `FillRegion` | `x0, y0, z0, x1, y1, z1, value` | - | Fill box region |
| `MarkDirty` | - | - | Mark for rebuild |
| `RebuildMesh` | - | - | Force immediate rebuild |
| `IsDirty` | - | `bool` | Check if rebuild pending |
| `GetDimensions` | - | `vec3` | Get volume dimensions |

---

## Platform-Specific Behavior

### Vulkan (Windows / Linux / Android)

- **Full support**: Mesh data uploaded to GPU vertex/index buffers
- Uses `VertexColor` vertex type for per-voxel coloring
- Single draw call per `Voxel3D` node

### GameCube / Wii (GX) and 3DS (C3D)

- **Stub implementations**: Currently not rendering
- Future work: Implement display list generation for GX, vertex buffer for C3D
- Consider chunk-based rendering and reduced volume sizes for memory constraints

---

## Serialization

Voxel data is embedded in scene files using RLE (Run-Length Encoding) compression:

```
Format:
  - 3x int32: dimensions (x, y, z)
  - uint32: compressed data size
  - N bytes: RLE pairs [count, value, count, value, ...]
```

For a 32x32x32 volume:
- Worst case (random data): ~65KB
- Best case (uniform fill): ~2 bytes
- Typical case: ~1-10KB depending on complexity

---

## Collision

Current implementation uses a simple bounding box collision shape covering the entire volume dimensions. This provides basic physics interaction but does not conform to the voxel geometry.

**Future enhancements** (not in MVP):
- Per-voxel box collision
- Triangle mesh collision from extracted geometry
- Chunk-based collision for larger volumes

---

## Practical Guidance

### When to Use Voxel3D

**Good use cases:**
- Destructible terrain or structures
- Runtime-editable building/crafting systems
- Procedurally generated cave systems
- Simple block-based levels

**Consider alternatives when:**
- Static geometry (use `StaticMesh3D` for better performance)
- Large open worlds (current implementation doesn't support chunking/streaming)
- High-detail organic shapes (voxels are inherently blocky)

### Performance Considerations

- **Volume size**: Keep dimensions small for MVP (32x32x32 = 32K voxels)
- **Rebuild frequency**: Batch voxel changes before triggering rebuild
- **Draw calls**: Each `Voxel3D` node is one draw call
- **Memory**: Volume data + generated mesh vertices + GPU buffers

### Best Practices

1. **Batch modifications**: Set multiple voxels before the next tick to avoid multiple rebuilds
2. **Use FillRegion**: More efficient than individual `SetVoxel` calls for large areas
3. **Material override**: Assign a custom material for texturing instead of vertex colors
4. **Smaller volumes**: Multiple smaller volumes can be more efficient than one large volume

---

## Configurable Properties

Properties exposed in the editor inspector:

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| Dimension X | Integer | 32 | Volume width in voxels |
| Dimension Y | Integer | 32 | Volume height in voxels |
| Dimension Z | Integer | 32 | Volume depth in voxels |
| Material Override | Material | None | Custom material (inherited from Mesh3D) |

---

## Future Roadmap

Features planned for future phases:

1. **Chunking**: Split large volumes into smaller chunks for efficient updates
2. **Streaming**: Load/unload chunks based on camera distance
3. **LOD**: Simplified meshes for distant chunks
4. **Triangle collision**: Accurate collision from mesh geometry
5. **Editor tools**: Paint mode for sculpting voxels
6. **Console optimization**: Platform-specific rendering for GX/C3D
7. **Marching cubes**: Smooth surface extraction option
