#if EDITOR

#include "GitWorkspaceWindow.h"
#include "GitService.h"
#include "GitRepository.h"
#include "GitOperationQueue.h"
#include "GitCliProbe.h"
#include "Dialogs/GitOpenDialog.h"
#include "Dialogs/GitInitDialog.h"
#include "Dialogs/GitCloneDialog.h"
#include "Dialogs/GitRemoteEditDialog.h"
#include "Dialogs/GitCreateTagDialog.h"
#include "Dialogs/GitCreateBranchDialog.h"
#include "Dialogs/GitMergeDialog.h"
#include "Dialogs/GitSyncBranchDialog.h"
#include "Engine.h"
#include "Log.h"
#include <git2.h>
#include "imgui.h"

#include <algorithm>
#include <cstring>
#include <ctime>

static GitWorkspaceWindow sGitWorkspaceWindow;

GitWorkspaceWindow* GetGitWorkspaceWindow()
{
    return &sGitWorkspaceWindow;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* ChangeTypeLabel(GitChangeType type)
{
    switch (type)
    {
        case GitChangeType::Modified:   return "M";
        case GitChangeType::Added:      return "A";
        case GitChangeType::Deleted:    return "D";
        case GitChangeType::Renamed:    return "R";
        case GitChangeType::Copied:     return "C";
        case GitChangeType::Untracked:  return "?";
        case GitChangeType::Conflicted: return "!";
        case GitChangeType::Ignored:    return "I";
        default:                        return " ";
    }
}

static ImVec4 ChangeTypeColor(GitChangeType type)
{
    switch (type)
    {
        case GitChangeType::Modified:   return ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
        case GitChangeType::Added:      return ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
        case GitChangeType::Deleted:    return ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
        case GitChangeType::Renamed:    return ImVec4(0.5f, 0.7f, 1.0f, 1.0f);
        case GitChangeType::Copied:     return ImVec4(0.5f, 0.7f, 1.0f, 1.0f);
        case GitChangeType::Untracked:  return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        case GitChangeType::Conflicted: return ImVec4(1.0f, 0.4f, 0.0f, 1.0f);
        case GitChangeType::Ignored:    return ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
        default:                        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

static const char* SidebarSectionName(int32_t section)
{
    switch (section)
    {
        case 0: return "File Status";
        case 1: return "History";
        case 2: return "Branches";
        case 3: return "Tags";
        case 4: return "Remotes";
        default: return "Unknown";
    }
}

static std::string FormatTimestamp(int64_t timestamp)
{
    if (timestamp <= 0)
    {
        return "";
    }
    time_t t = static_cast<time_t>(timestamp);
    struct tm tmBuf;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tmBuf);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// Open / Close / IsOpen
// ---------------------------------------------------------------------------

void GitWorkspaceWindow::Open()
{
    mIsOpen = true;

    // Auto-open the current project's repo if no repo is open yet
    GitService* service = GitService::Get();
    if (service && !service->IsRepositoryOpen())
    {
        const std::string& projectDir = GetEngineState()->mProjectDirectory;
        if (!projectDir.empty())
        {
            git_buf repoPath = GIT_BUF_INIT;
            if (git_repository_discover(&repoPath, projectDir.c_str(), 0, nullptr) == 0)
            {
                service->OpenRepository(projectDir);
                git_buf_dispose(&repoPath);
            }
        }
    }
}

void GitWorkspaceWindow::Close()
{
    mIsOpen = false;
}

bool GitWorkspaceWindow::IsOpen() const
{
    return mIsOpen;
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void GitWorkspaceWindow::Draw()
{
    if (!mIsOpen)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar;

    if (!ImGui::Begin("Version Control###GitWorkspace", &mIsOpen, windowFlags))
    {
        ImGui::End();
        return;
    }

    // ---- Menu bar with section tabs ----
    if (ImGui::BeginMenuBar())
    {
        for (int32_t i = 0; i < 5; ++i)
        {
            bool isSelected = (mActiveSidebarSection == i);
            if (isSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }
            if (ImGui::Button(SidebarSectionName(i)))
            {
                mActiveSidebarSection = i;
            }
            if (isSelected)
            {
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndMenuBar();
    }

    // ---- Check if service / repo is available ----
    GitService* service = GitService::Get();
    if (service == nullptr || !service->IsRepositoryOpen())
    {
        ImGui::TextDisabled("No repository open.");
        ImGui::Spacing();

        // Check if the current project directory is a git repo
        const std::string& projectDir = GetEngineState()->mProjectDirectory;
        bool projectLoaded = !projectDir.empty();
        bool projectIsRepo = false;

        if (projectLoaded)
        {
            git_buf repoPath = GIT_BUF_INIT;
            if (git_repository_discover(&repoPath, projectDir.c_str(), 0, nullptr) == 0)
            {
                projectIsRepo = true;
                git_buf_dispose(&repoPath);
            }
        }

        // Project-aware buttons
        if (projectLoaded && projectIsRepo)
        {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Git repository detected in project directory.");
            ImGui::Spacing();
            if (ImGui::Button("Set Project As Repository"))
            {
                service->OpenRepository(projectDir);
            }
        }
        else if (projectLoaded)
        {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Project directory is not a Git repository.");
            ImGui::Spacing();
            if (ImGui::Button("Init Project Repository"))
            {
                if (service->InitRepository(projectDir))
                {
                    LogDebug("GitWorkspaceWindow: Initialized and opened repo at %s", projectDir.c_str());
                }
            }
        }
        else
        {
            ImGui::TextDisabled("No project loaded.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // General buttons
        if (ImGui::Button("Open Repository..."))
        {
            GetGitOpenDialog()->Open();
        }
        ImGui::SameLine();
        if (ImGui::Button("Init Repository..."))
        {
            GetGitInitDialog()->Open();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clone Repository..."))
        {
            GetGitCloneDialog()->Open();
        }

        ImGui::End();
        return;
    }

    // ---- Toolbar ----
    DrawToolbar();

    ImGui::Separator();

    // ---- Layout: sidebar + main area ----
    float sidebarWidth = 200.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Sidebar
    DrawSidebar(sidebarWidth, avail.y);

    ImGui::SameLine();

    // Main area
    float mainWidth = avail.x - sidebarWidth - ImGui::GetStyle().ItemSpacing.x;
    DrawMainArea(mainWidth, avail.y);

    // Checkout choice popup (rendered inside the Git window)
    DrawCheckoutChoicePopup();

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Checkout Choice Popup
// ---------------------------------------------------------------------------

void GitWorkspaceWindow::DrawCheckoutChoicePopup()
{
    if (mCheckoutChoiceOpen)
    {
        ImGui::OpenPopup("Checkout Commit###CheckoutChoice");
        mCheckoutChoiceOpen = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Checkout Commit###CheckoutChoice", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("This commit is the tip of one or more branches.");
        ImGui::Text("Would you like to checkout a branch instead?");
        ImGui::Spacing();

        GitService* service = GitService::Get();
        GitRepository* repo = service ? service->GetCurrentRepo() : nullptr;

        // Branch selector
        if (mCheckoutChoiceBranches.size() == 1)
        {
            ImGui::Text("Branch: ");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "%s", mCheckoutChoiceBranches[0].c_str());
        }
        else
        {
            ImGui::Text("Select branch:");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##BranchChoice", mCheckoutChoiceBranches[mCheckoutChoiceSelected].c_str()))
            {
                for (int32_t i = 0; i < (int32_t)mCheckoutChoiceBranches.size(); ++i)
                {
                    bool selected = (mCheckoutChoiceSelected == i);
                    if (ImGui::Selectable(mCheckoutChoiceBranches[i].c_str(), selected))
                        mCheckoutChoiceSelected = i;
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Checkout Branch", ImVec2(140, 0)))
        {
            if (repo && mCheckoutChoiceSelected < (int32_t)mCheckoutChoiceBranches.size())
            {
                const std::string& branchName = mCheckoutChoiceBranches[mCheckoutChoiceSelected];

                // Check if this is a remote branch by matching against known remote names
                bool isRemote = false;
                std::string localName = branchName;
                std::vector<GitRemoteInfo> remotes = repo->GetRemotes();
                for (const auto& remote : remotes)
                {
                    std::string prefix = remote.mName + "/";
                    if (branchName.rfind(prefix, 0) == 0)
                    {
                        isRemote = true;
                        localName = branchName.substr(prefix.size());
                        break;
                    }
                }

                if (isRemote)
                {
                    if (!repo->CreateBranch(localName, mCheckoutChoiceOid, true))
                        repo->CheckoutBranch(localName);
                }
                else
                {
                    repo->CheckoutBranch(branchName);
                }
                repo->RefreshStatus();
                mDiffDirty = true;
                mLogEntries.push_back({"Checked out branch: " + localName, 0});
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Detached HEAD", ImVec2(140, 0)))
        {
            if (repo)
            {
                repo->CheckoutCommit(mCheckoutChoiceOid);
                repo->RefreshStatus();
                mDiffDirty = true;
                mLogEntries.push_back({"Checked out commit (detached): " + mCheckoutChoiceOid.substr(0, 7), 0});
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

void GitWorkspaceWindow::DrawToolbar()
{
    GitService* service = GitService::Get();
    GitRepository* repo = service ? service->GetCurrentRepo() : nullptr;
    GitOperationQueue* queue = service ? service->GetOperationQueue() : nullptr;

    // Branch display
    std::string branch = repo ? repo->GetCurrentBranch() : "unknown";
    ImGui::Text("Branch:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", branch.c_str());

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // Fetch
    if (ImGui::Button("Fetch"))
    {
        if (repo && queue)
        {
            GitOperationRequest req;
            req.mKind = GitOperationKind::Fetch;
            req.mRepoPath = repo->GetPath();
            req.mFetchAll = true;
            req.mCancelToken = CreateCancelToken();
            queue->Enqueue(req);
            mLogEntries.push_back({"Fetch started...", 0});
        }
    }

    ImGui::SameLine();

    // Pull
    if (ImGui::Button("Pull"))
    {
        if (repo && queue)
        {
            GitOperationRequest req;
            req.mKind = GitOperationKind::Pull;
            req.mRepoPath = repo->GetPath();
            req.mCancelToken = CreateCancelToken();
            queue->Enqueue(req);
            mLogEntries.push_back({"Pull started...", 0});
        }
    }

    ImGui::SameLine();

    // Push (with dropdown arrow for options)
    if (ImGui::Button("Push"))
    {
        if (repo && queue)
        {
            GitOperationRequest req;
            req.mKind = GitOperationKind::Push;
            req.mRepoPath = repo->GetPath();
            req.mBranchName = repo->GetCurrentBranch();
            std::vector<GitRemoteInfo> remotes = repo->GetRemotes();
            if (!remotes.empty())
                req.mRemoteName = remotes[0].mName;
            req.mCancelToken = CreateCancelToken();
            queue->Enqueue(req);
            mLogEntries.push_back({"Push started...", 0});
        }
    }
    ImGui::SameLine(0, 1);
    if (ImGui::ArrowButton("##PushOptions", ImGuiDir_Down))
    {
        ImGui::OpenPopup("PushOptionsPopup");
    }
    if (ImGui::BeginPopup("PushOptionsPopup"))
    {
        if (ImGui::MenuItem("Push with Tags"))
        {
            if (repo && queue)
            {
                // Push branch
                GitOperationRequest req;
                req.mKind = GitOperationKind::Push;
                req.mRepoPath = repo->GetPath();
                req.mBranchName = repo->GetCurrentBranch();
                std::vector<GitRemoteInfo> remotes = repo->GetRemotes();
                if (!remotes.empty())
                    req.mRemoteName = remotes[0].mName;
                req.mCancelToken = CreateCancelToken();
                queue->Enqueue(req);

                // Push tags
                GitOperationRequest tagReq;
                tagReq.mKind = GitOperationKind::PushTags;
                tagReq.mRepoPath = repo->GetPath();
                if (!remotes.empty())
                    tagReq.mRemoteName = remotes[0].mName;
                tagReq.mCancelToken = CreateCancelToken();
                queue->Enqueue(tagReq);
                mLogEntries.push_back({"Push with tags started...", 0});
            }
        }
        if (ImGui::MenuItem("Push Tags Only"))
        {
            if (repo && queue)
            {
                GitOperationRequest req;
                req.mKind = GitOperationKind::PushTags;
                req.mRepoPath = repo->GetPath();
                std::vector<GitRemoteInfo> remotes = repo->GetRemotes();
                if (!remotes.empty())
                    req.mRemoteName = remotes[0].mName;
                req.mCancelToken = CreateCancelToken();
                queue->Enqueue(req);
                mLogEntries.push_back({"Pushing tags...", 0});
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // Refresh
    if (ImGui::Button("Refresh"))
    {
        if (repo)
        {
            repo->RefreshStatus();
            mDiffDirty = true;
            mLogEntries.push_back({"Status refreshed.", 0});
        }
    }

    // Show operation progress if any
    if (queue && queue->HasPendingOps())
    {
        ImGui::SameLine();
        const GitProgressEvent& prog = queue->GetCurrentProgress();
        if (prog.mTotal > 0)
        {
            float frac = static_cast<float>(prog.mCurrent) / static_cast<float>(prog.mTotal);
            ImGui::ProgressBar(frac, ImVec2(120.0f, 0.0f));
        }
        else
        {
            ImGui::TextDisabled("Working...");
        }
    }

    // Poll results
    if (queue)
    {
        std::vector<GitOperationResult> results;
        queue->PollResults(results);
        for (const auto& res : results)
        {
            if (res.mStatus == GitOperationStatus::Success)
            {
                mLogEntries.push_back({res.mMessage.empty() ? "Operation succeeded." : res.mMessage, 0});
                if (repo)
                {
                    repo->RefreshStatus();
                    mDiffDirty = true;
                }
            }
            else if (res.mStatus == GitOperationStatus::Failed)
            {
                std::string msg = "Operation failed";
                if (!res.mMessage.empty())
                {
                    msg += ": " + res.mMessage;
                }
                mLogEntries.push_back({msg, 2});
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Sidebar
// ---------------------------------------------------------------------------

void GitWorkspaceWindow::DrawSidebar(float width, float height)
{
    ImGui::BeginChild("##GitSidebar", ImVec2(width, height), true);

    ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

    for (int32_t i = 0; i < 5; ++i)
    {
        ImGuiTreeNodeFlags flags = baseFlags | ImGuiTreeNodeFlags_Leaf;
        if (mActiveSidebarSection == i)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        bool nodeOpen = ImGui::TreeNodeEx(SidebarSectionName(i), flags);
        if (ImGui::IsItemClicked())
        {
            mActiveSidebarSection = i;
        }
        if (nodeOpen)
        {
            ImGui::TreePop();
        }
    }

    ImGui::Separator();

    // Output log section at the bottom of the sidebar
    float logHeight = ImGui::GetContentRegionAvail().y;
    if (logHeight > 60.0f)
    {
        ImGui::Text("Output");
        DrawOutputLog(width - 16.0f, logHeight - ImGui::GetTextLineHeightWithSpacing() - 4.0f);
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Main Area (dispatches to sub-views)
// ---------------------------------------------------------------------------

void GitWorkspaceWindow::DrawMainArea(float width, float height)
{
    ImGui::BeginChild("##GitMainArea", ImVec2(width, height), true);

    switch (mActiveSidebarSection)
    {
        case 0:
            DrawStatusView(width - 16.0f, ImGui::GetContentRegionAvail().y);
            break;
        case 1:
            DrawHistoryView(width - 16.0f, ImGui::GetContentRegionAvail().y);
            break;
        case 2:
        {
            // Branches view
            GitService* service = GitService::Get();
            GitRepository* repo = service ? service->GetCurrentRepo() : nullptr;
            if (repo)
            {
                std::vector<GitBranchInfo> branches = repo->GetBranches();
                std::string currentBranch = repo->GetCurrentBranch();

                if (ImGui::Button("+ Branch"))
                {
                    GetGitCreateBranchDialog()->Open();
                }
                ImGui::Separator();

                if (ImGui::CollapsingHeader("Local Branches", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (const auto& br : branches)
                    {
                        if (!br.mIsLocal)
                        {
                            continue;
                        }

                        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
                        if (br.mIsCurrent)
                        {
                            nodeFlags |= ImGuiTreeNodeFlags_Selected;
                        }

                        bool open = ImGui::TreeNodeEx(br.mName.c_str(), nodeFlags);

                        // Right-click context menu
                        if (ImGui::BeginPopupContextItem())
                        {
                            if (!br.mIsCurrent && ImGui::MenuItem("Checkout"))
                            {
                                repo->CheckoutBranch(br.mName);
                                repo->RefreshStatus();
                                mDiffDirty = true;
                                mLogEntries.push_back({"Checked out branch: " + br.mName, 0});
                            }
                            if (ImGui::MenuItem("Sync With Commit..."))
                            {
                                GetGitSyncBranchDialog()->Open(br.mName);
                            }
                            ImGui::Separator();
                            if (!br.mIsCurrent && ImGui::MenuItem("Delete"))
                            {
                                repo->DeleteBranch(br.mName, false);
                                mLogEntries.push_back({"Deleted branch: " + br.mName, 0});
                            }
                            if (ImGui::MenuItem("Copy Name"))
                            {
                                ImGui::SetClipboardText(br.mName.c_str());
                            }
                            ImGui::EndPopup();
                        }

                        if (open)
                        {
                            ImGui::TreePop();
                        }
                    }
                }

                if (ImGui::CollapsingHeader("Remote Branches", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (const auto& br : branches)
                    {
                        if (br.mIsLocal)
                        {
                            continue;
                        }

                        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
                        bool open = ImGui::TreeNodeEx(br.mName.c_str(), nodeFlags);

                        if (ImGui::BeginPopupContextItem())
                        {
                            if (ImGui::MenuItem("Checkout as Local"))
                            {
                                // Strip remote name prefix (e.g. "origin/") to get local branch name
                                std::string localName = br.mName;
                                std::vector<GitRemoteInfo> remotes = repo->GetRemotes();
                                for (const auto& remote : remotes)
                                {
                                    std::string prefix = remote.mName + "/";
                                    if (localName.rfind(prefix, 0) == 0)
                                    {
                                        localName = localName.substr(prefix.size());
                                        break;
                                    }
                                }
                                // Try to create the branch; if it already exists, just checkout
                                if (!repo->CreateBranch(localName, br.mLastCommitOid, true))
                                {
                                    repo->CheckoutBranch(localName);
                                }
                                repo->RefreshStatus();
                                mDiffDirty = true;
                                mLogEntries.push_back({"Checked out local branch: " + localName, 0});
                            }
                            if (ImGui::MenuItem("Copy Name"))
                            {
                                ImGui::SetClipboardText(br.mName.c_str());
                            }
                            ImGui::EndPopup();
                        }

                        if (open)
                        {
                            ImGui::TreePop();
                        }
                    }
                }
            }
            break;
        }
        case 3:
        {
            // Tags view
            GitService* service = GitService::Get();
            GitRepository* repo = service ? service->GetCurrentRepo() : nullptr;
            if (repo)
            {
                std::vector<GitTagInfo> tags = repo->GetTags();

                ImGui::Text("Tags (%d)", (int)tags.size());
                ImGui::SameLine();
                if (ImGui::Button("Create Tag..."))
                {
                    GetGitCreateTagDialog()->Open();
                }
                if (!tags.empty())
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Push All Tags"))
                    {
                        GitOperationQueue* queue = service->GetOperationQueue();
                        if (queue)
                        {
                            GitOperationRequest req;
                            req.mKind = GitOperationKind::PushTags;
                            req.mRepoPath = repo->GetPath();
                            std::vector<GitRemoteInfo> remotes = repo->GetRemotes();
                            if (!remotes.empty())
                                req.mRemoteName = remotes[0].mName;
                            req.mCancelToken = CreateCancelToken();
                            queue->Enqueue(req);
                            mLogEntries.push_back({"Pushing all tags...", 0});
                        }
                    }
                }
                ImGui::Separator();

                for (const auto& tag : tags)
                {
                    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
                    bool open = ImGui::TreeNodeEx(tag.mName.c_str(), nodeFlags);

                    if (ImGui::IsItemHovered() && !tag.mMessage.empty())
                    {
                        ImGui::SetTooltip("%s", tag.mMessage.c_str());
                    }

                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Checkout"))
                        {
                            repo->CheckoutCommit(tag.mTargetOid);
                            repo->RefreshStatus();
                            mDiffDirty = true;
                            mLogEntries.push_back({"Checked out tag: " + tag.mName, 0});
                        }
                        if (ImGui::MenuItem("Push Tag"))
                        {
                            GitOperationQueue* queue = service->GetOperationQueue();
                            if (queue)
                            {
                                GitOperationRequest req;
                                req.mKind = GitOperationKind::Push;
                                req.mRepoPath = repo->GetPath();
                                req.mBranchName = "refs/tags/" + tag.mName;
                                std::vector<GitRemoteInfo> remotes = repo->GetRemotes();
                                if (!remotes.empty())
                                    req.mRemoteName = remotes[0].mName;
                                req.mCancelToken = CreateCancelToken();
                                queue->Enqueue(req);
                                mLogEntries.push_back({"Pushing tag: " + tag.mName, 0});
                            }
                        }
                        if (ImGui::MenuItem("Create Branch From Tag..."))
                        {
                            GetGitCreateBranchDialog()->Open(tag.mTargetOid);
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Delete"))
                        {
                            repo->DeleteTag(tag.mName);
                            mLogEntries.push_back({"Deleted tag: " + tag.mName, 0});
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Copy Name"))
                        {
                            ImGui::SetClipboardText(tag.mName.c_str());
                        }
                        ImGui::EndPopup();
                    }

                    if (open)
                    {
                        ImGui::TreePop();
                    }
                }
            }
            break;
        }
        case 4:
        {
            // Remotes view
            GitService* service = GitService::Get();
            GitRepository* repo = service ? service->GetCurrentRepo() : nullptr;
            if (repo)
            {
                std::vector<GitRemoteInfo> remotes = repo->GetRemotes();

                ImGui::Text("Remotes (%d)", (int)remotes.size());
                ImGui::SameLine();
                if (ImGui::Button("Add Remote..."))
                {
                    GetGitRemoteEditDialog()->SetMode(GitRemoteEditDialog::Mode::Add);
                    GetGitRemoteEditDialog()->Open();
                }
                ImGui::Separator();

                for (const auto& remote : remotes)
                {
                    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
                    bool open = ImGui::TreeNodeEx(remote.mName.c_str(), nodeFlags);

                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Edit Remote..."))
                        {
                            GetGitRemoteEditDialog()->SetMode(
                                GitRemoteEditDialog::Mode::Edit,
                                remote.mName,
                                remote.mFetchUrl);
                            GetGitRemoteEditDialog()->Open();
                        }
                        if (ImGui::MenuItem("Remove"))
                        {
                            repo->RemoveRemote(remote.mName);
                            mLogEntries.push_back({"Removed remote: " + remote.mName, 0});
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Copy URL"))
                        {
                            ImGui::SetClipboardText(remote.mFetchUrl.c_str());
                        }
                        ImGui::EndPopup();
                    }

                    if (open)
                    {
                        ImGui::Text("URL: %s", remote.mFetchUrl.c_str());
                        if (remote.mIsDefault)
                        {
                            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "(default)");
                        }
                        ImGui::TreePop();
                    }
                }
            }
            break;
        }
        default:
            break;
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// History View
// ---------------------------------------------------------------------------

void GitWorkspaceWindow::DrawHistoryView(float width, float height)
{
    GitService* service = GitService::Get();
    GitRepository* repo = service ? service->GetCurrentRepo() : nullptr;
    if (!repo)
    {
        return;
    }

    // Filter input
    ImGui::SetNextItemWidth(width * 0.5f);
    ImGui::InputTextWithHint("##HistoryFilter", "Filter commits...", mHistoryFilterText, sizeof(mHistoryFilterText));
    ImGui::SameLine();
    ImGui::Checkbox("All branches", &mShowAllBranches);

    ImGui::Separator();

    std::string branchFilter = mShowAllBranches ? "" : repo->GetCurrentBranch();
    std::vector<GitCommitInfo> commits = repo->GetCommits(mHistoryPageCount, branchFilter);

    // Apply text filter
    std::string filterStr(mHistoryFilterText);
    std::vector<const GitCommitInfo*> filtered;
    filtered.reserve(commits.size());
    for (const auto& c : commits)
    {
        if (!filterStr.empty())
        {
            bool matchSummary = c.mSummary.find(filterStr) != std::string::npos;
            bool matchAuthor = c.mAuthorName.find(filterStr) != std::string::npos;
            bool matchOid = c.mShortOid.find(filterStr) != std::string::npos;
            if (!matchSummary && !matchAuthor && !matchOid)
            {
                continue;
            }
        }
        filtered.push_back(&c);
    }

    // Columns: Subject | Author | Date | Hash
    float availHeight = ImGui::GetContentRegionAvail().y;
    float commitListHeight = availHeight * 0.5f;

    ImGui::BeginChild("##CommitList", ImVec2(width, commitListHeight), false);

    if (ImGui::BeginTable("##CommitTable", 6, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Subject", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Tags", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Author", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Hash", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)filtered.size());
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const GitCommitInfo* commit = filtered[row];

                ImGui::TableNextRow();

                // Subject column
                ImGui::TableSetColumnIndex(0);
                bool isSelected = (mSelectedCommitIndex == row);

                if (ImGui::Selectable(commit->mSummary.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    mSelectedCommitIndex = row;
                    mSelectedCommitOid = commit->mOid;
                    mSelectedFilePath.clear();
                    mDiffDirty = true;
                }

                // Right-click context menu
                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Copy Hash"))
                    {
                        ImGui::SetClipboardText(commit->mOid.c_str());
                    }
                    if (ImGui::MenuItem("Create Branch..."))
                    {
                        GetGitCreateBranchDialog()->Open(commit->mOid);
                    }
                    if (ImGui::MenuItem("Create Tag..."))
                    {
                        GetGitCreateTagDialog()->Open(commit->mOid);
                    }
                    if (ImGui::MenuItem("Merge Into Current Branch..."))
                    {
                        GetGitMergeDialog()->Open(commit->mOid, commit->mSummary);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Checkout..."))
                    {
                        // Check if any branches point at this commit
                        std::vector<std::string> branchesAtCommit;
                        for (const auto& ref : commit->mRefNames)
                        {
                            if (ref.rfind("tag: ", 0) != 0) // skip tags
                                branchesAtCommit.push_back(ref);
                        }

                        if (branchesAtCommit.empty())
                        {
                            // No branches — detached checkout
                            repo->CheckoutCommit(commit->mOid);
                            repo->RefreshStatus();
                            mDiffDirty = true;
                            mLogEntries.push_back({"Checked out commit (detached): " + commit->mShortOid, 0});
                        }
                        else
                        {
                            // Branches exist — show choice popup
                            mCheckoutChoiceOid = commit->mOid;
                            mCheckoutChoiceBranches = branchesAtCommit;
                            mCheckoutChoiceSelected = 0;
                            mCheckoutChoiceOpen = true;
                        }
                    }
                    ImGui::EndPopup();
                }

                // Refs column (HEAD + branches)
                ImGui::TableSetColumnIndex(1);
                if (commit->mIsHead)
                {
                    ImVec4 headColor(0.9f, 0.3f, 0.3f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, headColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, headColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, headColor);
                    ImGui::SmallButton("HEAD");
                    ImGui::PopStyleColor(3);
                    ImGui::SameLine();
                }
                for (const auto& ref : commit->mRefNames)
                {
                    if (ref.rfind("tag: ", 0) == 0)
                        continue; // tags go in the Tags column

                    ImVec4 badgeColor;
                    if (ref.find('/') != std::string::npos)
                        badgeColor = ImVec4(0.2f, 0.6f, 0.8f, 1.0f); // remote — cyan
                    else
                        badgeColor = ImVec4(0.2f, 0.7f, 0.3f, 1.0f); // local — green

                    ImGui::PushStyleColor(ImGuiCol_Button, badgeColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, badgeColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, badgeColor);
                    ImGui::SmallButton(ref.c_str());
                    ImGui::PopStyleColor(3);
                    ImGui::SameLine();
                }

                // Tags column
                ImGui::TableSetColumnIndex(2);
                for (const auto& ref : commit->mRefNames)
                {
                    if (ref.rfind("tag: ", 0) != 0)
                        continue; // only tags here

                    std::string tagName = ref.substr(5); // strip "tag: " prefix
                    ImVec4 tagColor(0.8f, 0.7f, 0.1f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, tagColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, tagColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, tagColor);
                    ImGui::SmallButton(tagName.c_str());
                    ImGui::PopStyleColor(3);
                    ImGui::SameLine();
                }

                // Author
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(commit->mAuthorName.c_str());

                // Date
                ImGui::TableSetColumnIndex(4);
                std::string dateStr = FormatTimestamp(commit->mTimestamp);
                ImGui::TextUnformatted(dateStr.c_str());

                // Hash
                ImGui::TableSetColumnIndex(5);
                ImGui::TextDisabled("%s", commit->mShortOid.c_str());
            }
        }
        clipper.End();

        ImGui::EndTable();
    }

    ImGui::EndChild();

    // Show more button
    if ((int)commits.size() >= mHistoryPageCount)
    {
        if (ImGui::Button("Load More"))
        {
            mHistoryPageCount += 100;
        }
    }

    ImGui::Separator();

    // Diff inspector for selected commit
    if (!mSelectedCommitOid.empty())
    {
        float diffHeight = ImGui::GetContentRegionAvail().y;
        if (diffHeight > 20.0f)
        {
            if (mDiffDirty)
            {
                mCurrentDiff = repo->GetCommitDiff(mSelectedCommitOid);
                mDiffDirty = false;
            }
            DrawDiffInspector(width, diffHeight);
        }
    }
}

// ---------------------------------------------------------------------------
// Status View
// ---------------------------------------------------------------------------

void GitWorkspaceWindow::DrawStatusView(float width, float height)
{
    GitService* service = GitService::Get();
    GitRepository* repo = service ? service->GetCurrentRepo() : nullptr;
    if (!repo)
    {
        return;
    }

    const std::vector<GitStatusEntry>& status = repo->GetStatus();

    // Separate staged and unstaged
    std::vector<const GitStatusEntry*> staged;
    std::vector<const GitStatusEntry*> unstaged;
    for (const auto& entry : status)
    {
        if (entry.mStagedState == GitStagedState::Staged || entry.mStagedState == GitStagedState::Both)
        {
            staged.push_back(&entry);
        }
        if (entry.mStagedState == GitStagedState::Unstaged || entry.mStagedState == GitStagedState::Both)
        {
            unstaged.push_back(&entry);
        }
    }

    // Calculate layout: file lists take upper portion, commit composer takes lower portion
    float composerHeight = 200.0f;
    float fileListHeight = height - composerHeight - ImGui::GetStyle().ItemSpacing.y;
    if (fileListHeight < 100.0f)
    {
        fileListHeight = height * 0.5f;
        composerHeight = height * 0.5f;
    }

    // ---- Staged Changes ----
    ImGui::BeginChild("##FileStatusLists", ImVec2(width, fileListHeight), false);

    float halfHeight = (ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y) * 0.5f;

    // Staged header with unstage all button
    ImGui::Text("Staged Changes (%d)", (int)staged.size());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80.0f);
    if (ImGui::SmallButton("Unstage All"))
    {
        std::vector<std::string> paths;
        for (const auto* entry : staged)
        {
            paths.push_back(entry->mPath);
        }
        if (!paths.empty())
        {
            repo->UnstageFiles(paths);
            repo->RefreshStatus();
            mDiffDirty = true;
            mLogEntries.push_back({"Unstaged all files.", 0});
        }
    }

    ImGui::BeginChild("##StagedFiles", ImVec2(0.0f, halfHeight), true);
    for (int i = 0; i < (int)staged.size(); ++i)
    {
        const GitStatusEntry* entry = staged[i];
        ImGui::PushID(i);

        ImVec4 color = ChangeTypeColor(entry->mChangeType);
        ImGui::TextColored(color, "[%s]", ChangeTypeLabel(entry->mChangeType));
        ImGui::SameLine();

        bool selected = (mSelectedFilePath == entry->mPath);
        if (ImGui::Selectable(entry->mPath.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
        {
            mSelectedFilePath = entry->mPath;
            mDiffDirty = true;

            if (ImGui::IsMouseDoubleClicked(0))
            {
                mCurrentDiff = repo->GetStagedDiff(entry->mPath);
                mDiffDirty = false;
            }
        }

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Unstage"))
            {
                repo->UnstageFiles({entry->mPath});
                repo->RefreshStatus();
                mDiffDirty = true;
            }
            if (ImGui::MenuItem("Open Diff"))
            {
                mSelectedFilePath = entry->mPath;
                mCurrentDiff = repo->GetStagedDiff(entry->mPath);
                mDiffDirty = false;
            }
            if (ImGui::MenuItem("Copy Path"))
            {
                ImGui::SetClipboardText(entry->mPath.c_str());
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
    ImGui::EndChild();

    // ---- Unstaged Changes ----
    ImGui::Text("Unstaged Changes (%d)", (int)unstaged.size());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 65.0f);
    if (ImGui::SmallButton("Stage All"))
    {
        std::vector<std::string> paths;
        for (const auto* entry : unstaged)
        {
            paths.push_back(entry->mPath);
        }
        if (!paths.empty())
        {
            repo->StageFiles(paths);
            repo->RefreshStatus();
            mDiffDirty = true;
            mLogEntries.push_back({"Staged all files.", 0});
        }
    }

    ImGui::BeginChild("##UnstagedFiles", ImVec2(0.0f, 0.0f), true);
    for (int i = 0; i < (int)unstaged.size(); ++i)
    {
        const GitStatusEntry* entry = unstaged[i];
        ImGui::PushID(10000 + i);

        ImVec4 color = ChangeTypeColor(entry->mChangeType);
        ImGui::TextColored(color, "[%s]", ChangeTypeLabel(entry->mChangeType));
        ImGui::SameLine();

        bool selected = (mSelectedFilePath == entry->mPath);
        if (ImGui::Selectable(entry->mPath.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
        {
            mSelectedFilePath = entry->mPath;
            mDiffDirty = true;

            if (ImGui::IsMouseDoubleClicked(0))
            {
                mCurrentDiff = repo->GetWorkingTreeDiff(entry->mPath);
                mDiffDirty = false;
            }
        }

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Stage"))
            {
                repo->StageFiles({entry->mPath});
                repo->RefreshStatus();
                mDiffDirty = true;
            }
            if (ImGui::MenuItem("Discard Changes"))
            {
                repo->DiscardFiles({entry->mPath});
                repo->RefreshStatus();
                mDiffDirty = true;
                mLogEntries.push_back({"Discarded changes: " + entry->mPath, 1});
            }
            if (ImGui::MenuItem("Open Diff"))
            {
                mSelectedFilePath = entry->mPath;
                mCurrentDiff = repo->GetWorkingTreeDiff(entry->mPath);
                mDiffDirty = false;
            }
            if (ImGui::MenuItem("Copy Path"))
            {
                ImGui::SetClipboardText(entry->mPath.c_str());
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::EndChild(); // ##FileStatusLists

    // ---- Diff inspector or Commit Composer ----
    if (!mSelectedFilePath.empty())
    {
        if (mDiffDirty)
        {
            // Determine if file is staged or unstaged to pick the right diff
            bool isStaged = false;
            for (const auto* entry : staged)
            {
                if (entry->mPath == mSelectedFilePath)
                {
                    isStaged = true;
                    break;
                }
            }
            if (isStaged)
            {
                mCurrentDiff = repo->GetStagedDiff(mSelectedFilePath);
            }
            else
            {
                mCurrentDiff = repo->GetWorkingTreeDiff(mSelectedFilePath);
            }
            mDiffDirty = false;
        }
        DrawDiffInspector(width, composerHeight * 0.5f);
        DrawCommitComposer(width, composerHeight * 0.5f);
    }
    else
    {
        DrawCommitComposer(width, composerHeight);
    }
}

// ---------------------------------------------------------------------------
// Diff Inspector
// ---------------------------------------------------------------------------

void GitWorkspaceWindow::DrawDiffInspector(float width, float height)
{
    ImGui::BeginChild("##DiffInspector", ImVec2(width, height), true, ImGuiWindowFlags_HorizontalScrollbar);

    if (mCurrentDiff.mHunks.empty())
    {
        if (mCurrentDiff.mIsBinary)
        {
            ImGui::TextDisabled("Binary file — diff not available.");
        }
        else
        {
            ImGui::TextDisabled("No diff to display.");
        }
        ImGui::EndChild();
        return;
    }

    // Header
    if (!mCurrentDiff.mOldPath.empty() || !mCurrentDiff.mNewPath.empty())
    {
        std::string diffHeader;
        if (mCurrentDiff.mOldPath == mCurrentDiff.mNewPath || mCurrentDiff.mOldPath.empty())
        {
            diffHeader = mCurrentDiff.mNewPath;
        }
        else
        {
            diffHeader = mCurrentDiff.mOldPath + " -> " + mCurrentDiff.mNewPath;
        }
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", diffHeader.c_str());
        ImGui::Separator();
    }

    // Render hunks
    ImVec4 colorAdd(0.3f, 0.9f, 0.3f, 1.0f);
    ImVec4 colorDel(0.9f, 0.3f, 0.3f, 1.0f);
    ImVec4 colorCtx(0.7f, 0.7f, 0.7f, 1.0f);
    ImVec4 colorHunk(0.3f, 0.8f, 0.9f, 1.0f);

    for (const auto& hunk : mCurrentDiff.mHunks)
    {
        ImGui::TextColored(colorHunk, "%s", hunk.mHeader.c_str());

        for (const auto& line : hunk.mLines)
        {
            ImVec4 lineColor;
            switch (line.mOrigin)
            {
                case '+': lineColor = colorAdd; break;
                case '-': lineColor = colorDel; break;
                default:  lineColor = colorCtx; break;
            }

            // Line numbers
            char lineNoBuf[32];
            if (line.mOldLineNo >= 0 && line.mNewLineNo >= 0)
            {
                snprintf(lineNoBuf, sizeof(lineNoBuf), "%4d %4d ", line.mOldLineNo, line.mNewLineNo);
            }
            else if (line.mOldLineNo >= 0)
            {
                snprintf(lineNoBuf, sizeof(lineNoBuf), "%4d      ", line.mOldLineNo);
            }
            else if (line.mNewLineNo >= 0)
            {
                snprintf(lineNoBuf, sizeof(lineNoBuf), "     %4d ", line.mNewLineNo);
            }
            else
            {
                snprintf(lineNoBuf, sizeof(lineNoBuf), "          ");
            }

            ImGui::TextDisabled("%s", lineNoBuf);
            ImGui::SameLine();
            ImGui::TextColored(lineColor, "%c %s", line.mOrigin, line.mContent.c_str());
        }
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Commit Composer
// ---------------------------------------------------------------------------

void GitWorkspaceWindow::DrawCommitComposer(float width, float height)
{
    ImGui::BeginChild("##CommitComposer", ImVec2(width, height), true);

    GitService* service = GitService::Get();
    GitRepository* repo = service ? service->GetCurrentRepo() : nullptr;
    GitCliProbe* probe = service ? service->GetCliProbe() : nullptr;

    // Author identity (read-only)
    if (probe)
    {
        std::string authorLine = probe->GetGitUserName();
        if (!probe->GetUserEmail().empty())
        {
            authorLine += " <" + probe->GetUserEmail() + ">";
        }
        ImGui::TextDisabled("Author: %s", authorLine.c_str());
    }

    // Summary
    ImGui::Text("Summary:");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputText("##CommitSummary", mCommitSummary, sizeof(mCommitSummary));

    // Body
    ImGui::Text("Description:");
    float bodyHeight = ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() * 2.0f - 8.0f;
    if (bodyHeight < 40.0f)
    {
        bodyHeight = 40.0f;
    }
    ImGui::InputTextMultiline("##CommitBody", mCommitBody, sizeof(mCommitBody), ImVec2(ImGui::GetContentRegionAvail().x, bodyHeight));

    // Amend checkbox
    ImGui::Checkbox("Amend last commit", &mAmendCommit);
    ImGui::SameLine();

    // Count staged files
    int stagedCount = 0;
    if (repo)
    {
        const std::vector<GitStatusEntry>& status = repo->GetStatus();
        for (const auto& entry : status)
        {
            if (entry.mStagedState == GitStagedState::Staged || entry.mStagedState == GitStagedState::Both)
            {
                ++stagedCount;
            }
        }
    }

    // Commit button
    bool canCommit = (strlen(mCommitSummary) > 0) && (stagedCount > 0 || mAmendCommit);
    if (!canCommit)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Commit"))
    {
        if (repo)
        {
            std::string body(mCommitBody);
            if (repo->Commit(std::string(mCommitSummary), body, mAmendCommit))
            {
                mLogEntries.push_back({"Committed: " + std::string(mCommitSummary), 0});
                memset(mCommitSummary, 0, sizeof(mCommitSummary));
                memset(mCommitBody, 0, sizeof(mCommitBody));
                mAmendCommit = false;
                repo->RefreshStatus();
                mDiffDirty = true;
            }
            else
            {
                mLogEntries.push_back({"Commit failed.", 2});
            }
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Commit & Push"))
    {
        if (repo)
        {
            std::string body(mCommitBody);
            if (repo->Commit(std::string(mCommitSummary), body, mAmendCommit))
            {
                mLogEntries.push_back({"Committed: " + std::string(mCommitSummary), 0});
                memset(mCommitSummary, 0, sizeof(mCommitSummary));
                memset(mCommitBody, 0, sizeof(mCommitBody));
                mAmendCommit = false;
                repo->RefreshStatus();
                mDiffDirty = true;

                // Enqueue push
                GitOperationQueue* queue = service->GetOperationQueue();
                if (queue)
                {
                    GitOperationRequest req;
                    req.mKind = GitOperationKind::Push;
                    req.mRepoPath = repo->GetPath();
                    req.mBranchName = repo->GetCurrentBranch();
                    std::vector<GitRemoteInfo> pushRemotes = repo->GetRemotes();
                    if (!pushRemotes.empty())
                        req.mRemoteName = pushRemotes[0].mName;
                    req.mCancelToken = CreateCancelToken();
                    queue->Enqueue(req);
                    mLogEntries.push_back({"Push started...", 0});
                }
            }
            else
            {
                mLogEntries.push_back({"Commit failed.", 2});
            }
        }
    }

    if (!canCommit)
    {
        ImGui::EndDisabled();
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Output Log
// ---------------------------------------------------------------------------

void GitWorkspaceWindow::DrawOutputLog(float width, float height)
{
    ImGui::BeginChild("##OutputLog", ImVec2(width, height), true, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& entry : mLogEntries)
    {
        ImVec4 color;
        switch (entry.mLevel)
        {
            case 1:  color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); break; // warn
            case 2:  color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break; // error
            default: color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); break; // info
        }
        ImGui::TextColored(color, "%s", entry.mText.c_str());
    }

    if (mLogAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

#endif
