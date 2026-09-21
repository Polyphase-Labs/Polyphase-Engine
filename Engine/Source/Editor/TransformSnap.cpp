#if EDITOR

#include "TransformSnap.h"

#include "TriangleBvh.h"
#include "EditorState.h"
#include "Preferences/Appearance/Viewport/ViewportModule.h"

#include "Engine.h"
#include "World.h"
#include "Log.h"
#include "InputDevices.h"
#include "Nodes/3D/Camera3d.h"
#include "Nodes/3D/Primitive3d.h"
#include "Nodes/3D/Skybox3D.h"
#include "Nodes/3D/ShadowMesh3d.h"

#include "imgui.h"

#include <algorithm>

static TriangleBvh sBvh;
static bool sBvhBuilt = false;

static glm::vec3 sIndicatorPos = {};
static SnapMode sIndicatorMode = SnapMode::Face;
static int32_t sIndicatorTtl = 0;

static void BuildSnapBvh(const std::vector<Node*>& excluded)
{
    std::vector<BvhTri> tris;

    World* world = GetWorld(0);
    Node* root = world ? world->GetRootNode() : nullptr;

    if (root != nullptr)
    {
        auto gather = [&](Node* node) -> bool
        {
            if (!node->IsVisible())
                return false;

            if (std::find(excluded.begin(), excluded.end(), node) != excluded.end())
                return false;

            if (node->IsPrimitive3D() &&
                node->As<Skybox3D>() == nullptr &&
                node->As<ShadowMesh3D>() == nullptr)
            {
                GatherPrimitiveTriangles((Primitive3D*)node, tris);
            }

            return true;
        };

        root->Traverse(gather);
    }

    LogDebug("Transform snap: %u target triangles", (uint32_t)tris.size());

    sBvh.Build(std::move(tris));
    sBvhBuilt = true;
}

static glm::vec3 ClosestPointOnSegment(const glm::vec3& a, const glm::vec3& b, const glm::vec3& p)
{
    glm::vec3 ab = b - a;
    float lenSq = glm::dot(ab, ab);
    if (lenSq <= 1e-12f)
        return a;

    float t = glm::clamp(glm::dot(p - a, ab) / lenSq, 0.0f, 1.0f);
    return a + t * ab;
}

bool TransformSnap::IsActive()
{
    ViewportModule* prefs = ViewportModule::Get();
    bool enabled = prefs ? prefs->GetSnapEnabled() : false;
    return enabled != IsShiftDown();
}

SnapMode TransformSnap::GetMode()
{
    ViewportModule* prefs = ViewportModule::Get();
    return prefs ? (SnapMode)prefs->GetSnapMode() : SnapMode::Increment;
}

void TransformSnap::ToggleEnabled()
{
    if (ViewportModule* prefs = ViewportModule::Get())
    {
        prefs->SetSnapEnabled(!prefs->GetSnapEnabled());
        LogDebug("Transform snapping %s", prefs->GetSnapEnabled() ? "on" : "off");
    }
}

void TransformSnap::CycleMode()
{
    if (ViewportModule* prefs = ViewportModule::Get())
    {
        prefs->SetSnapMode((prefs->GetSnapMode() + 1) % (int)SnapMode::Count);

        static const char* kModeNames[] = { "Increment", "Vertex", "Edge", "Face" };
        LogDebug("Transform snap mode: %s", kModeNames[prefs->GetSnapMode()]);
    }
}

float TransformSnap::GetModalSpeedMultiplier()
{
    ViewportModule* prefs = ViewportModule::Get();
    if (prefs == nullptr)
        return IsControlDown() ? 0.1f : 1.0f;

    return IsControlDown() ? prefs->GetPrecisionCtrl() : prefs->GetPrecisionNormal();
}

float TransformSnap::GetGizmoPrecisionScale()
{
    if (!IsControlDown())
        return 1.0f;

    ViewportModule* prefs = ViewportModule::Get();
    return prefs ? prefs->GetPrecisionCtrl() : 0.1f;
}

float TransformSnap::GetTranslateIncrement()
{
    ViewportModule* prefs = ViewportModule::Get();
    return prefs ? prefs->GetSnapTranslate() : 1.0f;
}

float TransformSnap::GetRotateIncrement()
{
    ViewportModule* prefs = ViewportModule::Get();
    return prefs ? prefs->GetSnapRotate() : 15.0f;
}

float TransformSnap::GetScaleIncrement()
{
    ViewportModule* prefs = ViewportModule::Get();
    return prefs ? prefs->GetSnapScale() : 0.1f;
}

float TransformSnap::GetWidgetIncrement()
{
    ViewportModule* prefs = ViewportModule::Get();
    return prefs ? prefs->GetSnapWidgetPixels() : 8.0f;
}

float TransformSnap::SnapValue(float value, float increment)
{
    if (increment <= 0.0f)
        return value;

    return roundf(value / increment) * increment;
}

glm::vec2 TransformSnap::SnapValue(glm::vec2 value, float increment)
{
    return glm::vec2(SnapValue(value.x, increment), SnapValue(value.y, increment));
}

glm::vec3 TransformSnap::SnapValue(glm::vec3 value, float increment)
{
    return glm::vec3(SnapValue(value.x, increment), SnapValue(value.y, increment), SnapValue(value.z, increment));
}

void TransformSnap::BeginDrag()
{
    sBvh.Clear();
    sBvhBuilt = false;
    sIndicatorTtl = 0;
}

void TransformSnap::EndDrag()
{
    sBvh.Clear();
    sBvhBuilt = false;
    sIndicatorTtl = 0;
}

bool TransformSnap::SnapToSurface(Camera3D* camera, const std::vector<Node*>& excluded, glm::vec3 rawPos, glm::vec3& outPos)
{
    SnapMode mode = GetMode();
    if (camera == nullptr || mode == SnapMode::Increment)
        return false;

    // Built on the first frame of a drag that actually needs it. The scene is
    // static for the drag apart from the excluded selection.
    if (!sBvhBuilt)
    {
        BuildSnapBvh(excluded);
    }

    if (sBvh.IsEmpty())
        return false;

    // Ray from the camera through the unsnapped pivot. Works for cursor-locked
    // transforms (no visible cursor) and for gizmo handles alike.
    const glm::vec3 fwd = camera->GetForwardVector();
    const glm::vec3 camPos = camera->GetWorldPosition();

    glm::vec3 origin;
    glm::vec3 dir;
    float tMax = camera->GetFarZ();

    if (camera->GetProjectionMode() == ProjectionMode::ORTHOGRAPHIC)
    {
        origin = rawPos - fwd * glm::dot(rawPos - camPos, fwd);
        dir = fwd;
    }
    else
    {
        glm::vec3 toRaw = rawPos - camPos;
        if (glm::dot(toRaw, fwd) <= camera->GetNearZ())
            return false;

        origin = camPos;
        dir = glm::normalize(toRaw);
        tMax = camera->GetFarZ() / glm::max(glm::dot(dir, fwd), 1e-3f);
    }

    float hitT = 0.0f;
    uint32_t hitTri = 0;
    if (!sBvh.ClosestHit(origin, dir, tMax, hitT, hitTri))
        return false;

    const glm::vec3 hit = origin + dir * hitT;
    glm::vec3 result = hit;

    if (mode != SnapMode::Face)
    {
        const BvhTri& tri = sBvh.GetTri(hitTri);
        const glm::vec3 v0 = tri.mV0;
        const glm::vec3 v1 = tri.mV0 + tri.mE1;
        const glm::vec3 v2 = tri.mV0 + tri.mE2;

        glm::vec3 candidates[3];
        if (mode == SnapMode::Vertex)
        {
            candidates[0] = v0;
            candidates[1] = v1;
            candidates[2] = v2;
        }
        else
        {
            candidates[0] = ClosestPointOnSegment(v0, v1, hit);
            candidates[1] = ClosestPointOnSegment(v1, v2, hit);
            candidates[2] = ClosestPointOnSegment(v2, v0, hit);
        }

        // WorldToScreenPosition works in device pixels; the threshold is in UI pixels.
        float interfaceScale = GetEngineConfig()->mEditorInterfaceScale;
        if (interfaceScale == 0.0f)
        {
            interfaceScale = 1.0f;
        }

        ViewportModule* prefs = ViewportModule::Get();
        const float thresholdPx = (prefs ? prefs->GetSnapPixelThreshold() : 16.0f) * interfaceScale;
        const glm::vec3 rawScreen = camera->WorldToScreenPosition(rawPos);

        float bestDist = thresholdPx;
        bool found = false;
        for (int32_t i = 0; i < 3; ++i)
        {
            glm::vec3 screen = camera->WorldToScreenPosition(candidates[i]);
            if (screen.z <= 0.0f)
                continue;

            float dist = glm::length(glm::vec2(screen) - glm::vec2(rawScreen));
            if (dist <= bestDist)
            {
                bestDist = dist;
                result = candidates[i];
                found = true;
            }
        }

        if (!found)
            return false;
    }

    outPos = result;
    sIndicatorPos = result;
    sIndicatorMode = mode;
    sIndicatorTtl = 2;
    return true;
}

void TransformSnap::DrawIndicator()
{
    if (sIndicatorTtl <= 0)
        return;

    --sIndicatorTtl;

    EditorState* edState = GetEditorState();
    Camera3D* camera = edState ? edState->GetEditorCamera() : nullptr;
    if (camera == nullptr)
        return;

    glm::vec3 screen = camera->WorldToScreenPosition(sIndicatorPos);
    if (screen.z <= 0.0f)
        return;

    float interfaceScale = GetEngineConfig()->mEditorInterfaceScale;
    if (interfaceScale == 0.0f)
    {
        interfaceScale = 1.0f;
    }
    const float invScale = 1.0f / interfaceScale;

    const ImVec2 center(screen.x * invScale, screen.y * invScale);
    const ImVec2 clipMin(edState->mViewportX * invScale, edState->mViewportY * invScale);
    const ImVec2 clipMax(clipMin.x + edState->mViewportWidth * invScale, clipMin.y + edState->mViewportHeight * invScale);

    const float kRadius = 5.0f;
    const ImU32 kOutline = IM_COL32(0, 0, 0, 255);
    const ImU32 kColor = IM_COL32(255, 170, 40, 255);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    drawList->PushClipRect(clipMin, clipMax, true);

    if (sIndicatorMode == SnapMode::Face)
    {
        drawList->AddCircle(center, kRadius, kOutline, 16, 3.0f);
        drawList->AddCircle(center, kRadius, kColor, 16, 1.5f);
    }
    else
    {
        const ImVec2 boxMin(center.x - kRadius, center.y - kRadius);
        const ImVec2 boxMax(center.x + kRadius, center.y + kRadius);
        drawList->AddRect(boxMin, boxMax, kOutline, 0.0f, 0, 3.0f);
        drawList->AddRect(boxMin, boxMax, kColor, 0.0f, 0, 1.5f);
    }

    drawList->PopClipRect();
}

#endif
