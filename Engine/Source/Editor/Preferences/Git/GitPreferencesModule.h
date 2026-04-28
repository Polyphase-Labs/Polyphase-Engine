#pragma once

#if EDITOR

#include "../PreferencesModule.h"

class GitPreferencesModule : public PreferencesModule
{
public:
    DECLARE_PREFERENCES_MODULE(GitPreferencesModule)

    GitPreferencesModule();
    virtual ~GitPreferencesModule();

    virtual const char* GetName() const override       { return GetStaticName(); }
    virtual const char* GetParentPath() const override { return GetStaticParentPath(); }
    virtual void Render() override;
    virtual void LoadSettings(const rapidjson::Document& doc) override;
    virtual void SaveSettings(rapidjson::Document& doc) override;

    // General
    bool GetAutoRefresh() const { return mAutoRefresh; }
    int32_t GetAutoFetchIntervalMinutes() const { return mAutoFetchIntervalMinutes; }
    bool GetShowRemoteBranches() const { return mShowRemoteBranches; }
    bool GetShowTags() const { return mShowTags; }
    bool GetShowIgnoredFiles() const { return mShowIgnoredFiles; }

    // Commit
    bool GetRequireNonEmptySummary() const { return mRequireNonEmptySummary; }
    bool GetPromptBeforeAmend() const { return mPromptBeforeAmend; }

    // Safety
    bool GetConfirmBeforeDiscard() const { return mConfirmBeforeDiscard; }
    bool GetConfirmBeforeHardReset() const { return mConfirmBeforeHardReset; }
    bool GetConfirmBeforeDeleteBranch() const { return mConfirmBeforeDeleteBranch; }
    bool GetConfirmBeforeDeleteRemoteTag() const { return mConfirmBeforeDeleteRemoteTag; }
    bool GetAdvancedModeEnabled() const { return mAdvancedModeEnabled; }

    // Network
    bool GetFetchTagsByDefault() const { return mFetchTagsByDefault; }
    bool GetPruneOnFetch() const { return mPruneOnFetch; }
    bool GetFastForwardOnly() const { return mFastForwardOnly; }

private:
    // General
    bool mAutoRefresh = true;
    int32_t mAutoFetchIntervalMinutes = 0;
    bool mShowRemoteBranches = true;
    bool mShowTags = true;
    bool mShowIgnoredFiles = false;

    // Commit
    bool mRequireNonEmptySummary = true;
    bool mPromptBeforeAmend = true;

    // Branching
    bool mConfirmCheckoutDirty = true;
    bool mShowDetachedHeadWarnings = true;
    bool mAutoTrackRemote = true;

    // Network
    bool mFetchTagsByDefault = true;
    bool mPruneOnFetch = false;
    bool mFastForwardOnly = false;

    // Safety
    bool mConfirmBeforeDiscard = true;
    bool mConfirmBeforeHardReset = true;
    bool mConfirmBeforeDeleteBranch = true;
    bool mConfirmBeforeDeleteRemoteTag = true;
    bool mAdvancedModeEnabled = false;
};

#endif
