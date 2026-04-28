#pragma once

#if EDITOR

#include <vector>
#include <set>
#include <string>
#include <cstdint>
#include "Maths.h"
#include "Assets/TileMap.h"

class TileMap2D;
class Node;

enum class TileSculptMode : uint8_t
{
    Pencil = 0,
    Eraser,
    Picker,
    RectFill,
    FloodFill,
    Line,
    NineBox,
    Select,
    Autotile,

    Count
};

// One mutated cell in a stroke. Captured both for forward application and
// for reverse-undo.
struct TilePaintChange
{
    int32_t mCellX = 0;
    int32_t mCellY = 0;
    int32_t mLayer = 0;
    TileCell mOldCell;
    TileCell mNewCell;
};

struct TileSculptOptions
{
    TileSculptMode mMode = TileSculptMode::Pencil;
    int32_t mSelectedTileIndex = 0;
    int32_t mActiveLayer = 0;

    // Index into TileSet::mNineBoxBrushes; -1 means none chosen.
    int32_t mActiveNineBoxIndex = -1;

    // Index into TileSet::mAutotileSets; -1 means none chosen.
    int32_t mActiveAutotileIndex = -1;

    // View toggles
    bool mShowCollisionOverlay = false;
    bool mShowTagOverlay = false;
    bool mShowCellGrid = false;
    std::string mTagOverlayName = "";
};

// Snapshot of a rectangular region of tiles, used by Copy/Cut/Paste/Transform.
struct TileClipboard
{
    int32_t mWidth = 0;
    int32_t mHeight = 0;
    std::vector<TileCell> mCells;  // row-major, mWidth * mHeight entries

    bool IsValid() const { return mWidth > 0 && mHeight > 0 && !mCells.empty(); }
    void Clear() { mWidth = 0; mHeight = 0; mCells.clear(); }
};

class TilePaintManager
{
public:

    TilePaintManager();
    ~TilePaintManager();

    void Update();
    void HandleNodeDestroy(Node* node);

    TileSculptOptions mOptions;

    // Hover state — read by the paint panel UI.
    bool mHoverValid = false;
    glm::ivec2 mHoverCell = { 0, 0 };

    // Preview state for drag-based tools (RectFill, Line, NineBox, Select).
    bool mPreviewActive = false;
    glm::ivec2 mDragStartCell = { 0, 0 };

    // Per-frame interpolation state for free-stroke pencil/eraser drags.
    // Reset when a new stroke starts, updated each tick the user holds the
    // mouse and moves between cells. The panel uses this to ghost-preview
    // the cells the pencil is filling in along the way.
    bool mStrokeActive = false;
    bool mLastStrokeCellValid = false;
    glm::ivec2 mLastStrokeCell = { 0, 0 };

    // Active marquee / freeform selection. mSelectedCells is the source of
    // truth — it stores every cell coordinate currently selected, packed as
    // (uint32_t(x) | (uint64_t(uint32_t(y)) << 32)). The min/max bounding
    // rect is recomputed from the set whenever it changes so the existing
    // copy/cut/paste paths and the 4-corner outline still work.
    bool mHasSelection = false;
    glm::ivec2 mSelectionMin = { 0, 0 };
    glm::ivec2 mSelectionMax = { 0, 0 };
    std::set<int64_t> mSelectedCells;

    static int64_t SelectionCellKey(int32_t cellX, int32_t cellY)
    {
        return int64_t(uint32_t(cellX)) | (int64_t(uint32_t(cellY)) << 32);
    }
    static void DecodeSelectionCellKey(int64_t key, int32_t& outX, int32_t& outY)
    {
        outX = int32_t(uint32_t(key & 0xFFFFFFFFu));
        outY = int32_t(uint32_t((key >> 32) & 0xFFFFFFFFu));
    }
    bool IsCellSelected(int32_t cellX, int32_t cellY) const
    {
        return mSelectedCells.count(SelectionCellKey(cellX, cellY)) > 0;
    }
    void RecomputeSelectionBounds();

    // Public API the panel calls for clipboard / transform operations.
    void DoCopy();
    void DoCut();
    void DoPaste(glm::ivec2 destOrigin);  // origin = bottom-left cell
    void DoPasteAtCursor();
    void DoFlipClipboardX();
    void DoFlipClipboardY();
    void DoRotateClipboard90();
    // Apply the active 9-box brush to the current marquee selection — every
    // cell in the selection gets the appropriate corner/edge/center tile.
    void DoApply9BoxToSelection();
    bool HasClipboard() const { return mClipboard.IsValid(); }
    bool HasSelection() const { return mHasSelection; }
    glm::ivec2 GetSelectionMin() const { return mSelectionMin; }
    glm::ivec2 GetSelectionMax() const { return mSelectionMax; }
    void ClearSelection() { mHasSelection = false; mSelectedCells.clear(); }

private:

    void UpdateHover();
    void UpdateStroke();
    void CommitStroke();
    void CancelStroke();
    void DrawCursor();
    void DrawOverlays();
    void DrawGridOverlay();
    void DrawSelectionOutline();
    void ApplyPencilOrEraser(TileMap2D* node, glm::ivec2 cell);
    void StrokePencilOrEraserInterp(TileMap2D* node, glm::ivec2 from, glm::ivec2 to);
    void CommitRectFill(TileMap2D* node, glm::ivec2 a, glm::ivec2 b);
    void CommitLine(TileMap2D* node, glm::ivec2 a, glm::ivec2 b);
    void CommitFloodFill(TileMap2D* node, glm::ivec2 origin);
    void CommitNineBox(TileMap2D* node, glm::ivec2 a, glm::ivec2 b);
    void CommitAutotileAt(TileMap2D* node, glm::ivec2 cell);
    void StageCellChange(int32_t cellX, int32_t cellY, int32_t layer,
                         const TileCell& oldCell, const TileCell& newCell);
    TileCell BuildBrushCell(const TileCell& existing) const;
    int32_t PickNineBoxSlot(int32_t cx, int32_t cy,
                            int32_t minX, int32_t maxX,
                            int32_t minY, int32_t maxY) const;
    // Compute the 8-neighbor "self mask" for the given cell. The pendingPaint
    // map lets the caller pretend a set of cells are already members (used by
    // autotile so the painted cell counts as self before the asset has it).
    uint8_t ComputeAutotileSelfMask(TileMap2D* node, int32_t cellX, int32_t cellY,
                                    int32_t layer, int32_t autotileIdx,
                                    const std::set<int64_t>& pendingSelfCells) const;

    static int64_t CellKey(int32_t cellX, int32_t cellY, int32_t layer);

    TileMap2D* mPendingTarget = nullptr;
    std::vector<TilePaintChange> mPendingChanges;
    std::set<int64_t> mModifiedSet;

    TileClipboard mClipboard;
};

#endif
