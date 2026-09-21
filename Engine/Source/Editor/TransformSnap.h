#pragma once

#if EDITOR

#include "Maths.h"

#include <vector>

class Camera3D;
class Node;

enum class SnapMode : uint8_t
{
    Increment,
    Vertex,
    Edge,
    Face,

    Count
};

// Cursor-locked (G/R/S) transforms are incremental, so snapping works on the
// raw value accumulated since the drag began and applies the difference.
struct TransformSnapAccum
{
    glm::vec3 mRawTranslate = {};
    glm::vec3 mAppliedTranslate = {};
    glm::vec2 mRawTranslate2D = {};
    glm::vec2 mRawSize2D = {};
    float mRawAngle = 0.0f;
    float mAppliedAngle = 0.0f;
    float mRawScale = 0.0f;
    float mAppliedScale = 0.0f;
    bool mWasActive = false;

    void Reset() { *this = TransformSnapAccum(); }
};

namespace TransformSnap
{
    // Snapping preference XOR Shift held.
    bool IsActive();
    SnapMode GetMode();
    void ToggleEnabled();
    void CycleMode();

    // Speed multiplier for cursor-locked transforms (normal vs Control held).
    float GetModalSpeedMultiplier();
    // Mouse scale for ImGuizmo handle drags. 1.0 unless Control is held.
    float GetGizmoPrecisionScale();

    float GetTranslateIncrement();
    float GetRotateIncrement();     // degrees
    float GetScaleIncrement();
    float GetWidgetIncrement();     // pixels

    // Rounds to the nearest multiple. increment <= 0 passes the value through.
    float SnapValue(float value, float increment);
    glm::vec2 SnapValue(glm::vec2 value, float increment);
    glm::vec3 SnapValue(glm::vec3 value, float increment);

    void BeginDrag();
    void EndDrag();

    // Vertex / Edge / Face: finds the snap target on the scene geometry behind
    // rawPos (as seen from camera). Nodes in 'excluded' and their descendants
    // are ignored. Returns false when nothing qualifies; keep rawPos then.
    bool SnapToSurface(Camera3D* camera, const std::vector<Node*>& excluded, glm::vec3 rawPos, glm::vec3& outPos);

    void DrawIndicator();
}

#endif
