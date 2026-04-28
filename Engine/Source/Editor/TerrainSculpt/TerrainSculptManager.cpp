#if EDITOR

#include "TerrainSculpt/TerrainSculptManager.h"
#include "EditorState.h"
#include "ActionManager.h"
#include "Viewport3d.h"
#include "Renderer.h"
#include "World.h"
#include "InputDevices.h"
#include "Log.h"

#include "Nodes/3D/Camera3d.h"
#include "Nodes/3D/Terrain3d.h"
#include "Assets/StaticMesh.h"
#include "Assets/Texture.h"

#include <algorithm>
#include <cmath>

TerrainSculptManager::TerrainSculptManager()
{
}

TerrainSculptManager::~TerrainSculptManager()
{
}

uint64_t TerrainSculptManager::GridKey(int32_t x, int32_t z)
{
    return (uint64_t(uint32_t(x)) << 32) | uint64_t(uint32_t(z));
}

void TerrainSculptManager::Update()
{
    Node* selected = GetEditorState()->GetSelectedNode();
    Terrain3D* terrain = nullptr;

    if (selected != nullptr && selected->GetType() == Terrain3D::GetStaticType())
    {
        terrain = static_cast<Terrain3D*>(selected);
    }

    if (terrain == nullptr)
    {
        mHoverValid = false;
        if (mStrokeActive) CommitStroke();
        return;
    }

    UpdateHover();
    DrawCursor();
    UpdateStroke();
}

void TerrainSculptManager::HandleNodeDestroy(Node* node)
{
    if (mPendingTarget == node)
    {
        mPendingChanges.clear();
        mModifiedSet.clear();
        mStrokeActive = false;
        mPendingTarget = nullptr;
        mHoverValid = false;
    }
}

void TerrainSculptManager::UpdateHover()
{
    mHoverValid = false;

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != Terrain3D::GetStaticType())
        return;

    Terrain3D* terrain = static_cast<Terrain3D*>(selected);

    if (!GetEditorState()->GetViewport3D()->ShouldHandleInput())
        return;

    Camera3D* camera = GetWorld(0)->GetActiveCamera();
    if (camera == nullptr) return;

    int32_t mouseX, mouseY;
    INP_GetMousePosition(mouseX, mouseY);

    // Raycast against the world physics to hit the terrain collision shape
    RayTestResult rayResult;
    camera->TraceScreenToWorld(mouseX, mouseY, ColGroupAll, rayResult);

    if (rayResult.mHitNode == terrain)
    {
        mHoverValid = true;
        mHoverWorldPos = rayResult.mHitPosition;

        // Convert world hit position to grid coordinates
        glm::vec3 localPos = rayResult.mHitPosition - terrain->GetWorldPosition();
        float stepX = terrain->GetWorldWidth() / (terrain->GetResolutionX() - 1);
        float stepZ = terrain->GetWorldDepth() / (terrain->GetResolutionZ() - 1);
        mHoverGridX = (int32_t)std::round((localPos.x + terrain->GetWorldWidth() * 0.5f) / stepX);
        mHoverGridZ = (int32_t)std::round((localPos.z + terrain->GetWorldDepth() * 0.5f) / stepZ);
        mHoverGridX = std::max(0, std::min(mHoverGridX, terrain->GetResolutionX() - 1));
        mHoverGridZ = std::max(0, std::min(mHoverGridZ, terrain->GetResolutionZ() - 1));
    }
}

void TerrainSculptManager::DrawCursor()
{
    if (!mHoverValid) return;

    glm::vec4 color;
    switch (mOptions.mMode)
    {
        case TerrainSculptMode::Raise:         color = glm::vec4(0.2f, 0.9f, 0.2f, 0.4f); break;
        case TerrainSculptMode::Lower:         color = glm::vec4(0.9f, 0.2f, 0.2f, 0.4f); break;
        case TerrainSculptMode::Flatten:       color = glm::vec4(0.9f, 0.9f, 0.2f, 0.4f); break;
        case TerrainSculptMode::Smooth:        color = glm::vec4(0.2f, 0.4f, 0.9f, 0.4f); break;
        case TerrainSculptMode::PaintMaterial: color = glm::vec4(0.9f, 0.4f, 0.9f, 0.4f); break;
        default: color = glm::vec4(1.0f); break;
    }

    float radius = mOptions.mBrushRadius;

    DebugDraw cursorDraw;
    cursorDraw.mMesh = LoadAsset<StaticMesh>("SM_Sphere");
    cursorDraw.mTransform = glm::translate(glm::mat4(1.0f), mHoverWorldPos) *
                            glm::scale(glm::mat4(1.0f), glm::vec3(radius));
    cursorDraw.mColor = color;
    cursorDraw.mLife = 0.0f;
    Renderer::Get()->AddDebugDraw(cursorDraw);
}

float TerrainSculptManager::ComputeFalloff(float distance, float radius) const
{
    if (radius <= 0.0f) return 0.0f;
    float t = distance / radius;
    if (t >= 1.0f) return 0.0f;
    // Smooth falloff: lerp between hard edge (constant 1) and smooth (cosine)
    float hard = 1.0f;
    float smooth = 0.5f * (1.0f + std::cos(t * 3.14159265f));
    return hard + (smooth - hard) * mOptions.mBrushFalloff;
}

float TerrainSculptManager::SampleBrushMask(float dx, float dz, float radius) const
{
    Texture* mask = mOptions.mBrushMask.Get<Texture>();
    if (mask == nullptr || mask->GetPixels().empty())
        return ComputeFalloff(std::sqrt(dx * dx + dz * dz), radius);

    // Map offset to mask UV [0,1]
    float u = (dx / radius + 1.0f) * 0.5f;
    float v = (dz / radius + 1.0f) * 0.5f;
    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));

    uint32_t texW = mask->GetWidth();
    uint32_t texH = mask->GetHeight();
    if (texW == 0 || texH == 0) return 0.0f;

    uint32_t px = std::min((uint32_t)(u * (texW - 1)), texW - 1);
    uint32_t py = std::min((uint32_t)(v * (texH - 1)), texH - 1);
    uint32_t idx = (py * texW + px) * 4; // RGBA8

    if (idx < mask->GetPixels().size())
        return mask->GetPixels()[idx] / 255.0f; // R channel

    return 0.0f;
}

void TerrainSculptManager::UpdateStroke()
{
    if (!GetEditorState()->GetViewport3D()->ShouldHandleInput())
        return;

    Node* selected = GetEditorState()->GetSelectedNode();
    if (selected == nullptr || selected->GetType() != Terrain3D::GetStaticType())
        return;

    Terrain3D* terrain = static_cast<Terrain3D*>(selected);

    if (IsMouseButtonJustDown(MOUSE_LEFT) && mHoverValid)
    {
        mStrokeActive = true;
        mPendingTarget = terrain;
        mPendingChanges.clear();
        mModifiedSet.clear();
        ApplyBrush(terrain, mHoverWorldPos);
    }
    else if (IsMouseButtonDown(MOUSE_LEFT) && mStrokeActive && mHoverValid && mPendingTarget == terrain)
    {
        ApplyBrush(terrain, mHoverWorldPos);
    }

    if (IsMouseButtonJustUp(MOUSE_LEFT) && mStrokeActive)
    {
        CommitStroke();
    }
}

void TerrainSculptManager::ApplyBrush(Terrain3D* terrain, glm::vec3 worldCenter)
{
    int32_t resX = terrain->GetResolutionX();
    int32_t resZ = terrain->GetResolutionZ();
    float worldW = terrain->GetWorldWidth();
    float worldD = terrain->GetWorldDepth();
    float stepX = worldW / (resX - 1);
    float stepZ = worldD / (resZ - 1);
    float halfW = worldW * 0.5f;
    float halfD = worldD * 0.5f;

    glm::vec3 terrainPos = terrain->GetWorldPosition();
    glm::vec3 localCenter = worldCenter - terrainPos;

    float radius = mOptions.mBrushRadius;
    float strength = mOptions.mBrushStrength;

    // Determine grid range affected by the brush
    int32_t minGX = (int32_t)std::floor((localCenter.x - radius + halfW) / stepX);
    int32_t maxGX = (int32_t)std::ceil((localCenter.x + radius + halfW) / stepX);
    int32_t minGZ = (int32_t)std::floor((localCenter.z - radius + halfD) / stepZ);
    int32_t maxGZ = (int32_t)std::ceil((localCenter.z + radius + halfD) / stepZ);

    minGX = std::max(0, minGX);
    maxGX = std::min(resX - 1, maxGX);
    minGZ = std::max(0, minGZ);
    maxGZ = std::min(resZ - 1, maxGZ);

    float dt = 18.0f / 60.0f; // Brush speed multiplier (6x previous rate)

    for (int32_t gz = minGZ; gz <= maxGZ; ++gz)
    {
        for (int32_t gx = minGX; gx <= maxGX; ++gx)
        {
            // World position of this grid vertex
            float vx = gx * stepX - halfW;
            float vz = gz * stepZ - halfD;
            float dx = vx - localCenter.x;
            float dz = vz - localCenter.z;
            float dist = std::sqrt(dx * dx + dz * dz);

            if (dist > radius) continue;

            float maskVal = SampleBrushMask(dx, dz, radius);
            if (maskVal <= 0.0f) continue;

            float effect = strength * maskVal * dt;

            float oldHeight = terrain->GetHeight(gx, gz);
            float newHeight = oldHeight;

            switch (mOptions.mMode)
            {
                case TerrainSculptMode::Raise:
                    newHeight = oldHeight + effect;
                    break;

                case TerrainSculptMode::Lower:
                    newHeight = oldHeight - effect;
                    break;

                case TerrainSculptMode::Flatten:
                    newHeight = oldHeight + (mOptions.mFlattenHeight - oldHeight) * effect;
                    break;

                case TerrainSculptMode::Smooth:
                {
                    // Average neighbor heights
                    float sum = 0.0f;
                    int count = 0;
                    for (int32_t nz = gz - 1; nz <= gz + 1; ++nz)
                    {
                        for (int32_t nx = gx - 1; nx <= gx + 1; ++nx)
                        {
                            if (nx >= 0 && nx < resX && nz >= 0 && nz < resZ && !(nx == gx && nz == gz))
                            {
                                sum += terrain->GetHeight(nx, nz);
                                count++;
                            }
                        }
                    }
                    if (count > 0)
                    {
                        float avg = sum / count;
                        newHeight = oldHeight + (avg - oldHeight) * effect;
                    }
                    break;
                }

                case TerrainSculptMode::PaintMaterial:
                {
                    // For paint mode, we modify the splatmap instead of height
                    // Record both old and new splat values for undo
                    // Increase weight for the active slot, reduce others proportionally
                    // Use SetMaterialWeight which handles per-channel packing
                    int32_t slot = mOptions.mPaintSlot;
                    float curWeight = terrain->GetMaterialWeight(gx, gz, slot);
                    float addWeight = effect;
                    float newWeight = std::min(1.0f, curWeight + addWeight);

                    // Get old splatmap value for undo
                    uint32_t oldSplat = terrain->mSplatmap[gz * resX + gx];

                    terrain->SetMaterialWeight(gx, gz, slot, newWeight);

                    // Normalize: reduce other slots proportionally
                    float totalOther = 0.0f;
                    for (int32_t s = 0; s < Terrain3D::MAX_MATERIAL_SLOTS; ++s)
                    {
                        if (s != slot) totalOther += terrain->GetMaterialWeight(gx, gz, s);
                    }
                    if (totalOther > 0.0f)
                    {
                        float scale = (1.0f - newWeight) / totalOther;
                        for (int32_t s = 0; s < Terrain3D::MAX_MATERIAL_SLOTS; ++s)
                        {
                            if (s != slot)
                                terrain->SetMaterialWeight(gx, gz, s, terrain->GetMaterialWeight(gx, gz, s) * scale);
                        }
                    }

                    uint32_t newSplat = terrain->mSplatmap[gz * resX + gx];

                    // Record change for undo if not already modified in this stroke
                    uint64_t key = GridKey(gx, gz);
                    if (mModifiedSet.count(key) == 0)
                    {
                        mModifiedSet.insert(key);
                        TerrainHeightChange change;
                        change.x = gx;
                        change.z = gz;
                        change.oldHeight = oldHeight;
                        change.newHeight = oldHeight; // height unchanged for paint mode
                        change.oldSplat = oldSplat;
                        change.newSplat = newSplat;
                        mPendingChanges.push_back(change);
                    }
                    else
                    {
                        // Update existing change entry's newSplat
                        for (auto& c : mPendingChanges)
                        {
                            if (c.x == gx && c.z == gz)
                            {
                                c.newSplat = newSplat;
                                break;
                            }
                        }
                    }

                    continue; // Skip the height-based recording below
                }

                default:
                    break;
            }

            // Record height change for undo (height modes only)
            if (mOptions.mMode != TerrainSculptMode::PaintMaterial && newHeight != oldHeight)
            {
                terrain->SetHeight(gx, gz, newHeight);

                uint64_t key = GridKey(gx, gz);
                if (mModifiedSet.count(key) == 0)
                {
                    mModifiedSet.insert(key);
                    uint32_t splatVal = terrain->mSplatmap[gz * resX + gx];

                    TerrainHeightChange change;
                    change.x = gx;
                    change.z = gz;
                    change.oldHeight = oldHeight;
                    change.newHeight = newHeight;
                    change.oldSplat = splatVal;
                    change.newSplat = splatVal; // splat unchanged for height modes
                    mPendingChanges.push_back(change);
                }
                else
                {
                    // Update the existing change's newHeight (continuous stroke)
                    for (auto& c : mPendingChanges)
                    {
                        if (c.x == gx && c.z == gz)
                        {
                            c.newHeight = newHeight;
                            break;
                        }
                    }
                }
            }
        }
    }
}

void TerrainSculptManager::CommitStroke()
{
    if (!mPendingChanges.empty() && mPendingTarget != nullptr)
    {
        ActionManager::Get()->EXE_SetTerrainHeights(mPendingTarget, mPendingChanges);

        // Rebake and auto-save splatmap after material painting
        if (mOptions.mMode == TerrainSculptMode::PaintMaterial &&
            mPendingTarget->mBakeSplatmap && mPendingTarget->mEnableAtlasTexturing)
        {
            mPendingTarget->BakeAndSaveMap();
        }
    }

    mPendingChanges.clear();
    mModifiedSet.clear();
    mStrokeActive = false;
    mPendingTarget = nullptr;
}

#endif
