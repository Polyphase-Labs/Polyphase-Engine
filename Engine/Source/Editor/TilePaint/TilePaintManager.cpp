#if EDITOR

#include "TilePaint/TilePaintManager.h"
#include "EditorState.h"
#include "ActionManager.h"
#include "Viewport3d.h"
#include "Renderer.h"
#include "World.h"
#include "Line.h"
#include "Preferences/Appearance/Viewport/TilemapGrid/TilemapGridModule.h"
#include "InputDevices.h"
#include "AssetManager.h"
#include "Log.h"

#include "imgui.h"

#include "Nodes/3D/Camera3d.h"
#include "Nodes/3D/TileMap2d.h"
#include "Assets/TileMap.h"
#include "Assets/TileSet.h"
#include "Assets/StaticMesh.h"

#include <algorithm>
#include <cmath>
#include <queue>

TilePaintManager::TilePaintManager()
{
}

TilePaintManager::~TilePaintManager()
{
}

int64_t TilePaintManager::CellKey(int32_t cellX, int32_t cellY, int32_t layer)
{
    // Pack signed cell coords + layer into 64 bits for stroke deduplication.
    return (int64_t(uint32_t(cellX) & 0xFFFFFFu)) |
           (int64_t(uint32_t(cellY) & 0xFFFFFFu) << 24) |
           (int64_t(uint32_t(layer)  & 0xFFFFu)   << 48);
}

void TilePaintManager::Update()
{
    Node* selected = GetEditorState()->GetSelectedNode();
    TileMap2D* tileMapNode = nullptr;

    if (selected != nullptr && selected->GetType() == TileMap2D::GetStaticType())
    {
        tileMapNode = static_cast<TileMap2D*>(selected);
    }

    if (tileMapNode == nullptr)
    {
        mHoverValid = false;
        mPreviewActive = false;
        if (mStrokeActive)
        {
            CancelStroke();
        }
        return;
    }

    UpdateHover();
    DrawCursor();
    UpdateStroke();
}

void TilePaintManager::HandleNodeDestroy(Node* node)
{
    if (mPendingTarget == static_cast<TileMap2D*>(node))
    {
        mPendingChanges.clear();
        mModifiedSet.clear();
        mStrokeActive = false;
        mPreviewActive = false;
        mPendingTarget = nullptr;
    }
}

void TilePaintManager::UpdateHover()
{
    mHoverValid = false;

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != TileMap2D::GetStaticType())
        return;

    TileMap2D* tileMapNode = static_cast<TileMap2D*>(selected);

    // The viewport-hovered check guards entry, but during an active stroke we
    // want to keep updating the hover cell even if the cursor briefly drifts
    // over a floating panel. Otherwise the drag-paint trail freezes the moment
    // ImGui reports the Tile Paint panel as hovered.
    if (!mStrokeActive && !GetEditorState()->GetViewport3D()->ShouldHandleInput())
        return;

    Camera3D* camera = GetWorld(0)->GetActiveCamera();
    if (camera == nullptr)
        return;

    // Read the live mouse position straight from ImGui's IO. The engine's
    // INP_GetMousePosition routes through WM_MOUSEMOVE, but ImGui's Win32
    // backend handler claims those messages first (and only updates the
    // engine's mPointerX/mPointerY occasionally), so during a held-button
    // drag the engine sees a frozen cursor. ImGui's IO always reports the
    // live position because the backend updates it every frame.
    ImVec2 imguiMouse = ImGui::GetIO().MousePos;
    int32_t mouseX = int32_t(imguiMouse.x);
    int32_t mouseY = int32_t(imguiMouse.y);

    // Build the cursor ray. The math is different for perspective vs ortho
    // cameras: a perspective camera fires every ray from a single origin
    // (camera position) in the direction of the screen pixel; an orthographic
    // camera fires PARALLEL rays — each one originates at the cursor's
    // screen-to-world point and travels along the camera's forward vector.
    // Treating the ortho case like perspective gives a near-zero parallax
    // ray that hits the wrong cell (effectively pinned near the camera).
    glm::vec3 rayOrigin;
    glm::vec3 rayDir;
    glm::vec3 screenWorld = camera->ScreenToWorldPosition(mouseX, mouseY);
    if (camera->GetProjectionMode() == ProjectionMode::ORTHOGRAPHIC)
    {
        rayOrigin = screenWorld;
        rayDir = camera->GetForwardVector();
        if (glm::length(rayDir) < 1e-6f)
            return;
        rayDir = glm::normalize(rayDir);
    }
    else
    {
        rayOrigin = camera->GetWorldPosition();
        rayDir = screenWorld - rayOrigin;
        if (glm::length(rayDir) < 1e-6f)
            return;
        rayDir = glm::normalize(rayDir);
    }

    TileMap* tileMap = tileMapNode->GetTileMap();
    int32_t layerIndex = mOptions.mActiveLayer;
    float planeZ = 0.0f;
    if (tileMap != nullptr)
    {
        const TileMapLayer* layer = tileMap->GetLayer(layerIndex);
        if (layer != nullptr)
        {
            planeZ = float(layer->mZOrder) * 0.01f;
        }
    }
    planeZ += tileMapNode->GetWorldPosition().z;

    if (std::fabs(rayDir.z) < 1e-6f)
        return;

    float t = (planeZ - rayOrigin.z) / rayDir.z;
    if (t < 0.0f)
        return;

    glm::vec3 hit = rayOrigin + rayDir * t;
    glm::ivec2 cell = tileMapNode->WorldToCell(glm::vec2(hit.x, hit.y));

    mHoverValid = true;
    mHoverCell = cell;
}

void TilePaintManager::DrawCursor()
{
    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != TileMap2D::GetStaticType())
        return;

    TileMap2D* tileMapNode = static_cast<TileMap2D*>(selected);
    TileMap* tileMap = tileMapNode->GetTileMap();
    if (tileMap == nullptr)
        return;

    glm::ivec2 tileSize = tileMap->GetTileSize();
    if (tileSize.x <= 0 || tileSize.y <= 0)
        return;

    // The grid + collision/tag overlays should be visible whenever their
    // toggles are on, even if the mouse isn't over the viewport. Only the
    // hover-cell cursor cube and the drag preview depend on a valid hover.
    DrawOverlays();
    DrawGridOverlay();

    // Persistent selection outline — also independent of mouse hover.
    glm::vec4 selOutlineColor(0.9f, 0.9f, 0.2f, 0.7f);
    auto drawCellCubeColored = [&](glm::ivec2 cell, const glm::vec4& col)
    {
        glm::vec2 worldXY = tileMapNode->CellCenterToWorld(cell);
        // Place the cursor cube SLIGHTLY ABOVE the tile mesh in Z so it's
        // always visible from a top-down ortho view (otherwise the tile mesh
        // depth-fights the cursor cube and the cursor disappears). Z scale is
        // tiny so we're effectively rendering a flat colored quad.
        // SM_Cube vertices are at ±1 (a 2-unit cube), so scale by half the
        // tile size to make the highlight cover exactly one cell.
        float baseZ = tileMapNode->GetWorldPosition().z + 0.05f;
        DebugDraw d;
        d.mMesh = LoadAsset<StaticMesh>("SM_Cube");
        d.mTransform =
            glm::translate(glm::mat4(1.0f), glm::vec3(worldXY.x, worldXY.y, baseZ)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(float(tileSize.x) * 0.5f, float(tileSize.y) * 0.5f, 0.02f));
        d.mColor = col;
        d.mLife = 0.0f;
        Renderer::Get()->AddDebugDraw(d);
    };

    if (mHasSelection)
    {
        // Highlight every cell in the (possibly freeform) selection. Capped
        // to keep the debug-draw count manageable on huge selections.
        constexpr size_t kMaxSelectionDraws = 2048;
        size_t drawn = 0;
        for (int64_t key : mSelectedCells)
        {
            if (drawn >= kMaxSelectionDraws) break;
            int32_t cx, cy;
            DecodeSelectionCellKey(key, cx, cy);
            drawCellCubeColored({ cx, cy }, selOutlineColor);
            ++drawn;
        }
    }

    // Hover-dependent visuals: cursor cube and drag preview corners.
    if (!mHoverValid)
        return;

    glm::vec4 color;
    switch (mOptions.mMode)
    {
        case TileSculptMode::Pencil:    color = glm::vec4(0.2f, 0.9f, 0.2f, 0.5f); break;
        case TileSculptMode::Eraser:    color = glm::vec4(0.9f, 0.2f, 0.2f, 0.5f); break;
        case TileSculptMode::Picker:    color = glm::vec4(0.2f, 0.4f, 0.9f, 0.5f); break;
        case TileSculptMode::RectFill:  color = glm::vec4(0.2f, 0.9f, 0.9f, 0.5f); break;
        case TileSculptMode::FloodFill: color = glm::vec4(0.9f, 0.6f, 0.2f, 0.5f); break;
        case TileSculptMode::Line:      color = glm::vec4(0.9f, 0.4f, 0.9f, 0.5f); break;
        case TileSculptMode::NineBox:   color = glm::vec4(0.5f, 0.9f, 0.4f, 0.5f); break;
        case TileSculptMode::Select:    color = glm::vec4(0.9f, 0.9f, 0.9f, 0.5f); break;
        case TileSculptMode::Autotile:  color = glm::vec4(0.4f, 0.6f, 1.0f, 0.5f); break;
        default:                        color = glm::vec4(1.0f); break;
    }

    drawCellCubeColored(mHoverCell, color);

    if (mPreviewActive && mPendingTarget == tileMapNode)
    {
        if (mOptions.mMode == TileSculptMode::RectFill ||
            mOptions.mMode == TileSculptMode::NineBox ||
            mOptions.mMode == TileSculptMode::Select)
        {
            int32_t minX = std::min(mDragStartCell.x, mHoverCell.x);
            int32_t maxX = std::max(mDragStartCell.x, mHoverCell.x);
            int32_t minY = std::min(mDragStartCell.y, mHoverCell.y);
            int32_t maxY = std::max(mDragStartCell.y, mHoverCell.y);

            drawCellCubeColored({ minX, minY }, color);
            drawCellCubeColored({ maxX, minY }, color);
            drawCellCubeColored({ minX, maxY }, color);
            drawCellCubeColored({ maxX, maxY }, color);
        }
        else if (mOptions.mMode == TileSculptMode::Line)
        {
            drawCellCubeColored(mDragStartCell, color);
        }
    }
}

void TilePaintManager::DrawOverlays()
{
    if (!mOptions.mShowCollisionOverlay && !mOptions.mShowTagOverlay)
        return;

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != TileMap2D::GetStaticType())
        return;

    TileMap2D* tileMapNode = static_cast<TileMap2D*>(selected);
    TileMap* tileMap = tileMapNode->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;
    if (tileMap == nullptr || tileSet == nullptr)
        return;

    glm::ivec2 tileSize = tileMap->GetTileSize();
    if (tileSize.x <= 0 || tileSize.y <= 0)
        return;

    int32_t layerIndex = mOptions.mActiveLayer;
    const TileMapLayer* layer = tileMap->GetLayer(layerIndex);
    if (layer == nullptr)
        return;

    glm::vec4 collisionColor(0.95f, 0.15f, 0.15f, 0.35f);
    glm::vec4 tagColor(0.2f, 0.95f, 0.95f, 0.35f);
    const std::string& tagToHighlight = mOptions.mTagOverlayName;

    // Cap iteration to avoid lagging in huge maps.
    constexpr int32_t kMaxOverlayCells = 4096;
    int32_t drawn = 0;

    for (const auto& kv : layer->mChunks)
    {
        if (drawn >= kMaxOverlayCells) break;

        int32_t cx, cy;
        TileMap::UnpackChunkKey(kv.first, cx, cy);
        int32_t baseX = cx * kTileChunkSize;
        int32_t baseY = cy * kTileChunkSize;

        for (int32_t ly = 0; ly < kTileChunkSize && drawn < kMaxOverlayCells; ++ly)
        {
            for (int32_t lx = 0; lx < kTileChunkSize && drawn < kMaxOverlayCells; ++lx)
            {
                const TileCell& cell = kv.second.mCells[ly * kTileChunkSize + lx];
                if (cell.mTileIndex < 0) continue;

                bool drawCollision = mOptions.mShowCollisionOverlay && tileSet->IsTileSolid(cell.mTileIndex);
                bool drawTag = mOptions.mShowTagOverlay && !tagToHighlight.empty() &&
                               tileSet->HasTileTag(cell.mTileIndex, tagToHighlight);
                if (!drawCollision && !drawTag)
                    continue;

                glm::ivec2 worldCell = { baseX + lx, baseY + ly };
                glm::vec2 worldXY = tileMapNode->CellCenterToWorld(worldCell);
                DebugDraw d;
                d.mMesh = LoadAsset<StaticMesh>("SM_Cube");
                // SM_Cube extents are ±1, so half-tile scale gives a one-cell quad.
                d.mTransform =
                    glm::translate(glm::mat4(1.0f), glm::vec3(worldXY.x, worldXY.y, tileMapNode->GetWorldPosition().z + 0.04f)) *
                    glm::scale(glm::mat4(1.0f), glm::vec3(float(tileSize.x) * 0.5f, float(tileSize.y) * 0.5f, 0.02f));
                d.mColor = drawCollision ? collisionColor : tagColor;
                d.mLife = 0.0f;
                Renderer::Get()->AddDebugDraw(d);
                ++drawn;
            }
        }
    }
}

void TilePaintManager::UpdateStroke()
{
    // Allow an in-progress stroke to keep running even when the viewport
    // isn't "hovered" by ImGui's metric — otherwise dragging the cursor
    // anywhere near the floating Tile Paint panel kills the stroke
    // mid-motion. New strokes still gate on ShouldHandleInput via the
    // IsMouseButtonJustDown branch below (the click that starts a stroke
    // must happen while the viewport is active).
    if (!mStrokeActive && !GetEditorState()->GetViewport3D()->ShouldHandleInput())
        return;

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != TileMap2D::GetStaticType())
        return;

    TileMap2D* tileMapNode = static_cast<TileMap2D*>(selected);

    // Mouse down: begin appropriate stroke for the active tool.
    if (IsMouseButtonJustDown(MOUSE_LEFT) && mHoverValid)
    {
        switch (mOptions.mMode)
        {
            case TileSculptMode::Picker:
            {
                TileMap* tileMap = tileMapNode->GetTileMap();
                if (tileMap != nullptr)
                {
                    int32_t picked = tileMap->GetTile(mHoverCell.x, mHoverCell.y, mOptions.mActiveLayer);
                    if (picked >= 0)
                        mOptions.mSelectedTileIndex = picked;
                }
                return;
            }

            case TileSculptMode::FloodFill:
            {
                CommitFloodFill(tileMapNode, mHoverCell);
                return;
            }

            case TileSculptMode::Autotile:
            {
                // Drag-paint autotile. Each cell touched paints itself + its
                // 8 neighbors via the active autotile rules. The whole stroke
                // commits as a single undoable action.
                mStrokeActive = true;
                mPendingTarget = tileMapNode;
                mPendingChanges.clear();
                mModifiedSet.clear();
                CommitAutotileAt(tileMapNode, mHoverCell);
                return;
            }

            case TileSculptMode::RectFill:
            case TileSculptMode::Line:
            case TileSculptMode::NineBox:
            case TileSculptMode::Select:
            {
                mStrokeActive = true;
                mPreviewActive = true;
                mPendingTarget = tileMapNode;
                mDragStartCell = mHoverCell;
                mPendingChanges.clear();
                mModifiedSet.clear();
                return;
            }

            case TileSculptMode::Pencil:
            case TileSculptMode::Eraser:
            default:
            {
                mStrokeActive = true;
                mPendingTarget = tileMapNode;
                mPendingChanges.clear();
                mModifiedSet.clear();
                mLastStrokeCell = mHoverCell;
                mLastStrokeCellValid = true;
                ApplyPencilOrEraser(tileMapNode, mHoverCell);
                return;
            }
        }
    }

    // Mouse held: continue freehand strokes for pencil/eraser/autotile.
    // Rect/Line/9-Box/Select just update their preview via DrawCursor and
    // apply at mouse-up.
    if (IsMouseButtonDown(MOUSE_LEFT) && mStrokeActive && mHoverValid && mPendingTarget == tileMapNode)
    {
        if (mOptions.mMode == TileSculptMode::Pencil ||
            mOptions.mMode == TileSculptMode::Eraser)
        {
            // Interpolate cells between the previous frame's cell and the
            // current cell so fast drags don't leave gaps. Falls back to a
            // single-cell paint when the cursor stays put.
            if (mLastStrokeCellValid)
            {
                StrokePencilOrEraserInterp(tileMapNode, mLastStrokeCell, mHoverCell);
            }
            else
            {
                ApplyPencilOrEraser(tileMapNode, mHoverCell);
            }
            mLastStrokeCell = mHoverCell;
            mLastStrokeCellValid = true;
        }
        else if (mOptions.mMode == TileSculptMode::Autotile)
        {
            CommitAutotileAt(tileMapNode, mHoverCell);
        }
    }

    // Mouse up: commit drag-based tools, then close out the stroke.
    if (IsMouseButtonJustUp(MOUSE_LEFT) && mStrokeActive)
    {
        if (mPreviewActive && mPendingTarget != nullptr && mHoverValid)
        {
            switch (mOptions.mMode)
            {
                case TileSculptMode::RectFill:
                    CommitRectFill(mPendingTarget, mDragStartCell, mHoverCell);
                    break;
                case TileSculptMode::Line:
                    CommitLine(mPendingTarget, mDragStartCell, mHoverCell);
                    break;
                case TileSculptMode::NineBox:
                    CommitNineBox(mPendingTarget, mDragStartCell, mHoverCell);
                    break;
                case TileSculptMode::Select:
                {
                    // Freeform selection. Plain drag REPLACES the selection,
                    // Shift+drag ADDS to it, Ctrl+drag REMOVES from it. Each
                    // commit pushes every cell within the drag rectangle into
                    // (or out of) mSelectedCells, then derives the bounding
                    // rect for downstream code that still works in rect terms.
                    int32_t minX = std::min(mDragStartCell.x, mHoverCell.x);
                    int32_t maxX = std::max(mDragStartCell.x, mHoverCell.x);
                    int32_t minY = std::min(mDragStartCell.y, mHoverCell.y);
                    int32_t maxY = std::max(mDragStartCell.y, mHoverCell.y);

                    bool shiftDown = IsShiftDown();
                    bool ctrlDown  = IsControlDown();

                    if (!shiftDown && !ctrlDown)
                        mSelectedCells.clear();

                    if (ctrlDown)
                    {
                        for (int32_t cy = minY; cy <= maxY; ++cy)
                            for (int32_t cx = minX; cx <= maxX; ++cx)
                                mSelectedCells.erase(SelectionCellKey(cx, cy));
                    }
                    else
                    {
                        for (int32_t cy = minY; cy <= maxY; ++cy)
                            for (int32_t cx = minX; cx <= maxX; ++cx)
                                mSelectedCells.insert(SelectionCellKey(cx, cy));
                    }

                    RecomputeSelectionBounds();
                    break;
                }
                default:
                    break;
            }
        }

        CommitStroke();
    }
}

TileCell TilePaintManager::BuildBrushCell(const TileCell& existing) const
{
    TileCell out = existing;
    if (mOptions.mMode == TileSculptMode::Eraser)
    {
        out.mTileIndex = -1;
        out.mFlags = 0;
        out.mVariant = 0;
    }
    else
    {
        // Pencil / RectFill / FloodFill / Line all paint the selected index.
        out.mTileIndex = mOptions.mSelectedTileIndex;
    }
    return out;
}

void TilePaintManager::StageCellChange(int32_t cellX, int32_t cellY, int32_t layer,
                                       const TileCell& oldCell, const TileCell& newCell)
{
    if (oldCell.mTileIndex == newCell.mTileIndex &&
        oldCell.mFlags == newCell.mFlags &&
        oldCell.mVariant == newCell.mVariant)
    {
        return;
    }

    int64_t key = CellKey(cellX, cellY, layer);
    if (mModifiedSet.count(key) > 0)
        return;

    TileMap* tileMap = mPendingTarget->GetTileMap();
    tileMap->SetCell(cellX, cellY, newCell, layer);
    mModifiedSet.insert(key);

    TilePaintChange change;
    change.mCellX = cellX;
    change.mCellY = cellY;
    change.mLayer = layer;
    change.mOldCell = oldCell;
    change.mNewCell = newCell;
    mPendingChanges.push_back(change);
}

void TilePaintManager::ApplyPencilOrEraser(TileMap2D* node, glm::ivec2 cell)
{
    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return;

    int32_t layer = mOptions.mActiveLayer;
    TileCell oldCell = tileMap->GetCell(cell.x, cell.y, layer);
    TileCell newCell = BuildBrushCell(oldCell);
    StageCellChange(cell.x, cell.y, layer, oldCell, newCell);
}

void TilePaintManager::StrokePencilOrEraserInterp(TileMap2D* node, glm::ivec2 from, glm::ivec2 to)
{
    // Bresenham line walk between cells. Paints every cell along the path,
    // including both endpoints. Used during pencil/eraser drag so a fast
    // mouse drag still produces a contiguous stroke instead of skipping
    // cells based on per-frame mouse positions.
    int32_t x0 = from.x, y0 = from.y;
    int32_t x1 = to.x,   y1 = to.y;
    int32_t dx = std::abs(x1 - x0);
    int32_t dy = -std::abs(y1 - y0);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;

    constexpr int32_t kMaxInterpCells = 4096;
    int32_t safety = 0;
    while (safety++ < kMaxInterpCells)
    {
        ApplyPencilOrEraser(node, { x0, y0 });
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void TilePaintManager::CommitRectFill(TileMap2D* node, glm::ivec2 a, glm::ivec2 b)
{
    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return;

    int32_t layer = mOptions.mActiveLayer;
    int32_t minX = std::min(a.x, b.x);
    int32_t maxX = std::max(a.x, b.x);
    int32_t minY = std::min(a.y, b.y);
    int32_t maxY = std::max(a.y, b.y);

    // Cap the rect at a sane size to avoid runaway brushes.
    constexpr int32_t kMaxRectCells = 1024 * 1024;
    int64_t totalCells = int64_t(maxX - minX + 1) * int64_t(maxY - minY + 1);
    if (totalCells > kMaxRectCells)
    {
        LogWarning("RectFill capped at %d cells (requested %lld)", kMaxRectCells, (long long)totalCells);
        return;
    }

    for (int32_t cy = minY; cy <= maxY; ++cy)
    {
        for (int32_t cx = minX; cx <= maxX; ++cx)
        {
            TileCell oldCell = tileMap->GetCell(cx, cy, layer);
            TileCell newCell = BuildBrushCell(oldCell);
            StageCellChange(cx, cy, layer, oldCell, newCell);
        }
    }
}

void TilePaintManager::CommitLine(TileMap2D* node, glm::ivec2 a, glm::ivec2 b)
{
    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return;

    int32_t layer = mOptions.mActiveLayer;

    // Bresenham line between cell coords (handles negative cells natively).
    int32_t x0 = a.x, y0 = a.y;
    int32_t x1 = b.x, y1 = b.y;
    int32_t dx = std::abs(x1 - x0);
    int32_t dy = -std::abs(y1 - y0);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;

    int32_t safetyCounter = 0;
    constexpr int32_t kMaxLineCells = 16 * 1024;

    while (true)
    {
        TileCell oldCell = tileMap->GetCell(x0, y0, layer);
        TileCell newCell = BuildBrushCell(oldCell);
        StageCellChange(x0, y0, layer, oldCell, newCell);

        if (x0 == x1 && y0 == y1) break;
        if (++safetyCounter > kMaxLineCells)
        {
            LogWarning("Line tool aborted at %d cells", kMaxLineCells);
            break;
        }
        int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void TilePaintManager::CommitFloodFill(TileMap2D* node, glm::ivec2 origin)
{
    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return;

    int32_t layer = mOptions.mActiveLayer;
    int32_t targetTile = tileMap->GetTile(origin.x, origin.y, layer);
    int32_t newTile = mOptions.mSelectedTileIndex;

    // Eraser flood replaces with empty (-1) instead.
    if (mOptions.mMode == TileSculptMode::Eraser)
        newTile = -1;

    if (targetTile == newTile)
        return;

    constexpr int32_t kMaxFillCells = 16384;

    // Stage everything as a single action so undo restores it in one step.
    mStrokeActive = true;
    mPendingTarget = node;
    mPendingChanges.clear();
    mModifiedSet.clear();

    std::queue<glm::ivec2> q;
    q.push(origin);

    int32_t painted = 0;
    while (!q.empty() && painted < kMaxFillCells)
    {
        glm::ivec2 c = q.front();
        q.pop();

        int64_t key = CellKey(c.x, c.y, layer);
        if (mModifiedSet.count(key) > 0)
            continue;

        TileCell oldCell = tileMap->GetCell(c.x, c.y, layer);
        if (oldCell.mTileIndex != targetTile)
            continue;

        TileCell newCell = oldCell;
        newCell.mTileIndex = newTile;
        if (newTile < 0)
        {
            newCell.mFlags = 0;
            newCell.mVariant = 0;
        }

        StageCellChange(c.x, c.y, layer, oldCell, newCell);
        ++painted;

        q.push({ c.x + 1, c.y });
        q.push({ c.x - 1, c.y });
        q.push({ c.x, c.y + 1 });
        q.push({ c.x, c.y - 1 });
    }

    if (painted >= kMaxFillCells)
    {
        LogWarning("FloodFill capped at %d cells", kMaxFillCells);
    }

    CommitStroke();
}

void TilePaintManager::CommitStroke()
{
    if (!mPendingChanges.empty() && mPendingTarget != nullptr)
    {
        ActionManager::Get()->EXE_PaintTiles(mPendingTarget, mPendingChanges);
    }

    mPendingChanges.clear();
    mModifiedSet.clear();
    mStrokeActive = false;
    mPreviewActive = false;
    mLastStrokeCellValid = false;
    mPendingTarget = nullptr;
}

void TilePaintManager::CancelStroke()
{
    mPendingChanges.clear();
    mModifiedSet.clear();
    mStrokeActive = false;
    mPreviewActive = false;
    mLastStrokeCellValid = false;
    mPendingTarget = nullptr;
}

// ---------------------------------------------------------------------------
// 9-Box brush
// ---------------------------------------------------------------------------

int32_t TilePaintManager::PickNineBoxSlot(int32_t cx, int32_t cy,
                                          int32_t minX, int32_t maxX,
                                          int32_t minY, int32_t maxY) const
{
    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != TileMap2D::GetStaticType())
        return -1;
    TileMap2D* node = static_cast<TileMap2D*>(selected);
    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;
    if (tileSet == nullptr) return -1;

    int32_t brushIdx = mOptions.mActiveNineBoxIndex;
    const auto& brushes = tileSet->GetNineBoxBrushes();
    if (brushIdx < 0 || brushIdx >= int32_t(brushes.size())) return -1;
    const NineBoxBrushDef& brush = brushes[brushIdx];

    int32_t width = maxX - minX + 1;
    int32_t height = maxY - minY + 1;

    // Special cases per spec § "Special cases".
    if (width == 1 && height == 1)
        return brush.mCenter;

    if (width == 1)
    {
        if (cy == maxY) return brush.mTop;
        if (cy == minY) return brush.mBottom;
        return brush.mCenter;
    }

    if (height == 1)
    {
        if (cx == minX) return brush.mLeft;
        if (cx == maxX) return brush.mRight;
        return brush.mCenter;
    }

    bool isLeft  = (cx == minX);
    bool isRight = (cx == maxX);
    bool isTop   = (cy == maxY);  // +Y is up in TileMap2D world space
    bool isBot   = (cy == minY);

    if (isTop && isLeft)  return brush.mTopLeft;
    if (isTop && isRight) return brush.mTopRight;
    if (isBot && isLeft)  return brush.mBottomLeft;
    if (isBot && isRight) return brush.mBottomRight;
    if (isTop)  return brush.mTop;
    if (isBot)  return brush.mBottom;
    if (isLeft) return brush.mLeft;
    if (isRight) return brush.mRight;
    return brush.mCenter;
}

void TilePaintManager::RecomputeSelectionBounds()
{
    if (mSelectedCells.empty())
    {
        mHasSelection = false;
        return;
    }

    bool first = true;
    int32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
    for (int64_t key : mSelectedCells)
    {
        int32_t cx, cy;
        DecodeSelectionCellKey(key, cx, cy);
        if (first)
        {
            minX = maxX = cx;
            minY = maxY = cy;
            first = false;
        }
        else
        {
            minX = std::min(minX, cx);
            maxX = std::max(maxX, cx);
            minY = std::min(minY, cy);
            maxY = std::max(maxY, cy);
        }
    }
    mSelectionMin = { minX, minY };
    mSelectionMax = { maxX, maxY };
    mHasSelection = true;
}

void TilePaintManager::DoApply9BoxToSelection()
{
    if (!mHasSelection || mSelectedCells.empty()) return;

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != TileMap2D::GetStaticType())
        return;
    TileMap2D* node = static_cast<TileMap2D*>(selected);
    TileMap* tileMap = node->GetTileMap();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;
    if (tileMap == nullptr || tileSet == nullptr) return;

    int32_t brushIdx = mOptions.mActiveNineBoxIndex;
    const auto& brushes = tileSet->GetNineBoxBrushes();
    if (brushIdx < 0 || brushIdx >= int32_t(brushes.size()))
    {
        LogWarning("9-Box Fill needs an active 9-box brush in the panel.");
        return;
    }
    const NineBoxBrushDef& brush = brushes[brushIdx];

    // Freeform 9-box classification: each selected cell looks at its 4
    // cardinal neighbors. The cell is on a "side" if the corresponding
    // neighbor isn't part of the selection, on a "corner" if both an
    // adjacent vertical AND horizontal side are missing, and in the
    // center otherwise. This produces correct corner/edge/center tiles
    // for any selection shape — works for rectangles, L-shapes,
    // S-curves, isolated regions, etc.
    auto pickSlot = [&](int32_t cx, int32_t cy) -> int32_t
    {
        bool hasTop   = IsCellSelected(cx,     cy + 1);  // +Y is up
        bool hasBot   = IsCellSelected(cx,     cy - 1);
        bool hasLeft  = IsCellSelected(cx - 1, cy    );
        bool hasRight = IsCellSelected(cx + 1, cy    );

        if (!hasTop && !hasLeft)  return brush.mTopLeft;
        if (!hasTop && !hasRight) return brush.mTopRight;
        if (!hasBot && !hasLeft)  return brush.mBottomLeft;
        if (!hasBot && !hasRight) return brush.mBottomRight;
        if (!hasTop)              return brush.mTop;
        if (!hasBot)              return brush.mBottom;
        if (!hasLeft)             return brush.mLeft;
        if (!hasRight)            return brush.mRight;
        return brush.mCenter;
    };

    int32_t layer = mOptions.mActiveLayer;
    mStrokeActive = true;
    mPendingTarget = node;
    mPendingChanges.clear();
    mModifiedSet.clear();

    for (int64_t key : mSelectedCells)
    {
        int32_t cx, cy;
        DecodeSelectionCellKey(key, cx, cy);
        int32_t slotTile = pickSlot(cx, cy);
        if (slotTile < 0) continue;  // empty brush slot — leave the cell alone

        TileCell oldCell = tileMap->GetCell(cx, cy, layer);
        TileCell newCell = oldCell;
        newCell.mTileIndex = slotTile;
        StageCellChange(cx, cy, layer, oldCell, newCell);
    }

    CommitStroke();
}

void TilePaintManager::CommitNineBox(TileMap2D* node, glm::ivec2 a, glm::ivec2 b)
{
    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return;

    TileSet* tileSet = tileMap->GetTileSet();
    if (tileSet == nullptr) return;

    int32_t brushIdx = mOptions.mActiveNineBoxIndex;
    const auto& brushes = tileSet->GetNineBoxBrushes();
    if (brushIdx < 0 || brushIdx >= int32_t(brushes.size()))
    {
        LogWarning("9-box brush has no active brush selected");
        return;
    }

    int32_t layer = mOptions.mActiveLayer;
    int32_t minX = std::min(a.x, b.x);
    int32_t maxX = std::max(a.x, b.x);
    int32_t minY = std::min(a.y, b.y);
    int32_t maxY = std::max(a.y, b.y);

    constexpr int32_t kMaxRectCells = 1024 * 1024;
    int64_t totalCells = int64_t(maxX - minX + 1) * int64_t(maxY - minY + 1);
    if (totalCells > kMaxRectCells)
    {
        LogWarning("NineBox capped at %d cells", kMaxRectCells);
        return;
    }

    for (int32_t cy = minY; cy <= maxY; ++cy)
    {
        for (int32_t cx = minX; cx <= maxX; ++cx)
        {
            int32_t slotTile = PickNineBoxSlot(cx, cy, minX, maxX, minY, maxY);
            if (slotTile < 0) continue;  // empty slot — leave existing cell

            TileCell oldCell = tileMap->GetCell(cx, cy, layer);
            TileCell newCell = oldCell;
            newCell.mTileIndex = slotTile;
            StageCellChange(cx, cy, layer, oldCell, newCell);
        }
    }
}

// ---------------------------------------------------------------------------
// Selection / clipboard / transforms
// ---------------------------------------------------------------------------

void TilePaintManager::DoCopy()
{
    if (!mHasSelection) return;

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != TileMap2D::GetStaticType())
        return;
    TileMap2D* node = static_cast<TileMap2D*>(selected);
    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return;

    int32_t layer = mOptions.mActiveLayer;
    int32_t w = mSelectionMax.x - mSelectionMin.x + 1;
    int32_t h = mSelectionMax.y - mSelectionMin.y + 1;
    if (w <= 0 || h <= 0) return;

    mClipboard.mWidth = w;
    mClipboard.mHeight = h;
    mClipboard.mCells.assign(w * h, TileCell{});

    for (int32_t row = 0; row < h; ++row)
    {
        for (int32_t col = 0; col < w; ++col)
        {
            int32_t cx = mSelectionMin.x + col;
            int32_t cy = mSelectionMin.y + row;
            mClipboard.mCells[row * w + col] = tileMap->GetCell(cx, cy, layer);
        }
    }
}

void TilePaintManager::DoCut()
{
    if (!mHasSelection) return;
    DoCopy();

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != TileMap2D::GetStaticType())
        return;
    TileMap2D* node = static_cast<TileMap2D*>(selected);
    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return;

    int32_t layer = mOptions.mActiveLayer;
    mStrokeActive = true;
    mPendingTarget = node;
    mPendingChanges.clear();
    mModifiedSet.clear();

    for (int32_t cy = mSelectionMin.y; cy <= mSelectionMax.y; ++cy)
    {
        for (int32_t cx = mSelectionMin.x; cx <= mSelectionMax.x; ++cx)
        {
            TileCell oldCell = tileMap->GetCell(cx, cy, layer);
            if (oldCell.mTileIndex < 0) continue;
            TileCell newCell;  // empty
            StageCellChange(cx, cy, layer, oldCell, newCell);
        }
    }

    CommitStroke();
}

void TilePaintManager::DoPaste(glm::ivec2 destOrigin)
{
    if (!mClipboard.IsValid()) return;

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != TileMap2D::GetStaticType())
        return;
    TileMap2D* node = static_cast<TileMap2D*>(selected);
    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return;

    int32_t layer = mOptions.mActiveLayer;
    mStrokeActive = true;
    mPendingTarget = node;
    mPendingChanges.clear();
    mModifiedSet.clear();

    for (int32_t row = 0; row < mClipboard.mHeight; ++row)
    {
        for (int32_t col = 0; col < mClipboard.mWidth; ++col)
        {
            const TileCell& src = mClipboard.mCells[row * mClipboard.mWidth + col];
            // Skip empty source cells so paste doesn't erase under-painted tiles.
            if (src.mTileIndex < 0) continue;

            int32_t cx = destOrigin.x + col;
            int32_t cy = destOrigin.y + row;
            TileCell oldCell = tileMap->GetCell(cx, cy, layer);
            StageCellChange(cx, cy, layer, oldCell, src);
        }
    }

    CommitStroke();
}

void TilePaintManager::DoPasteAtCursor()
{
    if (!mHoverValid) return;
    DoPaste(mHoverCell);
}

void TilePaintManager::DoFlipClipboardX()
{
    if (!mClipboard.IsValid()) return;
    int32_t w = mClipboard.mWidth;
    int32_t h = mClipboard.mHeight;
    for (int32_t row = 0; row < h; ++row)
    {
        for (int32_t col = 0; col < w / 2; ++col)
        {
            std::swap(mClipboard.mCells[row * w + col],
                      mClipboard.mCells[row * w + (w - 1 - col)]);
        }
    }
    // Toggle horizontal flip flag on each cell.
    for (TileCell& c : mClipboard.mCells)
        c.mFlags ^= 0x01;
}

void TilePaintManager::DoFlipClipboardY()
{
    if (!mClipboard.IsValid()) return;
    int32_t w = mClipboard.mWidth;
    int32_t h = mClipboard.mHeight;
    for (int32_t row = 0; row < h / 2; ++row)
    {
        for (int32_t col = 0; col < w; ++col)
        {
            std::swap(mClipboard.mCells[row * w + col],
                      mClipboard.mCells[(h - 1 - row) * w + col]);
        }
    }
    for (TileCell& c : mClipboard.mCells)
        c.mFlags ^= 0x02;
}

// ---------------------------------------------------------------------------
// Cell grid overlay (Phase 4)
// ---------------------------------------------------------------------------

void TilePaintManager::DrawGridOverlay()
{
    if (!mOptions.mShowCellGrid)
        return;

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != TileMap2D::GetStaticType())
        return;

    TileMap2D* tileMapNode = static_cast<TileMap2D*>(selected);
    TileMap* tileMap = tileMapNode->GetTileMap();
    if (tileMap == nullptr)
        return;

    glm::ivec2 tileSize = tileMap->GetTileSize();
    if (tileSize.x <= 0 || tileSize.y <= 0)
        return;

    // Determine the grid range. Prefer the used bounds + a buffer; fall back
    // to a window around the hover cell when the map is empty so the grid is
    // visible while you start painting.
    constexpr int32_t kBufferCells = 4;
    constexpr int32_t kEmptyMapHalfRange = 12;
    constexpr int32_t kMaxLinesPerAxis = 96;

    int32_t minX, maxX, minY, maxY;
    if (tileMap->HasContent())
    {
        glm::ivec2 minU = tileMap->GetMinUsed();
        glm::ivec2 maxU = tileMap->GetMaxUsed();
        minX = minU.x - kBufferCells;
        maxX = maxU.x + kBufferCells;
        minY = minU.y - kBufferCells;
        maxY = maxU.y + kBufferCells;
    }
    else
    {
        // Empty map: anchor on the hover cell if we have one, otherwise fall
        // back to a window centered at the node origin so the grid is visible
        // even when the mouse is outside the viewport.
        glm::ivec2 anchor = mHoverValid ? mHoverCell : glm::ivec2{ 0, 0 };
        minX = anchor.x - kEmptyMapHalfRange;
        maxX = anchor.x + kEmptyMapHalfRange;
        minY = anchor.y - kEmptyMapHalfRange;
        maxY = anchor.y + kEmptyMapHalfRange;
    }

    // Cap the line count so huge maps don't flood the debug-line buffer.
    if (maxX - minX > kMaxLinesPerAxis)
    {
        int32_t mid = (minX + maxX) / 2;
        minX = mid - kMaxLinesPerAxis / 2;
        maxX = mid + kMaxLinesPerAxis / 2;
    }
    if (maxY - minY > kMaxLinesPerAxis)
    {
        int32_t mid = (minY + maxY) / 2;
        minY = mid - kMaxLinesPerAxis / 2;
        maxY = mid + kMaxLinesPerAxis / 2;
    }

    // Map cell coords back to world space. CellToWorld returns the cell's
    // bottom-left corner so we can sweep from minX..maxX+1 cleanly.
    glm::vec2 minWorld = tileMapNode->CellToWorld({ minX, minY });
    glm::vec2 maxWorld = tileMapNode->CellToWorld({ maxX + 1, maxY + 1 });
    float gridZ = tileMapNode->GetWorldPosition().z + 0.002f;

    World* world = tileMapNode->GetWorld();
    if (world == nullptr) return;

    // Pull colors from Preferences > Appearance > Viewport > Tilemap Grid.
    // Falls back to the original hardcoded values if the module isn't loaded
    // yet (preferences register asynchronously during editor startup).
    glm::vec4 minorColor(1.0f, 1.0f, 1.0f, 0.35f);
    glm::vec4 majorColor(1.0f, 1.0f, 0.4f, 0.85f);
    bool highlightAxes = true;
    if (TilemapGridModule* prefs = TilemapGridModule::Get())
    {
        minorColor    = prefs->GetMinorColor();
        majorColor    = prefs->GetAxisColor();
        highlightAxes = prefs->GetHighlightAxes();
    }

    // World::UpdateLines decrements lifetime each tick and removes lines that
    // hit <= 0, so adding a line with lifetime 0 means the very next frame
    // erases it before the renderer ever sees it. Use a small positive
    // lifetime — long enough to survive a frame, short enough to disappear
    // promptly when the toggle is turned off. The dedup logic in
    // World::AddLine bumps the lifetime back to this max each frame, so the
    // grid persists as long as we keep re-adding it.
    constexpr float kGridLineLifetime = 0.25f;

    // Vertical lines
    for (int32_t cx = minX; cx <= maxX + 1; ++cx)
    {
        glm::vec2 a = tileMapNode->CellToWorld({ cx, minY });
        Line line;
        line.mStart = glm::vec3(a.x, minWorld.y, gridZ);
        line.mEnd   = glm::vec3(a.x, maxWorld.y, gridZ);
        line.mColor = (highlightAxes && cx == 0) ? majorColor : minorColor;
        line.mLifetime = kGridLineLifetime;
        world->AddLine(line);
    }
    // Horizontal lines
    for (int32_t cy = minY; cy <= maxY + 1; ++cy)
    {
        glm::vec2 a = tileMapNode->CellToWorld({ minX, cy });
        Line line;
        line.mStart = glm::vec3(minWorld.x, a.y, gridZ);
        line.mEnd   = glm::vec3(maxWorld.x, a.y, gridZ);
        line.mColor = (highlightAxes && cy == 0) ? majorColor : minorColor;
        line.mLifetime = kGridLineLifetime;
        world->AddLine(line);
    }
}

// ---------------------------------------------------------------------------
// Autotile (Phase 4)
// ---------------------------------------------------------------------------

// Neighbor offset table — must match AutotileRule::mNeighbors slot order.
//   [0]=NW [1]=N [2]=NE
//   [3]=W       [4]=E
//   [5]=SW [6]=S [7]=SE
// +Y is up in TileMap2D world space.
static const int32_t kAutotileNeighborOffsets[8][2] =
{
    { -1,  1 }, {  0,  1 }, {  1,  1 },
    { -1,  0 },             {  1,  0 },
    { -1, -1 }, {  0, -1 }, {  1, -1 }
};

uint8_t TilePaintManager::ComputeAutotileSelfMask(TileMap2D* node, int32_t cellX, int32_t cellY,
                                                  int32_t layer, int32_t autotileIdx,
                                                  const std::set<int64_t>& pendingSelfCells) const
{
    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return 0;
    TileSet* tileSet = tileMap->GetTileSet();
    if (tileSet == nullptr) return 0;

    uint8_t mask = 0;
    for (int32_t i = 0; i < 8; ++i)
    {
        int32_t nx = cellX + kAutotileNeighborOffsets[i][0];
        int32_t ny = cellY + kAutotileNeighborOffsets[i][1];

        // pendingSelfCells lets the caller treat freshly painted cells as
        // members before the asset has been updated, which lets the rule
        // evaluator see the cell we just painted as "self".
        int64_t pendingKey = CellKey(nx, ny, layer);
        bool isSelf = false;
        if (pendingSelfCells.count(pendingKey) > 0)
        {
            isSelf = true;
        }
        else
        {
            int32_t neighborTile = tileMap->GetTile(nx, ny, layer);
            if (neighborTile >= 0)
                isSelf = tileSet->IsTileMemberOfAutotile(autotileIdx, neighborTile);
        }

        if (isSelf)
            mask |= uint8_t(1u << i);
    }
    return mask;
}

void TilePaintManager::CommitAutotileAt(TileMap2D* node, glm::ivec2 cell)
{
    TileMap* tileMap = node->GetTileMap();
    if (tileMap == nullptr) return;
    TileSet* tileSet = tileMap->GetTileSet();
    if (tileSet == nullptr) return;

    int32_t autotileIdx = mOptions.mActiveAutotileIndex;
    if (autotileIdx < 0 || autotileIdx >= int32_t(tileSet->GetAutotileSets().size()))
    {
        LogWarning("Autotile brush has no active autotile selected");
        return;
    }

    int32_t layer = mOptions.mActiveLayer;

    // The freshly painted cell counts as "self" for the purpose of
    // recomputing neighbor masks. Track ALL pending self cells so multiple
    // strokes in a row interact correctly.
    std::set<int64_t> pendingSelf;
    int64_t centerKey = CellKey(cell.x, cell.y, layer);
    pendingSelf.insert(centerKey);
    // Reuse mModifiedSet to find prior strokes from this brush stroke that
    // already promoted cells to self.
    for (int64_t k : mModifiedSet)
        pendingSelf.insert(k);

    // Recompute the painted cell + each of its 8 neighbors. Skip neighbors
    // that aren't already members (we don't want to overwrite unrelated tiles).
    auto rebuildAt = [&](int32_t cx, int32_t cy)
    {
        // Determine if this cell should participate in the autotile group.
        // The painted cell always does. Neighbors only do if they're already
        // members of the same group.
        bool isCenter = (cx == cell.x && cy == cell.y);
        if (!isCenter)
        {
            int32_t existing = tileMap->GetTile(cx, cy, layer);
            if (existing < 0 || !tileSet->IsTileMemberOfAutotile(autotileIdx, existing))
                return;
        }

        uint8_t selfMask = ComputeAutotileSelfMask(node, cx, cy, layer, autotileIdx, pendingSelf);
        int32_t resultTile = tileSet->MatchAutotileRule(autotileIdx, selfMask);
        if (resultTile < 0) return;

        TileCell oldCell = tileMap->GetCell(cx, cy, layer);
        TileCell newCell = oldCell;
        newCell.mTileIndex = resultTile;
        StageCellChange(cx, cy, layer, oldCell, newCell);
    };

    rebuildAt(cell.x, cell.y);
    for (int32_t i = 0; i < 8; ++i)
    {
        rebuildAt(cell.x + kAutotileNeighborOffsets[i][0],
                  cell.y + kAutotileNeighborOffsets[i][1]);
    }
}

void TilePaintManager::DoRotateClipboard90()
{
    if (!mClipboard.IsValid()) return;
    int32_t w = mClipboard.mWidth;
    int32_t h = mClipboard.mHeight;
    std::vector<TileCell> rotated(w * h);

    // 90° clockwise: new[col][h-1-row] = old[row][col]
    for (int32_t row = 0; row < h; ++row)
    {
        for (int32_t col = 0; col < w; ++col)
        {
            int32_t newRow = col;
            int32_t newCol = h - 1 - row;
            // After rotation the dimensions swap (h x w).
            rotated[newRow * h + newCol] = mClipboard.mCells[row * w + col];
            rotated[newRow * h + newCol].mFlags ^= 0x04;  // toggle rot90 flag
        }
    }

    mClipboard.mCells = std::move(rotated);
    std::swap(mClipboard.mWidth, mClipboard.mHeight);
}

#endif
