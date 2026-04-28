#if EDITOR

#include "AnimationBrowser.h"

#include "World.h"
#include "Renderer.h"
#include "Engine.h"
#include "Log.h"
#include "EditorState.h"
#include "EditorIcons.h"
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

static AnimationBrowser sAnimationBrowser;

AnimationBrowser* GetAnimationBrowser()
{
    return &sAnimationBrowser;
}

void AnimationBrowser::Open(SkeletalMesh* mesh)
{
    if (mesh == nullptr)
        return;

    EnsurePreviewWorld();

    if (mPreviewNode == nullptr)
        return;

    // Stop any prior playback before swapping the mesh
    mPreviewNode->StopAllAnimations(true);
    mPreviewNode->SetSkeletalMesh(mesh);
    mPreviewNode->SetScale(glm::vec3(mPreviewScale));

    mTargetMesh = mesh;
    mScrubTime = 0.0f;
    mPlaying = true;

    FrameMesh();

    const std::vector<Animation>& anims = mesh->GetAnimations();
    if (!anims.empty())
    {
        SelectAnimation(0);
    }
    else
    {
        mSelectedAnimIndex = -1;
    }

    mEnabled = true;
    mPendingFocus = true;
    mPendingDock = true;
    GetEditorState()->mShowAnimationBrowser = true;
}

void AnimationBrowser::Close()
{
    if (mPreviewNode != nullptr)
    {
        mPreviewNode->StopAllAnimations(true);
    }
    mEnabled = false;
}

void AnimationBrowser::EnsurePreviewWorld()
{
    if (mPreviewWorld != nullptr)
        return;

    mPreviewWorld = new World();
    mPreviewWorld->SpawnDefaultRoot();

    mPreviewCamera = mPreviewWorld->SpawnNode<Camera3D>();
    if (mPreviewCamera != nullptr)
    {
        mPreviewCamera->SetName("AnimBrowserCamera");
        mPreviewWorld->SetActiveCamera(mPreviewCamera);
    }

    mPreviewLight = mPreviewWorld->SpawnNode<DirectionalLight3D>();
    if (mPreviewLight != nullptr)
    {
        mPreviewLight->SetName("AnimBrowserLight");
        mPreviewLight->SetRotation(glm::vec3(-45.0f, 35.0f, 0.0f));
    }

    mPreviewNode = mPreviewWorld->SpawnNode<SkeletalMesh3D>();
    if (mPreviewNode != nullptr)
    {
        mPreviewNode->SetName("AnimBrowserMesh");
    }

    mPreviewWorld->SetAmbientLightColor(glm::vec4(0.35f, 0.35f, 0.4f, 1.0f));
}

void AnimationBrowser::CreateRenderTargets(uint32_t w, uint32_t h)
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

        mColorTarget = new Image(desc, sampDesc, "AnimationBrowser Color");
        mColorTarget->Transition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    {
        ImageDesc desc;
        desc.mWidth = w;
        desc.mHeight = h;
        desc.mFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
        desc.mUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        mDepthTarget = new Image(desc, SamplerDesc(), "AnimationBrowser Depth");
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

void AnimationBrowser::DestroyRenderTargets()
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

void AnimationBrowser::FrameMesh()
{
    SkeletalMesh* mesh = mTargetMesh.Get<SkeletalMesh>();
    if (mesh == nullptr)
        return;

    Bounds b = mesh->GetBounds();
    float scale = glm::max(mPreviewScale, 0.0001f);
    mCamPivot = b.mCenter * scale;

    float radius = glm::max(b.mRadius, 0.1f) * scale;
    // Pull the camera back generously; better too far than clipping into the mesh.
    // Floor at 3 units so tiny / zero-radius meshes still produce a usable view.
    mCamDistance = glm::max(radius * 5.0f, 3.0f);

    mCamYaw = 0.6f;
    mCamPitch = 0.25f;

#if LOGGING_ENABLED
    LogDebug("AnimationBrowser::FrameMesh: mesh='%s' bounds(c=%.3f,%.3f,%.3f r=%.3f) "
        "scale=%.4f -> pivot(%.3f,%.3f,%.3f) dist=%.3f",
        mesh->GetName().c_str(),
        b.mCenter.x, b.mCenter.y, b.mCenter.z, b.mRadius,
        mPreviewScale,
        mCamPivot.x, mCamPivot.y, mCamPivot.z,
        mCamDistance);
#endif
}

void AnimationBrowser::BuildGrid()
{
    if (mPreviewWorld == nullptr)
        return;

    // Rebuilt every frame so the spacing can adapt to the camera distance.
    // Lifetime -1 means "permanent until removed", but we clear first so
    // stale lines from a previous spacing are not left behind.
    mPreviewWorld->RemoveAllLines();

    if (!mShowGrid)
        return;

    // Pick a "nice" power-of-ten spacing roughly tied to camera distance,
    // so the grid stays usable across very small and very large meshes.
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

        // Line parallel to Z axis at x = pos
        glm::vec4 colZ = (i == 0) ? zAxisColor : lineColor;
        mPreviewWorld->AddLine(Line(glm::vec3(pos, kY, -extent),
            glm::vec3(pos, kY, extent),
            colZ, -1.0f));

        // Line parallel to X axis at z = pos
        glm::vec4 colX = (i == 0) ? xAxisColor : lineColor;
        mPreviewWorld->AddLine(Line(glm::vec3(-extent, kY, pos),
            glm::vec3(extent, kY, pos),
            colX, -1.0f));
    }
}

void AnimationBrowser::ApplyOrbitCamera()
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

void AnimationBrowser::HandleViewportInput()
{
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    if (!hovered && !active)
        return;

    ImGuiIO& io = ImGui::GetIO();

    // Wheel zoom (only when hovered)
    if (hovered && io.MouseWheel != 0.0f)
    {
        float zoomFactor = (io.MouseWheel > 0.0f) ? 0.9f : 1.1f;
        mCamDistance = glm::clamp(mCamDistance * zoomFactor, 0.001f, 100000.0f);
    }

    // Drags only count while the InvisibleButton is active so the gesture
    // keeps tracking even when the cursor leaves the viewport bounds.
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

const char* AnimationBrowser::GetCurrentAnimName() const
{
    SkeletalMesh* mesh = mTargetMesh.Get<SkeletalMesh>();
    if (mesh == nullptr || mSelectedAnimIndex < 0)
        return nullptr;
    const std::vector<Animation>& anims = mesh->GetAnimations();
    if (mSelectedAnimIndex >= (int32_t)anims.size())
        return nullptr;
    return anims[mSelectedAnimIndex].mName.c_str();
}

float AnimationBrowser::GetCurrentDuration() const
{
    SkeletalMesh* mesh = mTargetMesh.Get<SkeletalMesh>();
    if (mesh == nullptr || mSelectedAnimIndex < 0)
        return 0.0f;
    const std::vector<Animation>& anims = mesh->GetAnimations();
    if (mSelectedAnimIndex >= (int32_t)anims.size())
        return 0.0f;
    return anims[mSelectedAnimIndex].mDuration;
}

float AnimationBrowser::GetFrameStep() const
{
    SkeletalMesh* mesh = mTargetMesh.Get<SkeletalMesh>();
    if (mesh == nullptr || mSelectedAnimIndex < 0)
        return 1.0f / 30.0f;
    const std::vector<Animation>& anims = mesh->GetAnimations();
    if (mSelectedAnimIndex >= (int32_t)anims.size())
        return 1.0f / 30.0f;
    float tps = anims[mSelectedAnimIndex].mTicksPerSecond;
    if (tps <= 0.0f)
        return 1.0f / 30.0f;
    return 1.0f / tps;
}

void AnimationBrowser::SelectAnimation(int32_t index)
{
    if (mPreviewNode == nullptr)
        return;

    SkeletalMesh* mesh = mTargetMesh.Get<SkeletalMesh>();
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

void AnimationBrowser::SetPlaying(bool playing)
{
    mPlaying = playing;
    if (mPreviewNode != nullptr)
    {
        // Use animation speed (not the pause flag) so that scrub-time changes
        // still drive bone evaluation. SkeletalMesh3D::Tick skips the entire
        // pose-evaluation block when mAnimationPaused is true, which would
        // freeze the visible mesh and make the scrubber look unresponsive.
        // Speed=0 stops time advancement but lets each Tick re-pose to the
        // current mTime, which is what we set when the user drags the slider.
        mPreviewNode->SetAnimationSpeed(mPlaying ? 1.0f : 0.0f);
    }
}

void AnimationBrowser::SyncLoopFlag()
{
    // Loop is set when PlayAnimation is called, so re-issue Play to apply
    // a new loop flag without losing the current scrub time.
    if (mPreviewNode == nullptr)
        return;
    const char* name = GetCurrentAnimName();
    if (name == nullptr)
        return;

    float savedTime = mScrubTime;
    mPreviewNode->StopAllAnimations(true);
    mPreviewNode->PlayAnimation(name, mLoop, 1.0f, 1.0f, 0);
    mPreviewNode->SetAnimationSpeed(mPlaying ? 1.0f : 0.0f);

    ActiveAnimation* active = mPreviewNode->FindActiveAnimation(name);
    if (active != nullptr)
    {
        active->mTime = savedTime;
    }
    mScrubTime = savedTime;
}

void AnimationBrowser::StepFrame(int32_t direction)
{
    SetPlaying(false);
    float duration = GetCurrentDuration();
    if (duration <= 0.0f)
        return;
    float step = GetFrameStep();
    mScrubTime = glm::clamp(mScrubTime + step * (float)direction, 0.0f, duration);

    const char* name = GetCurrentAnimName();
    if (name != nullptr && mPreviewNode != nullptr)
    {
        ActiveAnimation* active = mPreviewNode->FindActiveAnimation(name);
        if (active != nullptr)
        {
            active->mTime = mScrubTime;
        }
    }
}

void AnimationBrowser::Render()
{
    if (!mEnabled || mPreviewWorld == nullptr || mTargetMesh.Get() == nullptr)
        return;

    if (mColorTarget == nullptr || mDepthTarget == nullptr)
    {
        uint32_t w = mCurrentWidth ? mCurrentWidth : 512;
        uint32_t h = mCurrentHeight ? mCurrentHeight : 512;
        CreateRenderTargets(w, h);
        if (mColorTarget == nullptr || mDepthTarget == nullptr)
            return;
    }

    // Push scrub time into the active animation BEFORE the world tick.
    // While paused the tick still re-evaluates bones at the current mTime.
    if (!mPlaying)
    {
        const char* name = GetCurrentAnimName();
        if (name != nullptr && mPreviewNode != nullptr)
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

    // Read playback time back out so the slider tracks playback.
    if (mPlaying)
    {
        const char* name = GetCurrentAnimName();
        if (name != nullptr && mPreviewNode != nullptr)
        {
            ActiveAnimation* active = mPreviewNode->FindActiveAnimation(name);
            if (active != nullptr)
            {
                mScrubTime = active->mTime;
            }
        }
    }

    // Override the editor viewport dimensions so Camera3D::ComputeMatrices()
    // picks up the render-target aspect ratio (same trick as GamePreview).
    EditorState* es = GetEditorState();
    uint32_t prevW = es->mViewportWidth;
    uint32_t prevH = es->mViewportHeight;
    uint32_t prevX = es->mViewportX;
    uint32_t prevY = es->mViewportY;
    es->mViewportX = 0;
    es->mViewportY = 0;
    es->mViewportWidth = mCurrentWidth;
    es->mViewportHeight = mCurrentHeight;

    // Suppress camera / light / debug gizmos in the preview render so the
    // user only sees the skeletal mesh, not the helper nodes we spawned.
    bool prevProxy = Renderer::Get()->IsProxyRenderingEnabled();
    Renderer::Get()->EnableProxyRendering(false);

    // Pass drawAccumulatedDebugDraws=false: mDebugDraws / Gizmos draws were
    // populated during the main editor render and refer to nodes in the main
    // world. Drawing them in this preview viewport leaks unrelated camera /
    // light gizmos into the AnimationBrowser image.
    Renderer::Get()->RenderSecondScreen(mPreviewWorld, mColorTarget, mDepthTarget,
        mCurrentWidth, mCurrentHeight, mPreviewCamera,
        -1, false);

    Renderer::Get()->EnableProxyRendering(prevProxy);

    es->mViewportX = prevX;
    es->mViewportY = prevY;
    es->mViewportWidth = prevW;
    es->mViewportHeight = prevH;
}

void AnimationBrowser::DrawPanel()
{
    if (mPendingFocus)
    {
        ImGui::SetWindowFocus();
        mPendingFocus = false;
    }

    SkeletalMesh* mesh = mTargetMesh.Get<SkeletalMesh>();
    if (!mEnabled || mesh == nullptr)
    {
        ImGui::TextDisabled("Right-click a SkeletalMesh asset and choose 'View Animations' to begin.");
        return;
    }

    // Header: mesh name + close
    ImGui::Text("Mesh: %s", mesh->GetName().c_str());
    ImGui::SameLine();
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > 60.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - 60.0f));
    if (ImGui::SmallButton(ICON_DASHICONS_NO_ALT))
    {
        Close();
        return;
    }

    const std::vector<Animation>& anims = mesh->GetAnimations();

    // Animation dropdown
    {
        const char* current = GetCurrentAnimName();
        const char* preview = current ? current : "(no animations)";
        ImGui::SetNextItemWidth(220.0f);
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
    }

    // Lua hint
    if (mSelectedAnimIndex >= 0)
    {
        ImGui::TextDisabled("Lua: mesh:PlayAnimation(\"%s\", 0, %s)",
            anims[mSelectedAnimIndex].mName.c_str(),
            mLoop ? "true" : "false");
    }
    else
    {
        ImGui::TextDisabled("This mesh has no animations.");
    }

    // Scale + Reset View
    {
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::SliderFloat(ICON_MINGCUTE_SCALE_LINE " Scale", &mPreviewScale, 0.001f, 100.0f, "%.4f", ImGuiSliderFlags_Logarithmic))
        {
            if (mPreviewNode != nullptr)
                mPreviewNode->SetScale(glm::vec3(mPreviewScale));
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_MATERIAL_SYMBOLS_RESET_ISO " Reset View"))
        {
            FrameMesh();
        }
    }

    // Transport toolbar
    {
        if (ImGui::Button(ICON_RI_REWIND_START_FILL))
        {
            mScrubTime = 0.0f;
            const char* name = GetCurrentAnimName();
            if (name != nullptr && mPreviewNode != nullptr)
            {
                ActiveAnimation* active = mPreviewNode->FindActiveAnimation(name);
                if (active != nullptr)
                    active->mTime = 0.0f;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_MATERIAL_SYMBOLS_ARROW_LEFT))
        {
            StepFrame(-1);
        }
        ImGui::SameLine();
        if (ImGui::Button(mPlaying ? ICON_MATERIAL_SYMBOLS_PAUSE : ICON_MDI_PLAY))
        {
            SetPlaying(!mPlaying);
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_MATERIAL_SYMBOLS_ARROW_RIGHT))
        {
            StepFrame(1);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox(ICON_MATERIAL_SYMBOLS_DEVICE_RESET, &mLoop))
        {
            SyncLoopFlag();
        }
        ImGui::SameLine();
        ImGui::Checkbox(ICON_FLUENT_SPACE_3D_32_REGULAR, &mShowGrid);
    }

    // Scrubber
    {
        float duration = GetCurrentDuration();
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

    ImGui::Separator();

    // 3D viewport
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
        ImGui::InvisibleButton("##animview", imgSize,
            ImGuiButtonFlags_MouseButtonLeft |
            ImGuiButtonFlags_MouseButtonRight |
            ImGuiButtonFlags_MouseButtonMiddle);
        mImageMin = cursor;
        mImageMax = ImVec2(cursor.x + imgSize.x, cursor.y + imgSize.y);
        ImGui::GetWindowDrawList()->AddImage(mImGuiTexId, mImageMin, mImageMax);
        HandleViewportInput();
    }
}

#endif
