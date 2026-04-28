#if EDITOR

#include "GitCliStatusDialog.h"
#include "../GitService.h"
#include "../GitCliProbe.h"
#include "../GitProcessRunner.h"
#include "imgui.h"
#include <thread>

static GitCliStatusDialog sInstance;

GitCliStatusDialog* GetGitCliStatusDialog()
{
    return &sInstance;
}

void GitCliStatusDialog::Open()
{
    mIsOpen = true;
    mTestCompleted = false;
    mTestRunning = false;
    mTestSuccess = false;
    mTestOutput.clear();
}

void GitCliStatusDialog::Close()
{
    if (mTestRunning && mTestCancelToken)
    {
        mTestCancelToken->store(true);
    }
    mIsOpen = false;
}

bool GitCliStatusDialog::IsOpen() const
{
    return mIsOpen;
}

void GitCliStatusDialog::Draw()
{
    if (!mIsOpen) return;

    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);

    if (ImGui::Begin("Git CLI Status", &mIsOpen))
    {
        GitService* service = GitService::Get();
        GitCliProbe* probe = service ? service->GetCliProbe() : nullptr;

        if (probe == nullptr)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Git CLI probe not available.");
            ImGui::End();
            return;
        }

        // Section: Git CLI Information
        ImGui::SeparatorText("Git CLI Information");

        if (!probe->IsGitAvailable())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Git is not available on this system.");
            ImGui::Spacing();
            ImGui::TextWrapped("Polyphase requires a system git installation for network operations "
                "(push, pull, fetch, clone). Please install git and ensure it is on your PATH.");
        }
        else
        {
            // Version
            ImGui::Text("Git Version:");
            ImGui::SameLine(160);
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "%s", probe->GetGitVersion().c_str());

            // Path
            ImGui::Text("Git Path:");
            ImGui::SameLine(160);
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "%s", probe->GetGitPath().c_str());

            // User name
            ImGui::Text("User Name:");
            ImGui::SameLine(160);
            const std::string& userName = probe->GetGitUserName();
            if (userName.empty())
            {
                ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "(not set)");
            }
            else
            {
                ImGui::Text("%s", userName.c_str());
            }

            // User email
            ImGui::Text("User Email:");
            ImGui::SameLine(160);
            const std::string& userEmail = probe->GetUserEmail();
            if (userEmail.empty())
            {
                ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "(not set)");
            }
            else
            {
                ImGui::Text("%s", userEmail.c_str());
            }

            // Credential helper
            ImGui::Text("Credential Helper:");
            ImGui::SameLine(160);
            const std::string& credHelper = probe->GetCredentialHelper();
            if (credHelper.empty())
            {
                ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "(none)");
            }
            else
            {
                ImGui::Text("%s", credHelper.c_str());
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Section: About
        ImGui::SeparatorText("About Network Operations");
        ImGui::TextWrapped(
            "Polyphase uses your system's git installation for network operations "
            "(push, pull, fetch, clone). This allows Polyphase to leverage your existing "
            "git credentials, SSH keys, and credential helpers without requiring separate "
            "authentication configuration."
        );
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Local operations such as staging, committing, diffing, and branch management "
            "use libgit2 directly for performance and reliability."
        );

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Section: CLI Test
        ImGui::SeparatorText("Connection Test");

        if (mTestRunning)
        {
            ImGui::BeginDisabled();
            ImGui::Button("Testing...", ImVec2(180, 0));
            ImGui::EndDisabled();
        }
        else
        {
            if (ImGui::Button("Test git CLI", ImVec2(180, 0)))
            {
                mTestRunning = true;
                mTestCompleted = false;
                mTestSuccess = false;
                mTestOutput.clear();
                mTestCancelToken = CreateCancelToken();

                GitCancelToken cancelToken = mTestCancelToken;

                std::thread testThread([this, cancelToken]()
                {
                    std::vector<std::string> args;
                    args.push_back("git");
                    args.push_back("--version");

                    std::string output;
                    int32_t exitCode = GitProcessRunner::Run(
                        ".",
                        args,
                        [&output](const std::string& line) { output += line + "\n"; },
                        [&output](const std::string& line) { output += line + "\n"; },
                        cancelToken
                    );

                    if (cancelToken && cancelToken->load())
                    {
                        mTestOutput = "Test cancelled.";
                        mTestSuccess = false;
                    }
                    else if (exitCode == 0)
                    {
                        mTestOutput = "Git CLI is working correctly.\n" + output;
                        mTestSuccess = true;
                    }
                    else
                    {
                        mTestOutput = "Git CLI test failed (exit code " + std::to_string(exitCode) + ").\n" + output;
                        mTestSuccess = false;
                    }

                    mTestCompleted = true;
                    mTestRunning = false;
                });

                testThread.detach();
            }
        }

        if (mTestCompleted)
        {
            ImGui::Spacing();
            if (mTestSuccess)
            {
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "%s", mTestOutput.c_str());
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", mTestOutput.c_str());
            }
        }
    }
    ImGui::End();
}

#endif
