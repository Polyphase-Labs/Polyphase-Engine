# TileMap2D in Polyphase Engine

## Overview

`TileMap2D` is a 2D tile-painting system for Polyphase that fits the engine's
asset, editor, scripting, and undo/redo model. It is designed for grid-based
2D games — top-down RPGs, platformers, dungeon crawlers, strategy maps — and
ships with a complete authoring workflow inside the editor plus a runtime Lua
query API for gameplay code.

The system is built around three pieces:

1. **`TileSet` asset** — owns the texture atlas, slicing, per-tile metadata
   (tags, collision, animation), 9-box brushes, and autotile rule sets.
2. **`TileMap` asset** — owns the painted cell data: sparse chunked storage,
   layers, used-bounds bookkeeping. References a `TileSet`.
3. **`TileMap2D` node** — a `Mesh3D`-derived scene node that renders a
   `TileMap` asset by rebuilding per-frame chunked vertex meshes and binding
   the tileset's atlas texture to a `MaterialLite` instance.

In the editor, a dedicated **Tile Paint** mode wires all three together with
a paint manager (`TilePaintManager`), a tile palette, layer panel, fill
tools, freeform selection, 9-box and autotile authoring, find-uses helpers,
and overlay toggles. The editor camera auto-snaps to a top-down orthographic
view when entering tile paint mode and restores on exit.

Key features:

- Sparse chunked storage (32×32 chunks per layer) that supports negative
  coordinates and grows in any direction without allocating empty cells
- Multi-layer maps with visibility, lock, opacity, and Z-order
- 9 paint tools: **Pencil, Eraser, Picker, Rect Fill, Flood Fill, Line,
  9-Box brush, Marquee Select, Autotile**
- Bresenham interpolation on pencil drag (no gaps when dragging fast)
- Live ghost-tile previews under the cursor for every paint tool
- Freeform selection (Shift+drag adds, Ctrl+drag removes); copy/cut/paste
  with flip-X / flip-Y / rotate-90 transforms; "Fill Selection With 9-Box
  Brush" applies the per-cell corner/edge/center algorithm to any shape
- Cell grid, collision, and tag overlay toggles in the View panel
- Per-tile metadata: tags, collision (None / Full Solid / Box / Boxes /
  Slope / Polygon), animation frames + fps
- Autotile rules with 3×3 tri-state neighbor pattern (`Don't Care` /
  `Must Be Self` / `Must Not Be Self`) and member-tag / member-tile
  classification
- Auto-orthographic camera + camera rotation lock in Tile Paint mode
- Editor preferences entry for grid colors at
  **Preferences > Appearance > Viewport > Tilemap Grid**
- Stroke-batched undo/redo (entire drag commits as one action)
- Asset dirty-flag wired into the existing unsaved-changes shutdown popup
- Cross-platform: Vulkan render path implemented; GameCube/Wii (`API_GX`)
  and 3DS (`API_C3D`) backends are stubbed (no-op) so retro builds compile

---

## Quick Start

### Creating a TileSet

1. In the asset browser, right-click → **Create Asset → Tile Set**.
2. Open the new TileSet in the inspector and assign:
   - **Texture** — your atlas image (e.g. a 1024×512 PNG)
   - **Atlas Tile Width** / **Atlas Tile Height** — pixel size of one tile
     (e.g. `32` and `32`)
   - **Atlas Margin X/Y** — pixel border around the atlas (default `0`)
   - **Atlas Spacing X/Y** — pixel gap between tiles (default `0`)
3. The `mTiles` array auto-populates to `(atlas_width / tile_width) ×
   (atlas_height / tile_height)` slots. Each slot has a stable index that
   stays valid even if you change the atlas later.

### Creating a TileMap

1. Asset browser → **Create Asset → Tile Map**.
2. In the inspector, set **TileSet** to the TileSet you just made.
3. Optionally adjust **Cell Size X/Y** — the world-space dimensions of one
   cell (default `1` × `1`). This is independent of the atlas tile pixel
   size; the atlas slicing controls how the texture is sampled, while the
   cell size controls how big each painted cell is in world units.

### Spawning a TileMap2D node

1. Drop a `TileMap2D` node into your scene (right-click in the hierarchy or
   from a node spawn menu).
2. In the inspector, assign your `TileMap` asset to the **Tile Map** field.
3. Switch the editor mode combo to **Tile Paint**, or click "Open In Tile
   Paint" in the inspector when a TileMap2D is selected.
4. The editor camera snaps directly above the node looking down -Z, switches
   to orthographic projection, and locks rotation.

### Painting

1. In the Tile Paint panel, expand **Tile Palette** and click any tile to
   select it.
2. Switch the tool radio to **Pencil**.
3. Click + drag in the viewport — every cell along the drag path gets the
   selected tile, with a ghost preview leading the cursor.
4. **Ctrl+S** (or right-click the TileMap asset → Save) to persist the
   changes. The asset gets a `*` prefix in the asset browser when it has
   unsaved edits, and the editor's existing unsaved-changes shutdown popup
   catches it on close.

---

## Architecture

### TileSet asset

`Engine/Source/Engine/Assets/TileSet.{h,cpp}`

```cpp
class TileSet : public Asset
{
    AssetRef mTexture;
    int32_t mTileWidth, mTileHeight;
    int32_t mMarginX, mMarginY;
    int32_t mSpacingX, mSpacingY;
    int32_t mAtlasColumns, mAtlasRows;     // derived

    std::vector<TileDefinition> mTiles;             // one per atlas slot
    std::vector<NineBoxBrushDef> mNineBoxBrushes;
    std::vector<AutotileSet> mAutotileSets;
};

struct TileDefinition
{
    int32_t mTileIndex;
    glm::ivec2 mAtlasCoord;
    std::string mName;
    std::vector<std::string> mTags;
    bool mHasCollision;
    TileCollisionType mCollisionType;
    std::vector<glm::vec4> mCollisionRects;     // tile-local pixels
    std::vector<glm::vec2> mCollisionPoly;
    bool mIsAnimated;
    std::vector<int32_t> mAnimFrames;
    float mAnimFps;
};

struct NineBoxBrushDef
{
    std::string mName;
    int32_t mTopLeft, mTop, mTopRight;
    int32_t mLeft,    mCenter, mRight;
    int32_t mBottomLeft, mBottom, mBottomRight;
    std::vector<std::string> mTags;
};

struct AutotileSet
{
    std::string mName;
    std::vector<std::string> mMemberTags;        // a tile is "self" if it has any of these
    std::vector<int32_t> mMemberTileIndices;     // ...or its index appears in this list
    std::vector<AutotileRule> mRules;
};

struct AutotileRule
{
    AutotileNeighborState mNeighbors[8];   // see § Autotile slot order
    std::vector<int32_t> mResultTiles;     // single-pick for now
};
```

Key methods on `TileSet`:

| Method | Purpose |
|---|---|
| `RebuildTileGrid()` | Recompute `mAtlasColumns/Rows` from texture + slicing parameters and resize `mTiles`. Called automatically when slicing properties change in the inspector. |
| `GetTileUVs(tileIndex, &uv0, &uv1)` | Compute the `[0,1]` UV rectangle for a tile, accounting for margin and spacing. Used by the node's mesh builder, the panel's tile picker, and the panel's preview overlay. |
| `IsTileSolid(tileIndex)` | Returns `mHasCollision && mCollisionType != None`. |
| `HasTileTag(tileIndex, tag)` | Linear scan over the tile's `mTags`. |
| `IsTileMemberOfAutotile(brushIdx, tileIdx)` | Used by the autotile evaluator to decide whether a neighbor counts as "self". |
| `MatchAutotileRule(brushIdx, selfMask)` | Walks the rule list in order; first matching rule wins. |

The slicing properties have a custom `HandlePropChange` that **writes the
new value into the member directly** (because the Datum framework calls the
handler before its own write — see § Gotchas) and then calls
`RebuildTileGrid()` so the atlas grid updates the moment you change a value
in the inspector.

### TileMap asset

`Engine/Source/Engine/Assets/TileMap.{h,cpp}`

```cpp
static constexpr int32_t kTileChunkSize = 32;

struct TileCell
{
    int32_t mTileIndex = -1;     // -1 = empty
    uint8_t mFlags = 0;          // bit 0 flipX, 1 flipY, 2 rot90, 3 hidden
    uint8_t mPadding = 0;
    int16_t mVariant = 0;
};

struct TileChunk
{
    TileCell mCells[kTileChunkSize * kTileChunkSize];
};

struct TileMapLayer
{
    std::string mName;
    bool mVisible, mLocked;
    float mOpacity;
    TileMapLayerType mType;     // Visual / Collision / Decal / Gameplay
    int32_t mZOrder;
    std::unordered_map<int64_t, TileChunk> mChunks;   // key = pack(chunkX, chunkY)
};

class TileMap : public Asset
{
    AssetRef mTileSet;
    glm::ivec2 mTileSize = { 1, 1 };
    glm::vec2 mOrigin = { 0, 0 };
    bool mAllowNegativeCoords = true;
    std::vector<TileMapLayer> mLayers;

    glm::ivec2 mMinUsed, mMaxUsed;     // bounding box of all painted cells
    bool mHasContent = false;

    std::unordered_set<int64_t> mDirtyChunks;
};
```

**Sparse chunked storage:** each layer holds an `unordered_map` of chunk
key → `TileChunk`. Chunks are 32×32 cells (1024 `TileCell`s, 8 KB each).
Chunks are allocated lazily — the very first cell painted in a chunk
allocates it, and the chunk lives until the asset is destroyed.

**Negative coordinates:** the chunk key is a packed `int64_t` of two
`int32_t`s, and `FloorDivChunk` / `FloorModChunk` use proper floor-division
math so negative cells route to the correct chunk:

```cpp
static int32_t FloorDivChunk(int32_t cell)
{
    if (cell >= 0) return cell / kTileChunkSize;
    return -((kTileChunkSize - 1 - cell) / kTileChunkSize);
}
```

**Used bounds:** `mMinUsed` / `mMaxUsed` are grown on every `SetCell` and
recomputed from scratch on `LoadStream`. Used by `GetUsedBounds`,
the cell-grid overlay's "anchor on the map" path, and the closest-cell
queries.

**Dirty chunks:** every `SetCell` that actually changes a value adds the
chunk key to `mDirtyChunks`. The `TileMap2D` node drains this set every Tick
to know which chunks need a vertex rebuild. **Important:** in the current
Phase 1 implementation the node rebuilds the *entire* mesh on any dirty
chunk; per-chunk meshes are a Phase 4+ optimization.

Cell mutation API:

| Method | Notes |
|---|---|
| `GetCell(x, y, layer)` | Returns `TileCell{}` for empty cells or out-of-bounds layers. |
| `SetCell(x, y, cell, layer)` | The canonical write path. Allocates the chunk if needed. Skips no-op writes. Sets the asset dirty flag in editor builds. |
| `ClearCell(x, y, layer)` | Calls `SetCell` with an empty `TileCell`. |
| `GetTile(x, y, layer)` | Convenience: returns `mTileIndex` only. |
| `SetTile(x, y, idx, layer)` | Convenience: writes through `SetCell`. |
| `ReplaceTile(oldIdx, newIdx, layer)` | Bulk swap; returns count. |
| `ReplaceTilesWithTag(tag, newIdx, layer)` | Bulk swap by TileSet tag; returns count. |
| `CountTileUses(idx, layer)` | Linear scan over all chunks; returns count. |
| `MarkAllDirty()` | Adds every existing chunk key to the dirty set (used after `SetTileSet`). |

**Save/Load** is version-gated. Versions:
- `ASSET_VERSION_TILESET_BASE` (28) — initial TileSet
- `ASSET_VERSION_TILEMAP_BASE` (29) — initial TileMap + node
- `ASSET_VERSION_TILESET_METADATA` (30) — per-tile tags, collision rects,
  animation frames
- `ASSET_VERSION_TILESET_AUTOTILE` (31) — autotile sets

The TileMap save format RLE-skips empty chunks: a chunk is only written if
at least one of its cells has `mTileIndex >= 0`.

### TileMap2D node

`Engine/Source/Engine/Nodes/3D/TileMap2d.{h,cpp}`

```cpp
class TileMap2D : public Mesh3D
{
    AssetRef mTileMap;

    std::vector<VertexColor> mVertices;     // CPU-side mesh
    std::vector<IndexType>   mIndices;
    uint32_t mNumVertices, mNumIndices;

    bool mMeshDirty;
    bool mUploadDirty[MAX_FRAMES];

    Bounds mBounds;
    TileMap2DResource mResource;            // VkBuffer pair
    MaterialRef mDefaultMaterial;
    float mLayerZSpacing = 0.01f;
};
```

The node walks all visible layers in Z order, iterates each layer's chunks,
and builds one big `VertexColor` mesh for the entire map. Each painted cell
becomes 4 vertices + 6 indices (one quad). UVs come from
`TileSet::GetTileUVs` and respect the `mFlags` flip bits. Layer Z-stacking
multiplies the layer's `mZOrder` by `mLayerZSpacing` so layers separate
slightly along Z to avoid Z-fighting.

The default material is a `MaterialLite::New(LoadAsset<MaterialLite>("M_DefaultUnlit"))`
with `VertexColorMode::Modulate`. The atlas texture is bound to slot 0
every Tick via `EnsureMaterialBinding` so swapping the underlying TileSet's
texture propagates without requiring a manual rebuild.

**Vertex color compensation:** the engine multiplies vertex colors by
`GlobalUniforms::mColorScale` (default `2.0`) in the forward shader. Engine
code that builds vertex meshes pre-divides white. The TileMap2D builder
reads `GetEngineConfig()->mColorScale` and writes `0xFFFFFFFFu`,
`0x7F7F7F7Fu`, or `0x3F3F3F3Fu` per the scale value.

**Mesh upload critical detail:** `TickCommon` drains the asset's dirty
chunks and calls `MarkDirty()` (NOT `mMeshDirty = true`). `MarkDirty()` sets
both `mMeshDirty` and every entry of `mUploadDirty[]`. Without the upload
dirty flag, the CPU rebuild produces correct vertex data but
`UploadMeshData()` skips the GPU upload, and the GPU keeps drawing the
previous mesh — this caused live pencil-drag-paint to be invisible until
mouse-up release. See § Gotchas.

**World layout:** cell `(cx, cy)` maps to world position
`(origin.x + cx * tileSize.x, origin.y + cy * tileSize.y, layer.zOrder * layerZSpacing)`.
+X is right, +Y is up, layer Z stacks toward +Z. The auto-ortho editor
camera puts itself at the node's world position + `(0, 0, 50)` looking
straight down -Z.

`WorldToCell(worldXY)` and `CellToWorld(cell)` round-trip these
coordinates. `CellCenterToWorld(cell)` returns the center of the cell in
world space — used by the cursor highlight cube and the tile preview
overlay's screen-projection math.

### TilePaintManager (editor)

`Engine/Source/Editor/TilePaint/TilePaintManager.{h,cpp}` (`#if EDITOR`)

The paint manager is the editor-only state machine that handles:

- Hover detection (raycast from camera onto the layer Z plane)
- Stroke lifecycle: click → drag → release
- Per-tool dispatch (`TileSculptMode` enum)
- Bresenham interpolation for free-stroke pencil/eraser drags
- Marquee + freeform selection
- Clipboard (copy/cut/paste/flip/rotate)
- Cursor cube + collision/tag/grid overlays
- Action commit through `ActionPaintTiles`

It is constructed in `EditorState::Init()` and ticked from
`Viewport3D::Update()` only when `GetPaintMode() == PaintMode::TilePaint`
and `controlMode == ControlMode::Default`. It mirrors `VoxelSculptManager`
and `TerrainSculptManager` in structure.

```cpp
enum class TileSculptMode : uint8_t
{
    Pencil, Eraser, Picker,
    RectFill, FloodFill, Line,
    NineBox, Select, Autotile,
    Count
};

struct TileSculptOptions
{
    TileSculptMode mMode = TileSculptMode::Pencil;
    int32_t mSelectedTileIndex = 0;
    int32_t mActiveLayer = 0;
    int32_t mActiveNineBoxIndex = -1;
    int32_t mActiveAutotileIndex = -1;
    bool mShowCollisionOverlay = false;
    bool mShowTagOverlay = false;
    bool mShowCellGrid = false;
    std::string mTagOverlayName;
};
```

Stroke state (public so the panel can read it for live previews):

```cpp
bool mStrokeActive;
bool mHoverValid;
glm::ivec2 mHoverCell;
bool mPreviewActive;            // drag-based tools (Rect/Line/9Box/Select)
glm::ivec2 mDragStartCell;
bool mLastStrokeCellValid;      // pencil/eraser interpolation anchor
glm::ivec2 mLastStrokeCell;
bool mHasSelection;
glm::ivec2 mSelectionMin, mSelectionMax;     // bounding rect of mSelectedCells
std::set<int64_t> mSelectedCells;            // freeform selection set
TileClipboard mClipboard;
```

`mPendingChanges` (vector of `TilePaintChange { cellX, cellY, layer, oldCell, newCell }`)
accumulates per-stroke changes and flushes through `ActionManager::EXE_PaintTiles`
on `CommitStroke()`. `mModifiedSet` is a per-stroke dedup set so tools can
visit the same cell multiple times during a stroke without spamming the
change list.

---

## Paint Tools

| Tool | Click | Drag | Release | Notes |
|---|---|---|---|---|
| **Pencil** | Paints click cell | Bresenham trail of selected tile | Commits stroke action | Live painting, gap-free |
| **Eraser** | Clears click cell | Bresenham trail of clears | Commits stroke action | Same path as Pencil with `mTileIndex = -1` |
| **Picker** | Reads cell into `mSelectedTileIndex` | — | — | Single-click; doesn't start a stroke |
| **Rect Fill** | Anchor start cell | Live rect ghost preview | Fills entire rect from start to release cell | Capped at 1024×1024 cells |
| **Flood Fill** | BFS over connected matching cells | — | — | Cap of 16384 cells, single click |
| **Line** | Anchor start cell | Live Bresenham ghost preview | Stamps Bresenham line | Cap of 16384 cells |
| **9-Box** | Anchor start cell | Live rect ghost preview (corners) | Per-cell corner/edge/center fill from active brush | Empty brush slots leave existing cells alone |
| **Select** | Anchor start cell | Live rect outline | Adds cells to `mSelectedCells` | Plain = replace, Shift = add, Ctrl = remove |
| **Autotile** | Paint click cell + recompute 8 neighbors | Drag-paints continuously | Commits stroke action | Uses active autotile rules |

### Pencil drag interpolation

`StrokePencilOrEraserInterp(prev, current)` walks Bresenham between the
previous frame's cell and the current frame's cell, calling
`ApplyPencilOrEraser` for each cell on the path. This means a fast mouse
drag that crosses 7 cells in one frame still paints all 7 cells, instead of
leaving 6 holes. Per-stroke dedup via `mModifiedSet` ensures cells visited
multiple times aren't re-staged.

### 9-Box brush algorithm

Implemented in `TilePaintManager::PickNineBoxSlot(cx, cy, minX, maxX, minY, maxY)`.
For a rectangular drag, each cell picks the slot based on its position
within the rect:

```
+---+---+---+---+---+
| TL| T | T | T | TR|
+---+---+---+---+---+
| L | C | C | C | R |
+---+---+---+---+---+
| BL| B | B | B | BR|
+---+---+---+---+---+
```

Special-cases per spec § "Special cases":
- 1×1 → Center
- 1×N column → Top / Center / Bottom
- N×1 row → Left / Center / Right

For **freeform selection** 9-box fill (`DoApply9BoxToSelection`), each cell
in `mSelectedCells` looks at its 4 cardinal neighbors. If a neighbor is NOT
in the selection, that side of the cell is "exposed":

| Exposed sides | Slot |
|---|---|
| top + left | Top-Left corner |
| top + right | Top-Right corner |
| bottom + left | Bottom-Left corner |
| bottom + right | Bottom-Right corner |
| top only | Top edge |
| bottom only | Bottom edge |
| left only | Left edge |
| right only | Right edge |
| none | Center |

This works for L-shapes, paths, S-curves, isolated regions, donut shapes —
any cell set you can build with the freeform selection tools. Empty brush
slots (`-1`) leave the cell untouched, so partial brushes are allowed.

### Autotile

A more general version of the 9-box brush that uses arbitrary 3×3
neighborhood predicates. Each `AutotileRule` has 8 neighbor states
(`DontCare` / `MustBeSelf` / `MustNotBeSelf`) and a list of result tiles.

**Slot order** (matches `mNeighbors[8]`):

```
[0]=NW [1]=N [2]=NE
[3]=W       [4]=E
[5]=SW [6]=S [7]=SE
```

(+Y is up.)

**Membership classification:** a tile counts as "self" for an autotile set
if its index appears in the set's `mMemberTileIndices` OR it carries any of
the set's `mMemberTags`. So you can author by tagging tiles (`grass`,
`wall`, `road`) or by listing specific tile indices.

**Paint behavior:** clicking with the autotile tool calls
`CommitAutotileAt(cell)` which:

1. Adds the click cell to a `pendingSelf` set so it counts as self even
   though it isn't yet committed to the asset
2. Recomputes the click cell's tile from the rule that matches its 8-neighbor
   "self mask"
3. Recomputes each of the 8 cardinal/diagonal neighbors that are *already*
   members of the same autotile group (i.e., painted with a tile from the
   group earlier) — neighbors of foreign tiles are left alone
4. Stages every result through `StageCellChange` so the entire autotile
   operation is one undoable action

Drag-painting works: each frame the cursor lands on a new cell,
`CommitAutotileAt` runs again and the prior pending-self set is preserved
so the rules see continuous edges. The Bresenham interpolation isn't used
here — autotile is single-cell per frame.

### Selection / Clipboard

Selection is stored in `mSelectedCells` as a `std::set<int64_t>` of packed
cell coordinates. The `Select` tool's drag-rectangle commit:

- **Plain drag** → clears `mSelectedCells`, adds every cell in the rect
- **Shift+drag** → adds every cell in the rect (additive, freeform build-up)
- **Ctrl+drag** → erases every cell in the rect (carve a hole)

After every commit, `RecomputeSelectionBounds()` derives the bounding rect
into `mSelectionMin/Max` so legacy code paths (the old corner outline,
copy/cut/paste) keep working.

The persistent selection outline draws every cell in the set as a small
yellow cube each frame (capped at 2048 to keep the debug-draw count
manageable on huge selections).

Clipboard (`TileClipboard`):

```cpp
struct TileClipboard
{
    int32_t mWidth, mHeight;
    std::vector<TileCell> mCells;     // row-major
};
```

- **Copy** snapshots the bounding rect of the selection into `mCells`,
  including empty cells.
- **Cut** copies + clears all cells in the bounding rect (one undoable
  action).
- **Paste at cursor** stamps the clipboard at the hover cell, skipping any
  empty source cells so non-rectangular shapes don't erase under-painted
  tiles.
- **Flip X/Y** mirror the clipboard in place AND toggle the corresponding
  `TileCell::mFlags` bit so the visual flip persists when pasted.
- **Rotate 90** rotates clockwise + swaps width/height + toggles bit 2 in
  every flag byte.

> **Note:** Copy/cut/paste currently use the bounding rect of the freeform
> selection. Cells outside the freeform shape but inside the rect are
> included as empty cells in the clipboard, then skipped on paste. A future
> improvement would store the clipboard as a sparse map for true freeform
> support.

### Find All Uses + Bulk Replace

The selected tile's metadata block in the panel has a **Find All Uses**
button that calls `TileMap::CountTileUses(tileIdx, layer)` and logs the
count via `LogDebug`. The Lua API also exposes `tileMap:CountTileUses(idx)`,
`tileMap:ReplaceTile(oldIdx, newIdx)`, and
`tileMap:ReplaceTilesWithTag(tag, newIdx)` for bulk gameplay-side
operations.

---

## Editor Panel UI

`Engine/Source/Editor/EditorImgui.cpp`, inside the `paintMode == TilePaint`
branch of `EditorImguiDraw`. The panel is created with a regular floating
ImGui window — `ImGui::Begin("Tile Paint", ...)` — and contains:

- **Tool radio buttons**: Pencil / Eraser / Picker / Rect Fill / Flood Fill
  / Line / 9-Box / Select / Autotile
- **Selected Tile** preview thumbnail
- **Tile Palette** collapsing header — full atlas grid via
  `TilePicker::DrawInlineTileGrid`. Click any tile to select it.
- **Layers** collapsing header — per-layer Active/Visible/Locked/Name with
  Add Layer button
- **9-Box Brushes** collapsing header — list/create AutotileSets with a
  3×3 grid of slot tile pickers per brush
- **Autotile Brushes** collapsing header — name + member tags list + rules
  list with the 3×3 tri-state grid + result tile picker
- **Selection & Clipboard** collapsing header — Copy/Cut/Paste/Clear,
  Flip X/Y, Rotate 90, **Fill Selection With 9-Box Brush**, hint text for
  Shift+/Ctrl+ modifiers
- **View** collapsing header — Cell Grid, Collision Overlay, Tag Overlay
  toggles + tag-to-highlight input
- **Hover info** — Cell, Chunk, Tile-under-cursor live readout
- **Tile Metadata** collapsing header (selected tile) — Find All Uses,
  Name, Has Collision, collision type combo, tag list with add/remove

The panel also draws the **tile preview overlay** at the end of the block
using `ImGui::GetForegroundDrawList()->AddImageQuad`. For each tool that
"draws a tile", it computes the cell's 4 world corners, projects them to
screen via `Camera3D::WorldToScreenPosition`, looks up the selected tile's
UVs from the tileset, and renders an `AddImageQuad` of the atlas texture
at that position. RectFill/Line/Pencil drag previews iterate the
appropriate cell list (rect / Bresenham / interpolated path) and ghost each
cell.

### Tile Picker helper

`Engine/Source/Editor/TilePaint/TilePicker.{h,cpp}` is a small reusable
namespace that maintains a cache of `ImTextureID`s per call site (so
multiple panels can show the atlas without leaking ImGui descriptors) and
exposes:

```cpp
namespace TilePicker
{
    void* GetOrCreateImTextureID(const char* cacheKey, Texture* atlas);
    bool DrawTileButton(const char* id, void* atlasImId, int32_t tileIdx,
                        int32_t tilesX, int32_t tilesY, float size);
    void DrawTilePickerPopup(const char* popupId, void* atlasImId,
                             int32_t tilesX, int32_t tilesY,
                             int32_t& outTileIdx);
    bool DrawInlineTileGrid(const char* id, void* atlasImId,
                            int32_t tilesX, int32_t tilesY,
                            int32_t& selectedTileIdx,
                            float tileDrawSize,
                            float maxWidth, float maxHeight);
}
```

`DrawTileButton` is reused by the 9-box brush slot grids and the selected
tile preview. `DrawInlineTileGrid` is the full-atlas palette grid.
`DrawTilePickerPopup` is the modal grid used when clicking a 9-box slot
button.

### Auto-orthographic camera

Entering Tile Paint mode (via the editor mode combo or "Open In Tile Paint"
button) calls `EditorState::SetPaintMode(PaintMode::TilePaint)`. The
implementation stashes the previous projection mode AND the previous camera
transform:

```cpp
mTilePaintProjectionStashed = true;
mTilePaintPrevWasPerspective = (cam->GetProjectionMode() == ProjectionMode::PERSPECTIVE);
mTilePaintPrevCameraPosition = cam->GetWorldPosition();
mTilePaintPrevCameraRotation = cam->GetWorldRotationQuat();
mTilePaintTransformStashed = true;

if (mTilePaintPrevWasPerspective)
{
    cam->SetProjectionMode(ProjectionMode::ORTHOGRAPHIC);
    ApplyEditorCameraSettings();
}

cam->SetWorldPosition(anchor + glm::vec3(0, 0, 50));
cam->SetWorldRotation(glm::vec3(0, 0, 0));     // straight down -Z
```

`anchor` is the selected `TileMap2D` node's world position (or world origin
if nothing is selected). Exiting tile paint restores both the projection
and the transform.

**Camera rotation lock:** in `Viewport3D::HandleDefaultControls`, the
right-mouse → Pilot mode and middle-mouse → Orbit mode triggers are gated
on `paintMode != TilePaint`. Pan (Shift+Middle) and scroll-wheel zoom
remain available so you can still navigate the map.

**Selection click protection:** the editor's `Shift+click` and `Ctrl+click`
multi-select branch in `HandleDefaultControls` is now also gated on
`paintMode == None` so that the modifiers can be repurposed for additive /
subtractive freeform selection inside Tile Paint mode without interference.

---

## Preferences

**Preferences > Appearance > Viewport > Tilemap Grid**

Lives at
`Engine/Source/Editor/Preferences/Appearance/Viewport/TilemapGrid/TilemapGridModule.{h,cpp}`,
registered in `PreferencesManager::Create()` as a sub-module of
`ViewportModule` (which is itself a sub-module of `AppearanceModule`).

Settings:

- **Minor Line Color** (`glm::vec4`, default `(1, 1, 1, 0.35)`) — color
  of normal cell-boundary grid lines
- **Highlight World Axes** (`bool`, default `true`) — when on, draws the
  X=0 / Y=0 lines in a separate color
- **Axis Color** (`glm::vec4`, default `(1, 1, 0.4, 0.85)`) — color of the
  highlight lines

`TilePaintManager::DrawGridOverlay` reads `TilemapGridModule::Get()` every
frame and falls back to the original hardcoded defaults if the module
hasn't been registered yet (preferences register asynchronously during
editor startup).

The grid is rendered through `World::AddLine` with a positive lifetime
(`0.25f`) so the dedup logic in `AddLine` keeps the lines alive while the
toggle is on, and they automatically expire ~250 ms after the toggle is
turned off. See § Gotchas for why lifetime > 0 matters.

---

## Lua Runtime API

Bindings live in
`Engine/Source/LuaBindings/TileMap2d_Lua.{h,cpp}` and are registered in
`LuaBindings.cpp` after `Terrain3D_Lua::Bind()`. `TileMap2D` inherits from
`Mesh3D` in the Lua type hierarchy, so all Node3D / Mesh3D methods are
available too.

After adding or changing bindings, run:

```bash
python Tools/generate_lua_stubs.py
```

The stubs land in `Engine/Generated/LuaMeta/TileMap2D.lua` and provide
EmmyLua-style annotations for IDE autocomplete.

### Coordinate helpers

```lua
cellX, cellY = tileMap:WorldToCell(worldVec)
worldVec     = tileMap:CellToWorld(cellX, cellY)
worldVec     = tileMap:CellCenterToWorld(cellX, cellY)
```

### Cell queries / mutation

```lua
tileIdx = tileMap:GetTile(cellX, cellY [, layer])
tileMap:SetTile(cellX, cellY, tileIdx [, layer])
tileMap:ClearTile(cellX, cellY [, layer])

cellInfo = tileMap:GetCell(cellX, cellY [, layer])
-- Returns a table:
-- {
--   tileIndex   = integer,
--   atlasX      = integer,
--   atlasY      = integer,
--   cellX       = integer,
--   cellY       = integer,
--   worldX      = number,
--   worldY      = number,
--   hasCollision = boolean,
--   flags       = integer,
--   tags        = { string, ... }   -- only if the tile has tags
-- }

bool = tileMap:IsCellOccupied(cellX, cellY [, layer])
```

### Tile-definition queries

```lua
tagsArray = tileMap:GetTileTags(tileIdx)            -- string array
bool      = tileMap:HasTileTag(tileIdx, "solid")
atlasX, atlasY = tileMap:GetTileAtlasCoord(tileIdx)
```

### Region search

```lua
cells = tileMap:GetCellsInRect(x1, y1, x2, y2 [, layer])
cells = tileMap:FindAllTiles(targetTileIdx [, layer])
cells = tileMap:FindAllTilesWithTag("ground" [, layer])
minX, minY, maxX, maxY = tileMap:GetUsedBounds()
count = tileMap:CountTileUses(tileIdx [, layer])
```

### Closest-cell queries

Results are sorted by distance from the world position. `maxDist` is
optional — pass `0` or omit for unbounded.

```lua
cellInfo = tileMap:GetClosestTile(worldVec [, layer])    -- nil if map empty
cells    = tileMap:GetClosestTilesWithTag(tag, worldVec [, maxDist [, layer]])
cells    = tileMap:GetClosestTilesOfType(tileIdx, worldVec [, maxDist [, layer]])
```

### Neighborhood

```lua
cells = tileMap:GetNeighborCells(cellX, cellY [, includeDiagonals [, layer]])
-- 4 cardinal cells (or 8 with diagonals=true)
```

### Collision

```lua
bool = tileMap:IsSolidAt(worldVec [, layer])
bool = tileMap:IsSolidCell(cellX, cellY [, layer])

info = tileMap:GetCollisionAt(worldVec [, layer])
-- nil if cell is empty or non-solid, otherwise:
-- {
--   cellX, cellY  = integer,
--   tileIndex     = integer,
--   hasCollision  = true,
--   collisionType = integer,
--   rects         = { {x, y, w, h}, ... }   -- if rect-based
-- }
```

### Bulk editing

```lua
tileMap:FillRect(x1, y1, x2, y2, tileIdx [, layer])
tileMap:ClearRect(x1, y1, x2, y2 [, layer])
n = tileMap:ReplaceTile(oldIdx, newIdx [, layer])         -- returns count
n = tileMap:ReplaceTilesWithTag(tag, newIdx [, layer])    -- returns count
```

### Navigation

```lua
hit = tileMap:RaycastTiles(startWorld, endWorld [, layer])
-- Returns { hit, cellX, cellY, worldX, worldY, tileIndex }
-- Walks 2D DDA across cell space, stops at the first cell whose tile is solid.

cells = tileMap:GetReachableCells(startX, startY, maxDistance [, passTag [, layer]])
-- BFS bounded by Manhattan distance maxDistance.
-- If passTag is given, cells without a tile OR cells whose tile carries
-- passTag are traversable; everything else blocks.
-- Capped at 4096 cells emitted.
```

### Example: click-to-paint test

`Polyphase-Examples/Tilemap2DExample/Tilemap2DExample/Scripts/TilemapClickTest.lua`
demonstrates the click → cell math + bulk operations:

```lua
function TilemapClickTest:Tick(deltaTime)
    if not self.tileMap or not self.tileMap:IsValid() then return end

    local mouseX, mouseY = Input.GetMousePosition()
    local rayOrigin = self.camera:GetWorldPosition()
    local rayTarget = self.camera:ScreenToWorldPosition(mouseX, mouseY)
    local rayDir = rayTarget - rayOrigin
    rayDir:Normalize()

    -- Project onto the tilemap's z-plane:
    local nodeZ = self.tileMap:GetWorldPosition().z
    local t = (nodeZ - rayOrigin.z) / rayDir.z
    if t < 0 then return end

    local hitX = rayOrigin.x + rayDir.x * t
    local hitY = rayOrigin.y + rayDir.y * t
    local cellX, cellY = self.tileMap:WorldToCell(Vector.Create(hitX, hitY, 0))

    if Input.IsMouseButtonJustDown(Mouse.Left) then
        local cell = self.tileMap:GetCell(cellX, cellY, self.layer)
        Log.Debug(string.format("tile=%d  tags=%s",
            cell.tileIndex, table.concat(cell.tags or {}, ",")))
    end
end
```

---

## Asset versioning

```cpp
#define ASSET_VERSION_TILESET_BASE 28
#define ASSET_VERSION_TILEMAP_BASE 29
#define ASSET_VERSION_TILESET_METADATA 30
#define ASSET_VERSION_TILESET_AUTOTILE 31
#define ASSET_VERSION_CURRENT 31
```

When loading older TileSet/TileMap assets, the load path version-gates each
new field block:

```cpp
if (mVersion >= ASSET_VERSION_TILESET_METADATA)
{
    // read tags, collision rects, animation frames
}
if (mVersion >= ASSET_VERSION_TILESET_AUTOTILE)
{
    // read autotile sets
}
```

Old assets load cleanly into the newer struct layouts because the new
fields default to empty/false in the struct definitions.

---

## Build system integration

When you add files for new TileMap features, **all three** build systems
need updating. See `.claude/skills/octave/SKILL.md` § "Build System
Integration" for the canonical checklist; the abbreviated version:

1. `Engine/Engine.vcxproj` — `<ClCompile>` and `<ClInclude>` entries
2. `Engine/Engine.vcxproj.filters` — matching entries with `<Filter>` tags;
   add a new `<Filter Include="…">` block with a GUID if the directory is
   new
3. `Engine/Makefile_Linux` — add the directory to `SOURCES` IF it's a new
   directory. The Makefile uses `wildcard $(dir)/*.cpp` so existing
   directories pick up new files automatically, but new directories are
   silently dropped on Linux unless added to `SOURCES`. Editor-only
   directories go in the `SOURCES +=` line inside the `ifeq POLYPHASE_EDITOR`
   block.
4. CMake (`CMakeLists.txt`) uses `file(GLOB_RECURSE SRC *.cpp *.c *.h)` so
   no CMake update is needed.

The TileMap system created several new directories that needed all three
files updated:

- `Engine/Source/Editor/TilePaint/`
- `Engine/Source/Editor/Preferences/Appearance/Viewport/TilemapGrid/`

And several new files in existing directories (which only needed the
vcxproj + filters):

- `Engine/Source/Engine/Assets/TileSet.{h,cpp}`
- `Engine/Source/Engine/Assets/TileMap.{h,cpp}`
- `Engine/Source/Engine/Nodes/3D/TileMap2d.{h,cpp}`
- `Engine/Source/LuaBindings/TileMap2d_Lua.{h,cpp}`

`FORCE_LINK_CALL(TileSet)`, `FORCE_LINK_CALL(TileMap)`, and
`FORCE_LINK_CALL(TileMap2D)` were added to `Engine.cpp::ForceLinkage()`.

---

## Gotchas

A handful of subtle bugs surfaced while building this system. Each one is
reflected in `.claude/skills/octave/SKILL.md` § "Things to Watch Out For"
so future engine work doesn't repeat them, but here they are with TileMap-
specific context:

### 1. `HandlePropChange` is called BEFORE the value is written

`Datum::SetInteger` (and the other typed setters) calls the change handler
with the new value as a `void*`, then **only writes the value into the
member if the handler returns false**. Naïve handlers that read the changed
member see the **old** value.

The TileSet inspector's "Atlas Tile Width / Height / Margin / Spacing"
properties needed `RebuildTileGrid()` to recompute `mAtlasColumns/Rows`.
Reading `mTileWidth` from `RebuildTileGrid` always saw the previous value,
so the palette grid rendered the OLD slicing forever. Fix: in
`TileSet::HandlePropChange`, dispatch on `prop->mName`, write the new value
into the member directly from the `void* newValue` pointer, then return
`true` so the framework skips its own write. Same pattern as
`Texture::HandlePropChange`.

### 2. Vertex color × `mColorScale` brightness doubling

The forward shader multiplies vertex colors by `GlobalUniforms::mColorScale`
(default `2.0`, a Wii/GameCube TEV compatibility hack — see
`ForwardVertColor.glsl`). Engine code that builds vertex meshes pre-divides
white so the shader's scale brings it back to 1.0. Voxel3D works because
its colors come from `MaterialIdToColor` which produces non-saturated
values; my TileMap2D builder originally wrote `0xFFFFFFFFu` which the
shader doubled to 2× white. Fix: read `GetEngineConfig()->mColorScale` and
write `0x7F7F7F7Fu` (scale=2) or `0x3F3F3F3Fu` (scale=4) instead of
`0xFFFFFFFFu`.

### 3. `mUploadDirty[]` must be set on every rebuild

`TileMap2D::TickCommon` drains the asset's dirty chunks and rebuilds the
mesh. `RebuildMeshInternal()` only touches the CPU-side `mVertices`/
`mIndices` arrays — it does **not** set `mUploadDirty[*]`.
`UploadMeshData()` then checks `mUploadDirty[frameIndex]` and skips the
GPU upload if it's false. The result: the CPU vertex array is correct, the
GPU keeps drawing the previous mesh, and live pencil-drag-paint is invisible
until something else (like the action commit on mouse-up) calls
`MarkDirty()` which sets both flags.

Fix: in the dirty-chunk drain, call `MarkDirty()` instead of
`mMeshDirty = true`. `MarkDirty()` sets BOTH `mMeshDirty` and every entry of
`mUploadDirty[]`.

### 4. `INP_GetMousePosition` is unreliable in editor builds

ImGui's Win32 backend handler runs first in `WndProc`
(`System_Windows.cpp:41`). When ImGui has captured input — which happens
basically any time a button is held — it intercepts `WM_MOUSEMOVE` and the
engine's `INP_SetMousePosition` rarely fires. `INP_GetMousePosition`
returns near-stuck coordinates throughout a held drag.

Fix: editor-only code should read from `ImGui::GetIO().MousePos` instead.
That value is updated every frame by ImGui's backend regardless of input
capture state. Pattern:

```cpp
ImVec2 mp = ImGui::GetIO().MousePos;
int32_t mouseX = int32_t(mp.x);
int32_t mouseY = int32_t(mp.y);
```

### 5. `Camera3D::ScreenToWorldPosition` works differently for ortho

For perspective cameras, `rayDir = normalize(screenWorld - cameraPos)`
gives a correct ray. For **orthographic** cameras, parallel rays don't
share an origin: every ray emanates from the screen-projected near-plane
point in the camera's forward direction. Treating ortho like perspective
gives a near-zero-parallax ray that hits the wrong cell (effectively pinned
near the camera). Branch on `camera->GetProjectionMode()`:

```cpp
glm::vec3 screenWorld = camera->ScreenToWorldPosition(mouseX, mouseY);
if (camera->GetProjectionMode() == ProjectionMode::ORTHOGRAPHIC)
{
    rayOrigin = screenWorld;
    rayDir = camera->GetForwardVector();
}
else
{
    rayOrigin = camera->GetWorldPosition();
    rayDir = glm::normalize(screenWorld - rayOrigin);
}
```

### 6. `Viewport3D::ShouldHandleInput()` kills strokes mid-drag

`ShouldHandleInput` returns false if any non-dock floating ImGui window is
hovered, even if the click started on the viewport. The "Tile Paint" panel
is a floating window, so the moment the cursor drifts toward it during a
drag, `ShouldHandleInput` returns false and any code that early-returns on
that check stops running mid-stroke.

Fix: gate the early-return on `!mStrokeActive`. New strokes still need
`ShouldHandleInput == true` to start, but in-progress strokes keep running:

```cpp
if (!mStrokeActive && !GetEditorState()->GetViewport3D()->ShouldHandleInput())
    return;
```

### 7. `World::AddLine` lifetime semantics

`World::UpdateLines` decrements `mLifetime` every tick and erases lines at
`<= 0`. A line added with `mLifetime = 0.0f` is erased on the very next
tick before the renderer ever sees it. For per-frame "always visible while
toggled on" debug lines, use a small positive lifetime (`0.05f`–`0.25f`)
and rely on `World::AddLine`'s dedup logic to bump the lifetime each frame
(it picks `max(existing, new)`).

The cell grid overlay uses `0.25f` so the lines stay visible for ~250 ms
after the toggle is turned off, then expire cleanly.

### 8. `SM_Cube` is a 2-unit cube

`Engine/Assets/Meshes/SM_Cube.dae` has vertices at ±1, not ±0.5. Scaling by
`(tileSize.x, tileSize.y, ...)` produces a quad twice as big as one cell.
Halve the scale: `(tileSize.x * 0.5, tileSize.y * 0.5, ...)`. Affected the
cursor cube, the drag-preview corners, the selection outline, and the
collision/tag overlay quads.

### 9. `Asset::SetDirtyFlag()` is what triggers the unsaved-changes popup

The editor's existing unsaved-changes shutdown popup keys off
`Asset::GetDirtyFlag()`. If you mutate an asset programmatically (e.g.
from a paint stroke) and forget to call `SetDirtyFlag()`, the editor closes
cleanly without warning the user and the changes vanish. Wrap calls in
`#if EDITOR` since the symbol only exists in editor builds.

`TileMap::SetCell` (and `ReplaceTile` / `ReplaceTilesWithTag` /
`SetTileSet`) set the dirty flag automatically. The Layer panel's
visible/locked/name edits and the Add Layer button also call
`tileMap->SetDirtyFlag()` directly.

---

## File map

| File | Purpose |
|---|---|
| `Engine/Source/Engine/Assets/TileSet.{h,cpp}` | Atlas slicing, tile metadata, 9-box brushes, autotile sets |
| `Engine/Source/Engine/Assets/TileMap.{h,cpp}` | Sparse chunked layered cell storage |
| `Engine/Source/Engine/Nodes/3D/TileMap2d.{h,cpp}` | Scene node + render path |
| `Engine/Source/Graphics/GraphicsTypes.h` | `TileMap2DResource` struct |
| `Engine/Source/Graphics/Graphics.h` | `GFX_*TileMap2D*` declarations |
| `Engine/Source/Graphics/Vulkan/VulkanUtils.{h,cpp}` | Vulkan implementation |
| `Engine/Source/Graphics/Vulkan/Graphics_Vulkan.cpp` | `GFX_*` wrappers |
| `Engine/Source/Graphics/GX/Graphics_GX.cpp` | GX no-op stubs |
| `Engine/Source/Graphics/C3D/Graphics_C3D.cpp` | C3D no-op stubs |
| `Engine/Source/Editor/TilePaint/TilePaintManager.{h,cpp}` | Editor paint state machine |
| `Engine/Source/Editor/TilePaint/TilePicker.{h,cpp}` | Reusable atlas picker UI |
| `Engine/Source/Editor/EditorImgui.cpp` | Tile Paint panel UI + tile preview overlay |
| `Engine/Source/Editor/EditorState.{h,cpp}` | `PaintMode::TilePaint`, mTilePaintManager, camera stash |
| `Engine/Source/Editor/Viewport3d.cpp` | Camera rotation lock, Update dispatch, modifier gate |
| `Engine/Source/Editor/ActionManager.{h,cpp}` | `ActionPaintTiles` |
| `Engine/Source/Editor/Preferences/Appearance/Viewport/TilemapGrid/TilemapGridModule.{h,cpp}` | Grid color preferences |
| `Engine/Source/Editor/Preferences/PreferencesManager.cpp` | Module registration |
| `Engine/Source/LuaBindings/TileMap2d_Lua.{h,cpp}` | Lua runtime API |
| `Engine/Source/LuaBindings/LuaBindings.cpp` | Bind() registration |
| `Engine/Source/Engine/Engine.cpp` | `FORCE_LINK_CALL` registrations |
| `Engine/Source/Engine/Asset.h` | Version constants |
| `Engine/Engine.vcxproj` + `.filters` | Windows build registration |
| `Engine/Makefile_Linux` | Linux build registration (TilePaint + TilemapGrid directories) |
| `Engine/Generated/LuaMeta/TileMap2D.lua` | Auto-generated Lua autocomplete stubs |

---

## Phased development history

The TileMap system was implemented in four phases following the spec at
`.dev/Polyphase_TileMap_Authoring_System_Spec.md`:

- **Phase 1** — Foundation: TileSet/TileMap/TileMap2D, sparse chunks, atlas
  slicing, single-layer painting, pencil/eraser/picker, save/load,
  Vulkan render path, basic editor panel
- **Phase 2** — Proper authoring: tile palette grid, multi-layer panel,
  rect/flood/line tools, per-tile tags + collision storage, core Lua
  query API
- **Phase 3** — Advanced productivity: 9-box brushes, marquee selection
  with copy/cut/paste/flip/rotate, collision + tag debug overlays, Find
  All Uses + bulk Replace helpers, full region/closest/neighbor Lua API
- **Phase 4** — Smart systems: autotile rule sets with 3×3 tri-state
  authoring UI, RaycastTiles + GetReachableCells nav helpers, freeform
  selection (Shift/Ctrl modifiers), 9-Box Fill on freeform selections,
  cell grid overlay with preferences entry, auto-orthographic camera,
  camera rotation lock

A handful of follow-up bug fixes hardened the system: vertex color scale
compensation, the `mUploadDirty[]` upload-flag bug, the `INP_GetMousePosition`
vs `ImGui::GetIO().MousePos` switch, the ortho `ScreenToWorldPosition`
branch, the `ShouldHandleInput` mid-stroke kill, the `World::AddLine`
lifetime, the `SM_Cube` 2-unit-cube scale, the `Asset::SetDirtyFlag` save
wiring, and the Datum-handler-before-write timing.
