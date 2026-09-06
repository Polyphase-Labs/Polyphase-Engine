#if EDITOR

#include "ProfilingWindow.h"
#include "EditorWidgets.h"
#include "Engine.h"
#include "Clock.h"
#include "Profiler.h"
#include "AssetManager.h"
#include "Assets/Texture.h"
#include "Assets/StaticMesh.h"
#include "Assets/SkeletalMesh.h"
#include "Assets/SoundWave.h"
#include "System/System.h"
#include "Packaging/PackagingSettings.h"
#include "GamePreview/GamePreview.h"
#include "MemorySnapshot/MemorySnapshotProfiler.h"
#include "MemorySnapshot/MemorySnapshotWindow.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

static ProfilingWindow sProfilingWindow;

ProfilingWindow* GetProfilingWindow()
{
    return &sProfilingWindow;
}

void ProfilingWindow::Draw()
{
    // This is called if you need a standalone window, but we use docked panels
    DrawContent();
}

void ProfilingWindow::DrawContent()
{
    // Memory Snapshot shortcuts -- the snapshot tool and this window share the
    // same game-scoped sizing, so the numbers match.
    if (ImGui::Button("Capture Snapshot"))
    {
        GetMemorySnapshotWindow()->Open();
        GetMemorySnapshotWindow()->CaptureNow();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Capture a full Memory Snapshot of the running game and open the viewer.");
    ImGui::SameLine();
    if (ImGui::Button("Open Memory Snapshot Window"))
    {
        GetMemorySnapshotWindow()->Open();
    }

    ImGui::Separator();

    // Toggle buttons row
    Polyphase::Checkbox("FPS", &mShowFPS);
    ImGui::SameLine();
    Polyphase::Checkbox("CPU", &mShowCpuStats);
    ImGui::SameLine();
    Polyphase::Checkbox("GPU", &mShowGpuStats);
    ImGui::SameLine();
    Polyphase::Checkbox("Memory", &mShowMemory);
    ImGui::SameLine();
    Polyphase::Checkbox("Graph", &mShowFrameGraph);

    ImGui::Separator();

    // Scrollable content area
    ImGui::BeginChild("ProfilingContent", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    if (mShowFPS)
    {
        DrawFPSSection();
    }

    if (mShowCpuStats)
    {
        DrawCpuStatsSection();
    }

    if (mShowGpuStats)
    {
        DrawGpuStatsSection();
    }

    if (mShowMemory)
    {
        DrawMemorySection();
    }

    if (mShowFrameGraph)
    {
        DrawFrameGraph();
    }

    // Custom addon stats
    if (!mCustomStats.empty())
    {
        DrawCustomStatsSection();
    }

    // Custom addon sections
    DrawCustomSections();

    ImGui::EndChild();
}

void ProfilingWindow::Tick()
{
    const Clock* clock = GetAppClock();
    if (clock == nullptr)
        return;

    float deltaTime = clock->DeltaTime();
    float frameTimeMs = deltaTime * 1000.0f;

    // Add to history
    mFrameTimeHistory.push_back(frameTimeMs);
    while (mFrameTimeHistory.size() > kFrameHistorySize)
    {
        mFrameTimeHistory.pop_front();
    }

    // Calculate smoothed values (exponential moving average)
    const float smoothFactor = 0.1f;
    mSmoothedFrameTime = mSmoothedFrameTime * (1.0f - smoothFactor) + frameTimeMs * smoothFactor;
    mSmoothedFPS = (mSmoothedFrameTime > 0.0f) ? (1000.0f / mSmoothedFrameTime) : 0.0f;

    // Calculate min/max/avg from history
    if (!mFrameTimeHistory.empty())
    {
        float minVal = mFrameTimeHistory[0];
        float maxVal = mFrameTimeHistory[0];
        float sum = 0.0f;

        for (float val : mFrameTimeHistory)
        {
            minVal = std::min(minVal, val);
            maxVal = std::max(maxVal, val);
            sum += val;
        }

        mMinFrameTime = minVal;
        mMaxFrameTime = maxVal;
        mAvgFrameTime = sum / (float)mFrameTimeHistory.size();
    }
}

void ProfilingWindow::DrawFPSSection()
{
    if (ImGui::CollapsingHeader("FPS / Frame Time", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Color coding based on FPS
        ImVec4 fpsColor;
        if (mSmoothedFPS >= 55.0f)
            fpsColor = ImVec4(0.2f, 0.9f, 0.2f, 1.0f); // Green
        else if (mSmoothedFPS >= 30.0f)
            fpsColor = ImVec4(0.9f, 0.9f, 0.2f, 1.0f); // Yellow
        else
            fpsColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); // Red

        ImGui::TextColored(fpsColor, "FPS: %.1f", mSmoothedFPS);
        ImGui::SameLine(150.0f);
        ImGui::Text("Frame: %.2f ms", mSmoothedFrameTime);

        ImGui::Text("Min: %.2f ms", mMinFrameTime);
        ImGui::SameLine(150.0f);
        ImGui::Text("Max: %.2f ms", mMaxFrameTime);
        ImGui::SameLine(300.0f);
        ImGui::Text("Avg: %.2f ms", mAvgFrameTime);

        ImGui::Spacing();
    }
}

void ProfilingWindow::DrawStatBar(const char* name, float value, float maxValue, const ImVec4& color)
{
    float fraction = (maxValue > 0.0f) ? (value / maxValue) : 0.0f;
    fraction = std::min(fraction, 1.0f);

    ImGui::Text("%-12s", name);
    ImGui::SameLine(100.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    char overlay[32];
    snprintf(overlay, sizeof(overlay), "%.2f ms", value);
    ImGui::ProgressBar(fraction, ImVec2(150.0f, 0), overlay);
    ImGui::PopStyleColor();
}

void ProfilingWindow::DrawCpuStatsSection()
{
    if (ImGui::CollapsingHeader("CPU Stats", ImGuiTreeNodeFlags_DefaultOpen))
    {
        Profiler* profiler = GetProfiler();
        if (profiler == nullptr)
        {
            ImGui::TextDisabled("Profiler not available");
            return;
        }

        const std::vector<CpuStat>& frameStats = profiler->GetCpuFrameStats();
        const float maxTime = 16.67f; // 60fps target
        ImVec4 cpuColor(0.3f, 0.7f, 0.9f, 1.0f); // Blue

        if (frameStats.empty())
        {
            ImGui::TextDisabled("No CPU stats recorded");
        }
        else
        {
            for (const CpuStat& stat : frameStats)
            {
                DrawStatBar(stat.mName, stat.mSmoothedTime, maxTime, cpuColor);
            }
        }

        // Show persistent stats in a tree node
        const std::vector<CpuStat>& persistentStats = profiler->GetCpuPersistentStats();
        if (!persistentStats.empty())
        {
            if (ImGui::TreeNode("Persistent Stats"))
            {
                for (const CpuStat& stat : persistentStats)
                {
                    DrawStatBar(stat.mName, stat.mSmoothedTime, maxTime, cpuColor);
                }
                ImGui::TreePop();
            }
        }

        ImGui::Spacing();
    }
}

void ProfilingWindow::DrawGpuStatsSection()
{
    if (ImGui::CollapsingHeader("GPU Stats", ImGuiTreeNodeFlags_DefaultOpen))
    {
        Profiler* profiler = GetProfiler();
        if (profiler == nullptr)
        {
            ImGui::TextDisabled("Profiler not available");
            return;
        }

        const std::vector<GpuStat>& gpuStats = profiler->GetGpuStats();
        const float maxTime = 16.67f; // 60fps target
        ImVec4 gpuColor(0.9f, 0.5f, 0.3f, 1.0f); // Orange/Red

        if (gpuStats.empty())
        {
            ImGui::TextDisabled("No GPU stats recorded");
        }
        else
        {
            for (const GpuStat& stat : gpuStats)
            {
                DrawStatBar(stat.mName, stat.mSmoothedTime, maxTime, gpuColor);
            }
        }

        ImGui::Spacing();
    }
}

void ProfilingWindow::DrawMemoryBar(const char* label, uint64_t used, uint64_t total, const ImVec4& color)
{
    float usedMB = (float)used / (1024.0f * 1024.0f);
    float totalMB = (float)total / (1024.0f * 1024.0f);
    float fraction = (total > 0) ? ((float)used / (float)total) : 0.0f;
    fraction = std::min(fraction, 1.0f);
    float percentage = fraction * 100.0f;

    // Color based on usage
    ImVec4 barColor = color;
    if (percentage >= 80.0f)
        barColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); // Red
    else if (percentage >= 60.0f)
        barColor = ImVec4(0.9f, 0.9f, 0.2f, 1.0f); // Yellow

    ImGui::Text("%-10s", label);
    ImGui::SameLine(80.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
    char overlay[64];
    if (total > 0)
        snprintf(overlay, sizeof(overlay), "%.1f / %.1f MB (%.0f%%)", usedMB, totalMB, percentage);
    else
        snprintf(overlay, sizeof(overlay), "%.2f MB", usedMB);
    ImGui::ProgressBar(fraction, ImVec2(-1, 0), overlay);
    ImGui::PopStyleColor();
}

// Helper to get memory limit for a platform
static uint64_t GetMemoryBytesForPlatform(Platform platform)
{
    switch (platform)
    {
        case Platform::Wii:      return 88ULL * 1024 * 1024;  // 24MB MEM1 + 64MB MEM2
        case Platform::GameCube: return 43ULL * 1024 * 1024;  // ~43MB total
        case Platform::N3DS:     return 128ULL * 1024 * 1024; // 128MB
        default:                 return 0; // Windows/Linux: no fixed cap
    }
}

// Helper to get name for a platform
static const char* GetNameForPlatform(Platform platform)
{
    switch (platform)
    {
        case Platform::Windows:  return "Windows";
        case Platform::Linux:    return "Linux";
        case Platform::Mac:      return "Mac";
        case Platform::Android:  return "Android";
        case Platform::GameCube: return "GameCube";
        case Platform::Wii:      return "Wii";
        case Platform::N3DS:     return "3DS";
        default:                 return "Unknown";
    }
}

uint64_t ProfilingWindow::GetPlatformTotalMemoryBytes() const
{
    // Check for current target profile first
    PackagingSettings* settings = PackagingSettings::Get();
    if (settings != nullptr)
    {
        BuildProfile* target = settings->GetCurrentTargetProfile();
        if (target != nullptr)
        {
            return GetMemoryBytesForPlatform(target->mTargetPlatform);
        }
    }

    // Fall back to compile-time platform
#if PLATFORM_WII
    return 88ULL * 1024 * 1024;
#elif PLATFORM_GCN
    return 43ULL * 1024 * 1024;
#elif PLATFORM_3DS
    return 128ULL * 1024 * 1024;
#else
    return 0;
#endif
}

const char* ProfilingWindow::GetPlatformName() const
{
    // Check for current target profile first
    PackagingSettings* settings = PackagingSettings::Get();
    if (settings != nullptr)
    {
        BuildProfile* target = settings->GetCurrentTargetProfile();
        if (target != nullptr)
        {
            return GetNameForPlatform(target->mTargetPlatform);
        }
    }

    // Fall back to compile-time platform
#if PLATFORM_WII
    return "Wii";
#elif PLATFORM_GCN
    return "GameCube";
#elif PLATFORM_3DS
    return "3DS";
#elif PLATFORM_WINDOWS
    return "Windows";
#elif PLATFORM_LINUX
    return "Linux";
#elif PLATFORM_ANDROID
    return "Android";
#else
    return "Unknown";
#endif
}

void ProfilingWindow::DrawMemorySection()
{
    if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char* platformName = GetPlatformName();

        // Check if using a target profile
        PackagingSettings* settings = PackagingSettings::Get();
        BuildProfile* target = (settings != nullptr) ? settings->GetCurrentTargetProfile() : nullptr;

        // Determine the platform used for byte sizing + the budget cap. With a
        // build target we size for that console; otherwise we size for the host
        // and show no fixed budget (the estimate is still game-scoped).
        Platform targetPlatform;
        uint64_t platformTotal;
        if (target != nullptr)
        {
            targetPlatform = target->mTargetPlatform;
            platformTotal = GetPlatformTotalMemoryBytes();

            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Target: %s", target->mName.c_str());
            if (platformTotal > 0)
            {
                float totalMB = (float)platformTotal / (1024.0f * 1024.0f);
                ImGui::Text("Platform: %s (%.0f MB limit)", platformName, totalMB);
            }
            else
            {
                ImGui::Text("Platform: %s (no memory limit)", platformName);
            }
        }
        else
        {
#if PLATFORM_LINUX
            targetPlatform = Platform::Linux;
#elif PLATFORM_MAC
            targetPlatform = Platform::Mac;
#elif PLATFORM_ANDROID
            targetPlatform = Platform::Android;
#else
            targetPlatform = Platform::Windows;
#endif
            platformTotal = 0;

            ImGui::Text("Platform: %s", platformName);
            ImGui::TextDisabled("No build target set - showing estimated game memory.");
            ImGui::TextDisabled("Set a Build Profile as Current Target for a console budget.");
        }

        ImGui::Spacing();

        // Game-scoped estimate (only assets the running game references).
        EstimatedMemory estimated = EstimateMemoryForPlatform(targetPlatform);
        uint64_t totalEstimated = estimated.Total();

        ImVec4 texColor(0.4f, 0.7f, 0.9f, 1.0f);   // Blue
        ImVec4 meshColor(0.5f, 0.9f, 0.5f, 1.0f);  // Green
        ImVec4 audioColor(0.9f, 0.7f, 0.4f, 1.0f); // Orange
        ImVec4 rtColor(0.9f, 0.5f, 0.9f, 1.0f);    // Purple
        ImVec4 otherColor(0.7f, 0.7f, 0.7f, 1.0f); // Grey
        ImVec4 totalColor(0.3f, 0.8f, 0.5f, 1.0f); // Green

        // Show breakdown
        if (estimated.mTextures > 0)
            DrawMemoryBar("Textures", estimated.mTextures, platformTotal, texColor);
        if (estimated.mMeshes > 0)
            DrawMemoryBar("Meshes", estimated.mMeshes, platformTotal, meshColor);
        if (estimated.mSkeletalMeshes > 0)
            DrawMemoryBar("Skel Mesh", estimated.mSkeletalMeshes, platformTotal, meshColor);
        if (estimated.mAudio > 0)
            DrawMemoryBar("Audio", estimated.mAudio, platformTotal, audioColor);
        if (estimated.mRenderTargets > 0)
            DrawMemoryBar("Frame Buf", estimated.mRenderTargets, platformTotal, rtColor);
        if (estimated.mOther > 0)
            DrawMemoryBar("Other", estimated.mOther, platformTotal, otherColor);

        ImGui::Spacing();
        ImGui::Separator();

        // Total bar
        if (platformTotal > 0)
        {
            DrawMemoryBar("TOTAL", totalEstimated, platformTotal, totalColor);

            // Warning if over budget
            float usage = (float)totalEstimated / (float)platformTotal * 100.0f;
            if (usage > 90.0f)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "WARNING: Over 90%% memory budget!");
            }
            else if (usage > 75.0f)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Caution: Over 75%% memory budget");
            }
        }
        else
        {
            float totalMB = (float)totalEstimated / (1024.0f * 1024.0f);
            ImGui::Text("Estimated game memory: %.2f MB", totalMB);
        }

        // Whole-process figure for reference -- this includes the editor itself
        // and is NOT the game estimate above.
        ImGui::Spacing();
        ImGui::TextDisabled("Whole process (incl. editor): %.1f MB RAM, %.1f MB VRAM",
                            SYS_GetRAMUsage(), SYS_GetVRAMUsage());

        ImGui::Spacing();
    }
}

void ProfilingWindow::DrawFrameGraph()
{
    if (ImGui::CollapsingHeader("Frame Time Graph", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (mFrameTimeHistory.empty())
        {
            ImGui::TextDisabled("No frame history");
            return;
        }

        // Scale options
        static const char* scaleNames[] = { "Auto", "16.67ms (60fps)", "33.33ms (30fps)", "50ms", "100ms", "200ms", "500ms", "1s", "5s", "10s", "20s" };
        static const float scaleValues[] = { 0.0f, 16.67f, 33.33f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f, 20000.0f };
        static const int scaleCount = sizeof(scaleValues) / sizeof(scaleValues[0]);

        // Scale dropdown
        ImGui::SetNextItemWidth(150.0f);
        ImGui::Combo("Scale", &mFrameGraphScaleIndex, scaleNames, scaleCount);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Zoom out to see larger frame time spikes");
        }

        // Convert deque to vector for ImGui
        std::vector<float> values(mFrameTimeHistory.begin(), mFrameTimeHistory.end());

        // Determine max scale value
        float maxVal;
        if (mFrameGraphScaleIndex == 0)
        {
            // Auto: scale based on actual data
            maxVal = 16.67f;
            for (float v : values)
            {
                maxVal = std::max(maxVal, v);
            }
            maxVal *= 1.2f; // Add headroom
        }
        else
        {
            // Fixed scale
            maxVal = scaleValues[mFrameGraphScaleIndex];
        }

        // Draw the graph
        char overlayText[64];
        snprintf(overlayText, sizeof(overlayText), "Frame Time (%.2f ms)", mSmoothedFrameTime);
        ImGui::PlotLines("##FrameGraph", values.data(), (int)values.size(),
                         0, overlayText, 0.0f, maxVal, ImVec2(-1, 80));

        // Reference lines legend
        ImGui::TextDisabled("60fps = 16.67ms | 30fps = 33.33ms");

        ImGui::Spacing();
    }
}

void ProfilingWindow::DrawCustomStatsSection()
{
    if (ImGui::CollapsingHeader("Custom Stats"))
    {
        ImVec4 customColor(0.8f, 0.5f, 0.8f, 1.0f); // Purple

        for (const CustomProfilingStat& stat : mCustomStats)
        {
            if (stat.mShowAsBar)
            {
                DrawStatBar(stat.mName.c_str(), stat.mValue, stat.mMaxValue, customColor);
            }
            else
            {
                ImGui::Text("%-12s: %.2f", stat.mName.c_str(), stat.mValue);
            }
        }

        ImGui::Spacing();
    }
}

void ProfilingWindow::DrawCustomSections()
{
    for (const CustomProfilingSection& section : mCustomSections)
    {
        if (ImGui::CollapsingHeader(section.mName.c_str()))
        {
            if (section.mDrawCallback)
            {
                section.mDrawCallback(section.mUserData);
            }
        }
    }
}

// Addon extension API implementation

void ProfilingWindow::RegisterCustomStat(uint64_t hookId, const char* name, const char* category, float maxValue, bool showAsBar)
{
    // Check if already registered
    for (CustomProfilingStat& stat : mCustomStats)
    {
        if (stat.mName == name)
        {
            stat.mHookId = hookId;
            stat.mCategory = category ? category : "";
            stat.mMaxValue = maxValue;
            stat.mShowAsBar = showAsBar;
            return;
        }
    }

    CustomProfilingStat stat;
    stat.mHookId = hookId;
    stat.mName = name ? name : "";
    stat.mCategory = category ? category : "";
    stat.mMaxValue = maxValue;
    stat.mShowAsBar = showAsBar;
    stat.mValue = 0.0f;
    mCustomStats.push_back(stat);
}

void ProfilingWindow::UnregisterCustomStat(uint64_t hookId, const char* name)
{
    std::string n = name ? name : "";
    mCustomStats.erase(
        std::remove_if(mCustomStats.begin(), mCustomStats.end(),
            [hookId, &n](const CustomProfilingStat& stat) {
                return stat.mHookId == hookId && stat.mName == n;
            }),
        mCustomStats.end());
}

void ProfilingWindow::SetCustomStatValue(const char* name, float value)
{
    for (CustomProfilingStat& stat : mCustomStats)
    {
        if (stat.mName == name)
        {
            stat.mValue = value;
            return;
        }
    }
}

void ProfilingWindow::RegisterCustomSection(uint64_t hookId, const char* name, void (*drawFunc)(void*), void* userData)
{
    // Check if already registered
    for (CustomProfilingSection& section : mCustomSections)
    {
        if (section.mName == name)
        {
            section.mHookId = hookId;
            section.mDrawCallback = drawFunc;
            section.mUserData = userData;
            return;
        }
    }

    CustomProfilingSection section;
    section.mHookId = hookId;
    section.mName = name ? name : "";
    section.mDrawCallback = drawFunc;
    section.mUserData = userData;
    mCustomSections.push_back(section);
}

void ProfilingWindow::UnregisterCustomSection(uint64_t hookId, const char* name)
{
    std::string n = name ? name : "";
    mCustomSections.erase(
        std::remove_if(mCustomSections.begin(), mCustomSections.end(),
            [hookId, &n](const CustomProfilingSection& section) {
                return section.mHookId == hookId && section.mName == n;
            }),
        mCustomSections.end());
}

void ProfilingWindow::RemoveAllHooks(uint64_t hookId)
{
    mCustomStats.erase(
        std::remove_if(mCustomStats.begin(), mCustomStats.end(),
            [hookId](const CustomProfilingStat& stat) {
                return stat.mHookId == hookId;
            }),
        mCustomStats.end());

    mCustomSections.erase(
        std::remove_if(mCustomSections.begin(), mCustomSections.end(),
            [hookId](const CustomProfilingSection& section) {
                return section.mHookId == hookId;
            }),
        mCustomSections.end());
}

// Platform-specific memory helpers

uint64_t ProfilingWindow::GetBytesPerPixelForPlatform(Platform platform) const
{
    // Most platforms use 4 bytes (RGBA8), but consoles often use 2 bytes (RGB565/RGBA5551)
    switch (platform)
    {
        case Platform::GameCube:
        case Platform::Wii:
        case Platform::N3DS:
            return 2; // RGB565 or RGBA5551
        default:
            return 4; // RGBA8
    }
}

uint32_t ProfilingWindow::GetIndexSizeForPlatform(Platform platform) const
{
    switch (platform)
    {
        case Platform::GameCube:
        case Platform::Wii:
        case Platform::N3DS:
            return 2; // 16-bit indices
        default:
            return 4; // 32-bit indices
    }
}

ProfilingWindow::EstimatedMemory ProfilingWindow::EstimateMemoryForPlatform(Platform platform) const
{
    // This walks the game world's nodes, so throttle to a few recomputes/sec.
    const Clock* clock = GetAppClock();
    float now = (clock != nullptr) ? clock->GetTime() : 0.0f;
    if (mHasCachedMemory &&
        mLastEstimatePlatform == (int32_t)platform &&
        (now - mLastMemoryEstimateTime) < 0.5f)
    {
        return mCachedMemory;
    }

    EstimatedMemory mem;

    // Single source of truth: the exact same game-scoped entries the Memory
    // Snapshot tool builds (format-accurate textures, streaming-aware audio,
    // frame buffers). We just bucket them into this window's categories, so the
    // two tools always report identical totals.
    std::vector<SnapshotEntry> entries;
    BuildGameMemoryEntries(entries);

    for (const SnapshotEntry& e : entries)
    {
        uint64_t footprint = (e.mGpuBytes > 0) ? e.mGpuBytes : e.mCpuBytes;
        switch (e.mCategory)
        {
        case SnapshotCategory::Textures:
            mem.mTextures += footprint;
            break;
        case SnapshotCategory::Geometry:
            if (e.mTypeName == "SkeletalMesh")
                mem.mSkeletalMeshes += footprint;
            else
                mem.mMeshes += footprint;
            break;
        case SnapshotCategory::Animation:
            mem.mSkeletalMeshes += footprint;
            break;
        case SnapshotCategory::Audio:
            mem.mAudio += footprint;
            break;
        case SnapshotCategory::FrameBuffer:
            mem.mRenderTargets += footprint;
            break;
        default:
            // Scripts, Materials, Fonts, Particles, Nodes, Other
            mem.mOther += footprint;
            break;
        }
    }

    mCachedMemory = mem;
    mHasCachedMemory = true;
    mLastMemoryEstimateTime = now;
    mLastEstimatePlatform = (int32_t)platform;
    return mem;
}

#endif
