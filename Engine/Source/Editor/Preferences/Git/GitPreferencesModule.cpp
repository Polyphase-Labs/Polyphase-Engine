#if EDITOR

#include "GitPreferencesModule.h"
#include "../JsonSettings.h"
#include "Git/GitCliProbe.h"
#include "Git/GitService.h"
#include "Git/Dialogs/GitCliStatusDialog.h"

#include "document.h"
#include "imgui.h"

DEFINE_PREFERENCES_MODULE(GitPreferencesModule, "Version Control", "")

GitPreferencesModule::GitPreferencesModule()
{
}

GitPreferencesModule::~GitPreferencesModule()
{
}

void GitPreferencesModule::Render()
{
    ImGui::Text("Git Version Control Settings");
    ImGui::Spacing();

    // --- General ---
    if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("Auto-refresh repository status", &mAutoRefresh))
            SetDirty(true);

        if (ImGui::InputInt("Auto-fetch interval (minutes, 0=disabled)", &mAutoFetchIntervalMinutes))
        {
            if (mAutoFetchIntervalMinutes < 0) mAutoFetchIntervalMinutes = 0;
            SetDirty(true);
        }

        if (ImGui::Checkbox("Show remote branches in history", &mShowRemoteBranches))
            SetDirty(true);

        if (ImGui::Checkbox("Show tags in history", &mShowTags))
            SetDirty(true);

        if (ImGui::Checkbox("Show ignored files", &mShowIgnoredFiles))
            SetDirty(true);
    }

    ImGui::Spacing();

    // --- Commit ---
    if (ImGui::CollapsingHeader("Commit", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("Require non-empty commit summary", &mRequireNonEmptySummary))
            SetDirty(true);

        if (ImGui::Checkbox("Prompt before amend", &mPromptBeforeAmend))
            SetDirty(true);
    }

    ImGui::Spacing();

    // --- Branching ---
    if (ImGui::CollapsingHeader("Branching"))
    {
        if (ImGui::Checkbox("Confirm before checkout with dirty files", &mConfirmCheckoutDirty))
            SetDirty(true);

        if (ImGui::Checkbox("Show detached HEAD warnings", &mShowDetachedHeadWarnings))
            SetDirty(true);

        if (ImGui::Checkbox("Auto-track remote branch on checkout", &mAutoTrackRemote))
            SetDirty(true);
    }

    ImGui::Spacing();

    // --- Network ---
    if (ImGui::CollapsingHeader("Network"))
    {
        if (ImGui::Checkbox("Fetch tags by default", &mFetchTagsByDefault))
            SetDirty(true);

        if (ImGui::Checkbox("Prune on fetch", &mPruneOnFetch))
            SetDirty(true);

        if (ImGui::Checkbox("Fast-forward only (pull)", &mFastForwardOnly))
            SetDirty(true);
    }

    ImGui::Spacing();

    // --- Credentials ---
    if (ImGui::CollapsingHeader("Credentials"))
    {
        ImGui::TextWrapped(
            "Credentials are managed by your system git installation. "
            "Polyphase does not store any passwords or keys.");
        ImGui::Spacing();

        if (ImGui::Button("Open Git CLI Status..."))
        {
            GetGitCliStatusDialog()->Open();
        }
    }

    ImGui::Spacing();

    // --- Safety ---
    if (ImGui::CollapsingHeader("Safety", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("Confirm before discard changes", &mConfirmBeforeDiscard))
            SetDirty(true);

        if (ImGui::Checkbox("Confirm before hard reset", &mConfirmBeforeHardReset))
            SetDirty(true);

        if (ImGui::Checkbox("Confirm before deleting branch", &mConfirmBeforeDeleteBranch))
            SetDirty(true);

        if (ImGui::Checkbox("Confirm before deleting remote tag", &mConfirmBeforeDeleteRemoteTag))
            SetDirty(true);

        ImGui::Spacing();

        if (ImGui::Checkbox("Enable advanced mode (force push, hard reset)", &mAdvancedModeEnabled))
            SetDirty(true);

        if (mAdvancedModeEnabled)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                "Advanced mode exposes potentially destructive operations.");
        }
    }
}

void GitPreferencesModule::LoadSettings(const rapidjson::Document& doc)
{
    mAutoRefresh = JsonSettings::GetBool(doc, "git_auto_refresh", mAutoRefresh);
    mAutoFetchIntervalMinutes = JsonSettings::GetInt(doc, "git_auto_fetch_interval", mAutoFetchIntervalMinutes);
    mShowRemoteBranches = JsonSettings::GetBool(doc, "git_show_remote_branches", mShowRemoteBranches);
    mShowTags = JsonSettings::GetBool(doc, "git_show_tags", mShowTags);
    mShowIgnoredFiles = JsonSettings::GetBool(doc, "git_show_ignored", mShowIgnoredFiles);

    mRequireNonEmptySummary = JsonSettings::GetBool(doc, "git_require_summary", mRequireNonEmptySummary);
    mPromptBeforeAmend = JsonSettings::GetBool(doc, "git_prompt_amend", mPromptBeforeAmend);

    mConfirmCheckoutDirty = JsonSettings::GetBool(doc, "git_confirm_checkout_dirty", mConfirmCheckoutDirty);
    mShowDetachedHeadWarnings = JsonSettings::GetBool(doc, "git_show_detached_head", mShowDetachedHeadWarnings);
    mAutoTrackRemote = JsonSettings::GetBool(doc, "git_auto_track_remote", mAutoTrackRemote);

    mFetchTagsByDefault = JsonSettings::GetBool(doc, "git_fetch_tags", mFetchTagsByDefault);
    mPruneOnFetch = JsonSettings::GetBool(doc, "git_prune_on_fetch", mPruneOnFetch);
    mFastForwardOnly = JsonSettings::GetBool(doc, "git_ff_only", mFastForwardOnly);

    mConfirmBeforeDiscard = JsonSettings::GetBool(doc, "git_confirm_discard", mConfirmBeforeDiscard);
    mConfirmBeforeHardReset = JsonSettings::GetBool(doc, "git_confirm_hard_reset", mConfirmBeforeHardReset);
    mConfirmBeforeDeleteBranch = JsonSettings::GetBool(doc, "git_confirm_delete_branch", mConfirmBeforeDeleteBranch);
    mConfirmBeforeDeleteRemoteTag = JsonSettings::GetBool(doc, "git_confirm_delete_remote_tag", mConfirmBeforeDeleteRemoteTag);
    mAdvancedModeEnabled = JsonSettings::GetBool(doc, "git_advanced_mode", mAdvancedModeEnabled);
}

void GitPreferencesModule::SaveSettings(rapidjson::Document& doc)
{
    JsonSettings::SetBool(doc, "git_auto_refresh", mAutoRefresh);
    JsonSettings::SetInt(doc, "git_auto_fetch_interval", mAutoFetchIntervalMinutes);
    JsonSettings::SetBool(doc, "git_show_remote_branches", mShowRemoteBranches);
    JsonSettings::SetBool(doc, "git_show_tags", mShowTags);
    JsonSettings::SetBool(doc, "git_show_ignored", mShowIgnoredFiles);

    JsonSettings::SetBool(doc, "git_require_summary", mRequireNonEmptySummary);
    JsonSettings::SetBool(doc, "git_prompt_amend", mPromptBeforeAmend);

    JsonSettings::SetBool(doc, "git_confirm_checkout_dirty", mConfirmCheckoutDirty);
    JsonSettings::SetBool(doc, "git_show_detached_head", mShowDetachedHeadWarnings);
    JsonSettings::SetBool(doc, "git_auto_track_remote", mAutoTrackRemote);

    JsonSettings::SetBool(doc, "git_fetch_tags", mFetchTagsByDefault);
    JsonSettings::SetBool(doc, "git_prune_on_fetch", mPruneOnFetch);
    JsonSettings::SetBool(doc, "git_ff_only", mFastForwardOnly);

    JsonSettings::SetBool(doc, "git_confirm_discard", mConfirmBeforeDiscard);
    JsonSettings::SetBool(doc, "git_confirm_hard_reset", mConfirmBeforeHardReset);
    JsonSettings::SetBool(doc, "git_confirm_delete_branch", mConfirmBeforeDeleteBranch);
    JsonSettings::SetBool(doc, "git_confirm_delete_remote_tag", mConfirmBeforeDeleteRemoteTag);
    JsonSettings::SetBool(doc, "git_advanced_mode", mAdvancedModeEnabled);
}

#endif
