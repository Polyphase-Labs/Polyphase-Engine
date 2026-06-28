#if EDITOR

#include "BoneMaskEditor.h"
#include "EditorWidgets.h"

#include "World.h"
#include "Renderer.h"
#include "Engine.h"
#include "Log.h"
#include "EditorState.h"
#include "EditorIcons.h"
#include "AssetManager.h"
#include "Assets/BoneMaskAsset.h"
#include "Assets/SkeletalMesh.h"
#include "Nodes/3D/Camera3d.h"
#include "Nodes/3D/SkeletalMesh3d.h"
#include "Nodes/3D/DirectionalLight3d.h"

#include "imgui.h"

#if API_VULKAN
#include "Graphics/Vulkan/Image.h"
#include "Graphics/Vulkan/DestroyQueue.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "backends/imgui_impl_vulkan.h"
#endif

#include <algorithm>

static BoneMaskEditor sBoneMaskEditor;

BoneMaskEditor* GetBoneMaskEditor()
{
    return &sBoneMaskEditor;
}

void BoneMaskEditor::Open(BoneMaskAsset* mask)
{
    mEditedMask = mask;
    mEnabled = true;
    GetEditorState()->mShowBoneMaskEditor = true;

    mCachedMesh = nullptr;
    mChildren.clear();
    mRootBone = -1;

    SyncPreviewMesh();
}

void BoneMaskEditor::Close()
{
    if (mPreviewNode != nullptr)
    {
        mPreviewNode->StopAllAnimations(true);
    }
    mEnabled = false;
    GetEditorState()->mShowBoneMaskEditor = false;
}

void BoneMaskEditor::RebuildHierarchy(SkeletalMesh* mesh)
{
    if (mesh == nullptr)
    {
        mChildren.clear();
        mRootBone = -1;
        mCachedMesh = nullptr;
        return;
    }

    const std::vector<Bone>& bones = mesh->GetBones();
    mChildren.assign(bones.size(), std::vector<int32_t>());
    mRootBone = -1;
    for (int32_t i = 0; i < (int32_t)bones.size(); ++i)
    {
        int32_t parent = bones[i].mParentIndex;
        if (parent >= 0 && parent < (int32_t)bones.size())
        {
            mChildren[parent].push_back(i);
        }
        else if (mRootBone < 0)
        {
            mRootBone = i;
        }
    }
    mCachedMesh = mesh;
}

bool BoneMaskEditor::ContainsName(const std::vector<std::string>& list, const std::string& name) const
{
    for (const std::string& s : list)
    {
        if (s == name) return true;
    }
    return false;
}

void BoneMaskEditor::EnsurePreviewWorld()
{
    if (mPreviewWorld != nullptr)
        return;

    mPreviewWorld = new World();
    mPreviewWorld->SpawnDefaultRoot();

    mPreviewCamera = mPreviewWorld->SpawnNode<Camera3D>();
    if (mPreviewCamera != nullptr)
    {
        mPreviewCamera->SetName("BoneMaskCamera");
        mPreviewWorld->SetActiveCamera(mPreviewCamera);
    }

    mPreviewLight = mPreviewWorld->SpawnNode<DirectionalLight3D>();
    if (mPreviewLight != nullptr)
    {
        mPreviewLight->SetName("BoneMaskLight");
        mPreviewLight->SetRotation(glm::vec3(-45.0f, 35.0f, 0.0f));
    }

    mPreviewNode = mPreviewWorld->SpawnNode<SkeletalMesh3D>();
    if (mPreviewNode != nullptr)
    {
        mPreviewNode->SetName("BoneMaskMesh");
    }

    mPreviewWorld->SetAmbientLightColor(glm::vec4(0.35f, 0.35f, 0.4f, 1.0f));
}

void BoneMaskEditor::CreateRenderTargets(uint32_t w, uint32_t h)
{
#if API_VULKAN
    if (w == 0 || h == 0)
        return;

    if (mImGuiTexId != 0)
    {
        DeviceWaitIdle();
        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)mImGuiTexId);
        mImGuiTexId = 0;
    }
    DestroyRenderTargets();

    {
        ImageDesc desc;
        desc.mWidth = w;
        desc.mHeight = h;
        desc.mFormat = VK_FORMAT_B8G8R8A8_UNORM;
        desc.mUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        SamplerDesc sampDesc;
        sampDesc.mAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        mColorTarget = new Image(desc, sampDesc, "BoneMaskEditor Color");
        mColorTarget->Transition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    {
        ImageDesc desc;
        desc.mWidth = w;
        desc.mHeight = h;
        desc.mFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
        desc.mUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        mDepthTarget = new Image(desc, SamplerDesc(), "BoneMaskEditor Depth");
        mDepthTarget->Transition(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    mImGuiTexId = (ImTextureID)ImGui_ImplVulkan_AddTexture(
        mColorTarget->GetSampler(),
        mColorTarget->GetView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    mCurrentWidth = w;
    mCurrentHeight = h;

    DeviceWaitIdle();
#endif
}

void BoneMaskEditor::DestroyRenderTargets()
{
#if API_VULKAN
    if (mColorTarget != nullptr)
    {
        GetDestroyQueue()->Destroy(mColorTarget);
        mColorTarget = nullptr;
    }
    if (mDepthTarget != nullptr)
    {
        GetDestroyQueue()->Destroy(mDepthTarget);
        mDepthTarget = nullptr;
    }
#endif
    mCurrentWidth = 0;
    mCurrentHeight = 0;
}

void BoneMaskEditor::FrameMesh()
{
    BoneMaskAsset* mask = mEditedMask.Get<BoneMaskAsset>();
    SkeletalMesh* mesh = mask ? mask->GetTargetMesh() : nullptr;
    if (mesh == nullptr)
        return;

    Bounds b = mesh->GetBounds();
    float scale = glm::max(mPreviewScale, 0.0001f);
    mCamPivot = b.mCenter * scale;

    float radius = glm::max(b.mRadius, 0.1f) * scale;
    mCamDistance = glm::max(radius * 5.0f, 3.0f);

    mCamYaw = 0.6f;
    mCamPitch = 0.25f;
}

void BoneMaskEditor::ApplyOrbitCamera()
{
    if (mPreviewCamera == nullptr)
        return;

    float cosP = cosf(mCamPitch);
    glm::vec3 offset;
    offset.x = mCamDistance * cosP * sinf(mCamYaw);
    offset.y = mCamDistance * sinf(mCamPitch);
    offset.z = mCamDistance * cosP * cosf(mCamYaw);

    glm::vec3 camPos = mCamPivot + offset;
    mPreviewCamera->SetWorldPosition(camPos);
    mPreviewCamera->LookAt(mCamPivot, glm::vec3(0.0f, 1.0f, 0.0f));
}

void BoneMaskEditor::BuildGrid()
{
    if (mPreviewWorld == nullptr)
        return;

    // Cleared and rebuilt every frame (RemoveAllLines also clears the bone
    // overlay below — both are repopulated each Render).
    mPreviewWorld->RemoveAllLines();

    if (!mShowGrid)
        return;

    float target = glm::max(mCamDistance, 0.001f) * 0.1f;
    float spacing = powf(10.0f, floorf(log10f(target)));
    if (spacing <= 0.0f)
        spacing = 1.0f;

    const int32_t kHalfCount = 10;
    float extent = spacing * (float)kHalfCount;
    const float kY = 0.0f;

    glm::vec4 minorColor(0.30f, 0.30f, 0.30f, 1.0f);
    glm::vec4 majorColor(0.55f, 0.55f, 0.55f, 1.0f);
    glm::vec4 xAxisColor(0.85f, 0.25f, 0.25f, 1.0f);
    glm::vec4 zAxisColor(0.25f, 0.45f, 0.95f, 1.0f);

    for (int32_t i = -kHalfCount; i <= kHalfCount; ++i)
    {
        float pos = (float)i * spacing;

        glm::vec4 lineColor = (i % 5 == 0) ? majorColor : minorColor;

        glm::vec4 colZ = (i == 0) ? zAxisColor : lineColor;
        mPreviewWorld->AddLine(Line(glm::vec3(pos, kY, -extent),
            glm::vec3(pos, kY, extent),
            colZ, -1.0f));

        glm::vec4 colX = (i == 0) ? xAxisColor : lineColor;
        mPreviewWorld->AddLine(Line(glm::vec3(-extent, kY, pos),
            glm::vec3(extent, kY, pos),
            colX, -1.0f));
    }
}

void BoneMaskEditor::DrawBoneOverlay(const std::vector<uint8_t>& resolved)
{
    // Screen-space ImGui overlay drawn on top of the rendered image so bones
    // are visible through the mesh body. The engine's world-space debug lines
    // share the depth buffer and get fully occluded by the skinned geometry.
    if (mPreviewNode == nullptr || mPreviewCamera == nullptr)
        return;
    if (mImageMax.x <= mImageMin.x || mImageMax.y <= mImageMin.y)
        return;

    SkeletalMesh* mesh = mPreviewNode->GetSkeletalMesh();
    if (mesh == nullptr)
        return;

    const std::vector<Bone>& bones = mesh->GetBones();
    if (bones.empty())
        return;

    const bool hasResolved = (resolved.size() == bones.size());

    const glm::mat4 vp = mPreviewCamera->GetViewProjectionMatrix();
    const float imgW = mImageMax.x - mImageMin.x;
    const float imgH = mImageMax.y - mImageMin.y;

    // Pre-project all bones once into image-space pixels.
    struct ProjBone { ImVec2 p; bool valid; };
    std::vector<ProjBone> proj(bones.size());
    for (size_t i = 0; i < bones.size(); ++i)
    {
        glm::vec3 wp = mPreviewNode->GetBonePosition((int32_t)i);
        glm::vec4 clip = vp * glm::vec4(wp, 1.0f);
        if (clip.w <= 0.0f)
        {
            proj[i] = { ImVec2(0, 0), false };
            continue;
        }
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        float px = mImageMin.x + (ndc.x * 0.5f + 0.5f) * imgW;
        float py = mImageMin.y + (ndc.y * 0.5f + 0.5f) * imgH;
        proj[i] = { ImVec2(px, py), true };
    }

    const ImU32 kIncluded = IM_COL32(80, 240, 120, 255);
    const ImU32 kExcluded = IM_COL32(110, 110, 110, 180);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(mImageMin, mImageMax, true);

    for (int32_t i = 0; i < (int32_t)bones.size(); ++i)
    {
        int32_t parent = bones[i].mParentIndex;
        if (parent < 0 || parent >= (int32_t)bones.size())
            continue;
        if (!proj[i].valid || !proj[parent].valid)
            continue;

        const bool inc = hasResolved && resolved[i] != 0;
        ImU32 col = inc ? kIncluded : kExcluded;
        float thick = inc ? 2.5f : 1.5f;
        dl->AddLine(proj[parent].p, proj[i].p, col, thick);
        dl->AddCircleFilled(proj[i].p, inc ? 3.0f : 2.0f, col);
    }

    dl->PopClipRect();
}

void BoneMaskEditor::HandleViewportInput()
{
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    if (!hovered && !active)
        return;

    ImGuiIO& io = ImGui::GetIO();

    if (hovered && io.MouseWheel != 0.0f)
    {
        float zoomFactor = (io.MouseWheel > 0.0f) ? 0.9f : 1.1f;
        mCamDistance = glm::clamp(mCamDistance * zoomFactor, 0.001f, 100000.0f);
    }

    if (active)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            ImVec2 delta = io.MouseDelta;
            const float kOrbitSpeed = 0.01f;
            mCamYaw -= delta.x * kOrbitSpeed;
            mCamPitch += delta.y * kOrbitSpeed;
            const float kPitchLimit = 1.55f;
            mCamPitch = glm::clamp(mCamPitch, -kPitchLimit, kPitchLimit);
        }

        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
            ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            ImVec2 delta = io.MouseDelta;
            if (mPreviewCamera != nullptr)
            {
                glm::vec3 right = mPreviewCamera->GetRightVector();
                glm::vec3 up = mPreviewCamera->GetUpVector();
                float panScale = mCamDistance * 0.0025f;
                mCamPivot -= right * (delta.x * panScale);
                mCamPivot += up * (delta.y * panScale);
            }
        }
    }
}

const char* BoneMaskEditor::GetCurrentAnimName() const
{
    if (mPreviewNode == nullptr || mSelectedAnimIndex < 0)
        return nullptr;
    SkeletalMesh* mesh = mPreviewNode->GetSkeletalMesh();
    if (mesh == nullptr)
        return nullptr;
    const std::vector<Animation>& anims = mesh->GetAnimations();
    if (mSelectedAnimIndex >= (int32_t)anims.size())
        return nullptr;
    return anims[mSelectedAnimIndex].mName.c_str();
}

void BoneMaskEditor::SelectAnimation(int32_t index)
{
    if (mPreviewNode == nullptr)
        return;
    SkeletalMesh* mesh = mPreviewNode->GetSkeletalMesh();
    if (mesh == nullptr)
        return;
    const std::vector<Animation>& anims = mesh->GetAnimations();
    if (index < 0 || index >= (int32_t)anims.size())
        return;

    mPreviewNode->StopAllAnimations(true);
    mSelectedAnimIndex = index;
    mScrubTime = 0.0f;
    mPreviewNode->PlayAnimation(anims[index].mName.c_str(), mLoop, 1.0f, 1.0f, 0);
    mPreviewNode->SetAnimationSpeed(mPlaying ? 1.0f : 0.0f);
}

void BoneMaskEditor::SetPlaying(bool playing)
{
    mPlaying = playing;
    if (mPreviewNode != nullptr)
    {
        mPreviewNode->SetAnimationSpeed(mPlaying ? 1.0f : 0.0f);
    }
}

void BoneMaskEditor::SyncPreviewMesh()
{
    BoneMaskAsset* mask = mEditedMask.Get<BoneMaskAsset>();
    SkeletalMesh* mesh = mask ? mask->GetTargetMesh() : nullptr;

    EnsurePreviewWorld();
    if (mPreviewNode == nullptr)
        return;

    if (mesh == mPreviewMeshCached)
        return;

    mPreviewNode->StopAllAnimations(true);
    mPreviewNode->SetSkeletalMesh(mesh);
    mPreviewNode->SetScale(glm::vec3(mPreviewScale));
    mPreviewMeshCached = mesh;

    mSelectedAnimIndex = -1;
    mScrubTime = 0.0f;

    if (mesh != nullptr)
    {
        FrameMesh();
        const std::vector<Animation>& anims = mesh->GetAnimations();
        if (!anims.empty())
        {
            SelectAnimation(0);
        }
    }
}

void BoneMaskEditor::Render()
{
    if (!mEnabled || mPreviewWorld == nullptr)
        return;

    SkeletalMesh* mesh = mPreviewNode ? mPreviewNode->GetSkeletalMesh() : nullptr;
    if (mesh == nullptr)
        return;

    if (mColorTarget == nullptr || mDepthTarget == nullptr)
    {
        uint32_t w = mCurrentWidth ? mCurrentWidth : 512;
        uint32_t h = mCurrentHeight ? mCurrentHeight : 512;
        CreateRenderTargets(w, h);
        if (mColorTarget == nullptr || mDepthTarget == nullptr)
            return;
    }

    // Inject scrub time into the active anim before world update so
    // pose evaluation samples at the slider position when paused.
    if (!mPlaying)
    {
        const char* name = GetCurrentAnimName();
        if (name != nullptr)
        {
            ActiveAnimation* active = mPreviewNode->FindActiveAnimation(name);
            if (active != nullptr)
            {
                active->mTime = mScrubTime;
            }
        }
    }

    ApplyOrbitCamera();
    BuildGrid();

    float dt = GetEngineState()->mRealDeltaTime;
    mPreviewWorld->Update(dt);

    if (mPlaying)
    {
        const char* name = GetCurrentAnimName();
        if (name != nullptr)
        {
            ActiveAnimation* active = mPreviewNode->FindActiveAnimation(name);
            if (active != nullptr)
            {
                mScrubTime = active->mTime;
            }
        }
    }

    EditorState* es = GetEditorState();
    uint32_t prevW = es->mViewportWidth;
    uint32_t prevH = es->mViewportHeight;
    uint32_t prevX = es->mViewportX;
    uint32_t prevY = es->mViewportY;
    es->mViewportX = 0;
    es->mViewportY = 0;
    es->mViewportWidth = mCurrentWidth;
    es->mViewportHeight = mCurrentHeight;

    bool prevProxy = Renderer::Get()->IsProxyRenderingEnabled();
    Renderer::Get()->EnableProxyRendering(false);

    Renderer::Get()->RenderSecondScreen(mPreviewWorld, mColorTarget, mDepthTarget,
        mCurrentWidth, mCurrentHeight, mPreviewCamera,
        -1, false);

    Renderer::Get()->EnableProxyRendering(prevProxy);

    es->mViewportX = prevX;
    es->mViewportY = prevY;
    es->mViewportWidth = prevW;
    es->mViewportHeight = prevH;
}

void BoneMaskEditor::DrawBoneRow(int32_t boneIndex, const std::vector<uint8_t>& resolved)
{
    BoneMaskAsset* mask = mEditedMask.Get<BoneMaskAsset>();
    if (mask == nullptr || mCachedMesh == nullptr)
        return;

    const std::vector<Bone>& bones = mCachedMesh->GetBones();
    if (boneIndex < 0 || boneIndex >= (int32_t)bones.size())
        return;

    const std::string& name = bones[boneIndex].mName;
    const bool included      = (boneIndex < (int32_t)resolved.size() && resolved[boneIndex] != 0);
    const bool inIncludeList = ContainsName(mask->GetIncludeRoots(), name);
    const bool inExcludeList = ContainsName(mask->GetExcludeRoots(), name);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (mChildren[boneIndex].empty())
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    ImVec4 tint;
    const char* tag = " ";
    if (inIncludeList)      { tint = ImVec4(0.30f, 0.85f, 0.45f, 1.0f); tag = "+"; }
    else if (inExcludeList) { tint = ImVec4(0.90f, 0.35f, 0.35f, 1.0f); tag = "-"; }
    else if (included)      { tint = ImVec4(0.55f, 0.80f, 0.55f, 1.0f); tag = "i"; }
    else                    { tint = ImVec4(0.60f, 0.60f, 0.60f, 1.0f); tag = " "; }

    ImGui::PushStyleColor(ImGuiCol_Text, tint);
    bool open = ImGui::TreeNodeEx((void*)(intptr_t)boneIndex, flags, "[%s] %s", tag, name.c_str());
    ImGui::PopStyleColor();

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Include subtree", nullptr, false, !inIncludeList))
        {
            mask->RemoveExcludeRoot(name);
            mask->AddIncludeRoot(name);
            mask->SetDirtyFlag();
        }
        if (ImGui::MenuItem("Exclude subtree", nullptr, false, !inExcludeList))
        {
            mask->RemoveIncludeRoot(name);
            mask->AddExcludeRoot(name);
            mask->SetDirtyFlag();
        }
        if (ImGui::MenuItem("Clear", nullptr, false, inIncludeList || inExcludeList))
        {
            mask->RemoveIncludeRoot(name);
            mask->RemoveExcludeRoot(name);
            mask->SetDirtyFlag();
        }
        ImGui::EndPopup();
    }

    if (open && !mChildren[boneIndex].empty())
    {
        for (int32_t child : mChildren[boneIndex])
        {
            DrawBoneRow(child, resolved);
        }
        ImGui::TreePop();
    }
}

void BoneMaskEditor::DrawPanel()
{
    BoneMaskAsset* mask = mEditedMask.Get<BoneMaskAsset>();
    if (mask == nullptr)
    {
        ImGui::TextWrapped("No BoneMaskAsset selected. Double-click a .bmask asset to open it here.");
        return;
    }

    // Header
    ImGui::Text("Editing: %s%s",
                mask->GetName().c_str(),
                mask->GetDirtyFlag() ? " *" : "");
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(12.0f, 0.0f));
    ImGui::SameLine();
    {
        const bool dirty = mask->GetDirtyFlag();
        if (!dirty)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }
        if (ImGui::Button("Save") && dirty)
        {
            AssetManager::Get()->SaveAsset(mask->GetName());
            mask->ClearDirtyFlag();
        }
        if (!dirty)
        {
            ImGui::PopStyleVar();
        }
    }
    ImGui::Separator();

    // Mask config row
    {
        AssetRef meshRef = mask->GetTargetMesh();
        if (Polyphase::AssetRefPicker("Target Mesh", meshRef, SkeletalMesh::GetStaticType()))
        {
            mask->SetTargetMesh(meshRef.Get<SkeletalMesh>());
            mask->SetDirtyFlag();
            SyncPreviewMesh();
        }
    }
    {
        bool selfOnly = mask->GetSelfOnly();
        if (ImGui::Checkbox("Self Only (no descendants)", &selfOnly))
        {
            mask->SetSelfOnly(selfOnly);
            mask->SetDirtyFlag();
        }
    }

    SkeletalMesh* mesh = mask->GetTargetMesh();
    if (mesh == nullptr)
    {
        ImGui::Spacing();
        ImGui::TextWrapped("Assign a Target Mesh to author the bone tree.");
        return;
    }

    if (mesh != mCachedMesh)
    {
        RebuildHierarchy(mesh);
    }
    if (mesh != mPreviewMeshCached)
    {
        SyncPreviewMesh();
    }

    static std::vector<uint8_t> sResolvedCopy;
    sResolvedCopy = mask->Resolve(mesh);

    // Resolved-bone summary
    {
        size_t included = 0;
        for (uint8_t v : sResolvedCopy) included += (v != 0);
        ImGui::Text("Included: %zu / %zu bones", included, sResolvedCopy.size());
    }

    ImGui::Separator();

    // Split: left = bone tree, right = preview
    float avail = ImGui::GetContentRegionAvail().x;
    float leftW = glm::max(avail * 0.40f, 220.0f);

    ImGui::BeginChild("BoneTreePane", ImVec2(leftW, 0), true);
    ImGui::TextDisabled("Right-click a bone to include/exclude its subtree.");
    if (mRootBone >= 0)
    {
        DrawBoneRow(mRootBone, sResolvedCopy);
    }
    else
    {
        for (int32_t i = 0; i < (int32_t)mesh->GetBones().size(); ++i)
        {
            if (mesh->GetBone(i).mParentIndex < 0)
            {
                DrawBoneRow(i, sResolvedCopy);
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("PreviewPane", ImVec2(0, 0), true);

    // Animation transport
    const std::vector<Animation>& anims = mesh->GetAnimations();
    {
        const char* current = GetCurrentAnimName();
        const char* preview = current ? current : "(no animations)";
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::BeginCombo("Animation", preview))
        {
            for (int32_t i = 0; i < (int32_t)anims.size(); ++i)
            {
                bool selected = (i == mSelectedAnimIndex);
                if (ImGui::Selectable(anims[i].mName.c_str(), selected))
                {
                    if (i != mSelectedAnimIndex)
                        SelectAnimation(i);
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button(mPlaying ? "Pause" : "Play"))
        {
            SetPlaying(!mPlaying);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset View"))
        {
            FrameMesh();
        }
        ImGui::SameLine();
        Polyphase::Checkbox("Grid", &mShowGrid);
    }

    {
        float duration = (mSelectedAnimIndex >= 0 && mSelectedAnimIndex < (int32_t)anims.size())
            ? anims[mSelectedAnimIndex].mDuration : 0.0f;
        if (duration <= 0.0f) duration = 0.0001f;
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = ImGui::SliderFloat("##scrub", &mScrubTime, 0.0f, duration * 0.001f, "%.3f s");
        if (ImGui::IsItemActive() || changed)
        {
            SetPlaying(false);
            const char* name = GetCurrentAnimName();
            if (name != nullptr && mPreviewNode != nullptr)
            {
                ActiveAnimation* active = mPreviewNode->FindActiveAnimation(name);
                if (active != nullptr)
                    active->mTime = mScrubTime;
            }
        }
    }

    // 3D viewport image
    ImVec2 region = ImGui::GetContentRegionAvail();
    if (region.x < 64.0f) region.x = 64.0f;
    if (region.y < 64.0f) region.y = 64.0f;

    uint32_t desiredW = (uint32_t)region.x;
    uint32_t desiredH = (uint32_t)region.y;
    if (desiredW != mCurrentWidth || desiredH != mCurrentHeight)
    {
        CreateRenderTargets(desiredW, desiredH);
    }

    if (mImGuiTexId != 0 && mCurrentWidth > 0 && mCurrentHeight > 0)
    {
        ImVec2 imgSize((float)mCurrentWidth, (float)mCurrentHeight);
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##bonemaskview", imgSize,
            ImGuiButtonFlags_MouseButtonLeft |
            ImGuiButtonFlags_MouseButtonRight |
            ImGuiButtonFlags_MouseButtonMiddle);
        mImageMin = cursor;
        mImageMax = ImVec2(cursor.x + imgSize.x, cursor.y + imgSize.y);
        ImGui::GetWindowDrawList()->AddImage(mImGuiTexId, mImageMin, mImageMax);

        // Bone wireframe overlay on top of the rendered image.
        DrawBoneOverlay(sResolvedCopy);

        HandleViewportInput();
    }

    ImGui::EndChild();
}

#endif
