#if EDITOR

#include "VoxelSculpt/VoxelSculptManager.h"
#include "EditorState.h"
#include "ActionManager.h"
#include "Viewport3d.h"
#include "Renderer.h"
#include "World.h"
#include "InputDevices.h"
#include "AssetManager.h"
#include "Log.h"

#include "Nodes/3D/Camera3d.h"
#include "Nodes/3D/Voxel3d.h"
#include "Assets/StaticMesh.h"

VoxelSculptManager::VoxelSculptManager()
{
}

VoxelSculptManager::~VoxelSculptManager()
{
}

uint64_t VoxelSculptManager::VoxelKey(int32_t x, int32_t y, int32_t z)
{
    return (uint64_t(uint32_t(x)) << 40) | (uint64_t(uint32_t(y)) << 20) | uint64_t(uint32_t(z));
}

void VoxelSculptManager::Update()
{
    Node* selected = GetEditorState()->GetSelectedNode();
    Voxel3D* voxel = nullptr;

    if (selected != nullptr && selected->GetType() == Voxel3D::GetStaticType())
    {
        voxel = static_cast<Voxel3D*>(selected);
    }

    if (voxel == nullptr)
    {
        mHoverValid = false;
        if (mStrokeActive)
        {
            CommitStroke();
        }
        return;
    }

    UpdateHover();
    DrawCursor();
    UpdateStroke();
}

void VoxelSculptManager::HandleNodeDestroy(Node* node)
{
    if (mPendingTarget == static_cast<Voxel3D*>(node))
    {
        mPendingChanges.clear();
        mModifiedSet.clear();
        mStrokeActive = false;
        mPendingTarget = nullptr;
    }
}

void VoxelSculptManager::UpdateHover()
{
    mHoverValid = false;

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != Voxel3D::GetStaticType())
        return;

    Voxel3D* voxel = static_cast<Voxel3D*>(selected);

    if (!GetEditorState()->GetViewport3D()->ShouldHandleInput())
        return;

    Camera3D* camera = GetWorld(0)->GetActiveCamera();
    if (camera == nullptr)
        return;

    int32_t mouseX, mouseY;
    INP_GetMousePosition(mouseX, mouseY);

    glm::vec3 rayOrigin = camera->GetWorldPosition();
    glm::vec3 rayTarget = camera->ScreenToWorldPosition(mouseX, mouseY);
    glm::vec3 rayDir = glm::normalize(rayTarget - rayOrigin);

    VoxelRayResult rayResult = voxel->RayTest(rayOrigin, rayDir, 200.0f);
    if (rayResult.mHit)
    {
        mHoverValid = true;
        mHoverVoxel = rayResult.mVoxel;
        mHoverPlaceVoxel = rayResult.mPrevVoxel;
    }
}

void VoxelSculptManager::DrawCursor()
{
    if (!mHoverValid)
        return;

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != Voxel3D::GetStaticType())
        return;

    Voxel3D* voxel = static_cast<Voxel3D*>(selected);
    glm::vec3 nodeScale = voxel->GetWorldScale();

    // Choose which voxel to highlight based on mode
    glm::ivec3 target = (mOptions.mMode == VoxelSculptMode::Add) ? mHoverPlaceVoxel : mHoverVoxel;

    // Color based on mode
    glm::vec4 color;
    switch (mOptions.mMode)
    {
        case VoxelSculptMode::Add:    color = glm::vec4(0.2f, 0.9f, 0.2f, 0.5f); break;
        case VoxelSculptMode::Remove: color = glm::vec4(0.9f, 0.2f, 0.2f, 0.5f); break;
        case VoxelSculptMode::Paint:  color = glm::vec4(0.2f, 0.4f, 0.9f, 0.5f); break;
        default: color = glm::vec4(1.0f); break;
    }

    glm::vec3 worldPos = voxel->GetVoxelWorldPosition(target.x, target.y, target.z);

    float brushScale = static_cast<float>(mOptions.mBrushRadius);
    glm::vec3 scale = nodeScale * brushScale;

    DebugDraw cursorDraw;
    cursorDraw.mMesh = LoadAsset<StaticMesh>("SM_Cube");
    cursorDraw.mTransform = glm::translate(glm::mat4(1.0f), worldPos) *
                            glm::scale(glm::mat4(1.0f), scale);
    cursorDraw.mColor = color;
    cursorDraw.mLife = 0.0f;
    Renderer::Get()->AddDebugDraw(cursorDraw);
}

void VoxelSculptManager::UpdateStroke()
{
    if (!GetEditorState()->GetViewport3D()->ShouldHandleInput())
        return;

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != Voxel3D::GetStaticType())
        return;

    Voxel3D* voxel = static_cast<Voxel3D*>(selected);

    if (IsMouseButtonJustDown(MOUSE_LEFT) && mHoverValid)
    {
        mStrokeActive = true;
        mPendingTarget = voxel;
        mPendingChanges.clear();
        mModifiedSet.clear();

        glm::ivec3 center = (mOptions.mMode == VoxelSculptMode::Add) ? mHoverPlaceVoxel : mHoverVoxel;
        ApplyBrush(voxel, center);
    }
    else if (IsMouseButtonDown(MOUSE_LEFT) && mStrokeActive && mHoverValid && mPendingTarget == voxel)
    {
        glm::ivec3 center = (mOptions.mMode == VoxelSculptMode::Add) ? mHoverPlaceVoxel : mHoverVoxel;
        ApplyBrush(voxel, center);
    }

    if (IsMouseButtonJustUp(MOUSE_LEFT) && mStrokeActive)
    {
        CommitStroke();
    }
}

void VoxelSculptManager::ApplyBrush(Voxel3D* voxel, glm::ivec3 center)
{
    glm::ivec3 dims = voxel->GetDimensions();
    int32_t r = mOptions.mBrushRadius;

    for (int32_t bx = -r + 1; bx <= r - 1; ++bx)
    {
        for (int32_t by = -r + 1; by <= r - 1; ++by)
        {
            for (int32_t bz = -r + 1; bz <= r - 1; ++bz)
            {
                float dist = glm::sqrt(float(bx*bx + by*by + bz*bz));
                if (dist >= float(r)) continue;

                int32_t vx = center.x + bx;
                int32_t vy = center.y + by;
                int32_t vz = center.z + bz;

                if (vx < 0 || vx >= dims.x || vy < 0 || vy >= dims.y || vz < 0 || vz >= dims.z)
                    continue;

                uint64_t key = VoxelKey(vx, vy, vz);
                if (mModifiedSet.count(key) > 0)
                    continue;

                uint8_t oldVal = voxel->GetVoxel(vx, vy, vz);
                uint8_t newVal = oldVal;

                switch (mOptions.mMode)
                {
                    case VoxelSculptMode::Add:
                        if (oldVal == 0) newVal = mOptions.mMaterialId;
                        break;
                    case VoxelSculptMode::Remove:
                        if (oldVal > 0) newVal = 0;
                        break;
                    case VoxelSculptMode::Paint:
                        if (oldVal > 0) newVal = mOptions.mMaterialId;
                        break;
                    default:
                        break;
                }

                if (newVal != oldVal)
                {
                    voxel->SetVoxel(vx, vy, vz, newVal);
                    mModifiedSet.insert(key);

                    VoxelChange change;
                    change.x = vx;
                    change.y = vy;
                    change.z = vz;
                    change.oldValue = oldVal;
                    change.newValue = newVal;
                    mPendingChanges.push_back(change);
                }
            }
        }
    }
}

void VoxelSculptManager::CommitStroke()
{
    if (!mPendingChanges.empty() && mPendingTarget != nullptr)
    {
        ActionManager::Get()->EXE_SetVoxels(mPendingTarget, mPendingChanges);
    }

    mPendingChanges.clear();
    mModifiedSet.clear();
    mStrokeActive = false;
    mPendingTarget = nullptr;
}

#endif
