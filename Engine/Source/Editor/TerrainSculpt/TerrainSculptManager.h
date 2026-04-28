#pragma once

#if EDITOR

#include "Maths.h"
#include "AssetRef.h"
#include <vector>
#include <set>
#include <cstdint>

class Terrain3D;
class Node;

enum class TerrainSculptMode : uint8_t
{
    Raise = 0,
    Lower,
    Flatten,
    Smooth,
    PaintMaterial,
    Count
};

struct TerrainHeightChange
{
    int32_t x, z;
    float oldHeight;
    float newHeight;
    uint32_t oldSplat;
    uint32_t newSplat;
};

struct TerrainSculptOptions
{
    TerrainSculptMode mMode = TerrainSculptMode::Raise;
    float mBrushRadius = 5.0f;       // world-space radius
    float mBrushStrength = 0.5f;     // 0-1
    float mBrushFalloff = 0.5f;      // 0=hard edge, 1=soft
    float mFlattenHeight = 0.0f;     // target for flatten mode
    int32_t mPaintSlot = 0;          // material slot for paint mode

    // Brush mask: grayscale texture for custom brush shape/hardness
    // White=full effect, black=no effect. nullptr = circular falloff
    TextureRef mBrushMask;
};

class TerrainSculptManager
{
public:
    TerrainSculptManager();
    ~TerrainSculptManager();

    void Update();
    void HandleNodeDestroy(Node* node);

    TerrainSculptOptions mOptions;

    // Hover state (read by UI panel)
    bool mHoverValid = false;
    glm::vec3 mHoverWorldPos = {};
    int32_t mHoverGridX = 0;
    int32_t mHoverGridZ = 0;

private:
    void UpdateHover();
    void UpdateStroke();
    void CommitStroke();
    void DrawCursor();
    void ApplyBrush(Terrain3D* terrain, glm::vec3 worldCenter);
    float ComputeFalloff(float distance, float radius) const;
    float SampleBrushMask(float dx, float dz, float radius) const;

    static uint64_t GridKey(int32_t x, int32_t z);

    // Stroke state
    bool mStrokeActive = false;
    Terrain3D* mPendingTarget = nullptr;
    std::vector<TerrainHeightChange> mPendingChanges;
    std::set<uint64_t> mModifiedSet;
};

#endif
