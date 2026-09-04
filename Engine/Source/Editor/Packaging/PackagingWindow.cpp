#if EDITOR

#include "PackagingWindow.h"
#include "EditorWidgets.h"
#include "PackagingSettings.h"
#include "BuildTargetRegistry.h"
#include "EditorUIHookManager.h"
#include <algorithm>

// Active-profile pointer for the DrawProfileOptions trampolines. The C ABI
// requires captureless function pointers, so this is the only viable channel
// between the per-frame draw and the lambdas. Single-threaded by construction
// (editor draws on the main thread); not thread-safe, but doesn't need to be.
BuildProfile* sActiveProfileForOptionsTrampoline = nullptr;
#include "Preferences/PreferencesWindow.h"
#include "Preferences/PreferencesManager.h"
#include "Preferences/External/LaunchersModule.h"
#include "Preferences/Packaging/DockerModule.h"
#include "Preferences/External/ExternalModule.h"
#include "ActionManager.h"

#include "Engine.h"
#include "Log.h"
#include "System/System.h"
#include "Utilities.h"

#include "imgui.h"

#include <cstring>
#include <cstdio>
#include <thread>
#include <mutex>
#include <unordered_map>

#if PLATFORM_LINUX
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#elif PLATFORM_WINDOWS
#include <Windows.h>
#endif

static PackagingWindow sPackagingWindow;

PackagingWindow* GetPackagingWindow()
{
    return &sPackagingWindow;
}

// Cache for BuildTarget Validate() results. Without this, the target-combo
// tooltip code calls addon Validate every frame the cursor hovers a Selectable.
// PSP's Validate shells out via `wsl bash -lc` (std::system), so a hover on
// the PSP entry was freezing the editor for the duration of every WSL spawn —
// up to 45s on a cold WSL, and indefinitely if WSL had hung.
//
// Validation now runs on a detached background thread; the main thread reads
// the cached state under a mutex and shows "Checking..." while pending.
namespace
{
    struct ValidateCacheEntry
    {
        enum State { kIdle, kPending, kOk, kFailed };
        State state = kIdle;
        std::string reason;
        double lastCheckedTime = 0.0;
    };
}
static std::unordered_map<std::string, ValidateCacheEntry> sValidateCache;
static std::mutex sValidateCacheMutex;
// Re-run Validate at most this often per target. Toolchains don't appear and
// disappear in real time — a generous TTL keeps the tooltip responsive without
// papering over a real env change forever.
constexpr double kValidateTTLSeconds = 60.0;

namespace
{
    // Push to both clipboards. ImGui::SetClipboardText alone leaves the system
    // clipboard empty on Linux because no SetClipboardTextFn bridge is wired
    // up. Mirrors the helper in Editor/CliTerminal/TerminalPanel.cpp.
    void CopyOutputToClipboard(const std::string& text)
    {
        SYS_SetClipboardText(text);
        ImGui::SetClipboardText(text.c_str());
    }

    // In-process clipboard for "Copy Values" / "Paste Values" on build
    // profiles. Holds a snapshot of the value-bearing fields only —
    // mId and mName are intentionally excluded so paste targets keep
    // their own identity.
    static BuildProfile sProfileValueClipboard;
    static bool sProfileValueClipboardValid = false;

    void CopyProfileValues(const BuildProfile& src)
    {
        sProfileValueClipboard.mTargetPlatform        = src.mTargetPlatform;
        sProfileValueClipboard.mEmbedded              = src.mEmbedded;
        sProfileValueClipboard.mStaticContent         = src.mStaticContent;
        sProfileValueClipboard.mContentPak            = src.mContentPak;
        sProfileValueClipboard.mOutputDirectory       = src.mOutputDirectory;
        sProfileValueClipboard.mUseDocker             = src.mUseDocker;
        sProfileValueClipboard.mOpenDirectoryOnFinish = src.mOpenDirectoryOnFinish;
        sProfileValueClipboardValid = true;
    }

    void PasteProfileValues(BuildProfile& dst)
    {
        if (!sProfileValueClipboardValid)
            return;
        dst.mTargetPlatform        = sProfileValueClipboard.mTargetPlatform;
        dst.mEmbedded              = sProfileValueClipboard.mEmbedded;
        dst.mStaticContent         = sProfileValueClipboard.mStaticContent;
        dst.mContentPak            = sProfileValueClipboard.mContentPak;
        dst.mOutputDirectory       = sProfileValueClipboard.mOutputDirectory;
        dst.mUseDocker             = sProfileValueClipboard.mUseDocker;
        dst.mOpenDirectoryOnFinish = sProfileValueClipboard.mOpenDirectoryOnFinish;
    }
}

PackagingWindow::PackagingWindow()
{
}

PackagingWindow::~PackagingWindow()
{
    // Clean up build thread if still running
    if (mBuildState.mRunning.load())
    {
        CancelDockerBuild();
    }
    if (mBuildState.mBuildThread.joinable())
    {
        mBuildState.mBuildThread.join();
    }
}

void PackagingWindow::Open()
{
    mIsOpen = true;
    mShowDockerWarning = false;
    mShow3dsLinkWarning = false;
    mShowWiiloadWarning = false;

    // Initialize buffers with selected profile data
    PackagingSettings* settings = PackagingSettings::Get();
    if (settings != nullptr)
    {
        BuildProfile* profile = settings->GetSelectedProfile();
        if (profile != nullptr)
        {
            strncpy(mNameBuffer, profile->mName.c_str(), sizeof(mNameBuffer) - 1);
            mNameBuffer[sizeof(mNameBuffer) - 1] = '\0';

            strncpy(mOutputDirBuffer, profile->mOutputDirectory.c_str(), sizeof(mOutputDirBuffer) - 1);
            mOutputDirBuffer[sizeof(mOutputDirBuffer) - 1] = '\0';
        }
    }
}

void PackagingWindow::Close()
{
    mIsOpen = false;
}

void PackagingWindow::Draw()
{
    // The queue runner has to tick every frame, even when the window is
    // closed, so an in-flight Build All survives a window close + reopen.
    TickBuildAllQueue();

    if (!mIsOpen)
    {
        return;
    }

    PackagingSettings* settings = PackagingSettings::Get();
    if (settings == nullptr)
    {
        return;
    }

    // Center the window
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize(700.0f, 500.0f);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) * 0.5f, (io.DisplaySize.y - windowSize.y) * 0.5f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Packaging", &mIsOpen, windowFlags))
    {
        // Calculate layout dimensions
        float listWidth = 180.0f;
        float buttonHeight = 40.0f;
        float contentHeight = ImGui::GetContentRegionAvail().y - buttonHeight - 16.0f;
        float settingsWidth = ImGui::GetContentRegionAvail().x - listWidth - 8.0f;

        // Left panel - Profile list
        ImGui::BeginChild("ProfileList", ImVec2(listWidth, contentHeight), true);
        DrawProfileList();
        ImGui::EndChild();

        ImGui::SameLine();

        // Right panel - Profile settings
        ImGui::BeginChild("ProfileSettings", ImVec2(settingsWidth, contentHeight), true);
        DrawProfileSettings();
        ImGui::EndChild();

        // Bottom - Build buttons
        ImGui::Spacing();
        DrawBuildButtons();
    }
    ImGui::End();

    // Draw popups
    DrawDockerWarningPopup();
    Draw3dsLinkWarningPopup();
    DrawWiiloadWarningPopup();
    DrawBuildOutputModal();

    // Handle window close
    if (!mIsOpen)
    {
        Close();
    }
}

void PackagingWindow::DrawProfileList()
{
    PackagingSettings* settings = PackagingSettings::Get();
    if (settings == nullptr)
    {
        return;
    }

    // Add/Remove buttons
    if (ImGui::Button("+", ImVec2(24, 0)))
    {
        settings->CreateProfile("New Profile");
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Add new profile");
    }

    ImGui::SameLine();

    BuildProfile* selectedProfile = settings->GetSelectedProfile();
    bool canDelete = selectedProfile != nullptr && settings->GetProfiles().size() > 1;

    if (!canDelete)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("-", ImVec2(24, 0)))
    {
        if (selectedProfile != nullptr)
        {
            settings->DeleteProfile(selectedProfile->mId);
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Delete selected profile");
    }

    if (!canDelete)
    {
        ImGui::EndDisabled();
    }

    ImGui::Separator();

    // Profile list
    std::vector<BuildProfile>& profiles = settings->GetProfiles();
    int32_t selectedIndex = settings->GetSelectedProfileIndex();

    uint32_t currentTargetId = settings->GetCurrentTargetProfileId();

    for (int32_t i = 0; i < static_cast<int32_t>(profiles.size()); ++i)
    {
        BuildProfile& profile = profiles[i];
        bool isSelected = (i == selectedIndex);
        bool isCurrentTarget = (profile.mId == currentTargetId);

        // Use profile ID for unique ImGui ID to prevent crash with empty names
        ImGui::PushID(static_cast<int>(profile.mId));

        // Build display name with star prefix for current target
        char displayBuffer[512];
        const char* name = profile.mName.empty() ? " " : profile.mName.c_str();
        if (isCurrentTarget)
        {
            snprintf(displayBuffer, sizeof(displayBuffer), "* %s", name);
        }
        else
        {
            snprintf(displayBuffer, sizeof(displayBuffer), "  %s", name);
        }

        if (ImGui::Selectable(displayBuffer, isSelected))
        {
            settings->SetSelectedProfileIndex(i);

            // Update buffers for editing
            strncpy(mNameBuffer, profile.mName.c_str(), sizeof(mNameBuffer) - 1);
            mNameBuffer[sizeof(mNameBuffer) - 1] = '\0';

            strncpy(mOutputDirBuffer, profile.mOutputDirectory.c_str(), sizeof(mOutputDirBuffer) - 1);
            mOutputDirBuffer[sizeof(mOutputDirBuffer) - 1] = '\0';
        }

        // Right-click context menu
        // The PushID above makes "ProfileContextMenu" unique per profile
        ImGui::OpenPopupOnItemClick("ProfileContextMenu", ImGuiPopupFlags_MouseButtonRight);
        if (ImGui::BeginPopup("ProfileContextMenu"))
        {
            if (isCurrentTarget)
            {
                if (ImGui::MenuItem("Clear as Target"))
                {
                    settings->SetCurrentTargetProfileId(0);
                }
            }
            else
            {
                if (ImGui::MenuItem("Set as Target"))
                {
                    settings->SetCurrentTargetProfileId(profile.mId);
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Duplicate"))
            {
                // Snapshot source values BEFORE CreateProfile, since the
                // underlying push_back can reallocate the profiles vector
                // and invalidate `profile`.
                BuildProfile srcSnapshot = profile;
                std::string newName = srcSnapshot.mName + " Copy";
                BuildProfile* newProfile = settings->CreateProfile(newName);
                if (newProfile != nullptr)
                {
                    uint32_t newId = newProfile->mId;
                    newProfile->mTargetPlatform        = srcSnapshot.mTargetPlatform;
                    newProfile->mEmbedded              = srcSnapshot.mEmbedded;
                    newProfile->mStaticContent         = srcSnapshot.mStaticContent;
                    newProfile->mContentPak            = srcSnapshot.mContentPak;
                    newProfile->mOutputDirectory       = srcSnapshot.mOutputDirectory;
                    newProfile->mUseDocker             = srcSnapshot.mUseDocker;
                    newProfile->mOpenDirectoryOnFinish = srcSnapshot.mOpenDirectoryOnFinish;
                    settings->SaveSettings();

                    // Select the duplicate so the user can immediately rename
                    // or tweak it. Match by id since CreateProfile may have
                    // reallocated the profiles vector.
                    std::vector<BuildProfile>& list = settings->GetProfiles();
                    for (int32_t k = 0; k < (int32_t)list.size(); ++k)
                    {
                        if (list[k].mId == newId)
                        {
                            settings->SetSelectedProfileIndex(k);
                            strncpy(mNameBuffer, list[k].mName.c_str(), sizeof(mNameBuffer) - 1);
                            mNameBuffer[sizeof(mNameBuffer) - 1] = '\0';
                            strncpy(mOutputDirBuffer, list[k].mOutputDirectory.c_str(), sizeof(mOutputDirBuffer) - 1);
                            mOutputDirBuffer[sizeof(mOutputDirBuffer) - 1] = '\0';
                            break;
                        }
                    }
                }
            }

            if (ImGui::MenuItem("Copy Values"))
            {
                CopyProfileValues(profile);
            }

            if (ImGui::MenuItem("Paste Values", nullptr, false, sProfileValueClipboardValid))
            {
                PasteProfileValues(profile);
                settings->SaveSettings();

                // Refresh the edit buffers if we just pasted into the
                // currently-selected profile.
                if (isSelected)
                {
                    strncpy(mOutputDirBuffer, profile.mOutputDirectory.c_str(), sizeof(mOutputDirBuffer) - 1);
                    mOutputDirBuffer[sizeof(mOutputDirBuffer) - 1] = '\0';
                }
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
}

void PackagingWindow::DrawProfileSettings()
{
    PackagingSettings* settings = PackagingSettings::Get();
    if (settings == nullptr)
    {
        return;
    }

    BuildProfile* profile = settings->GetSelectedProfile();
    if (profile == nullptr)
    {
        ImGui::TextDisabled("Select a profile from the list.");
        return;
    }

    ImGui::Text("Profile Settings");
    ImGui::Separator();
    ImGui::Spacing();

    bool changed = false;

    // Name
    ImGui::Text("Name:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##Name", mNameBuffer, sizeof(mNameBuffer)))
    {
        profile->mName = mNameBuffer;
        changed = true;
    }

    ImGui::Spacing();

    // Build Target (registry-driven; built-ins + addon-provided)
    ImGui::Text("Target:");

    // Resolve the current selection: prefer mTargetId, fall back to
    // mTargetPlatform for profiles saved before mTargetId existed.
    EditorUIHookManager* uiHookMgr = EditorUIHookManager::Get();
    const std::vector<RegisteredBuildTarget>& allTargets =
        (uiHookMgr != nullptr) ? uiHookMgr->GetBuildTargets().GetAll()
                               : std::vector<RegisteredBuildTarget>();

    std::string activeId = profile->mTargetId;
    if (activeId.empty() && uiHookMgr != nullptr)
    {
        const RegisteredBuildTarget* fallback =
            uiHookMgr->GetBuildTargets().FindBuiltInByPlatform(profile->mTargetPlatform);
        if (fallback != nullptr) activeId = fallback->mTargetId;
    }

    const RegisteredBuildTarget* activeTarget = nullptr;
    for (const RegisteredBuildTarget& t : allTargets)
    {
        if (t.mTargetId == activeId) { activeTarget = &t; break; }
    }

    std::string comboLabel = activeTarget ? activeTarget->mDisplayName : "(unknown target)";
    if (activeTarget == nullptr && !activeId.empty())
    {
        comboLabel = "(unavailable: " + activeId + ")";
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##Target", comboLabel.c_str()))
    {
        // Group entries by category for readability. Empty category is "Other".
        std::vector<std::string> categoryOrder;
        for (const RegisteredBuildTarget& t : allTargets)
        {
            std::string cat = t.mCategory.empty() ? std::string("Other") : t.mCategory;
            if (std::find(categoryOrder.begin(), categoryOrder.end(), cat) == categoryOrder.end())
                categoryOrder.push_back(cat);
        }

        for (const std::string& cat : categoryOrder)
        {
            ImGui::TextDisabled("%s", cat.c_str());
            ImGui::Separator();
            for (const RegisteredBuildTarget& t : allTargets)
            {
                std::string tcat = t.mCategory.empty() ? std::string("Other") : t.mCategory;
                if (tcat != cat) continue;

                bool selected = (t.mTargetId == activeId);
                std::string label = t.mDisplayName;
                if (!t.mIsBuiltIn) label += "  [addon]";

                if (ImGui::Selectable(label.c_str(), selected))
                {
                    profile->mTargetId = t.mTargetId;
                    profile->mTargetPlatform = static_cast<Platform>(t.mDesc.basePlatform);
                    changed = true;
                }
                if (ImGui::IsItemHovered())
                {
                    // Snapshot cache state under the lock and decide whether to
                    // kick off a background revalidation. Keep the lock window
                    // tiny — the validate call itself runs outside the lock,
                    // on a detached thread, so a slow/hung addon can never
                    // block the editor's main thread.
                    auto validateFn = t.mDesc.Validate;
                    const std::string targetId = t.mTargetId;
                    const double now = ImGui::GetTime();

                    ValidateCacheEntry snapshot;
                    bool kickOff = false;
                    {
                        std::lock_guard<std::mutex> lock(sValidateCacheMutex);
                        auto& entry = sValidateCache[targetId];
                        const bool resolved = (entry.state == ValidateCacheEntry::kOk ||
                                               entry.state == ValidateCacheEntry::kFailed);
                        const bool stale = (entry.state == ValidateCacheEntry::kIdle) ||
                                           (resolved && (now - entry.lastCheckedTime) > kValidateTTLSeconds);
                        if (stale && validateFn != nullptr)
                        {
                            entry.state = ValidateCacheEntry::kPending;
                            entry.lastCheckedTime = now;
                            kickOff = true;
                        }
                        snapshot = entry;
                    }

                    if (kickOff)
                    {
                        // Background thread does the actual Validate call (which
                        // may block on a subprocess for many seconds) and writes
                        // the result back under the cache mutex. lastCheckedTime
                        // was already stamped on kickoff above so the TTL clock
                        // starts now, not when the call eventually returns.
                        // Don't touch ImGui from this thread — ImGui APIs are
                        // main-thread-only.
                        std::thread([targetId, validateFn]() {
                            char reason[256] = {0};
                            const int32_t ok = validateFn(reason, sizeof(reason));
                            std::lock_guard<std::mutex> lock(sValidateCacheMutex);
                            auto& e = sValidateCache[targetId];
                            e.state = (ok != 0) ? ValidateCacheEntry::kOk
                                                : ValidateCacheEntry::kFailed;
                            e.reason = reason;
                        }).detach();
                    }

                    if (snapshot.state == ValidateCacheEntry::kPending)
                    {
                        ImGui::SetTooltip("Checking toolchain availability...");
                    }
                    else if (snapshot.state == ValidateCacheEntry::kFailed)
                    {
                        ImGui::SetTooltip("UNAVAILABLE: %s",
                            snapshot.reason.empty() ? "toolchain not found" : snapshot.reason.c_str());
                    }
                    else if (!t.mIsBuiltIn)
                    {
                        ImGui::SetTooltip("Addon target — id: %s", t.mTargetId.c_str());
                    }
                }
            }
            ImGui::Spacing();
        }
        ImGui::EndCombo();
    }

    // Show a warning row if the active id no longer resolves (addon uninstalled).
    if (activeTarget == nullptr && !activeId.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                           "Target '%s' is not registered — addon may be missing or disabled.",
                           activeId.c_str());
    }

    // Per-target options panel — addons that registered DrawProfileOptions
    // draw their settings inside a collapsing header so the engine doesn't
    // need to know anything about target-specific config. Addons interact
    // with the profile entirely through the context trampolines — they never
    // touch BuildProfile directly, so the ABI stays a pure C surface.
    if (activeTarget != nullptr && activeTarget->mDesc.DrawProfileOptions != nullptr)
    {
        if (ImGui::CollapsingHeader("Target Options"))
        {
            // Build a minimal context wired to this profile. opaqueEngineState
            // carries the BuildProfile* so the trampolines below resolve it.
            PolyphaseBuildContext drawCtx{};
            drawCtx.structVersion    = POLYPHASE_BUILD_TARGET_API_VERSION;
            drawCtx.targetId         = activeTarget->mDesc.targetId;
            drawCtx.basePlatform     = activeTarget->mDesc.basePlatform;
            drawCtx.opaqueEngineState = static_cast<void*>(profile);

            drawCtx.GetProfileSetting = [](const char* key, char* outVal, size_t cap) -> int32_t {
                // The lambda has no captures because the C ABI doesn't allow them;
                // we rely on the active-frame thread-local set immediately below.
                extern BuildProfile* sActiveProfileForOptionsTrampoline;
                BuildProfile* p = sActiveProfileForOptionsTrampoline;
                if (p == nullptr || key == nullptr || outVal == nullptr || cap == 0) return 0;
                auto it = p->mTargetOptions.find(key);
                if (it == p->mTargetOptions.end()) { outVal[0] = '\0'; return 0; }
                std::snprintf(outVal, cap, "%s", it->second.c_str());
                return 1;
            };
            drawCtx.SetProfileSetting = [](const char* key, const char* val) {
                extern BuildProfile* sActiveProfileForOptionsTrampoline;
                BuildProfile* p = sActiveProfileForOptionsTrampoline;
                if (p == nullptr || key == nullptr) return;
                p->mTargetOptions[key] = val ? val : "";
            };

            sActiveProfileForOptionsTrampoline = profile;
            activeTarget->mDesc.DrawProfileOptions(&drawCtx);
            sActiveProfileForOptionsTrampoline = nullptr;

            // Trampoline edits dirty the profile; flag for save.
            changed = true;
        }
    }

    // Built-in Android target options. Built-in targets don't ship a
    // DrawProfileOptions callback (that's for addon-provided targets),
    // so we render Android's customisable fields inline. Storage piggybacks
    // on the existing mTargetOptions map — same JSON serialisation, same
    // per-profile lifetime, no schema bump needed.
    if (profile->mTargetPlatform == Platform::Android)
    {
        if (ImGui::CollapsingHeader("Android Target Options", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& opts = profile->mTargetOptions;

            // applicationId — what the Play Store and `adb` see. Defaults to
            // the stock placeholder `com.you.appname` so first-time users
            // build/install/launch without touching this field; serious
            // projects MUST override before any real distribution.
            {
                std::string current = opts.count("android.applicationId")
                    ? opts["android.applicationId"]
                    : std::string("com.you.appname");
                char buf[256];
                std::snprintf(buf, sizeof(buf), "%s", current.c_str());
                if (ImGui::InputText("Application ID##Android", buf, sizeof(buf)))
                {
                    opts["android.applicationId"] = buf;
                    changed = true;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Java-style package id used as the APK's applicationId\n"
                                      "+ namespace in build.gradle. Default:\n"
                                      "  com.you.appname  (an obvious placeholder)\n"
                                      "Override to e.g. com.yourstudio.yourgame before\n"
                                      "shipping. The default will install fine for testing\n"
                                      "but Play Store rejects `com.you.*` reserved ids.");
                }
            }

            // App label — what shows under the launcher icon on the home
            // screen. Empty means "use EngineConfig::mProjectName" so the
            // App Settings window's Project Name field is the single source
            // of truth across desktop window title + Android launcher label.
            {
                std::string current = opts.count("android.appLabel") ? opts["android.appLabel"] : "";
                char buf[256];
                std::snprintf(buf, sizeof(buf), "%s", current.c_str());
                if (ImGui::InputText("App Label##Android", buf, sizeof(buf)))
                {
                    opts["android.appLabel"] = buf;
                    changed = true;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Display name under the launcher icon. Leave blank\n"
                                      "to inherit the Project Name from App Settings\n"
                                      "(Config.ini's `Project=`).");
                }
            }

            // Icon override — empty falls back to EngineConfig::mIconPath
            // (the project-wide icon set via App Settings → Application Icon).
            // Override only when shipping a separate Android-specific icon.
            {
                std::string current = opts.count("android.iconSource") ? opts["android.iconSource"] : "";
                char buf[512];
                std::snprintf(buf, sizeof(buf), "%s", current.c_str());
                if (ImGui::InputText("Icon Source##Android", buf, sizeof(buf)))
                {
                    opts["android.iconSource"] = buf;
                    changed = true;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("PNG to use as the launcher icon. Project-relative or\n"
                                      "absolute. Leave blank to inherit the project icon set\n"
                                      "via App Settings → Application Icon → Browse.\n"
                                      "Packager resizes to all mipmap-* densities at build time.");
                }
            }
        }
    }

    // Built-in 3DS target options — SMDH metadata shared by the plain .3dsx
    // target and the CIA packager (which adds its own fields via
    // DrawProfileOptions above). Keys are read in ActionManager when it
    // builds the make command line.
    if (profile->mTargetPlatform == Platform::N3DS)
    {
        if (ImGui::CollapsingHeader("3DS Target Options", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& opts = profile->mTargetOptions;

            struct Field { const char* label; const char* key; const char* tooltip; };
            const Field fields[] = {
                { "Title##N3DS", "n3ds.title",
                  "Name shown in the Homebrew Launcher / HOME Menu (SMDH).\n"
                  "Leave blank to use the project name." },
                { "Description##N3DS", "n3ds.description",
                  "One-line description stored in the SMDH." },
                { "Author##N3DS", "n3ds.author",
                  "Author / publisher stored in the SMDH." },
                { "Icon##N3DS", "n3ds.iconPath",
                  "Image file (PNG/JPG/BMP/TGA; project-relative or absolute) or an\n"
                  "imported Texture asset name, resized to the 48x48 SMDH icon.\n"
                  "Leave blank to use the project icon from App Settings when it is\n"
                  "an image file, otherwise libctru's default icon." },
            };

            for (const Field& field : fields)
            {
                std::string current = opts.count(field.key) ? opts[field.key] : "";
                char buf[512];
                std::snprintf(buf, sizeof(buf), "%s", current.c_str());
                if (ImGui::InputText(field.label, buf, sizeof(buf)))
                {
                    opts[field.key] = buf;
                    changed = true;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", field.tooltip);
                }
            }
        }
    }

    ImGui::Spacing();

    // Embedded mode
    if (Polyphase::Checkbox("Embedded Mode", &profile->mEmbedded))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Embed assets into the executable");
    }

    ImGui::Spacing();

    // Static content (obfuscation). Independent of Embedded Mode -- when both
    // are on, the generated byte arrays are obfuscated too.
    if (Polyphase::Checkbox("Static Content", &profile->mStaticContent))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Ship cooked assets (.oct), scripts (.lua) and the asset registry\n"
                          "in an obfuscated format, decoded on demand at runtime.\n"
                          "Works on every platform including GameCube, Wii, 3DS and PSP.\n\n"
                          "This is protection/obfuscation, NOT DRM -- the key ships inside\n"
                          "the game. It stops casual editing and asset ripping, not a\n"
                          "determined attacker with a disassembler.");
    }

    // Content Pak builds on top of Static -- there is no point packing content
    // that isn't obfuscated, and the pak index reuses the same container.
    //
    // With Embedded on, the pak's only remaining job is delivering the Vulkan
    // shaders, which are the one thing embedding doesn't cover. Console backends
    // compile their shaders in, so there Embedded is already a single
    // deliverable and a pak beside it would carry nothing -- hide the checkbox
    // rather than offer one that does nothing. Build-target addons whose
    // embedded build is likewise self-contained can opt in via
    // POLYPHASE_OPT_HIDE_CONTENT_PAK.
    // basePlatform is a plain int across the addon C ABI.
    const Platform basePlatform =
        (activeTarget != nullptr) ? (Platform)activeTarget->mDesc.basePlatform
                                  : profile->mTargetPlatform;

    const auto hideOptIt = profile->mTargetOptions.find(POLYPHASE_OPT_HIDE_CONTENT_PAK);
    const bool addonHidesPak =
        (hideOptIt != profile->mTargetOptions.end() && hideOptIt->second == "1");

    const bool pakRedundant =
        profile->mEmbedded && (!PlatformUsesShaderFiles(basePlatform) || addonHidesPak);

    if (profile->mStaticContent && pakRedundant && profile->mContentPak)
    {
        // Don't let a value saved while the checkbox was visible keep applying
        // silently once it's hidden.
        profile->mContentPak = false;
        changed = true;
    }

    if (profile->mStaticContent && !pakRedundant)
    {
        ImGui::Indent();
        if (Polyphase::Checkbox("Content Pak", &profile->mContentPak))
        {
            changed = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Pack assets, scripts and the asset registry into a single\n"
                              "obfuscated Content.pak and remove the loose copies.\n\n"
                              "Hides filenames and the folder tree as well as file contents.\n"
                              "Only the index is held in memory; entries are decoded on demand,\n"
                              "so console memory behaviour is unchanged.\n\n"
                              "Raw assets (.png/.json/.mp4) stay loose -- addon code opens\n"
                              "those with its own file I/O.");
        }
        ImGui::Unindent();
    }

    ImGui::Spacing();

    // Output directory
    ImGui::Text("Output Directory:");
    float browseWidth = 70.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float inputWidth = ImGui::GetContentRegionAvail().x - browseWidth - spacing;

    ImGui::SetNextItemWidth(inputWidth);
    if (ImGui::InputText("##OutputDir", mOutputDirBuffer, sizeof(mOutputDirBuffer)))
    {
        profile->mOutputDirectory = mOutputDirBuffer;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse...", ImVec2(browseWidth, 0)))
    {
        std::string folder = SYS_SelectFolderDialog();
        if (!folder.empty())
        {
            strncpy(mOutputDirBuffer, folder.c_str(), sizeof(mOutputDirBuffer) - 1);
            mOutputDirBuffer[sizeof(mOutputDirBuffer) - 1] = '\0';
            profile->mOutputDirectory = folder;
            changed = true;
        }
    }
    ImGui::TextDisabled("Leave empty for default: Packaged/{Platform}/");

    ImGui::Spacing();

    // Use Docker checkbox (optional on all platforms — Windows builds GCN/Wii/3DS natively)
    if (Polyphase::Checkbox("Use Docker", &profile->mUseDocker))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Use Docker for building instead of local tools");
    }

    ImGui::Spacing();

    // Open directory on finish
    if (Polyphase::Checkbox("Open Directory On Finish", &profile->mOpenDirectoryOnFinish))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Open the output directory when build completes");
    }

    if (changed)
    {
        settings->SaveSettings();
    }
}

void PackagingWindow::DrawBuildButtons()
{
    PackagingSettings* settings = PackagingSettings::Get();
    BuildProfile* profile = settings ? settings->GetSelectedProfile() : nullptr;

    bool canBuild = (profile != nullptr) && !mBuildInProgress && !mBuildAllRunning;
    bool canBuildAll = !mBuildInProgress && !mBuildAllRunning && !IsAnyBuildInProgress() &&
                       PackagingSettings::Get() != nullptr &&
                       !PackagingSettings::Get()->GetProfiles().empty();

    // Force Rebuild checkbox. Locked while Build All is running — the queue
    // force-enables this for every iteration and restores the user's prior
    // value when the queue ends.
    if (mBuildAllRunning)
    {
        ImGui::BeginDisabled();
    }
    Polyphase::Checkbox("Force Rebuild", &mForceRebuild);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip(mBuildAllRunning
            ? "Locked during Build All (every profile rebuilds from scratch)."
            : "Rebuild even if no files have changed");
    }
    if (mBuildAllRunning)
    {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    if (!canBuild)
    {
        ImGui::BeginDisabled();
    }

    float buttonWidth = 100.0f;
    float dropdownButtonWidth = 130.0f;
    float deviceButtonWidth = 150.0f;
    float arrowWidth = 20.0f;
    float gearWidth = 30.0f;

    if (ImGui::Button("Build", ImVec2(buttonWidth, 0)))
    {
        OnBuild();
    }

    // "Build All" sits next to "Build". It builds every profile in display
    // order and stops on the first failure. Gated independently of canBuild
    // — needs at least one profile to exist, and no other build in flight.
    if (!canBuild)
    {
        ImGui::EndDisabled();
    }
    if (!canBuildAll)
    {
        ImGui::BeginDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Build All", ImVec2(buttonWidth, 0)))
    {
        OnBuildAll();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("Build every profile sequentially; stops on first failure.");
    }
    if (!canBuildAll)
    {
        ImGui::EndDisabled();
    }
    if (!canBuild)
    {
        ImGui::BeginDisabled();
    }

    ImGui::SameLine();

    bool supportsRun = profile && PlatformSupportsRun(profile->mTargetPlatform);
    bool is3DS = profile && profile->mTargetPlatform == Platform::N3DS;
    bool isWii = profile && profile->mTargetPlatform == Platform::Wii;
    bool isAndroid = profile && profile->mTargetPlatform == Platform::Android;

    // "Set As Target" applies to every platform — keep it outside the
    // supportsRun-gated disabled block below (which exists for Build & Run,
    // not for the target-profile bookkeeping).
    ImGui::SameLine();
    {
        bool isCurrentTarget = profile && (profile->mId == settings->GetCurrentTargetProfileId());
        const char* targetLabel = isCurrentTarget ? "Clear As Target" : "Set As Target";
        if (ImGui::Button(targetLabel, ImVec2(dropdownButtonWidth, 0)))
        {
            settings->SetCurrentTargetProfileId(isCurrentTarget ? 0 : profile->mId);
        }
    }

    if (!supportsRun)
    {
        ImGui::BeginDisabled();
    }

    // For Wii: Show dropdown with Dolphin and Wii LAN options
    if (isWii)
    {
        // Main button

        ImGui::SameLine();

        if (ImGui::Button("Build & Run", ImVec2(dropdownButtonWidth, 0)))
        {
            ImGui::OpenPopup("WiiBuildRunMenu");
        }

        // Dropdown arrow
        ImGui::SameLine(0, 0);
        if (ImGui::ArrowButton("##WiiRunArrow", ImGuiDir_Down))
        {
            ImGui::OpenPopup("WiiBuildRunMenu");
        }

        // Popup menu
        if (ImGui::BeginPopup("WiiBuildRunMenu"))
        {
            if (ImGui::MenuItem("Dolphin", nullptr, false))
            {
                OnBuildAndRun();
            }
            if (ImGui::MenuItem("Wii LAN", nullptr, false))
            {
                OnBuildAndRunOnDevice();
            }
            ImGui::EndPopup();
        }
    }
    else if (isAndroid)
    {
        // Android has no integrated emulator path (BlueStacks etc. lack
        // Vulkan), so the plain "Build & Run" is hidden — the "Build & Run
        // On Device" button below is the only way and runs adb install +
        // shell am start + optional logcat.
    }
    else
    {
        // Standard Build & Run button for other platforms
        ImGui::SameLine();
        if (ImGui::Button("Build & Run", ImVec2(buttonWidth, 0)))
        {
            OnBuildAndRun();
        }
    }

    if (!supportsRun)
    {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("Build & Run is only available for GameCube, Wii, 3DS, and Android");
        }
    }

    // "Build & Run On Device" — for platforms that have a real-hardware path
    // distinct from emulator. 3DS via 3dslink, Android via adb. (Wii uses the
    // dropdown above so its "Wii LAN" option lives there alongside Dolphin.)
    if (is3DS)
    {
        ImGui::SameLine();

        if (ImGui::Button("Build & Run On Device", ImVec2(deviceButtonWidth, 0)))
        {
            OnBuildAndRunOnDevice();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Build and send to 3DS hardware via 3dslink");
        }
    }
    else if (isAndroid)
    {
        ImGui::SameLine();

        if (ImGui::Button("Build & Run On Device", ImVec2(deviceButtonWidth, 0)))
        {
            OnBuildAndRunOnDevice();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Build the APK, `adb install`, `adb shell am start`,\n"
                              "and (if enabled in Preferences) pop a detached\n"
                              "logcat window. Configure ADB path + device serial\n"
                              "via the gear icon (Preferences > External > Launchers).");
        }
    }

    ImGui::SameLine();

    // Gear icon for launcher settings
    if (ImGui::Button("...", ImVec2(gearWidth, 0)))
    {
        OpenLauncherSettings();
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Configure emulator paths");
    }

    if (!canBuild)
    {
        ImGui::EndDisabled();
    }

    if (mBuildInProgress)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Building...");
    }

    // Build All queue status row. Shown only while the queue is active or
    // there is a stored failure to surface; idle state draws nothing extra.
    if (mBuildAllRunning)
    {
        std::vector<BuildProfile>& profiles = PackagingSettings::Get()->GetProfiles();
        int total = (int)profiles.size();
        int current = mBuildAllIndex + 1;
        if (current < 1) current = 1;
        if (current > total) current = total;
        const char* name = (mBuildAllIndex >= 0 && mBuildAllIndex < total)
            ? profiles[mBuildAllIndex].mName.c_str()
            : "(starting)";
        ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.40f, 1.0f),
                           "Build All: building %d of %d: %s", current, total, name);
        ImGui::SameLine();
        if (ImGui::Button("Cancel All"))
        {
            CancelBuildAll();
        }
    }
    else if (mBuildAllFailedIndex >= 0)
    {
        ImGui::TextColored(ImVec4(0.95f, 0.40f, 0.40f, 1.0f),
                           "Build All stopped: '%s' failed.", mBuildAllFailedName.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Dismiss"))
        {
            mBuildAllFailedIndex = -1;
            mBuildAllFailedName.clear();
        }
    }
}

void PackagingWindow::DrawDockerWarningPopup()
{
    if (mShowDockerWarning)
    {
        ImGui::OpenPopup("Docker Required");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Docker Required", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Docker is not running or not installed.");
        ImGui::Spacing();

#if PLATFORM_WINDOWS
        ImGui::TextWrapped("Please ensure Docker Desktop is installed and running to build for this platform.");
#else
        ImGui::TextWrapped("Please install Docker and ensure the daemon is running.");
#endif

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = 80.0f;
        float windowWidth = ImGui::GetWindowSize().x;
        ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);

        if (ImGui::Button("OK", ImVec2(buttonWidth, 0)))
        {
            mShowDockerWarning = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void PackagingWindow::Draw3dsLinkWarningPopup()
{
    if (mShow3dsLinkWarning)
    {
        ImGui::OpenPopup("3DS Hardware Transfer");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("3DS Hardware Transfer", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Please make sure your 3DS has Homebrew");
        ImGui::Text("Launcher open and is ready to receive");
        ImGui::Text("files via 3dslink.");
        ImGui::Spacing();
        ImGui::Text("Both devices must be on the same network.");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = 80.0f;

        if (ImGui::Button("Send", ImVec2(buttonWidth, 0)))
        {
            mShow3dsLinkWarning = false;
            ImGui::CloseCurrentPopup();

            // Execute 3dslink
            Launch3dsLink(mPendingOutputPath);
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
        {
            mShow3dsLinkWarning = false;
            mPendingOutputPath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void PackagingWindow::DrawWiiloadWarningPopup()
{
    if (mShowWiiloadWarning)
    {
        ImGui::OpenPopup("Wii Hardware Transfer");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Wii Hardware Transfer", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Please make sure your Wii has Homebrew");
        ImGui::Text("Channel open and is ready to receive");
        ImGui::Text("files via wiiload.");
        ImGui::Spacing();
        ImGui::Text("Both devices must be on the same network.");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = 80.0f;

        if (ImGui::Button("Send", ImVec2(buttonWidth, 0)))
        {
            mShowWiiloadWarning = false;
            ImGui::CloseCurrentPopup();

            // Execute wiiload
            LaunchWiiload(mPendingOutputPath);
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
        {
            mShowWiiloadWarning = false;
            mPendingOutputPath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void PackagingWindow::OpenLauncherSettings()
{
    PreferencesWindow* prefsWindow = GetPreferencesWindow();
    if (prefsWindow != nullptr)
    {
        prefsWindow->Open();
        prefsWindow->SelectModule("External/Launchers");
    }
}

void PackagingWindow::OnBuild()
{
    ExecuteBuild(false, false);
}

bool PackagingWindow::IsAnyBuildInProgress() const
{
    ActionManager* am = ActionManager::Get();

    // Local: ActionManager owns the modal, the build thread, and the
    // pending flag. IsBuildRunning() folds three of them together.
    if (am && am->IsBuildRunning())
    {
        return true;
    }

    // Local: the build thread exits BEFORE FinalizeLocalBuild joins it.
    // Between those two points the thread is still joinable, and starting
    // a new build now reassigns mBuildThread on a joinable thread, which
    // is a std::terminate condition. Wait for the join to land before
    // kicking the next profile.
    //
    // Auto-finalize for success runs inside ActionManager::DrawBuildModal,
    // which is called AFTER PackagingWindow::Draw in the same frame, so
    // there is exactly one frame where IsBuildRunning() reports false but
    // the thread is not yet joined. The joinable() check covers it.
    if (am && am->GetBuildState().mBuildThread.joinable())
    {
        return true;
    }

    // Docker: state lives on PackagingWindow. mRunning covers the worker
    // thread; the modal-while-incomplete guard catches the brief window
    // between thread exit and the modal noticing.
    if (mBuildState.mRunning.load())
    {
        return true;
    }
    if (mShowBuildModal && !mBuildState.mComplete.load())
    {
        return true;
    }
    if (mBuildState.mBuildThread.joinable())
    {
        return true;
    }

    return false;
}

void PackagingWindow::OnBuildAll()
{
    PackagingSettings* settings = PackagingSettings::Get();
    if (settings == nullptr)
    {
        return;
    }

    std::vector<BuildProfile>& profiles = settings->GetProfiles();
    if (profiles.empty())
    {
        LogWarning("Build All: no build profiles to build.");
        return;
    }

    if (IsAnyBuildInProgress())
    {
        LogWarning("Build All: a build is already running; ignoring.");
        return;
    }

    // Force-rebuild every profile to bypass the cache-skip path in
    // ActionManager::BuildData (which sets mSuccess=false). Restored on
    // queue end so the user's Force Rebuild checkbox preference is preserved.
    mBuildAllSavedForceRebuild = mForceRebuild;
    mForceRebuild = true;

    mBuildAllRunning = true;
    mBuildAllIndex = -1;            // TickBuildAllQueue advances to 0 on first tick
    mBuildAllFailedIndex = -1;
    mBuildAllFailedName.clear();
    mBuildAllLastWasDocker = false;
}

void PackagingWindow::CancelBuildAll()
{
    // Just stop the queue. The in-flight per-profile build keeps running and
    // reports through its own modal (the user can cancel that build via the
    // modal's own Cancel button). When it finishes, our tick sees the queue
    // is no longer active and exits without kicking the next profile.
    mBuildAllRunning = false;
    mForceRebuild = mBuildAllSavedForceRebuild;
}

void PackagingWindow::TickBuildAllQueue()
{
    if (!mBuildAllRunning)
    {
        return;
    }

    if (IsAnyBuildInProgress())
    {
        return;
    }

    // First-ever tick: no prior build to inspect, fall through to "advance".
    if (mBuildAllIndex >= 0)
    {
        const bool success = mBuildAllLastWasDocker
            ? mBuildState.mSuccess.load()
            : ActionManager::Get()->GetBuildState().mSuccess.load();

        if (!success)
        {
            mBuildAllFailedIndex = mBuildAllIndex;
            std::vector<BuildProfile>& profiles = PackagingSettings::Get()->GetProfiles();
            if (mBuildAllIndex < (int32_t)profiles.size())
            {
                mBuildAllFailedName = profiles[mBuildAllIndex].mName;
            }
            mBuildAllRunning = false;
            mForceRebuild = mBuildAllSavedForceRebuild;
            LogError("Build All: profile '%s' failed; queue stopped.", mBuildAllFailedName.c_str());
            return;
        }
    }

    // Advance.
    mBuildAllIndex++;
    std::vector<BuildProfile>& profiles = PackagingSettings::Get()->GetProfiles();
    if (mBuildAllIndex >= (int32_t)profiles.size())
    {
        mBuildAllRunning = false;
        mForceRebuild = mBuildAllSavedForceRebuild;
        LogDebug("Build All: queue completed successfully.");
        return;
    }

    // Kick the next build directly via the same sub-paths the single-Build
    // button uses, so docker / local dispatch and Force Rebuild forwarding
    // stay identical. Bypass ExecuteBuild() so we don't depend on
    // GetSelectedProfile() — the user's UI selection is preserved.
    const BuildProfile& profile = profiles[mBuildAllIndex];
    mBuildAllLastWasDocker = profile.mUseDocker;
    if (profile.mUseDocker)
    {
        ExecuteDockerBuild(profile, false, false);
    }
    else
    {
        ExecuteLocalBuild(profile, false, false);
    }
}

void PackagingWindow::OnBuildAndRun()
{
    ExecuteBuild(true, false);
}

void PackagingWindow::OnBuildAndRunOnDevice()
{
    ExecuteBuild(true, true);
}

void PackagingWindow::ExecuteBuild(bool runAfterBuild, bool runOnDevice)
{
    PackagingSettings* settings = PackagingSettings::Get();
    if (settings == nullptr)
    {
        return;
    }

    BuildProfile* profile = settings->GetSelectedProfile();
    if (profile == nullptr)
    {
        LogError("No build profile selected");
        return;
    }

    bool useDocker = profile->mUseDocker;

    if (useDocker)
    {
        if (!CheckDockerAvailable())
        {
            mShowDockerWarning = true;
            return;
        }
        ExecuteDockerBuild(*profile, runAfterBuild, runOnDevice);
    }
    else
    {
        ExecuteLocalBuild(*profile, runAfterBuild, runOnDevice);
    }
}

void PackagingWindow::BuildAndRunWithProfile(Platform platform, bool embedded, bool runOnDevice)
{
    // Try to find an existing profile for this platform
    PackagingSettings* settings = PackagingSettings::Get();
    BuildProfile* existingProfile = nullptr;

    if (settings != nullptr)
    {
        std::vector<BuildProfile>& profiles = settings->GetProfiles();
        for (BuildProfile& p : profiles)
        {
            if (p.mTargetPlatform == platform)
            {
                existingProfile = &p;
                break;
            }
        }
    }

    BuildProfile tempProfile;
    const BuildProfile& profile = existingProfile ? *existingProfile : tempProfile;

    if (!existingProfile)
    {
        tempProfile.mName = "Quick Play";
        tempProfile.mTargetPlatform = platform;
        tempProfile.mEmbedded = embedded;
        tempProfile.mOpenDirectoryOnFinish = false;
    }

    bool useDocker = profile.mUseDocker;

    if (useDocker)
    {
        if (!CheckDockerAvailable())
        {
            mShowDockerWarning = true;
            return;
        }
        ExecuteDockerBuild(profile, true, runOnDevice);
    }
    else
    {
        ExecuteLocalBuild(profile, true, runOnDevice);
    }
}

void PackagingWindow::ExecuteDockerBuild(const BuildProfile& profile, bool runAfterBuild, bool runOnDevice)
{
    LogDebug("Starting Docker build for platform: %s", GetPlatformString(profile.mTargetPlatform));

    std::string dockerCmd = BuildDockerCommand(profile);
    if (dockerCmd.empty())
    {
        LogError("Failed to build Docker command");
        return;
    }

    // Start async build with modal
    StartAsyncDockerBuild(profile, runAfterBuild, runOnDevice);
}

void PackagingWindow::ExecuteLocalBuild(const BuildProfile& profile, bool runAfterBuild, bool runOnDevice)
{
    LogDebug("Starting local build for platform: %s", GetPlatformString(profile.mTargetPlatform));

    mBuildInProgress = true;

    // Delegate to ActionManager's BuildData (now non-blocking)
    ActionManager* am = ActionManager::Get();
    if (am != nullptr)
    {
        // Set flags before BuildData (for cache check which may return early)
        am->GetBuildState().mForceRebuild = mForceRebuild;
        am->GetBuildState().mRunAfterBuild = runAfterBuild;
        am->GetBuildState().mRunOnDevice = runOnDevice;
        am->GetBuildState().mOpenDirectoryOnFinish = profile.mOpenDirectoryOnFinish;
        // Hand the active profile's per-target options to the build so addon
        // build-target callbacks can read them via ctx->GetProfileSetting.
        am->GetBuildState().mTargetOptions = profile.mTargetOptions;
        am->GetBuildState().mStaticContent = profile.mStaticContent;
        am->GetBuildState().mContentPak = profile.mContentPak;
        // Prefer the registry-resolved target id when present so addon-
        // provided targets dispatch through their descriptor callbacks. Falls
        // back to legacy Platform-only build when mTargetId is empty (old
        // profiles saved before mTargetId existed).
        if (!profile.mTargetId.empty())
        {
            am->BuildData(profile.mTargetId, profile.mEmbedded);
        }
        else
        {
            am->BuildData(profile.mTargetPlatform, profile.mEmbedded);
        }
        // Re-set run-after-build flags after BuildData (Reset() clears them in normal path)
        am->GetBuildState().mRunAfterBuild = runAfterBuild;
        am->GetBuildState().mRunOnDevice = runOnDevice;
        am->GetBuildState().mTargetOptions = profile.mTargetOptions;
        am->GetBuildState().mOpenDirectoryOnFinish = profile.mOpenDirectoryOnFinish;
        am->GetBuildState().mStaticContent = profile.mStaticContent;
        am->GetBuildState().mContentPak = profile.mContentPak;
    }

    mBuildInProgress = false;
}

void PackagingWindow::LaunchEmulator(const BuildProfile& profile, const std::string& outputPath)
{
    if (!PlatformSupportsRun(profile.mTargetPlatform))
    {
        return;
    }

    // Get launcher settings from Preferences module
    LaunchersModule* launchers = static_cast<LaunchersModule*>(
        PreferencesManager::Get()->FindModule("External/Launchers"));

    if (launchers == nullptr)
    {
        LogError("Launchers module not found");
        return;
    }

    // Validate emulator is configured
    if (!launchers->IsEmulatorConfigured(profile.mTargetPlatform))
    {
        LogError("Emulator not configured for %s. Open Preferences > External > Launchers to configure.",
                 GetPlatformString(profile.mTargetPlatform));
        return;
    }

    // Build and execute the command using user's custom args
    std::string cmd = launchers->BuildLaunchCommand(profile.mTargetPlatform, outputPath);

    LogDebug("Launching emulator: %s", cmd.c_str());
    SYS_Exec(cmd.c_str());
}

void PackagingWindow::Launch3dsLink(const std::string& outputPath)
{
    // Get launcher settings from Preferences module
    LaunchersModule* launchers = static_cast<LaunchersModule*>(
        PreferencesManager::Get()->FindModule("External/Launchers"));

    if (launchers == nullptr)
    {
        LogError("Launchers module not found");
        return;
    }

    // Check if 3dslink is available
    if (!launchers->Is3dsLinkConfigured())
    {
        LogError("3dslink not available. Please ensure devkitPro is installed.");
        return;
    }

    // Build and execute the 3dslink command
    std::string cmd = launchers->Build3dsLinkCommand(outputPath);

    if (cmd.empty())
    {
        LogError("Failed to build 3dslink command");
        return;
    }

    LogDebug("Launching 3dslink: %s", cmd.c_str());
    SYS_Exec(cmd.c_str());
}

void PackagingWindow::LaunchWiiload(const std::string& outputPath)
{
    // Get launcher settings from Preferences module
    LaunchersModule* launchers = static_cast<LaunchersModule*>(
        PreferencesManager::Get()->FindModule("External/Launchers"));

    if (launchers == nullptr)
    {
        LogError("Launchers module not found");
        return;
    }

    // Check if wiiload is available
    if (!launchers->IsWiiloadConfigured())
    {
        LogError("wiiload not available. Please ensure devkitPro is installed and Wii IP is configured in Preferences > External > Launchers.");
        return;
    }

    // Build and execute the wiiload command
    std::string cmd = launchers->BuildWiiloadCommand(outputPath);

    if (cmd.empty())
    {
        LogError("Failed to build wiiload command");
        return;
    }

    LogDebug("Launching wiiload: %s", cmd.c_str());
    SYS_Exec(cmd.c_str());
}

bool PackagingWindow::CheckDockerAvailable()
{
    ExternalModule* ext = static_cast<ExternalModule*>(
        PreferencesManager::Get()->FindModule("External"));
    std::string cmd = ext ? ext->GetDockerCommand() : "docker";
    cmd += " --version";

    std::string output;
    SYS_Exec(cmd.c_str(), &output);
    return !output.empty() && output.find("Docker") != std::string::npos;
}

std::string PackagingWindow::BuildDockerCommand(const BuildProfile& profile)
{
    const EngineState* engine = GetEngineState();
    std::string projectDir = engine->mProjectDirectory;

    std::string outputDir = GetOutputDirectory(profile);

    std::string buildCmd;
    switch (profile.mTargetPlatform)
    {
        case Platform::Linux:    buildCmd = "build-linux"; break;
        case Platform::GameCube: buildCmd = "build-gcn";   break;
        case Platform::Wii:      buildCmd = "build-wii";   break;
        case Platform::N3DS:     buildCmd = "build-3ds";   break;
        default:
            LogError("Docker build not supported for platform: %s", GetPlatformString(profile.mTargetPlatform));
            return "";
    }

    // Get Docker image from preferences
    DockerModule* dockerModule = static_cast<DockerModule*>(
        PreferencesManager::Get()->FindModule("Packaging/Docker"));
    std::string dockerImage = dockerModule ? dockerModule->GetDockerImage() : "polyphase-engine/polyphase-engine-linux:dev";

    // Get Docker executable from External preferences
    ExternalModule* ext = static_cast<ExternalModule*>(
        PreferencesManager::Get()->FindModule("External"));
    std::string dockerExe = ext ? ext->GetDockerCommand() : "docker";

    std::string cmd = dockerExe + " run --rm "
                      "-v \"" + outputDir + ":/game\" "
                      "-v \"" + projectDir + ":/project\" "
                      "\"" + dockerImage + "\" " + buildCmd;

    return cmd;
}

std::string PackagingWindow::GetOutputDirectory(const BuildProfile& profile)
{
    if (!profile.mOutputDirectory.empty())
    {
        std::string dir = profile.mOutputDirectory;
        // Ensure trailing slash
        if (dir.back() != '/' && dir.back() != '\\')
        {
            dir += '/';
        }
        return dir;
    }

    const EngineState* engine = GetEngineState();
    return engine->mProjectDirectory + "Packaged/" + GetPlatformString(profile.mTargetPlatform) + "/";
}

void PackagingWindow::StartAsyncDockerBuild(const BuildProfile& profile, bool runAfterBuild, bool runOnDevice)
{
    // Wait for any previous build to complete
    if (mBuildState.mBuildThread.joinable())
    {
        mBuildState.mBuildThread.join();
    }

    // Reset state
    mBuildState.mRunning.store(true);
    mBuildState.mCancelRequested.store(false);
    mBuildState.mComplete.store(false);
    mBuildState.mSuccess.store(false);
    mBuildState.mExitCode.store(0);
#if PLATFORM_LINUX
    mBuildState.mProcessId = 0;
#elif PLATFORM_WINDOWS
    mBuildState.mProcessHandle = nullptr;
#endif

    {
        std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
        mBuildState.mOutput.clear();
        mBuildState.mOutputDirty = false;
    }

    // Store build configuration
    mBuildState.mCommand = BuildDockerCommand(profile);
    mBuildState.mRunAfterBuild = runAfterBuild;
    mBuildState.mRunOnDevice = runOnDevice;
    mBuildState.mOpenDirectoryOnFinish = profile.mOpenDirectoryOnFinish;
    mBuildState.mTargetPlatform = profile.mTargetPlatform;

    // Compute output path
    std::string outputDir = GetOutputDirectory(profile);
    const EngineState* engine = GetEngineState();
    std::string projectName = engine->mProjectName;
    std::string extension = GetPlatformOutputExtension(profile.mTargetPlatform);
    mBuildState.mOutputPath = outputDir + projectName + extension;

    // Reset display state
    mDisplayOutput.clear();
    mSelectedLineIndices.clear();
    mSelectionAnchor = -1;
    mSelectionLineCount = 0;
    mAutoScroll = true;

    // Show modal and mark build in progress
    mShowBuildModal = true;
    mBuildInProgress = true;

    // Add command to output
    {
        std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
        mBuildState.mOutput = "[CMD] " + mBuildState.mCommand + "\n\n";
        mBuildState.mOutputDirty = true;
    }

    LogDebug("Executing async: %s", mBuildState.mCommand.c_str());

    // Launch build thread
    mBuildState.mBuildThread = std::thread(&PackagingWindow::DockerBuildThreadFunc, this);
}

void PackagingWindow::DockerBuildThreadFunc()
{
#if PLATFORM_LINUX
    // Create pipe for capturing output
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
        mBuildState.mOutput += "[ERROR] Failed to create pipe\n";
        mBuildState.mOutputDirty = true;
        mBuildState.mSuccess.store(false);
        mBuildState.mExitCode.store(-1);
        mBuildState.mComplete.store(true);
        mBuildState.mRunning.store(false);
        return;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
        mBuildState.mOutput += "[ERROR] Failed to fork process\n";
        mBuildState.mOutputDirty = true;
        mBuildState.mSuccess.store(false);
        mBuildState.mExitCode.store(-1);
        mBuildState.mComplete.store(true);
        mBuildState.mRunning.store(false);
        return;
    }

    if (pid == 0)
    {
        // Child process
        close(pipefd[0]); // Close read end

        // Redirect stdout and stderr to pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // Execute command via shell
        execl("/bin/sh", "sh", "-c", mBuildState.mCommand.c_str(), nullptr);
        _exit(127); // exec failed
    }

    // Parent process
    mBuildState.mProcessId = pid;
    close(pipefd[1]); // Close write end

    // Set non-blocking read
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    char buffer[4096];
    bool processRunning = true;

    while (processRunning && !mBuildState.mCancelRequested.load())
    {
        // Try to read output
        ssize_t bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
            mBuildState.mOutput += buffer;
            mBuildState.mOutputDirty = true;
        }

        // Check if process is still running
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid)
        {
            // Process finished - read any remaining output
            while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0)
            {
                buffer[bytesRead] = '\0';
                std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
                mBuildState.mOutput += buffer;
                mBuildState.mOutputDirty = true;
            }

            if (WIFEXITED(status))
            {
                int exitCode = WEXITSTATUS(status);
                mBuildState.mExitCode.store(exitCode);
                mBuildState.mSuccess.store(exitCode == 0);
            }
            else
            {
                mBuildState.mExitCode.store(-1);
                mBuildState.mSuccess.store(false);
            }
            processRunning = false;
        }
        else if (result == -1)
        {
            // Error checking process
            processRunning = false;
            mBuildState.mSuccess.store(false);
        }
        else
        {
            // Process still running, sleep briefly
            usleep(50000); // 50ms
        }
    }

    close(pipefd[0]);

    // Handle cancellation
    if (mBuildState.mCancelRequested.load() && processRunning)
    {
        kill(pid, SIGTERM);
        usleep(100000); // 100ms grace period
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);

        std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
        mBuildState.mOutput += "\n[CANCELLED] Build was cancelled by user.\n";
        mBuildState.mOutputDirty = true;
        mBuildState.mSuccess.store(false);
    }

#elif PLATFORM_WINDOWS
    // Create pipes for stdout/stderr
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hStdOutRead, hStdOutWrite;
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0))
    {
        std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
        mBuildState.mOutput += "[ERROR] Failed to create pipe\n";
        mBuildState.mOutputDirty = true;
        mBuildState.mSuccess.store(false);
        mBuildState.mComplete.store(true);
        mBuildState.mRunning.store(false);
        return;
    }

    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdOutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};

    std::string cmdLine = "cmd /c " + mBuildState.mCommand;
    if (!CreateProcessA(nullptr, const_cast<char*>(cmdLine.c_str()), nullptr, nullptr,
                        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);

        std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
        mBuildState.mOutput += "[ERROR] Failed to create process\n";
        mBuildState.mOutputDirty = true;
        mBuildState.mSuccess.store(false);
        mBuildState.mComplete.store(true);
        mBuildState.mRunning.store(false);
        return;
    }

    mBuildState.mProcessHandle = pi.hProcess;
    CloseHandle(hStdOutWrite);
    CloseHandle(pi.hThread);

    char buffer[4096];
    DWORD bytesRead;
    DWORD bytesAvailable;

    while (!mBuildState.mCancelRequested.load())
    {
        // Check if data available
        if (PeekNamedPipe(hStdOutRead, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable > 0)
        {
            if (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
                mBuildState.mOutput += buffer;
                mBuildState.mOutputDirty = true;
            }
        }

        // Check if process finished
        DWORD exitCode;
        if (GetExitCodeProcess(pi.hProcess, &exitCode))
        {
            if (exitCode != STILL_ACTIVE)
            {
                // Read any remaining output
                while (PeekNamedPipe(hStdOutRead, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable > 0)
                {
                    if (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
                    {
                        buffer[bytesRead] = '\0';
                        std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
                        mBuildState.mOutput += buffer;
                        mBuildState.mOutputDirty = true;
                    }
                }

                mBuildState.mExitCode.store(static_cast<int>(exitCode));
                mBuildState.mSuccess.store(exitCode == 0);
                break;
            }
        }

        Sleep(50); // 50ms
    }

    // Handle cancellation
    if (mBuildState.mCancelRequested.load())
    {
        TerminateProcess(pi.hProcess, 1);
        std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
        mBuildState.mOutput += "\n[CANCELLED] Build was cancelled by user.\n";
        mBuildState.mOutputDirty = true;
        mBuildState.mSuccess.store(false);
    }

    CloseHandle(hStdOutRead);
    CloseHandle(pi.hProcess);
    mBuildState.mProcessHandle = nullptr;
#endif

    mBuildState.mComplete.store(true);
    mBuildState.mRunning.store(false);
}

void PackagingWindow::CancelDockerBuild()
{
    if (!mBuildState.mRunning.load())
    {
        return;
    }

    mBuildState.mCancelRequested.store(true);

#if PLATFORM_LINUX
    if (mBuildState.mProcessId > 0)
    {
        kill(mBuildState.mProcessId, SIGTERM);
    }
#elif PLATFORM_WINDOWS
    if (mBuildState.mProcessHandle != nullptr)
    {
        TerminateProcess(static_cast<HANDLE>(mBuildState.mProcessHandle), 1);
    }
#endif
}

void PackagingWindow::FinalizeBuild()
{
    // Join the build thread
    if (mBuildState.mBuildThread.joinable())
    {
        mBuildState.mBuildThread.join();
    }

    mBuildInProgress = false;

    // Handle post-build actions
    if (mBuildState.mSuccess.load() && mBuildState.mRunAfterBuild)
    {
        if (mBuildState.mRunOnDevice && mBuildState.mTargetPlatform == Platform::N3DS)
        {
            // Show 3dslink warning popup
            mPendingOutputPath = mBuildState.mOutputPath;
            mShow3dsLinkWarning = true;
        }
        else if (mBuildState.mRunOnDevice && mBuildState.mTargetPlatform == Platform::Wii)
        {
            // Show wiiload warning popup
            mPendingOutputPath = mBuildState.mOutputPath;
            mShowWiiloadWarning = true;
        }
        else if (mBuildState.mRunOnDevice && mBuildState.mTargetPlatform == Platform::Android)
        {
            // adb install + launch + (optional) detached logcat. No popup —
            // unlike 3dslink/wiiload there's nothing scary about adb install
            // (it's idempotent with -r, and the device approved this host's
            // RSA key at first connection). Just go.
            LaunchersModule* launchers = static_cast<LaunchersModule*>(
                PreferencesManager::Get()->FindModule("External/Launchers"));

            if (launchers != nullptr && launchers->IsAdbConfigured())
            {
                std::string installCmd = launchers->BuildAdbInstallCommand(mBuildState.mOutputPath);
                if (!installCmd.empty())
                {
                    LogDebug("adb install: %s", installCmd.c_str());
                    std::string out;
                    SYS_Exec(installCmd.c_str(), &out);
                    if (!out.empty()) LogDebug("%s", out.c_str());
                }
                std::string launchCmd = launchers->BuildAdbLaunchCommand();
                if (!launchCmd.empty())
                {
                    LogDebug("adb launch: %s", launchCmd.c_str());
                    SYS_Exec(launchCmd.c_str());
                }
                if (launchers->mAutoOpenLogcat)
                {
                    std::string logcatCmd = launchers->BuildAdbLogcatCommand();
                    if (!logcatCmd.empty())
                    {
                        LogDebug("adb logcat (detached): %s", logcatCmd.c_str());
                        SYS_ExecDetached(logcatCmd.c_str());
                    }
                }
            }
            else
            {
                LogError("adb not configured. Set ADB path in Preferences > External > Launchers.");
            }
        }
        else if (mBuildState.mTargetPlatform == Platform::Windows ||
                 mBuildState.mTargetPlatform == Platform::Linux)
        {
            // Desktop build: no emulator involved, run the packaged game
            // directly on the host.
            if (!ActionManager::LaunchDesktopBuild(mBuildState.mTargetPlatform, mBuildState.mOutputPath))
            {
                LogError("Cannot run a %s build on this host.",
                         GetPlatformString(mBuildState.mTargetPlatform));
            }
        }
        else if (PlatformSupportsRun(mBuildState.mTargetPlatform))
        {
            // Launch emulator (GameCube/Wii via Dolphin, 3DS via Azahar/Citra).
            // Android falls through here only if the user somehow set
            // runOnDevice=false for Android — which our UI doesn't expose, but
            // skip it explicitly to avoid the "Emulator not configured for
            // Android" error noise.
            if (mBuildState.mTargetPlatform == Platform::Android)
            {
                // No-op — Android has no integrated emulator path.
            }
            else
            {
                LaunchersModule* launchers = static_cast<LaunchersModule*>(
                    PreferencesManager::Get()->FindModule("External/Launchers"));

                if (launchers != nullptr && launchers->IsEmulatorConfigured(mBuildState.mTargetPlatform))
                {
                    std::string cmd = launchers->BuildLaunchCommand(mBuildState.mTargetPlatform, mBuildState.mOutputPath);
                    LogDebug("Launching emulator: %s", cmd.c_str());
                    SYS_Exec(cmd.c_str());
                }
                else
                {
                    LogError("Emulator not configured for %s", GetPlatformString(mBuildState.mTargetPlatform));
                }
            }
        }
    }
    else if (mBuildState.mSuccess.load() && !mBuildState.mRunAfterBuild && mBuildState.mOpenDirectoryOnFinish)
    {
        // Open output directory on successful build without run
        std::string outputDir = mBuildState.mOutputPath;
        size_t lastSlash = outputDir.find_last_of("/\\");
        if (lastSlash != std::string::npos)
        {
            outputDir = outputDir.substr(0, lastSlash + 1);
        }
        SYS_ExplorerOpenDirectory(outputDir.c_str());
    }
}

void PackagingWindow::DrawBuildOutputModal()
{
    if (!mShowBuildModal)
    {
        return;
    }

    // Update display output from shared buffer
    {
        std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
        if (mBuildState.mOutputDirty)
        {
            mDisplayOutput = mBuildState.mOutput;
            mBuildState.mOutputDirty = false;
        }
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(720, 500), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags modalFlags = ImGuiWindowFlags_NoCollapse;
    bool finalized = false;

    if (ImGui::Begin("Docker Build", &mShowBuildModal, modalFlags))
    {
        // Status header
        bool isComplete = mBuildState.mComplete.load();
        bool isSuccess = mBuildState.mSuccess.load();
        bool isCancelled = mBuildState.mCancelRequested.load();

        if (!isComplete)
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Building...");
        }
        else if (isCancelled)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Build Cancelled");
        }
        else if (isSuccess)
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Build Successful!");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Build Failed (exit code: %d)", mBuildState.mExitCode.load());
        }

        ImGui::Separator();

        // Output window
        float footerHeight = ImGui::GetFrameHeightWithSpacing() + 8.0f;
        ImVec2 outputSize(0, -footerHeight);

        ImGui::BeginChild("BuildOutput", outputSize, true,
            ImGuiWindowFlags_HorizontalScrollbar);

        // Render each line as an ImGui::Selectable so the user can click,
        // Ctrl/Shift-click, and right-click to copy. Mirrors the selection
        // pattern in TerminalPanel::DrawOutput().
        ImGuiIO& io = ImGui::GetIO();
        int globalLineIndex = 0;
        const std::string& s = mDisplayOutput;
        size_t startPos = 0;
        while (startPos <= s.size())
        {
            size_t nl = s.find('\n', startPos);
            size_t lineEnd = (nl == std::string::npos) ? s.size() : nl;
            size_t actualEnd = lineEnd;
            if (actualEnd > startPos && s[actualEnd - 1] == '\r')
            {
                --actualEnd;
            }

            std::string line;
            if (actualEnd > startPos)
            {
                line.assign(s.data() + startPos, actualEnd - startPos);
            }

            int rowIndex = globalLineIndex++;
            ImGui::PushID(rowIndex);

            const char* label = line.empty() ? " " : line.c_str();
            bool selected = (mSelectedLineIndices.count(rowIndex) != 0);
            if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (io.KeyShift && mSelectionAnchor >= 0)
                {
                    int lo = mSelectionAnchor < rowIndex ? mSelectionAnchor : rowIndex;
                    int hi = mSelectionAnchor > rowIndex ? mSelectionAnchor : rowIndex;
                    if (!io.KeyCtrl)
                    {
                        mSelectedLineIndices.clear();
                    }
                    for (int i = lo; i <= hi; ++i)
                    {
                        mSelectedLineIndices.insert(i);
                    }
                }
                else if (io.KeyCtrl)
                {
                    if (mSelectedLineIndices.count(rowIndex) != 0)
                    {
                        mSelectedLineIndices.erase(rowIndex);
                    }
                    else
                    {
                        mSelectedLineIndices.insert(rowIndex);
                    }
                    mSelectionAnchor = rowIndex;
                }
                else
                {
                    mSelectedLineIndices.clear();
                    mSelectedLineIndices.insert(rowIndex);
                    mSelectionAnchor = rowIndex;
                }

                if (ImGui::IsMouseDoubleClicked(0))
                {
                    CopyOutputToClipboard(line);
                }
            }

            if (ImGui::BeginPopupContextItem("##BuildLineCtx"))
            {
                if (mSelectedLineIndices.count(rowIndex) == 0)
                {
                    mSelectedLineIndices.clear();
                    mSelectedLineIndices.insert(rowIndex);
                    mSelectionAnchor = rowIndex;
                }

                if (ImGui::MenuItem("Copy line"))
                {
                    CopyOutputToClipboard(line);
                }
                char selLabel[64];
                snprintf(selLabel, sizeof(selLabel),
                    "Copy selected (%zu)", mSelectedLineIndices.size());
                if (ImGui::MenuItem(selLabel, nullptr, false,
                        !mSelectedLineIndices.empty()))
                {
                    CopyOutputToClipboard(BuildSelectedOutputLinesText());
                }
                if (ImGui::MenuItem("Copy all"))
                {
                    CopyOutputToClipboard(BuildAllOutputLinesText());
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Select all"))
                {
                    mSelectedLineIndices.clear();
                    for (int i = 0; i < mSelectionLineCount; ++i)
                    {
                        mSelectedLineIndices.insert(i);
                    }
                }
                if (ImGui::MenuItem("Clear selection", nullptr, false,
                        !mSelectedLineIndices.empty()))
                {
                    mSelectedLineIndices.clear();
                    mSelectionAnchor = -1;
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();

            if (nl == std::string::npos)
            {
                break;
            }
            startPos = nl + 1;
        }

        mSelectionLineCount = globalLineIndex;

        // Ctrl+A inside the focused output area selects every visible line.
        if (ImGui::IsWindowFocused() && io.KeyCtrl &&
            ImGui::IsKeyPressed(ImGuiKey_A, false))
        {
            mSelectedLineIndices.clear();
            for (int i = 0; i < mSelectionLineCount; ++i)
            {
                mSelectedLineIndices.insert(i);
            }
        }

        // Ctrl+C copies the current selection (or the whole log if nothing
        // is selected) so the standard shortcut works without right-click.
        if (ImGui::IsWindowFocused() && io.KeyCtrl &&
            ImGui::IsKeyPressed(ImGuiKey_C, false))
        {
            if (!mSelectedLineIndices.empty())
            {
                CopyOutputToClipboard(BuildSelectedOutputLinesText());
            }
            else if (!mDisplayOutput.empty())
            {
                CopyOutputToClipboard(BuildAllOutputLinesText());
            }
        }

        // Auto-scroll to bottom
        if (mAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        // Footer with checkboxes and button
        Polyphase::Checkbox("Auto-scroll", &mAutoScroll);
        ImGui::SameLine();
        Polyphase::Checkbox("Auto-close when finished", &mAutoCloseOnFinish);
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy All"))
        {
            if (!mDisplayOutput.empty())
            {
                CopyOutputToClipboard(BuildAllOutputLinesText());
            }
        }
        ImGui::SameLine();
        {
            bool hasSelection = !mSelectedLineIndices.empty();
            if (!hasSelection) ImGui::BeginDisabled();
            if (ImGui::SmallButton("Copy Selected"))
            {
                CopyOutputToClipboard(BuildSelectedOutputLinesText());
            }
            if (!hasSelection) ImGui::EndDisabled();
        }

        ImGui::SameLine();
        float buttonWidth = 80.0f;
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availWidth - buttonWidth);

        // Auto-close on successful completion
        if (isComplete && mAutoCloseOnFinish && isSuccess && !isCancelled)
        {
            mShowBuildModal = false;
            FinalizeBuild();
            finalized = true;
        }

        if (!finalized && isComplete)
        {
            if (ImGui::Button("Close", ImVec2(buttonWidth, 0)))
            {
                mShowBuildModal = false;
                FinalizeBuild();
                finalized = true;
            }
        }
        else if (!isComplete)
        {
            if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
            {
                CancelDockerBuild();
            }
        }
    }
    ImGui::End();

    // Handle window close via X button (skip if already finalized above)
    if (!mShowBuildModal && !finalized)
    {
        if (mBuildState.mRunning.load())
        {
            CancelDockerBuild();
        }
        // Wait briefly for thread to finish before finalizing
        if (mBuildState.mComplete.load())
        {
            FinalizeBuild();
        }
        else
        {
            // Re-show modal until build completes
            mShowBuildModal = true;
        }
    }
}

std::string PackagingWindow::BuildAllOutputLinesText() const
{
    return mDisplayOutput;
}

std::string PackagingWindow::BuildSelectedOutputLinesText() const
{
    if (mSelectedLineIndices.empty() || mDisplayOutput.empty())
        return std::string();

    std::string result;
    result.reserve(mDisplayOutput.size());

    int rowIndex = 0;
    size_t startPos = 0;
    const std::string& s = mDisplayOutput;
    while (startPos <= s.size())
    {
        size_t nl = s.find('\n', startPos);
        size_t lineEnd = (nl == std::string::npos) ? s.size() : nl;
        size_t actualEnd = lineEnd;
        if (actualEnd > startPos && s[actualEnd - 1] == '\r')
        {
            --actualEnd;
        }

        if (mSelectedLineIndices.count(rowIndex) != 0)
        {
            if (!result.empty())
                result.push_back('\n');
            if (actualEnd > startPos)
            {
                result.append(s.data() + startPos, actualEnd - startPos);
            }
        }

        ++rowIndex;

        if (nl == std::string::npos)
            break;
        startPos = nl + 1;
    }

    return result;
}

#endif
