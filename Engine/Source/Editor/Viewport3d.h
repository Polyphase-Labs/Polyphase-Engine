#pragma once

#include "EditorState.h"
#include "Nodes/3D/InstancedMesh3d.h"

class Viewport3D
{
public:

    Viewport3D();
    ~Viewport3D();

    void Update(float deltaTime);
    bool ShouldHandleInput() const;
    bool IsMouseInside() const;

    float GetFocalDistance() const;
    void SetFocalDistance(float distance);
    void ToggleTransformMode();

    // Bookmark hotkey pass — checks View_SaveBookmark* / View_GotoBookmark*
    // and dispatches to EditorState. Runs from HandleDefaultControls so it
    // inherits the viewport-focus / text-field / popup gating; exposed so
    // tests / addons can also drive it deterministically.
    void HandleCameraBookmarkHotkeys();

protected:

    static constexpr float sDefaultFocalDistance = 10.0f;

    void HandleDefaultControls();
    void HandlePilotControls();
    void HandleTransformControls();
    void HandlePanControls();
    void HandleOrbitControls();

    glm::vec2 HandleLockedCursor();
    void HandleAxisLocking();

    glm::vec2 GetTransformDelta() const;
    void SavePreTransforms();
    void RestorePreTransforms();

    glm::vec3 GetLockedTranslationDelta(glm::vec3 deltaWS) const;
    glm::vec3 GetLockedRotationAxis() const;
    glm::vec3 GetLockedScaleDelta();
    bool ShouldTransformInstance() const;

    float mFirstPersonMoveSpeed = 10.0f;
    float mFirstPersonRotationSpeed = 0.07f;

    // Transform Control vars
    int32_t mPrevMouseX = 0;
    int32_t mPrevMouseY = 0;
    std::vector<glm::mat4> mPreTransforms;

    float mFocalDistance = sDefaultFocalDistance;
    bool mNeedsMouseRecenter = false;
    bool mTransformLocal = false;

    MeshInstanceData mInstancePreTransform;
};
