#if EDITOR

#include "AddonsWindow.h"
#include "EditorWidgets.h"
#include "AddonsMenu.h"
#include "AddonManager.h"
#include "AddonDependencyResolver.h"
#include "NativeAddonManager.h"
#include "../ProjectSelect/TemplateData.h"
#include "Preferences/JsonSettings.h"

#include "Engine.h"
#include "Log.h"
#include "System/System.h"
#include "AssetManager.h"
#include "AssetDir.h"
#include "EditorState.h"

#include "imgui.h"

#include "document.h"

#if API_VULKAN
#include "Graphics/Vulkan/Image.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "backends/imgui_impl_vulkan.h"
#include <stb_image.h>
#endif

#include <algorithm>

static AddonsWindow sAddonsWindow;

AddonsWindow* GetAddonsWindow()
{
    return &sAddonsWindow;
}

AddonsWindow::AddonsWindow()
{
    memset(mSearchBuffer, 0, sizeof(mSearchBuffer));
    memset(mRepoUrlBuffer, 0, sizeof(mRepoUrlBuffer));
    // View-mode preference is loaded lazily in Open() to avoid touching the
    // filesystem during static initialization.
}

AddonsWindow::~AddonsWindow()
{
    // sAddonsWindow is a translation-unit static; this runs after Engine /
    // VulkanContext have already been destroyed, so we MUST NOT touch the
    // GPU here. Thumbnail resources are released earlier via Shutdown(),
    // called from EditorImguiPreShutdown().
}

void AddonsWindow::Shutdown()
{
    ClearThumbnailCache();
}

void AddonsWindow::ClearThumbnailCache()
{
#if API_VULKAN
    if (!mThumbnailCache.empty())
    {
        DeviceWaitIdle();
        for (auto& pair : mThumbnailCache)
        {
            if (pair.second.mTexId != 0)
            {
                ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)pair.second.mTexId);
            }
            if (pair.second.mImage != nullptr)
            {
                GetDestroyQueue()->Destroy(pair.second.mImage);
            }
        }
        mThumbnailCache.clear();
    }
#endif
}

ImTextureID AddonsWindow::GetAddonThumbnail(const std::string& addonId)
{
    // Check cache first
    auto it = mThumbnailCache.find(addonId);
    if (it != mThumbnailCache.end())
    {
        return it->second.mTexId;
    }

#if API_VULKAN
    // Try installed path first: {ProjectDir}/Packages/{addonId}/thumbnail.png
    std::string thumbPath;
    const std::string& projDir = GetEngineState()->mProjectDirectory;
    if (!projDir.empty())
    {
        thumbPath = projDir + "Packages/" + addonId + "/thumbnail.png";
        if (!SYS_DoesFileExist(thumbPath.c_str(), false))
        {
            thumbPath.clear();
        }
    }

    // Try addon cache: {CacheDir}/{addonId}/thumbnail.png
    if (thumbPath.empty())
    {
        AddonManager* am = AddonManager::Get();
        if (am != nullptr)
        {
            std::string cachePath = am->GetAddonCacheDirectory() + "/" + addonId + "/thumbnail.png";
            if (SYS_DoesFileExist(cachePath.c_str(), false))
            {
                thumbPath = cachePath;
            }
        }
    }

    if (thumbPath.empty())
    {
        mThumbnailCache[addonId] = {};
        return 0;
    }

    // Load with stb_image
    int width, height, channels;
    stbi_uc* pixels = stbi_load(thumbPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr)
    {
        mThumbnailCache[addonId] = {};
        return 0;
    }

    // Create Vulkan image
    ImageDesc imgDesc;
    imgDesc.mWidth = width;
    imgDesc.mHeight = height;
    imgDesc.mFormat = VK_FORMAT_R8G8B8A8_UNORM;
    imgDesc.mUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgDesc.mMipLevels = 1;
    imgDesc.mLayers = 1;

    SamplerDesc sampDesc;
    sampDesc.mMagFilter = VK_FILTER_LINEAR;
    sampDesc.mMinFilter = VK_FILTER_LINEAR;
    sampDesc.mAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    Image* image = new Image(imgDesc, sampDesc, "AddonThumbnail");
    image->Update(pixels);
    image->Transition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    stbi_image_free(pixels);

    ImTextureID texId = (ImTextureID)ImGui_ImplVulkan_AddTexture(
        image->GetSampler(),
        image->GetView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    ThumbnailEntry entry;
    entry.mTexId = texId;
    entry.mImage = image;
    mThumbnailCache[addonId] = entry;
    return texId;
#else
    mThumbnailCache[addonId] = {};
    return 0;
#endif
}

void AddonsWindow::Open()
{
    mIsOpen = true;
    mSelectedTab = 0;
    mShowAddonDetails = false;
    mShowAddRepoPopup = false;
    mSelectedAddonId.clear();
    mErrorMessage.clear();
    mStatusMessage.clear();

    // Load view-mode preference once per session.
    static bool sLoadedViewSettings = false;
    if (!sLoadedViewSettings)
    {
        LoadViewSettings();
        sLoadedViewSettings = true;
    }

    // Load installed addons when opening
    AddonManager* am = AddonManager::Get();
    if (am != nullptr)
    {
        am->LoadInstalledAddons();

        // Refresh on first open
        if (mNeedsRefresh)
        {
            OnRefreshRepositories();
            mNeedsRefresh = false;
        }
    }
}

void AddonsWindow::Close()
{
    mIsOpen = false;
    ClearThumbnailCache();
}

void AddonsWindow::Draw()
{
    if (!mIsOpen)
    {
        return;
    }

    // Check if project is loaded
    if (GetEngineState()->mProjectPath.empty())
    {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 windowSize(400.0f, 150.0f);
        ImVec2 windowPos((io.DisplaySize.x - windowSize.x) * 0.5f, (io.DisplaySize.y - windowSize.y) * 0.5f);
        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

        if (ImGui::Begin("Addons", &mIsOpen, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::TextWrapped("Please open a project before browsing addons.");
            ImGui::TextWrapped("Addons are installed into the current project.");

            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(80, 0)))
            {
                Close();
            }
        }
        ImGui::End();
        return;
    }

    // Center the window
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize(750.0f, 550.0f);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) * 0.5f, (io.DisplaySize.y - windowSize.y) * 0.5f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

    if (ImGui::Begin("Addons", &mIsOpen, windowFlags))
    {
        if (ImGui::BeginMenuBar())
        {
            DrawAddonsMenuBar();
            ImGui::EndMenuBar();
        }

        // Tab bar
        if (ImGui::BeginTabBar("AddonsTabs"))
        {
            if (ImGui::BeginTabItem("Browse Addons"))
            {
                mSelectedTab = 0;
                DrawAddonBrowser();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Installed"))
            {
                mSelectedTab = 1;
                DrawInstalledAddons();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Repositories"))
            {
                mSelectedTab = 2;
                DrawRepositoryManager();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    // Draw popups
    DrawAddonDetailsPopup();
    DrawAddRepoPopup();
    DrawUninstallConfirmPopup();

    // Draw build log popup
    if (mShowBuildLog)
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(600, 400));

        if (ImGui::Begin("Build Log", &mShowBuildLog,
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        {
            NativeAddonManager* nam = NativeAddonManager::Get();
            const NativeAddonState* state = nam ? nam->GetState(mBuildLogAddonId) : nullptr;

            ImGui::Text("Build Log: %s", mBuildLogAddonId.c_str());
            ImGui::Separator();

            if (state)
            {
                if (state->mBuildSucceeded)
                {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Build Succeeded");
                }
                else if (!state->mBuildError.empty())
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Build Failed");
                }

                ImGui::Spacing();

                // Scrollable log content
                ImGui::BeginChild("LogContent", ImVec2(0, -30), true);
                ImGui::TextWrapped("%s", state->mBuildLog.c_str());
                ImGui::EndChild();
            }
            else
            {
                ImGui::TextDisabled("No build log available.");
            }

            if (ImGui::Button("Close", ImVec2(80, 0)))
            {
                mShowBuildLog = false;
            }
        }
        ImGui::End();
    }
}

// ---------------------------------------------------------------------------
// View-mode + shared helpers
// ---------------------------------------------------------------------------

static const char* kViewPrefFile = "/AddonsWindow.json";
static const char* kViewPrefKey  = "useTableView";

void AddonsWindow::LoadViewSettings()
{
    rapidjson::Document doc;
    std::string path = JsonSettings::GetPreferencesDirectory() + kViewPrefFile;
    if (!JsonSettings::LoadFromFile(path, doc))
    {
        return; // first run -- keep the default (table)
    }
    mUseTableView = JsonSettings::GetBool(doc, kViewPrefKey, true);
}

void AddonsWindow::SaveViewSettings()
{
    JsonSettings::EnsurePreferencesDirectory();

    rapidjson::Document doc;
    std::string path = JsonSettings::GetPreferencesDirectory() + kViewPrefFile;
    JsonSettings::LoadFromFile(path, doc); // best-effort merge
    if (!doc.IsObject())
    {
        doc.SetObject();
    }
    JsonSettings::SetBool(doc, kViewPrefKey, mUseTableView);
    JsonSettings::SaveToFile(path, doc);
}

void AddonsWindow::DrawViewModeToggle()
{
    ImGui::TextDisabled("View:");
    ImGui::SameLine();

    auto drawOption = [this](const char* label, bool wantTable)
    {
        bool selected = (mUseTableView == wantTable);
        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
        }
        if (ImGui::SmallButton(label))
        {
            if (mUseTableView != wantTable)
            {
                mUseTableView = wantTable;
                SaveViewSettings();
            }
        }
        if (selected)
        {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
    };

    drawOption("Table", true);
    drawOption("Cards", false);
    ImGui::NewLine();
}

void AddonsWindow::DrawClampedName(const char* name, float maxWidth)
{
    if (name == nullptr || name[0] == '\0')
    {
        ImGui::TextUnformatted("");
        return;
    }

    ImVec2 fullSize = ImGui::CalcTextSize(name);
    if (fullSize.x <= maxWidth)
    {
        ImGui::TextUnformatted(name);
        return;
    }

    // Binary search for the longest prefix that, with ellipsis, fits in maxWidth.
    const char ellipsis[] = "...";
    float ellipsisW = ImGui::CalcTextSize(ellipsis).x;
    int len = (int)strlen(name);

    int lo = 0;
    int hi = len;
    int best = 0;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        ImVec2 sz = ImGui::CalcTextSize(name, name + mid);
        if (sz.x + ellipsisW <= maxWidth)
        {
            best = mid;
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }

    std::string truncated;
    truncated.reserve(best + 3);
    truncated.assign(name, name + best);
    truncated += ellipsis;
    ImGui::TextUnformatted(truncated.c_str());

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", name);
    }
}

void AddonsWindow::DrawAddonTable_Browse(const std::vector<const Addon*>& filtered)
{
    if (filtered.empty())
    {
        ImGui::TextDisabled("No addons match the current filter.");
        return;
    }

    AddonManager* am = AddonManager::Get();

    ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("##AddonsBrowseTable", 5, flags))
    {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("##Icon",  ImGuiTableColumnFlags_WidthFixed,    28.0f);
    ImGui::TableSetupColumn("Name",    ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Author",  ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Native",  ImGuiTableColumnFlags_WidthFixed,    70.0f);
    ImGui::TableSetupColumn("Status",  ImGuiTableColumnFlags_WidthFixed,   180.0f);
    ImGui::TableHeadersRow();

    for (const Addon* addonPtr : filtered)
    {
        const Addon& addon = *addonPtr;
        ImGui::PushID(addon.mMetadata.mId.c_str());

        ImGui::TableNextRow();

        // Icon
        ImGui::TableNextColumn();
        ImTextureID thumbTex = GetAddonThumbnail(addon.mMetadata.mId);
        if (thumbTex != 0)
        {
            ImGui::Image(thumbTex, ImVec2(24, 24));
        }
        else
        {
            ImVec2 cur = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                cur, ImVec2(cur.x + 24, cur.y + 24), IM_COL32(70, 70, 90, 255));
            ImGui::Dummy(ImVec2(24, 24));
        }

        // Name (clamped)
        ImGui::TableNextColumn();
        {
            float w = ImGui::GetContentRegionAvail().x;
            DrawClampedName(addon.mMetadata.mName.c_str(), w);
        }

        // Author
        ImGui::TableNextColumn();
        if (!addon.mMetadata.mAuthor.empty())
        {
            float w = ImGui::GetContentRegionAvail().x;
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            DrawClampedName(addon.mMetadata.mAuthor.c_str(), w);
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::TextDisabled("--");
        }

        // Native badge
        ImGui::TableNextColumn();
        if (addon.mNative.mHasNative)
        {
            ImVec4 col = (addon.mNative.mTarget == NativeAddonTarget::EditorOnly)
                ? ImVec4(0.40f, 0.40f, 0.80f, 1.0f)
                : ImVec4(0.80f, 0.40f, 0.40f, 1.0f);
            const char* lbl = (addon.mNative.mTarget == NativeAddonTarget::EditorOnly)
                ? "Editor" : "Eng+Ed";
            ImGui::TextColored(col, "%s", lbl);
        }
        else
        {
            ImGui::TextDisabled("--");
        }

        // Status + actions
        ImGui::TableNextColumn();
        if (addon.mIsInstalled)
        {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Installed");
            if (am && am->HasUpdate(addon.mMetadata.mId))
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Update"))
                {
                    OnDownloadAddon(addon.mMetadata.mId);
                }
            }
        }
        else
        {
            if (ImGui::SmallButton("Download"))
            {
                OnDownloadAddon(addon.mMetadata.mId);
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Info"))
        {
            OnViewMore(addon.mMetadata.mId);
        }

        ImGui::PopID();
    }

    ImGui::EndTable();
}

void AddonsWindow::DrawAddonTable_Installed(const std::vector<InstalledAddon>& installed)
{
    AddonManager* am = AddonManager::Get();
    NativeAddonManager* nam = NativeAddonManager::Get();
    if (am == nullptr)
    {
        return;
    }

    ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("##AddonsInstalledTable", 7, flags))
    {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("##Icon",  ImGuiTableColumnFlags_WidthFixed,    28.0f);
    ImGui::TableSetupColumn("Name",    ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Version", ImGuiTableColumnFlags_WidthFixed,    70.0f);
    ImGui::TableSetupColumn("Native",  ImGuiTableColumnFlags_WidthFixed,   100.0f);
    ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed,    65.0f);
    ImGui::TableSetupColumn("Status",  ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,   220.0f);
    ImGui::TableHeadersRow();

    for (const InstalledAddon& inst : installed)
    {
        ImGui::PushID(inst.mId.c_str());

        const Addon* addon = am->FindAddon(inst.mId);
        bool hasNative = addon && addon->mNative.mHasNative;
        const NativeAddonState* nativeState = nam ? nam->GetState(inst.mId) : nullptr;
        bool isBinaryMode = (inst.mNativeMode == NativeAddonResolveMode::Binary);

        ImGui::TableNextRow();

        // Icon
        ImGui::TableNextColumn();
        ImTextureID thumbTex = GetAddonThumbnail(inst.mId);
        if (thumbTex != 0)
        {
            ImGui::Image(thumbTex, ImVec2(24, 24));
        }
        else
        {
            ImVec2 cur = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                cur, ImVec2(cur.x + 24, cur.y + 24), IM_COL32(70, 70, 90, 255));
            ImGui::Dummy(ImVec2(24, 24));
        }

        // Name
        ImGui::TableNextColumn();
        {
            const std::string& name = addon ? addon->mMetadata.mName : inst.mId;
            float w = ImGui::GetContentRegionAvail().x;
            DrawClampedName(name.c_str(), w);
        }

        // Version
        ImGui::TableNextColumn();
        ImGui::TextDisabled("v%s", inst.mVersion.c_str());

        // Native (target chip + mode selector)
        ImGui::TableNextColumn();
        if (hasNative)
        {
            ImVec4 col = (addon->mNative.mTarget == NativeAddonTarget::EditorOnly)
                ? ImVec4(0.40f, 0.40f, 0.80f, 1.0f)
                : ImVec4(0.80f, 0.40f, 0.40f, 1.0f);
            const char* tgt = (addon->mNative.mTarget == NativeAddonTarget::EditorOnly)
                ? "Editor" : "Eng+Ed";
            ImGui::TextColored(col, "%s", tgt);

            const char* modeLabel = isBinaryMode ? "Binary" : "Source";
            ImGui::PushItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##Mode", modeLabel))
            {
                if (ImGui::Selectable("Source", !isBinaryMode))
                {
                    if (isBinaryMode) OnToggleNativeMode(inst.mId);
                }
                if (ImGui::Selectable("Binary", isBinaryMode))
                {
                    if (!isBinaryMode) OnToggleNativeMode(inst.mId);
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Source: compile from code\nBinary: use precompiled DLL");
            }
        }
        else
        {
            ImGui::TextDisabled("--");
        }

        // Enabled checkbox
        ImGui::TableNextColumn();
        if (hasNative)
        {
            bool enableNative = inst.mEnableNative;
            if (ImGui::Checkbox("##Enabled", &enableNative))
            {
                OnToggleNativeEnabled(inst.mId);
            }
        }
        else
        {
            ImGui::TextDisabled("--");
        }

        // Status
        ImGui::TableNextColumn();
        if (hasNative && nativeState)
        {
            if (nativeState->mBuildInProgress)
            {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Building...");
            }
            else if (nam && nam->IsLoaded(inst.mId))
            {
                if (nativeState->mLoadedFromBinary)
                {
                    if (nativeState->mBinaryStatus == "Synced")
                    {
                        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Loaded (Synced)");
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.8f, 1.0f), "Loaded (Local)");
                    }
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Loaded (Source)");
                }
            }
            else if (isBinaryMode && !nativeState->mBinaryStatus.empty())
            {
                if (nativeState->mBinaryStatus == "Missing Binary")
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Missing Binary");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("No precompiled binary. Use Sync to download or switch to Source.");
                    }
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", nativeState->mBinaryStatus.c_str());
                }
            }
            else if (nativeState->mBuildSucceeded)
            {
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Built");
            }
            else if (!nativeState->mBuildError.empty())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Build Failed");
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", nativeState->mBuildError.c_str());
                }
            }
            else
            {
                ImGui::TextDisabled("--");
            }
        }
        else
        {
            ImGui::TextDisabled("--");
        }

        // Actions
        ImGui::TableNextColumn();
        bool hasUpdate = addon && am->HasUpdate(inst.mId);
        if (hasUpdate)
        {
            if (ImGui::SmallButton("Update"))
            {
                OnDownloadAddon(inst.mId);
            }
            ImGui::SameLine();
        }
        if (hasNative)
        {
            if (!isBinaryMode)
            {
                if (ImGui::SmallButton("Build"))
                {
                    OnBuildNativeAddon(inst.mId);
                }
                ImGui::SameLine();
            }
            else if (addon && !addon->mNative.mBinaries.empty())
            {
                if (ImGui::SmallButton("Sync"))
                {
                    OnSyncNativeAddonBinary(inst.mId);
                }
                ImGui::SameLine();
            }
            if (ImGui::SmallButton("Reload"))
            {
                OnReloadNativeAddon(inst.mId);
            }
            ImGui::SameLine();
            if (nativeState && !nativeState->mBuildLog.empty())
            {
                if (ImGui::SmallButton("Log"))
                {
                    mShowBuildLog = true;
                    mBuildLogAddonId = inst.mId;
                }
                ImGui::SameLine();
            }
        }
        if (ImGui::SmallButton("Uninstall"))
        {
            mUninstallAddonId = inst.mId;
            mShowUninstallConfirm = true;
        }

        ImGui::PopID();
    }

    ImGui::EndTable();
}

void AddonsWindow::DrawAddonBrowser()
{
    AddonManager* am = AddonManager::Get();
    if (am == nullptr)
    {
        ImGui::TextDisabled("Addon manager not initialized.");
        return;
    }

    // Search and refresh bar
    ImGui::SetNextItemWidth(300);
    ImGui::InputTextWithHint("##Search", "Search addons...", mSearchBuffer, sizeof(mSearchBuffer));

    ImGui::SameLine(ImGui::GetWindowWidth() - 220);
    if (ImGui::Button("Resolve Deps", ImVec2(110, 0)))
    {
        OnResolveDependencies();
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Scan all installed addons and download any missing dependencies.");
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    if (ImGui::Button("Refresh", ImVec2(80, 0)))
    {
        OnRefreshRepositories();
    }

    // View-mode toggle
    DrawViewModeToggle();

    // Status message
    if (!mStatusMessage.empty())
    {
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "%s", mStatusMessage.c_str());
    }

    ImGui::Separator();
    ImGui::Spacing();

    const std::vector<Addon>& addons = am->GetAvailableAddons();

    if (addons.empty())
    {
        ImGui::TextDisabled("No addons found.");
        ImGui::TextDisabled("Click 'Refresh' to fetch addons from repositories.");
        return;
    }

    // Collect available tags
    mAvailableTags.clear();
    for (const Addon& addon : addons)
    {
        for (const std::string& tag : addon.mMetadata.mTags)
        {
            if (std::find(mAvailableTags.begin(), mAvailableTags.end(), tag) == mAvailableTags.end())
            {
                mAvailableTags.push_back(tag);
            }
        }
    }
    std::sort(mAvailableTags.begin(), mAvailableTags.end());

    // Tag filter buttons
    if (!mAvailableTags.empty())
    {
        ImGui::Text("Tags:");
        ImGui::SameLine();

        for (const std::string& tag : mAvailableTags)
        {
            bool selected = std::find(mSelectedTags.begin(), mSelectedTags.end(), tag) != mSelectedTags.end();

            if (selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
            }

            if (ImGui::SmallButton(tag.c_str()))
            {
                if (selected)
                {
                    mSelectedTags.erase(std::find(mSelectedTags.begin(), mSelectedTags.end(), tag));
                }
                else
                {
                    mSelectedTags.push_back(tag);
                }
            }

            if (selected)
            {
                ImGui::PopStyleColor();
            }

            ImGui::SameLine();
        }

        if (!mSelectedTags.empty())
        {
            if (ImGui::SmallButton("Clear"))
            {
                mSelectedTags.clear();
            }
        }
        else
        {
            ImGui::NewLine();
        }

        ImGui::Spacing();
    }

    // Filter addons
    std::vector<const Addon*> filteredAddons;
    std::string searchStr = mSearchBuffer;
    std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

    for (const Addon& addon : addons)
    {
        // Search filter
        if (!searchStr.empty())
        {
            std::string name = addon.mMetadata.mName;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            std::string desc = addon.mMetadata.mDescription;
            std::transform(desc.begin(), desc.end(), desc.begin(), ::tolower);

            if (name.find(searchStr) == std::string::npos &&
                desc.find(searchStr) == std::string::npos)
            {
                continue;
            }
        }

        // Tag filter
        if (!mSelectedTags.empty())
        {
            bool hasTag = false;
            for (const std::string& selectedTag : mSelectedTags)
            {
                if (std::find(addon.mMetadata.mTags.begin(), addon.mMetadata.mTags.end(), selectedTag) != addon.mMetadata.mTags.end())
                {
                    hasTag = true;
                    break;
                }
            }
            if (!hasTag)
            {
                continue;
            }
        }

        filteredAddons.push_back(&addon);
    }

    if (mUseTableView)
    {
        DrawAddonTable_Browse(filteredAddons);
    }
    else
    {
        // Addon grid
        float cardWidth = 200.0f;
        float spacing = 10.0f;
        int cardsPerRow = (int)((ImGui::GetContentRegionAvail().x + spacing) / (cardWidth + spacing));
        if (cardsPerRow < 1) cardsPerRow = 1;

        ImGui::BeginChild("AddonGrid", ImVec2(0, 0), true);

        for (int i = 0; i < (int)filteredAddons.size(); ++i)
        {
            if (i > 0 && i % cardsPerRow != 0)
            {
                ImGui::SameLine();
            }

            DrawAddonCard(*filteredAddons[i], cardWidth);
        }

        ImGui::EndChild();
    }
}

void AddonsWindow::DrawAddonCard(const Addon& addon, float cardWidth)
{
    ImGui::PushID(addon.mMetadata.mId.c_str());

    float cardHeight = 200.0f;
    ImVec2 cardPos = ImGui::GetCursorScreenPos();

    ImGui::BeginGroup();

    // Card background (Dummy reserves space without capturing clicks)
    ImGui::Dummy(ImVec2(cardWidth, cardHeight));

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 bgColor = addon.mIsInstalled ? IM_COL32(40, 60, 40, 255) : IM_COL32(50, 50, 60, 255);
    drawList->AddRectFilled(cardPos, ImVec2(cardPos.x + cardWidth, cardPos.y + cardHeight), bgColor, 4.0f);
    drawList->AddRect(cardPos, ImVec2(cardPos.x + cardWidth, cardPos.y + cardHeight), IM_COL32(80, 80, 100, 255), 4.0f);

    // Thumbnail (square)
    ImVec2 thumbPos(cardPos.x + 5, cardPos.y + 5);
    float thumbSide = cardWidth - 10;
    ImVec2 thumbSize(thumbSide, thumbSide);
    ImTextureID thumbTex = GetAddonThumbnail(addon.mMetadata.mId);
    if (thumbTex != 0)
    {
        drawList->AddImage(thumbTex, thumbPos, ImVec2(thumbPos.x + thumbSize.x, thumbPos.y + thumbSize.y));
    }
    else
    {
        drawList->AddRectFilled(thumbPos, ImVec2(thumbPos.x + thumbSize.x, thumbPos.y + thumbSize.y), IM_COL32(70, 70, 90, 255));
    }

    // Native badge
    if (addon.mNative.mHasNative)
    {
        ImVec2 badgePos(cardPos.x + cardWidth - 55, cardPos.y + 8);
        ImU32 badgeColor = (addon.mNative.mTarget == NativeAddonTarget::EditorOnly)
            ? IM_COL32(100, 100, 200, 255)
            : IM_COL32(200, 100, 100, 255);
        drawList->AddRectFilled(badgePos, ImVec2(badgePos.x + 50, badgePos.y + 16), badgeColor, 3.0f);
        drawList->AddText(ImVec2(badgePos.x + 5, badgePos.y + 1), IM_COL32(255, 255, 255, 255), "Native");
    }

    // Name (single line, ellipsis-clamped, tooltip on hover)
    float textStartY = cardPos.y + thumbSide + 10;
    ImGui::SetCursorScreenPos(ImVec2(cardPos.x + 5, textStartY));
    DrawClampedName(addon.mMetadata.mName.c_str(), cardWidth - 10);

    // Author
    if (!addon.mMetadata.mAuthor.empty())
    {
        ImGui::SetCursorScreenPos(ImVec2(cardPos.x + 5, textStartY + 20));
        ImGui::PushTextWrapPos(cardPos.x + cardWidth - 5);
        std::string authorLine = "by " + addon.mMetadata.mAuthor;
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        DrawClampedName(authorLine.c_str(), cardWidth - 10);
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
    }

    // Buttons
    ImGui::SetCursorScreenPos(ImVec2(cardPos.x + 5, cardPos.y + cardHeight - 30));

    if (addon.mIsInstalled)
    {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Installed");

        // Check for update
        AddonManager* am = AddonManager::Get();
        if (am && am->HasUpdate(addon.mMetadata.mId))
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Update"))
            {
                OnDownloadAddon(addon.mMetadata.mId);
            }
        }
    }
    else
    {
        if (ImGui::SmallButton("Download"))
        {
            OnDownloadAddon(addon.mMetadata.mId);
        }
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Info"))
    {
        OnViewMore(addon.mMetadata.mId);
    }

    ImGui::EndGroup();
    ImGui::PopID();
}

void AddonsWindow::DrawInstalledAddons()
{
    AddonManager* am = AddonManager::Get();
    if (am == nullptr)
    {
        ImGui::TextDisabled("Addon manager not initialized.");
        return;
    }

    const std::vector<InstalledAddon>& installed = am->GetInstalledAddons();

    ImGui::Text("Installed Addons (%d)", (int)installed.size());
    ImGui::SameLine();
    DrawViewModeToggle();

    if (installed.empty())
    {
        ImGui::Separator();
        ImGui::TextDisabled("No addons installed in this project.");
        return;
    }

    ImGui::Separator();
    ImGui::Spacing();

    if (mUseTableView)
    {
        DrawAddonTable_Installed(installed);
        return;
    }

    ImGui::BeginChild("InstalledList", ImVec2(0, 0), true);

    for (const InstalledAddon& inst : installed)
    {
        ImGui::PushID(inst.mId.c_str());

        // Find full addon info
        const Addon* addon = am->FindAddon(inst.mId);
        bool hasNative = addon && addon->mNative.mHasNative;

        // Get native state if available
        NativeAddonManager* nam = NativeAddonManager::Get();
        const NativeAddonState* nativeState = nam ? nam->GetState(inst.mId) : nullptr;

        ImGui::BeginGroup();

        // Name and version
        ImGui::Text("%s", addon ? addon->mMetadata.mName.c_str() : inst.mId.c_str());

        // Native badge
        if (hasNative)
        {
            ImGui::SameLine();
            const char* targetStr = (addon->mNative.mTarget == NativeAddonTarget::EditorOnly)
                ? "[Editor Only]" : "[Engine+Editor]";
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 1.0f, 1.0f), "%s", targetStr);
        }

        ImGui::SameLine(300);
        ImGui::TextDisabled("v%s", inst.mVersion.c_str());

        // Check for update
        bool hasUpdate = addon && am->HasUpdate(inst.mId);

        ImGui::SameLine(ImGui::GetWindowWidth() - 250);

        if (hasUpdate)
        {
            if (ImGui::SmallButton("Update"))
            {
                OnDownloadAddon(inst.mId);
            }
            ImGui::SameLine();
        }

        if (ImGui::SmallButton("Uninstall"))
        {
            mUninstallAddonId = inst.mId;
            mShowUninstallConfirm = true;
        }

        // Native addon controls
        if (hasNative)
        {
            bool isBinaryMode = (inst.mNativeMode == NativeAddonResolveMode::Binary);

            // Mode-aware buttons
            if (!isBinaryMode)
            {
                // Source mode: show Build button
                ImGui::SameLine();
                if (ImGui::SmallButton("Build"))
                {
                    OnBuildNativeAddon(inst.mId);
                }
            }
            else
            {
                // Binary mode: show Sync button if remote binaries are configured
                bool hasRemoteBinaries = addon && !addon->mNative.mBinaries.empty();
                if (hasRemoteBinaries)
                {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Sync"))
                    {
                        OnSyncNativeAddonBinary(inst.mId);
                    }
                }
            }

            ImGui::SameLine();
            if (ImGui::SmallButton("Reload"))
            {
                OnReloadNativeAddon(inst.mId);
            }

            // Status indicator with mode awareness
            if (nativeState)
            {
                ImGui::SameLine();
                if (nativeState->mBuildInProgress)
                {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Building...");
                }
                else if (nam->IsLoaded(inst.mId))
                {
                    // Show load source
                    if (nativeState->mLoadedFromBinary)
                    {
                        if (nativeState->mBinaryStatus == "Synced")
                        {
                            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Loaded (Synced)");
                        }
                        else
                        {
                            ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.8f, 1.0f), "Loaded (Local Binary)");
                        }
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Loaded (Source)");
                    }
                }
                else if (isBinaryMode)
                {
                    // Binary mode not loaded
                    if (!nativeState->mBinaryStatus.empty())
                    {
                        if (nativeState->mBinaryStatus == "Missing Binary")
                        {
                            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Missing Binary");
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::SetTooltip("No precompiled binary found. Use Sync to download or switch to Source mode.");
                            }
                        }
                        else
                        {
                            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", nativeState->mBinaryStatus.c_str());
                        }
                    }
                }
                else if (nativeState->mBuildSucceeded)
                {
                    ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Built");
                }
                else if (!nativeState->mBuildError.empty())
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Build Failed");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", nativeState->mBuildError.c_str());
                    }
                }
            }
        }

        // Second row: installed date + native controls
        ImGui::TextDisabled("Installed: %s", inst.mInstalledDate.c_str());

        // Native enable checkbox and mode selector
        if (hasNative)
        {
            ImGui::SameLine(300);
            bool enableNative = inst.mEnableNative;
            if (Polyphase::Checkbox("Enable Native", &enableNative))
            {
                OnToggleNativeEnabled(inst.mId);
            }

            // Mode selector
            ImGui::SameLine();
            bool isBinaryMode = (inst.mNativeMode == NativeAddonResolveMode::Binary);
            const char* modeLabel = isBinaryMode ? "Binary" : "Source";
            ImGui::PushItemWidth(80);
            if (ImGui::BeginCombo("##Mode", modeLabel))
            {
                if (ImGui::Selectable("Source", !isBinaryMode))
                {
                    if (isBinaryMode)
                    {
                        OnToggleNativeMode(inst.mId);
                    }
                }
                if (ImGui::Selectable("Binary", isBinaryMode))
                {
                    if (!isBinaryMode)
                    {
                        OnToggleNativeMode(inst.mId);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Source: compile from code\nBinary: use precompiled DLL (no compilation)");
            }

            // Show build log button if there's a log
            if (nativeState && !nativeState->mBuildLog.empty())
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Log"))
                {
                    mShowBuildLog = true;
                    mBuildLogAddonId = inst.mId;
                }
            }

            // Show sync info if available
            if (!inst.mLastSyncAt.empty())
            {
                ImGui::SameLine();
                ImGui::TextDisabled("Last sync: %s", inst.mLastSyncAt.c_str());
                if (!inst.mLastSyncStatus.empty() && ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Status: %s\nSource: %s",
                                      inst.mLastSyncStatus.c_str(),
                                      inst.mLastSyncSource.c_str());
                }
            }
        }

        ImGui::EndGroup();

        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void AddonsWindow::DrawRepositoryManager()
{
    AddonManager* am = AddonManager::Get();
    if (am == nullptr)
    {
        ImGui::TextDisabled("Addon manager not initialized.");
        return;
    }

    // Header with Add button
    if (ImGui::Button("+ Add Repository"))
    {
        mShowAddRepoPopup = true;
        memset(mRepoUrlBuffer, 0, sizeof(mRepoUrlBuffer));
        mErrorMessage.clear();
    }

    ImGui::Separator();
    ImGui::Spacing();

    const std::vector<AddonRepository>& repos = am->GetRepositories();

    if (repos.empty())
    {
        ImGui::TextDisabled("No repositories configured.");
        return;
    }

    ImGui::BeginChild("RepoList", ImVec2(0, 0), true);

    for (const AddonRepository& repo : repos)
    {
        ImGui::PushID(repo.mUrl.c_str());

        ImGui::BeginGroup();

        ImGui::Text("%s", repo.mName.c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - 100);
        if (ImGui::SmallButton("Remove"))
        {
            OnRemoveRepository(repo.mUrl);
        }

        ImGui::TextDisabled("%s", repo.mUrl.c_str());

        if (!repo.mAddonIds.empty())
        {
            ImGui::TextDisabled("%d addon(s)", (int)repo.mAddonIds.size());
        }

        ImGui::EndGroup();

        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void AddonsWindow::DrawAddonDetailsPopup()
{
    if (!mShowAddonDetails)
    {
        return;
    }

    AddonManager* am = AddonManager::Get();
    if (am == nullptr)
    {
        mShowAddonDetails = false;
        return;
    }

    const Addon* addon = am->FindAddon(mSelectedAddonId);
    if (addon == nullptr)
    {
        mShowAddonDetails = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(450, 450));

    if (ImGui::Begin("Addon Details", &mShowAddonDetails,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("%s", addon->mMetadata.mName.c_str());
        ImGui::Separator();
        ImGui::Spacing();

        // Thumbnail (square)
        ImVec2 thumbSize(200, 200);
        ImTextureID thumbTex = GetAddonThumbnail(addon->mMetadata.mId);
        if (thumbTex != 0)
        {
            ImGui::Image(thumbTex, thumbSize);
        }
        else
        {
            ImGui::Dummy(thumbSize);
            ImVec2 thumbPos = ImGui::GetItemRectMin();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(thumbPos, ImVec2(thumbPos.x + thumbSize.x, thumbPos.y + thumbSize.y), IM_COL32(60, 60, 80, 255));
        }

        ImGui::Spacing();

        if (!addon->mMetadata.mAuthor.empty())
        {
            ImGui::Text("Author: %s", addon->mMetadata.mAuthor.c_str());
        }

        if (!addon->mMetadata.mVersion.empty())
        {
            ImGui::Text("Version: %s", addon->mMetadata.mVersion.c_str());
        }

        if (!addon->mMetadata.mUpdated.empty())
        {
            ImGui::Text("Updated: %s", addon->mMetadata.mUpdated.c_str());
        }

        ImGui::Spacing();

        if (!addon->mMetadata.mDescription.empty())
        {
            ImGui::TextWrapped("%s", addon->mMetadata.mDescription.c_str());
        }

        ImGui::Spacing();

        // Tags
        if (!addon->mMetadata.mTags.empty())
        {
            ImGui::Text("Tags:");
            ImGui::SameLine();
            for (const std::string& tag : addon->mMetadata.mTags)
            {
                ImGui::SmallButton(tag.c_str());
                ImGui::SameLine();
            }
            ImGui::NewLine();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Buttons
        if (addon->mIsInstalled)
        {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Already installed (v%s)", addon->mInstalledVersion.c_str());

            if (am->HasUpdate(addon->mMetadata.mId))
            {
                ImGui::SameLine();
                if (ImGui::Button("Update"))
                {
                    OnDownloadAddon(addon->mMetadata.mId);
                    mShowAddonDetails = false;
                }
            }
        }
        else
        {
            if (ImGui::Button("Download", ImVec2(100, 0)))
            {
                OnDownloadAddon(addon->mMetadata.mId);
                mShowAddonDetails = false;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(80, 0)))
        {
            mShowAddonDetails = false;
        }

        if (!mErrorMessage.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", mErrorMessage.c_str());
        }
    }
    ImGui::End();
}

void AddonsWindow::DrawAddRepoPopup()
{
    if (!mShowAddRepoPopup)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(450, 200));

    if (ImGui::Begin("Add Repository", &mShowAddRepoPopup,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("Repository URL:");
        ImGui::SetNextItemWidth(420);
        ImGui::InputText("##RepoUrl", mRepoUrlBuffer, sizeof(mRepoUrlBuffer));

        ImGui::Spacing();
        ImGui::TextWrapped("Enter a GitHub repository URL containing addons.");
        ImGui::TextWrapped("The repository must have a package.json at its root listing available addons.");

        if (!mErrorMessage.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", mErrorMessage.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Add", ImVec2(80, 0)))
        {
            OnAddRepository();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
        {
            mShowAddRepoPopup = false;
        }
    }
    ImGui::End();
}

void AddonsWindow::OnDownloadAddon(const std::string& addonId)
{
    AddonManager* am = AddonManager::Get();
    if (am == nullptr)
    {
        return;
    }

    const Addon* addon = am->FindAddon(addonId);
    if (addon == nullptr)
    {
        mErrorMessage = "Addon not found: " + addonId;
        return;
    }

    mStatusMessage = "Downloading " + addon->mMetadata.mName + "...";

    std::string error;
    if (am->DownloadAddon(*addon, error))
    {
        mStatusMessage = addon->mMetadata.mName + " installed successfully!";
        mErrorMessage.clear();

        // Re-discover addon packages so the new addon shows up in the Assets panel
        AssetDir* rootDir = AssetManager::Get()->GetRootDirectory();
        if (rootDir != nullptr)
        {
            AssetDir* oldPackages = AssetManager::Get()->FindPackagesDirectory();
            if (oldPackages != nullptr)
            {
                rootDir->DeleteSubdirectory("Packages");
            }

            std::string packagesDir = GetEngineState()->mProjectDirectory + "Packages/";
            AssetManager::Get()->DiscoverAddonPackages(packagesDir);

            // Update Addons tab directory
            GetEditorState()->mTabCurrentDir[(int)AssetBrowserTab::Addons] = AssetManager::Get()->FindPackagesDirectory();
        }

        ClearThumbnailCache();
    }
    else
    {
        mStatusMessage.clear();
        mErrorMessage = "Failed to install: " + error;
        LogError("Failed to install addon %s: %s", addonId.c_str(), error.c_str());
    }
}

void AddonsWindow::OnViewMore(const std::string& addonId)
{
    mSelectedAddonId = addonId;
    mShowAddonDetails = true;
    mErrorMessage.clear();
}

void AddonsWindow::OnUninstallAddon(const std::string& addonId)
{
    AddonManager* am = AddonManager::Get();
    if (am == nullptr)
    {
        return;
    }

    if (am->UninstallAddon(addonId))
    {
        mStatusMessage = "Addon uninstalled successfully";
    }
    else
    {
        mErrorMessage = "Failed to uninstall addon";
    }
}

void AddonsWindow::DrawUninstallConfirmPopup()
{
    if (!mShowUninstallConfirm)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(350, 0));

    if (ImGui::Begin("Uninstall Addon", &mShowUninstallConfirm,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::TextWrapped("Are you sure you want to uninstall \"%s\"? This will delete the addon files from the Packages folder.", mUninstallAddonId.c_str());
        ImGui::Spacing();

        if (ImGui::Button("Uninstall", ImVec2(120, 0)))
        {
            OnUninstallAddon(mUninstallAddonId);
            mShowUninstallConfirm = false;
            mUninstallAddonId.clear();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            mShowUninstallConfirm = false;
            mUninstallAddonId.clear();
        }
    }
    ImGui::End();
}

void AddonsWindow::OnAddRepository()
{
    std::string url = mRepoUrlBuffer;
    if (url.empty())
    {
        mErrorMessage = "Please enter a repository URL.";
        return;
    }

    AddonManager* am = AddonManager::Get();
    if (am == nullptr)
    {
        mErrorMessage = "Addon manager not initialized.";
        return;
    }

    am->AddRepository(url);
    mShowAddRepoPopup = false;
    mErrorMessage.clear();
    mStatusMessage = "Repository added. Refreshing...";

    // Refresh to get addon list
    am->RefreshRepository(url);
    mStatusMessage = "Repository added successfully.";
}

void AddonsWindow::OnRemoveRepository(const std::string& url)
{
    AddonManager* am = AddonManager::Get();
    if (am == nullptr)
    {
        return;
    }

    am->RemoveRepository(url);
    mStatusMessage = "Repository removed.";
}

void AddonsWindow::OnRefreshRepositories()
{
    AddonManager* am = AddonManager::Get();
    if (am == nullptr)
    {
        return;
    }

    mStatusMessage = "Refreshing repositories...";
    mIsRefreshing = true;

    am->RefreshAllRepositories();

    mStatusMessage = "Repositories refreshed.";
    mIsRefreshing = false;
}

void AddonsWindow::OnResolveDependencies()
{
    mStatusMessage = "Resolving addon dependencies...";
    std::vector<std::string> order;
    std::vector<std::string> missing;
    std::string err;
    AddonDependencyResolver::ResolveAll(order, missing, err);

    if (!err.empty())
    {
        mErrorMessage = err;
    }

    if (missing.empty())
    {
        mStatusMessage = "All dependencies satisfied.";
    }
    else
    {
        std::string msg = "Unresolved dependencies: ";
        for (size_t i = 0; i < missing.size(); ++i)
        {
            if (i > 0) msg += ", ";
            msg += missing[i];
        }
        mStatusMessage = msg;
    }
}

void AddonsWindow::OnBuildNativeAddon(const std::string& addonId)
{
    NativeAddonManager* nam = NativeAddonManager::Get();
    if (nam == nullptr)
    {
        mErrorMessage = "Native addon manager not initialized.";
        return;
    }

    mStatusMessage = "Building native addon...";

    std::string error;
    if (nam->BuildNativeAddon(addonId, error))
    {
        mStatusMessage = "Native addon built successfully!";
        mErrorMessage.clear();
    }
    else
    {
        mStatusMessage.clear();
        mErrorMessage = "Build failed: " + error;
    }
}

void AddonsWindow::OnReloadNativeAddon(const std::string& addonId)
{
    NativeAddonManager* nam = NativeAddonManager::Get();
    if (nam == nullptr)
    {
        mErrorMessage = "Native addon manager not initialized.";
        return;
    }

    // Route through the project-restart chokepoint. The user gets a confirm
    // modal + per-scene dirty prompt; the project closes, the addon rebuilds,
    // and the project reopens with all scenes restored. Direct
    // ReloadNativeAddon() is unsafe with open scenes (dangling vtables on
    // live nodes; orphaned Node factory entries → Node3D fallback on reopen).
    //
    // forceRebuild=true so the user gets a fresh source compile every time
    // they click Reload — even on resolveMode=binary addons where the
    // synced binary cache would otherwise short-circuit the build. The
    // restart path additionally sets mForceSourceForNextLoad so the
    // subsequent LoadNativeAddon() ignores binary mode for this load and
    // picks up the freshly built source DLL. Auto-sync (first install of
    // a CI-published addon) is handled separately inside LoadNativeAddon
    // when no override is set.
    std::string reason = "Reload requested for addon '" + addonId + "'";
    nam->ReloadNativeAddonsWithProjectRestart({addonId}, /*forceRebuild*/true,
                                              reason.c_str());
    mStatusMessage = "Reload staged. Confirm in the dialog to close + reopen the project.";
    mErrorMessage.clear();
}

void AddonsWindow::OnToggleNativeEnabled(const std::string& addonId)
{
    AddonManager* am = AddonManager::Get();
    if (am == nullptr)
    {
        return;
    }

    // Get mutable access to installed addons (need to add method or make this work)
    // For now, we'll reload the installed addons list
    std::vector<InstalledAddon>& installedAddons =
        const_cast<std::vector<InstalledAddon>&>(am->GetInstalledAddons());

    for (InstalledAddon& inst : installedAddons)
    {
        if (inst.mId == addonId)
        {
            inst.mEnableNative = !inst.mEnableNative;

            // Unload or load based on new state
            NativeAddonManager* nam = NativeAddonManager::Get();
            if (nam != nullptr)
            {
                if (!inst.mEnableNative)
                {
                    nam->UnloadNativeAddon(addonId);
                    mStatusMessage = "Native addon disabled and unloaded.";
                }
                else
                {
                    std::string error;
                    if (nam->LoadNativeAddon(addonId, error))
                    {
                        mStatusMessage = "Native addon enabled and loaded.";
                    }
                    else
                    {
                        mErrorMessage = "Failed to load: " + error;
                    }
                }
            }

            // Save the change
            am->SaveInstalledAddons();
            break;
        }
    }
}

void AddonsWindow::OnToggleNativeMode(const std::string& addonId)
{
    AddonManager* am = AddonManager::Get();
    if (am == nullptr)
    {
        return;
    }

    std::vector<InstalledAddon>& installedAddons = am->GetInstalledAddonsMutable();

    for (InstalledAddon& inst : installedAddons)
    {
        if (inst.mId == addonId)
        {
            // Toggle between Source and Binary
            if (inst.mNativeMode == NativeAddonResolveMode::Source)
            {
                inst.mNativeMode = NativeAddonResolveMode::Binary;
                mStatusMessage = "Switched to Binary mode. Reload to apply.";
            }
            else
            {
                inst.mNativeMode = NativeAddonResolveMode::Source;
                mStatusMessage = "Switched to Source mode. Reload to apply.";
            }

            am->SaveInstalledAddons();
            break;
        }
    }
}

void AddonsWindow::OnSyncNativeAddonBinary(const std::string& addonId)
{
    AddonManager* am = AddonManager::Get();
    if (am == nullptr)
    {
        mErrorMessage = "Addon manager not initialized.";
        return;
    }

    mStatusMessage = "Syncing binary for " + addonId + "...";

    std::string error;
    if (am->SyncNativeAddonBinary(addonId, error))
    {
        mStatusMessage = "Binary synced successfully! Reload to apply.";
        mErrorMessage.clear();
    }
    else
    {
        mStatusMessage.clear();
        mErrorMessage = "Sync failed: " + error;
    }
}

#endif
