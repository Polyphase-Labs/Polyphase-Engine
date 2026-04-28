#pragma once

#if EDITOR

#include "GitTypes.h"
#include <string>
#include <vector>
#include <cstdint>

class GitWorkspaceWindow
{
public:
    void Open();
    void Close();
    bool IsOpen() const;
    void Draw();

private:
    bool mIsOpen = false;

    // Selection state shared across sub-views
    int32_t mSelectedCommitIndex = -1;
    std::string mSelectedCommitOid;
    std::string mSelectedFilePath;
    int32_t mActiveSidebarSection = 0; // 0=FileStatus, 1=History, 2=Branches, 3=Tags, 4=Remotes
    char mHistoryFilterText[256] = {0};
    bool mShowAllBranches = false;
    int32_t mHistoryPageCount = 100;

    // Sub-view draw methods (all inline in the .cpp)
    void DrawToolbar();
    void DrawSidebar(float width, float height);
    void DrawMainArea(float width, float height);
    void DrawHistoryView(float width, float height);
    void DrawStatusView(float width, float height);
    void DrawDiffInspector(float width, float height);
    void DrawCommitComposer(float width, float height);
    void DrawOutputLog(float width, float height);

    // Commit composer state
    char mCommitSummary[256] = {0};
    char mCommitBody[4096] = {0};
    bool mAmendCommit = false;

    // Output log
    struct LogEntry { std::string mText; int32_t mLevel; }; // 0=info, 1=warn, 2=error
    std::vector<LogEntry> mLogEntries;
    bool mLogAutoScroll = true;

    // Cached diff for display
    GitFileDiff mCurrentDiff;
    bool mDiffDirty = true;

    // Checkout choice popup state
    bool mCheckoutChoiceOpen = false;
    std::string mCheckoutChoiceOid;
    std::vector<std::string> mCheckoutChoiceBranches;
    int32_t mCheckoutChoiceSelected = 0;

    void DrawCheckoutChoicePopup();
};

GitWorkspaceWindow* GetGitWorkspaceWindow();

#endif
