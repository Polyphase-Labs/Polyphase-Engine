#if EDITOR

#include "ExternalModule.h"
#include "EditorWidgets.h"
#include "../JsonSettings.h"

#include "document.h"
#include "imgui.h"

DEFINE_PREFERENCES_MODULE(ExternalModule, "External", "")

ExternalModule::ExternalModule()
{
}

ExternalModule::~ExternalModule()
{
}

void ExternalModule::Render()
{
    bool changed = false;

    ImGui::Text("Build Tools");
    ImGui::Separator();
    ImGui::Spacing();

    // Docker Path
    ImGui::Text("Docker Path:");
    ImGui::SetNextItemWidth(-1);
    char dockerBuffer[512];
    strncpy(dockerBuffer, mDockerPath.c_str(), sizeof(dockerBuffer) - 1);
    dockerBuffer[sizeof(dockerBuffer) - 1] = '\0';
    if (ImGui::InputText("##DockerPath", dockerBuffer, sizeof(dockerBuffer)))
    {
        mDockerPath = dockerBuffer;
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Leave empty to use 'docker' from PATH");
    }

    ImGui::Spacing();

    // Gradle Path
    ImGui::Text("Gradle Path:");
    ImGui::SetNextItemWidth(-1);
    char gradleBuffer[512];
    strncpy(gradleBuffer, mGradlePath.c_str(), sizeof(gradleBuffer) - 1);
    gradleBuffer[sizeof(gradleBuffer) - 1] = '\0';
    if (ImGui::InputText("##GradlePath", gradleBuffer, sizeof(gradleBuffer)))
    {
        mGradlePath = gradleBuffer;
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Leave empty to use 'gradle' from PATH");
    }

    ImGui::Spacing();

#if PLATFORM_MAC
    // Vulkan SDK root (macOS). The Makefiles, glslc and the .app packager all
    // read VULKAN_SDK; a Finder-launched editor has no shell environment, so
    // this lets the user pin the SDK explicitly.
    ImGui::Text("Vulkan SDK Root:");
    ImGui::SetNextItemWidth(-1);
    char vulkanBuffer[512];
    strncpy(vulkanBuffer, mVulkanSdkPath.c_str(), sizeof(vulkanBuffer) - 1);
    vulkanBuffer[sizeof(vulkanBuffer) - 1] = '\0';
    if (ImGui::InputText("##VulkanSdkPath", vulkanBuffer, sizeof(vulkanBuffer)))
    {
        mVulkanSdkPath = vulkanBuffer;
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("e.g. ~/VulkanSDK/1.4.357.1/macOS. Leave empty to use $VULKAN_SDK or the\nnewest install under ~/VulkanSDK. Takes effect on the next editor start.");
    }

    ImGui::Spacing();
#endif

#if PLATFORM_WINDOWS
    // Use WSL
    if (Polyphase::Checkbox("Use WSL", &mUseWSL))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Route Docker commands through Windows Subsystem for Linux");
    }

    ImGui::Spacing();
#endif

    ImGui::Spacing();
    ImGui::TextDisabled("Select a subcategory from the left panel for more settings.");

    if (changed)
    {
        SetDirty(true);
    }
}

void ExternalModule::LoadSettings(const rapidjson::Document& doc)
{
    mDockerPath = JsonSettings::GetString(doc, "dockerPath", "");
    mGradlePath = JsonSettings::GetString(doc, "gradlePath", "");
    mVulkanSdkPath = JsonSettings::GetString(doc, "vulkanSdkPath", "");
    mUseWSL = JsonSettings::GetBool(doc, "useWSL", false);
}

void ExternalModule::SaveSettings(rapidjson::Document& doc)
{
    JsonSettings::SetString(doc, "dockerPath", mDockerPath);
    JsonSettings::SetString(doc, "gradlePath", mGradlePath);
    JsonSettings::SetString(doc, "vulkanSdkPath", mVulkanSdkPath);
    JsonSettings::SetBool(doc, "useWSL", mUseWSL);
}

std::string ExternalModule::GetDockerCommand() const
{
    std::string cmd;

#if PLATFORM_WINDOWS
    if (mUseWSL)
    {
        cmd = "wsl ";
    }
#endif

    if (!mDockerPath.empty())
    {
        cmd += mDockerPath;
    }
    else
    {
        cmd += "docker";
    }

    return cmd;
}

std::string ExternalModule::GetGradleCommand() const
{
    if (!mGradlePath.empty())
    {
        return mGradlePath;
    }
    return "gradle";
}

#endif
