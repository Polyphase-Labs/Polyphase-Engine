#pragma once

#if EDITOR

#include <cstdint>
#include "AssetRef.h"
#include "Maths.h"
#include "imgui.h"

class World;
class Image;
class Camera3D;
class DirectionalLight3D;
class SkeletalMesh3D;
class SkeletalMesh;

class AnimationBrowser
{
public:
    void Open(SkeletalMesh* mesh);
    void Close();
    void Render();
    void DrawPanel();

    bool IsEnabled() const { return mEnabled; }

    // Returns true exactly once after Open() has been called, so the caller
    // (DrawDockspace) can re-dock the AnimationBrowser tab next to the CLI
    // Terminal before its BeginDock runs. Without this, the dock floats away
    // because it was force-hidden on the editor's first frame.
    bool ConsumePendingDock()
    {
        bool v = mPendingDock;
        mPendingDock = false;
        return v;
    }

private:
    void EnsurePreviewWorld();
    void CreateRenderTargets(uint32_t w, uint32_t h);
    void DestroyRenderTargets();
    void FrameMesh();
    void ApplyOrbitCamera();
    void BuildGrid();
    void HandleViewportInput();
    void SelectAnimation(int32_t index);
    void StepFrame(int32_t direction);
    void SetPlaying(bool playing);
    void SyncLoopFlag();
    float GetCurrentDuration() const;
    float GetFrameStep() const;
    const char* GetCurrentAnimName() const;

    bool mEnabled = false;
    SkeletalMeshRef mTargetMesh;

    World* mPreviewWorld = nullptr;
    Camera3D* mPreviewCamera = nullptr;
    DirectionalLight3D* mPreviewLight = nullptr;
    SkeletalMesh3D* mPreviewNode = nullptr;

    int32_t mSelectedAnimIndex = -1;
    bool mLoop = true;
    bool mPlaying = true;
    bool mShowGrid = true;
    float mScrubTime = 0.0f;
    float mPreviewScale = 1.0f;

    // Offscreen render targets
    Image* mColorTarget = nullptr;
    Image* mDepthTarget = nullptr;
    ImTextureID mImGuiTexId = 0;
    uint32_t mCurrentWidth = 0;
    uint32_t mCurrentHeight = 0;

    // Orbit camera state
    float mCamYaw = 0.6f;
    float mCamPitch = 0.25f;
    float mCamDistance = 5.0f;
    glm::vec3 mCamPivot = { 0.0f, 1.0f, 0.0f };

    // Viewport rect captured by DrawPanel for hover/input gating
    ImVec2 mImageMin = { 0, 0 };
    ImVec2 mImageMax = { 0, 0 };
    bool mPendingFocus = false;
    bool mPendingDock = false;
};

AnimationBrowser* GetAnimationBrowser();

#endif
