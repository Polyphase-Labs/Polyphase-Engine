#pragma once

#if EDITOR

#include "Maths.h"
#include <vector>
#include <set>
#include <cstdint>

class Voxel3D;
class Node;

enum class VoxelSculptMode : uint8_t
{
    Add = 0,
    Remove,
    Paint,
    Count
};

struct VoxelChange
{
    int32_t x, y, z;
    uint8_t oldValue;
    uint8_t newValue;
};

struct VoxelSculptOptions
{
    VoxelSculptMode mMode = VoxelSculptMode::Add;
    int32_t mBrushRadius = 1;
    uint8_t mMaterialId = 1;
};

class VoxelSculptManager
{
public:
    VoxelSculptManager();
    ~VoxelSculptManager();

    void Update();
    void HandleNodeDestroy(Node* node);

    VoxelSculptOptions mOptions;

    // Hover state (read by UI panel)
    bool mHoverValid = false;
    glm::ivec3 mHoverVoxel = {};
    glm::ivec3 mHoverPlaceVoxel = {};

private:
    void UpdateHover();
    void UpdateStroke();
    void CommitStroke();
    void DrawCursor();
    void ApplyBrush(Voxel3D* voxel, glm::ivec3 center);

    // Key helper to avoid duplicate changes in one stroke
    static uint64_t VoxelKey(int32_t x, int32_t y, int32_t z);

    // Stroke state
    bool mStrokeActive = false;
    Voxel3D* mPendingTarget = nullptr;
    std::vector<VoxelChange> mPendingChanges;
    std::set<uint64_t> mModifiedSet;
};

#endif
