#pragma once

#if EDITOR

#include <cstdint>
#include <vector>
#include <string>

#include "AssetRef.h"
#include "Maths.h"
#include "imgui.h"

class BoneMaskAsset;
class SkeletalMesh;
class SkeletalMesh3D;
class Camera3D;
class DirectionalLight3D;
class World;
class Image;

// Hierarchical bone-tree picker for authoring BoneMaskAssets, with a live
// 3D preview viewport that mirrors AnimationBrowser's setup. The included
// bones are tinted green in the preview by drawing parent->child segments
// via World::AddLine, so the author can verify the mask visually while
// playing back a real animation on the target rig.
class BoneMaskEditor
{
public:

    void Open(BoneMaskAsset* mask);
    void Close();
    void DrawPanel();
    void Render();

    bool IsEnabled() const { return mEnabled; }

private:

    void RebuildHierarchy(SkeletalMesh* mesh);
    void DrawBoneRow(int32_t boneIndex, const std::vector<uint8_t>& resolved);
    bool ContainsName(const std::vector<std::string>& list, const std::string& name) const;

    void EnsurePreviewWorld();
    void CreateRenderTargets(uint32_t w, uint32_t h);
    void DestroyRenderTargets();
    void FrameMesh();
    void ApplyOrbitCamera();
    void BuildGrid();
    void DrawBoneOverlay(const std::vector<uint8_t>& resolved);
    void HandleViewportInput();
    void SelectAnimation(int32_t index);
    void SetPlaying(bool playing);
    void SyncPreviewMesh();

    const char* GetCurrentAnimName() const;

    bool mEnabled = false;
    BoneMaskRef mEditedMask;

    // Cached bone hierarchy from the target mesh; rebuilt when the mesh
    // pointer differs from mCachedMesh.
    const SkeletalMesh* mCachedMesh = nullptr;
    std::vector<std::vector<int32_t>> mChildren;
    int32_t mRootBone = -1;

    // Preview world / nodes (mirror AnimationBrowser).
    World* mPreviewWorld = nullptr;
    Camera3D* mPreviewCamera = nullptr;
    DirectionalLight3D* mPreviewLight = nullptr;
    SkeletalMesh3D* mPreviewNode = nullptr;
    const SkeletalMesh* mPreviewMeshCached = nullptr;

    // Offscreen render targets.
    Image* mColorTarget = nullptr;
    Image* mDepthTarget = nullptr;
    ImTextureID mImGuiTexId = 0;
    uint32_t mCurrentWidth = 0;
    uint32_t mCurrentHeight = 0;

    // Orbit camera state.
    float mCamYaw = 0.6f;
    float mCamPitch = 0.25f;
    float mCamDistance = 5.0f;
    glm::vec3 mCamPivot = { 0.0f, 1.0f, 0.0f };
    float mPreviewScale = 1.0f;
    bool mShowGrid = true;

    // Playback state.
    int32_t mSelectedAnimIndex = -1;
    bool mPlaying = true;
    bool mLoop = true;
    float mScrubTime = 0.0f;

    // Viewport rect captured during DrawPanel for hover/input gating.
    ImVec2 mImageMin = { 0, 0 };
    ImVec2 mImageMax = { 0, 0 };
};

BoneMaskEditor* GetBoneMaskEditor();

#endif
