#if EDITOR

#include "GitStatusBarWidget.h"
#include "GitService.h"
#include "GitRepository.h"
#include "GitCliProbe.h"
#include "imgui.h"

void GitStatusBarWidget::Draw()
{
    GitService* service = GitService::Get();
    if (service == nullptr || !service->IsRepositoryOpen())
    {
        return;
    }

    GitRepository* repo = service->GetCurrentRepo();
    if (repo == nullptr)
    {
        return;
    }

    std::string branch = repo->GetCurrentBranch();

    const std::vector<GitStatusEntry>& status = repo->GetStatus();
    int32_t modifiedCount = 0;
    for (const auto& entry : status)
    {
        if (entry.mChangeType != GitChangeType::Ignored)
        {
            ++modifiedCount;
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    if (modifiedCount > 0)
    {
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Git: %s | %d modified", branch.c_str(), modifiedCount);
    }
    else
    {
        ImGui::Text("Git: %s", branch.c_str());
    }
}

#endif
