#if EDITOR

#include "MemorySnapshotWindow.h"
#include "MemorySnapshotProfiler.h"

#include "Engine.h"
#include "Log.h"
#include "Utilities.h"
#include "System/System.h"
#include "EditorImageCache.h"

#include <stb_image_write.h>

#include <algorithm>
#include <cstdio>
#include <vector>

static MemorySnapshotWindow sMemorySnapshotWindow;

MemorySnapshotWindow* GetMemorySnapshotWindow()
{
    return &sMemorySnapshotWindow;
}

struct BudgetPreset
{
    const char* mName;
    int32_t mMegabytes;   // -1 == custom
};

static const BudgetPreset sBudgetPresets[] =
{
    { "2 MB",     2      },   // PS1
    { "4 MB",     4      },   // N64
    { "8 MB",     8      },
    { "16 MB",    16     },   // Dreamcast
    { "24 MB",    24     },   // GameCube
    { "32 MB",    32     },   // PS2 / PSP
    { "64 MB",    64     },   // Original Xbox
    { "88 MB",    88     },   // Wii
    { "128 MB",   128    },   // 3DS
    { "256 MB",   256    },   // New 3DS
    { "512 MB",   512    },   // PS3 / Xbox 360
    { "1 GB",     1024   },
    { "2 GB",     2048   },
    { "4 GB",     4096   },   // Switch
    { "8 GB",     8192   },
    { "16 GB",    16384  },
    { "32 GB",    32768  },
    { "Custom",   -1     },
};
static const int32_t kNumBudgetPresets = (int32_t)(sizeof(sBudgetPresets) / sizeof(sBudgetPresets[0]));

// Per-category tint for the tree / bars.
static ImVec4 CategoryColor(SnapshotCategory cat)
{
    switch (cat)
    {
    case SnapshotCategory::Geometry:  return ImVec4(0.40f, 0.70f, 1.00f, 1.0f);
    case SnapshotCategory::Textures:  return ImVec4(1.00f, 0.65f, 0.30f, 1.0f);
    case SnapshotCategory::Audio:     return ImVec4(0.55f, 0.85f, 0.45f, 1.0f);
    case SnapshotCategory::Scripts:   return ImVec4(0.80f, 0.60f, 1.00f, 1.0f);
    case SnapshotCategory::Materials: return ImVec4(1.00f, 0.85f, 0.40f, 1.0f);
    case SnapshotCategory::Fonts:     return ImVec4(0.45f, 0.85f, 0.85f, 1.0f);
    case SnapshotCategory::Particles: return ImVec4(1.00f, 0.50f, 0.70f, 1.0f);
    case SnapshotCategory::Animation: return ImVec4(0.70f, 0.75f, 0.95f, 1.0f);
    case SnapshotCategory::Nodes:     return ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
    case SnapshotCategory::FrameBuffer: return ImVec4(0.90f, 0.45f, 0.55f, 1.0f);
    default:                          return ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
    }
}

// The single "estimated runtime footprint" of an entry: GPU-resident cost when it
// has one (textures, meshes), otherwise its CPU cost (audio, scripts, animation).
// This is the number compared against the console RAM budget, and it avoids
// double-counting a texture's editor-only CPU pixel copy.
static uint64_t EntryFootprint(const SnapshotEntry& e)
{
    return e.mGpuBytes > 0 ? e.mGpuBytes : e.mCpuBytes;
}

static std::string FormatBytes(uint64_t bytes)
{
    char buf[32];
    if (bytes >= 1024ull * 1024ull * 1024ull)
        snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024ull * 1024ull)
        snprintf(buf, sizeof(buf), "%.2f MB", bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024ull)
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else
        snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    return buf;
}

void MemorySnapshotWindow::Open()
{
    mIsOpen = true;
}

void MemorySnapshotWindow::Close()
{
    mIsOpen = false;
}

void MemorySnapshotWindow::Shutdown()
{
    mThumbTexId = 0;
}

uint64_t MemorySnapshotWindow::GetSelectedBudgetBytes() const
{
    int32_t mb = sBudgetPresets[mBudgetPresetIndex].mMegabytes;
    if (mb < 0)
        mb = mCustomBudgetMB;
    if (mb < 1)
        mb = 1;
    return (uint64_t)mb * 1024ull * 1024ull;
}

void MemorySnapshotWindow::CaptureNow()
{
    mSnapshot = CaptureSnapshot();
    mHasSnapshot = true;
    mCategoryFilter = -1;
    mThumbDirty = true;
}

void MemorySnapshotWindow::SaveCurrent()
{
    if (!mHasSnapshot)
        return;

    std::string path = MakeDefaultSnapshotPath();
    if (path.empty())
    {
        LogError("Cannot save snapshot: no project open");
        return;
    }
    SaveSnapshot(mSnapshot, path);
}

void MemorySnapshotWindow::LoadFromDialog()
{
    std::vector<std::string> paths = SYS_OpenFileDialog();
    if (paths.empty())
        return;

    MemorySnapshot loaded;
    if (LoadSnapshot(loaded, paths[0]))
    {
        mSnapshot = loaded;
        mHasSnapshot = true;
        mCategoryFilter = -1;
        mThumbDirty = true;
    }
}

void MemorySnapshotWindow::RefreshThumbnailTexture()
{
    mThumbDirty = false;
    mThumbTexId = 0;

    if (mSnapshot.mThumbRgba.empty() || mSnapshot.mThumbW == 0 || mSnapshot.mThumbH == 0)
        return;

    std::string dir = GetDebugSnapshotDir();
    if (dir.empty())
        return;

    // Single reused cache file -- the viewer only ever shows one thumbnail at a time.
    std::string thumbPath = dir + ".snapshot_thumb.png";
    stbi_write_png(thumbPath.c_str(), (int)mSnapshot.mThumbW, (int)mSnapshot.mThumbH, 4,
                   mSnapshot.mThumbRgba.data(), (int)(mSnapshot.mThumbW * 4));

    EditorImageCache::Invalidate(thumbPath);
    mThumbTexId = EditorImageCache::Get(thumbPath);
}

void MemorySnapshotWindow::Draw()
{
    if (!mIsOpen)
        return;

    if (GetEngineState()->mProjectPath.empty())
    {
        ImGui::SetNextWindowSize(ImVec2(420, 140), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Memory Snapshot", &mIsOpen))
        {
            ImGui::TextWrapped("Open a project to profile its running game.");
        }
        ImGui::End();
        return;
    }

    if (mThumbDirty)
        RefreshThumbnailTexture();

    ImGui::SetNextWindowSize(ImVec2(960, 640), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Memory Snapshot", &mIsOpen))
    {
        DrawToolbar();
        ImGui::Separator();

        if (!mHasSnapshot)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("No snapshot yet. Click \"Capture Snapshot\" to profile the");
            ImGui::TextDisabled("running game (Game / 3DS preview surface, not the viewport).");
        }
        else
        {
            DrawHeaderStrip();
            ImGui::Separator();

            // Left: category tree. Right: per-asset table.
            float leftWidth = 260.0f;
            if (ImGui::BeginChild("##CategoryTree", ImVec2(leftWidth, 0), true))
            {
                DrawCategoryTree();
            }
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("##EntryTable", ImVec2(0, 0), true))
            {
                DrawEntryTable();
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void MemorySnapshotWindow::DrawToolbar()
{
    if (ImGui::Button("Capture Snapshot"))
        CaptureNow();
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Profiles the running game (Game / 3DS preview or live Play-In-Editor),\n"
                          "not the editor viewport. %s",
                          IsPlayingInEditor() ? "Game is currently playing."
                                              : "Game is not playing -- captures the edit-time scene.");
    }

    ImGui::SameLine();
    if (!mHasSnapshot) ImGui::BeginDisabled();
    if (ImGui::Button("Save"))
        SaveCurrent();
    if (!mHasSnapshot) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Load..."))
        LoadFromDialog();

    ImGui::SameLine();
    if (ImGui::Button("Reveal Debug Folder"))
    {
        std::string dir = GetDebugSnapshotDir();
        if (!dir.empty())
        {
            if (!DoesDirExist(dir.c_str()))
                SYS_CreateDirectory(dir.c_str());
            SYS_ExplorerOpenDirectory(dir);
        }
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();
    ImGui::TextUnformatted("RAM budget:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);

    const char* preview = sBudgetPresets[mBudgetPresetIndex].mName;
    if (ImGui::BeginCombo("##Budget", preview))
    {
        for (int32_t i = 0; i < kNumBudgetPresets; ++i)
        {
            bool selected = (i == mBudgetPresetIndex);
            if (ImGui::Selectable(sBudgetPresets[i].mName, selected))
                mBudgetPresetIndex = i;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (sBudgetPresets[mBudgetPresetIndex].mMegabytes < 0)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputInt("MB", &mCustomBudgetMB);
        if (mCustomBudgetMB < 1)
            mCustomBudgetMB = 1;
    }
}

void MemorySnapshotWindow::DrawHeaderStrip()
{
    // Compute the estimated runtime footprint (budget metric) and raw CPU/GPU.
    uint64_t footprint = 0;
    for (const SnapshotEntry& e : mSnapshot.mEntries)
        footprint += EntryFootprint(e);

    uint64_t budget = GetSelectedBudgetBytes();

    // Thumbnail on the left.
    if (mThumbTexId != 0)
    {
        float th = 96.0f;
        float aspect = (mSnapshot.mThumbH > 0) ? (float)mSnapshot.mThumbW / mSnapshot.mThumbH : 1.0f;
        ImGui::Image(mThumbTexId, ImVec2(th * aspect, th));
        ImGui::SameLine();
    }

    ImGui::BeginGroup();

    ImGui::Text("Captured: %s   |   %s",
                mSnapshot.mDateString.c_str(),
                mSnapshot.mWasPlayingInEditor ? "Playing in editor" : "Edit scene");
    ImGui::Text("Estimated runtime footprint: %s   (CPU %s + GPU %s)",
                FormatBytes(footprint).c_str(),
                FormatBytes(mSnapshot.GetTotalCpuBytes()).c_str(),
                FormatBytes(mSnapshot.GetTotalGpuBytes()).c_str());
    ImGui::Text("Active audio voices: %u", mSnapshot.mActiveAudioVoices);

    // Budget bar.
    float frac = (budget > 0) ? (float)((double)footprint / (double)budget) : 0.0f;
    ImVec4 barColor;
    if (frac <= 0.75f)      barColor = ImVec4(0.30f, 0.80f, 0.35f, 1.0f); // green
    else if (frac <= 1.0f)  barColor = ImVec4(0.90f, 0.75f, 0.20f, 1.0f); // amber
    else                    barColor = ImVec4(0.90f, 0.30f, 0.25f, 1.0f); // red

    std::string budgetName = (sBudgetPresets[mBudgetPresetIndex].mMegabytes < 0)
                                 ? (std::to_string(mCustomBudgetMB) + " MB")
                                 : sBudgetPresets[mBudgetPresetIndex].mName;

    char overlay[64];
    if (footprint <= budget)
        snprintf(overlay, sizeof(overlay), "Fits in %s  (%.0f%%)", budgetName.c_str(), frac * 100.0f);
    else
        snprintf(overlay, sizeof(overlay), "OVER by %s", FormatBytes(footprint - budget).c_str());

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
    ImGui::ProgressBar(frac > 1.0f ? 1.0f : frac, ImVec2(-1.0f, 0.0f), overlay);
    ImGui::PopStyleColor();

    ImGui::TextDisabled("Whole process (incl. editor): %s RAM, %s VRAM",
                        FormatBytes(mSnapshot.mSystemRamUsed).c_str(),
                        FormatBytes(mSnapshot.mSystemVramUsed).c_str());

    ImGui::EndGroup();
}

void MemorySnapshotWindow::DrawCategoryTree()
{
    uint64_t cpuTotals[(int)SnapshotCategory::Count];
    uint64_t gpuTotals[(int)SnapshotCategory::Count];
    mSnapshot.GetCategoryTotals(cpuTotals, gpuTotals);

    uint64_t budget = GetSelectedBudgetBytes();

    if (ImGui::Selectable("All Categories", mCategoryFilter == -1))
        mCategoryFilter = -1;

    ImGui::Separator();

    for (int32_t c = 0; c < (int)SnapshotCategory::Count; ++c)
    {
        SnapshotCategory cat = (SnapshotCategory)c;

        // Footprint per category (matches the header metric).
        uint64_t footprint = (gpuTotals[c] > 0) ? gpuTotals[c] : cpuTotals[c];
        // Nodes and other zero-byte informational categories still list.

        ImGui::PushID(c);
        ImGui::PushStyleColor(ImGuiCol_Text, CategoryColor(cat));

        char label[128];
        float pct = (budget > 0) ? (float)((double)footprint / budget * 100.0) : 0.0f;
        snprintf(label, sizeof(label), "%s  -  %s (%.0f%%)",
                 SnapshotCategoryName(cat), FormatBytes(footprint).c_str(), pct);

        if (ImGui::Selectable(label, mCategoryFilter == c))
            mCategoryFilter = c;

        ImGui::PopStyleColor();
        ImGui::PopID();
    }
}

void MemorySnapshotWindow::DrawEntryTable()
{
    ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Sortable |
        ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("##SnapshotEntries", 7, flags))
        return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Type",     ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Detail",   ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Refs",     ImGuiTableColumnFlags_WidthFixed,   50.0f);
    ImGui::TableSetupColumn("CPU",      ImGuiTableColumnFlags_WidthFixed,   90.0f);
    ImGui::TableSetupColumn("GPU",      ImGuiTableColumnFlags_WidthFixed,   90.0f);
    ImGui::TableSetupColumn("Total",    ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort, 90.0f);
    ImGui::TableHeadersRow();

    // Build a filtered index list.
    std::vector<int32_t> indices;
    indices.reserve(mSnapshot.mEntries.size());
    for (int32_t i = 0; i < (int32_t)mSnapshot.mEntries.size(); ++i)
    {
        if (mCategoryFilter == -1 || (int)mSnapshot.mEntries[i].mCategory == mCategoryFilter)
            indices.push_back(i);
    }

    // Sort per the table sort specs.
    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs())
    {
        if (specs->SpecsCount > 0)
        {
            const ImGuiTableColumnSortSpecs& s = specs->Specs[0];
            const std::vector<SnapshotEntry>& entries = mSnapshot.mEntries;
            bool asc = (s.SortDirection == ImGuiSortDirection_Ascending);

            std::sort(indices.begin(), indices.end(), [&](int32_t a, int32_t b)
            {
                const SnapshotEntry& ea = entries[a];
                const SnapshotEntry& eb = entries[b];
                int cmp = 0;
                switch (s.ColumnIndex)
                {
                case 0: cmp = ea.mName.compare(eb.mName); break;
                case 1: cmp = ea.mTypeName.compare(eb.mTypeName); break;
                case 2: cmp = ea.mDetail.compare(eb.mDetail); break;
                case 3: cmp = (int)ea.mRefCount - (int)eb.mRefCount; break;
                case 4: cmp = (ea.mCpuBytes < eb.mCpuBytes) ? -1 : (ea.mCpuBytes > eb.mCpuBytes ? 1 : 0); break;
                case 5: cmp = (ea.mGpuBytes < eb.mGpuBytes) ? -1 : (ea.mGpuBytes > eb.mGpuBytes ? 1 : 0); break;
                default:
                {
                    uint64_t ta = EntryFootprint(ea);
                    uint64_t tb = EntryFootprint(eb);
                    cmp = (ta < tb) ? -1 : (ta > tb ? 1 : 0);
                    break;
                }
                }
                return asc ? (cmp < 0) : (cmp > 0);
            });
        }
    }

    for (int32_t idx : indices)
    {
        const SnapshotEntry& e = mSnapshot.mEntries[idx];
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, CategoryColor(e.mCategory));
        ImGui::TextUnformatted(e.mName.c_str());
        ImGui::PopStyleColor();

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(e.mTypeName.c_str());

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(e.mDetail.c_str());

        ImGui::TableNextColumn();
        ImGui::Text("%u", e.mRefCount);

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(e.mCpuBytes > 0 ? FormatBytes(e.mCpuBytes).c_str() : "-");

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(e.mGpuBytes > 0 ? FormatBytes(e.mGpuBytes).c_str() : "-");

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(FormatBytes(EntryFootprint(e)).c_str());
    }

    ImGui::EndTable();
}

#endif
