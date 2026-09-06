#if EDITOR

#include "CharacterCreatorDialog.h"
#include "CharacterRigBuilder.h"

#include "EditorWidgets.h"
#include "EditorState.h"
#include "EditorConstants.h"
#include "EditorUtils.h"
#include "ActionManager.h"
#include "Engine.h"
#include "World.h"
#include "Log.h"
#include "AssetManager.h"
#include "AssetDir.h"
#include "AssetRef.h"
#include "Assets/SkeletalMesh.h"
#include "Assets/Scene.h"
#include "InputDevices.h"
#include "System/System.h"

#include "imgui.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

// ===== Dialog state =====

static bool sOpen = false;
static bool sFocusLatched = false;
static CharacterRigType sType = CharacterRigType::FirstPerson;
static char sName[128] = {};
static AssetRef sMeshRef;   // cleared whenever the dialog closes (see CloseDialog)
static float sHeight = 1.8f;
static bool sFitMeshToHeight = false;
static bool sCreateScene = false;
static char sOutputDir[512] = {};
static char sScript[256] = {};
static std::string sError;
static std::vector<std::string> sScriptList;
static bool sScriptListDirty = true;

static const char* kBuiltInScripts[] = { "FirstPersonController", "ThirdPersonController" };

// ===== Helpers =====

static std::string Trim(const char* s)
{
    std::string out = s ? s : "";
    while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) out.pop_back();
    while (!out.empty() && (out.front() == ' ' || out.front() == '\t')) out.erase(out.begin());
    return out;
}

static bool IsValidName(const std::string& name)
{
    if (name.empty()) return false;
    return name.find_first_of("/\\:*?\"<>|") == std::string::npos;
}

static void CopyToBuffer(char* dst, size_t dstSize, const std::string& src)
{
    strncpy(dst, src.c_str(), dstSize - 1);
    dst[dstSize - 1] = '\0';
}

static void CloseDialog()
{
    sOpen = false;
    sFocusLatched = false;
    sError.clear();
    // Drop the asset reference now rather than at static destruction, when
    // the AssetManager is already gone.
    sMeshRef = nullptr;
}

static void DoCreate()
{
    sError.clear();

    CharacterRigSpec spec;
    spec.mType = sType;
    spec.mName = Trim(sName);
    spec.mHeight = sHeight;
    spec.mFitMeshToHeight = sFitMeshToHeight;
    spec.mScriptFile = Trim(sScript);
    spec.mSkeletalMesh = sMeshRef.Get<SkeletalMesh>();

    // Stand the character where the user is looking.
    glm::vec3 feet = EditorGetFocusPosition();
    feet.y = 0.0f;

    ActionManager* am = ActionManager::Get();
    if (am == nullptr)
    {
        sError = "Editor is not ready.";
        return;
    }

    if (!sCreateScene)
    {
        std::string err;
        Node* spawned = CharacterRigBuilder::SpawnRigIntoScene(spec, nullptr, feet, err);
        if (spawned == nullptr)
        {
            sError = err;
            return;
        }
        LogDebug("Created %s character '%s' in the open scene.",
                 sType == CharacterRigType::ThirdPerson ? "third person" : "first person", spec.mName.c_str());
        CloseDialog();
        return;
    }

    std::string err;
    AssetDir* dir = CharacterRigBuilder::ResolveOrCreateOutputDir(sOutputDir, err);
    if (dir == nullptr)
    {
        sError = err;
        return;
    }

    AssetStub* stub = CharacterRigBuilder::SaveRigAsScene(spec, dir, err);
    if (stub == nullptr)
    {
        sError = err;
        return;
    }

    Scene* scene = stub->mAsset ? stub->mAsset->As<Scene>() : nullptr;
    World* world = GetWorld(0);
    const bool sceneOpen = (world != nullptr && world->GetRootNode() != nullptr);
    if (sceneOpen && scene != nullptr)
    {
        // The prefab root already sits at rootY above its feet; the instance
        // gets the same offset so it stands on the focus point.
        glm::vec3 pos = feet + glm::vec3(0.0f, CharacterRigBuilder::ComputeMetrics(spec).mRootY, 0.0f);
        am->BeginActionGroup("Place Character");
        Node* inst = am->SpawnBasicNode(BASIC_SCENE, nullptr, scene, true, pos);
        am->EndActionGroup();
        if (inst == nullptr)
        {
            sError = "Saved " + stub->mName + " but could not place it in the open scene.";
            return;
        }
        LogDebug("Saved character scene '%s' and placed an instance.", stub->mName.c_str());
    }
    else
    {
        // Opening a scene must not happen inside the ImGui frame.
        am->RequestOpenScene(stub);
        LogDebug("Saved character scene '%s'; opening it.", stub->mName.c_str());
    }
    CloseDialog();
}

static void RefreshScriptList()
{
    sScriptList.clear();
    if (AssetManager* am = AssetManager::Get())
    {
        sScriptList = am->GetAvailableScriptFiles();
    }
    for (std::string& s : sScriptList)
    {
        for (char& c : s) { if (c == '\\') c = '/'; }
        if (s.size() > 4 && s.compare(s.size() - 4, 4, ".lua") == 0)
        {
            s.erase(s.size() - 4);
        }
    }
    std::sort(sScriptList.begin(), sScriptList.end());
    sScriptList.erase(std::unique(sScriptList.begin(), sScriptList.end()), sScriptList.end());
    sScriptListDirty = false;
}

// ===== Draw =====

static void DrawCreateCharacterDialog()
{
    if (!sOpen)
    {
        return;
    }

    const bool thirdPerson = (sType == CharacterRigType::ThirdPerson);
    const char* title = thirdPerson ? "Create Third Person Character###CreateCharacterDialog"
                                    : "Create First Person Character###CreateCharacterDialog";

    // A regular window, not BeginPopupModal: a popup modal blocks input to
    // every other window, which kills the asset picker's autocomplete and
    // asset-browser drag-drop (see DrawRetargetAnimationModal).
    ImGui::SetNextWindowSize(ImVec2(580, 0), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!sFocusLatched)
    {
        ImGui::SetNextWindowFocus();
        sFocusLatched = true;
    }

    if (!ImGui::Begin(title, &sOpen, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        if (!sOpen) CloseDialog();
        return;
    }

    const float kLabelW = 140.0f;
    const float kFieldW = 420.0f;

    // Name
    ImGui::Text("Name *");
    ImGui::SameLine(kLabelW);
    ImGui::SetNextItemWidth(kFieldW);
    ImGui::InputText("##Name", sName, sizeof(sName));

    // Skeletal mesh
    ImGui::Text("Skeletal Mesh");
    ImGui::SameLine(kLabelW);
    ImGui::PushItemWidth(kFieldW);
    Polyphase::AssetRefPicker("##SkeletalMesh", sMeshRef, SkeletalMesh::GetStaticType());
    ImGui::PopItemWidth();
    ImGui::Indent(kLabelW);
    ImGui::TextDisabled(thirdPerson ? "Recommended. Placed with its origin on the capsule bottom (feet on the ground)."
                                    : "Optional. Added as a child mesh with its origin on the capsule bottom.");
    Polyphase::Checkbox("Fit mesh to height", &sFitMeshToHeight);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Scale the mesh so its bind pose is exactly Height tall.\nOff = scale 1, the mesh keeps its authored size.");
    }
    ImGui::Unindent(kLabelW);

    // Height
    ImGui::Text("Height (m)");
    ImGui::SameLine(kLabelW);
    ImGui::SetNextItemWidth(kFieldW);
    ImGui::DragFloat("##Height", &sHeight, 0.01f, 0.3f, 10.0f, "%.2f");
    {
        CharacterRigSpec preview;
        preview.mType = sType;
        preview.mHeight = sHeight;
        CharacterRigMetrics m = CharacterRigBuilder::ComputeMetrics(preview);
        ImGui::Indent(kLabelW);
        // Capsule3D's Height property is the cylinder section only; the two
        // hemispherical caps add a radius each. Spell that out so the 1.08
        // shown in the inspector for a 1.80 m character is not a surprise.
        ImGui::TextDisabled("Capsule: Height %.2f + 2 x radius %.2f caps = %.2f m total.",
                            m.mCapsuleSegment, m.mRadius, m.mHeight);
        if (thirdPerson)
        {
            ImGui::TextDisabled("Camera pivot at %.2f m, camera %.2f m behind.",
                                m.mRootY + m.mPivotLocalY, m.mCameraDistance);
        }
        else
        {
            ImGui::TextDisabled("Eye height %.2f m.", m.mRootY + m.mEyeLocalY);
        }
        ImGui::Unindent(kLabelW);
    }

    // Script
    ImGui::Text("Script *");
    ImGui::SameLine(kLabelW);
    ImGui::SetNextItemWidth(kFieldW - 30.0f);
    ImGui::InputText("##Script", sScript, sizeof(sScript));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_SCRIPT))
        {
            std::string scriptName((const char*)payload->Data, payload->DataSize - 1);
            CopyToBuffer(sScript, sizeof(sScript), scriptName);
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::SetNextItemWidth(26.0f);
    if (ImGui::BeginCombo("##ScriptList", "", ImGuiComboFlags_NoPreview))
    {
        if (sScriptListDirty)
        {
            RefreshScriptList();
        }
        for (const char* builtIn : kBuiltInScripts)
        {
            std::string label = std::string(builtIn) + "  (engine)";
            if (ImGui::Selectable(label.c_str(), strcmp(sScript, builtIn) == 0))
            {
                CopyToBuffer(sScript, sizeof(sScript), builtIn);
            }
        }
        if (!sScriptList.empty())
        {
            ImGui::Separator();
        }
        for (const std::string& s : sScriptList)
        {
            if (ImGui::Selectable(s.c_str(), s == sScript))
            {
                CopyToBuffer(sScript, sizeof(sScript), s);
            }
        }
        ImGui::EndCombo();
    }
    else
    {
        sScriptListDirty = true;
    }
    ImGui::Indent(kLabelW);
    ImGui::TextDisabled("Existing Lua script attached to the Controller node. Drag one from the Scripts panel or pick from the list.");
    ImGui::Unindent(kLabelW);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Create scene + output directory
    Polyphase::Checkbox("Create Scene asset (reusable prefab; an instance is placed in the open scene)", &sCreateScene);

    ImGui::BeginDisabled(!sCreateScene);
    ImGui::Text("Output Directory");
    ImGui::SameLine(kLabelW);
    ImGui::SetNextItemWidth(kFieldW - 90.0f);
    ImGui::InputText("##OutDir", sOutputDir, sizeof(sOutputDir));
    ImGui::SameLine();
    if (ImGui::Button("Browse...", ImVec2(80, 0)))
    {
        std::string folder = SYS_SelectFolderDialog();
        if (!folder.empty())
        {
            std::string rel;
            if (CharacterRigBuilder::ToProjectRelativeAssetPath(folder, rel))
            {
                CopyToBuffer(sOutputDir, sizeof(sOutputDir), rel);
                sError.clear();
            }
            else
            {
                sError = "The folder must be inside the project's asset directory.";
            }
        }
    }
    ImGui::Indent(kLabelW);
    {
        std::string root = CharacterRigBuilder::GetProjectAssetRoot();
        std::string shown = root.empty() ? std::string("(no project)") : root + Trim(sOutputDir);
        ImGui::TextDisabled("%s", shown.c_str());
    }
    ImGui::Unindent(kLabelW);
    ImGui::EndDisabled();

    ImGui::Spacing();

    if (!sError.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::TextWrapped("%s", sError.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // Buttons
    const bool hasProject = !GetEngineState()->mProjectDirectory.empty();
    const std::string nameStr = Trim(sName);
    const char* blockReason = nullptr;
    if (!hasProject)                    blockReason = "Open a project first.";
    else if (IsPlayingInEditor())       blockReason = "Stop play-in-editor first.";
    else if (nameStr.empty())           blockReason = "Enter a name.";
    else if (!IsValidName(nameStr))     blockReason = "The name cannot contain / \\ : * ? \" < > |";
    else if (Trim(sScript).empty())     blockReason = "Choose a script.";

    ImGui::BeginDisabled(blockReason != nullptr);
    if (ImGui::Button("Create", ImVec2(100, 0)))
    {
        DoCreate();
    }
    ImGui::EndDisabled();
    if (blockReason != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("%s", blockReason);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0)))
    {
        CloseDialog();
    }

    if (sOpen && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && IsKeyJustDown(POLYPHASE_KEY_ESCAPE))
    {
        CloseDialog();
    }

    ImGui::End();

    if (!sOpen)
    {
        CloseDialog();
    }
}

// ===== Public API =====

void OpenCreateCharacterDialog(CharacterRigType type)
{
    sOpen = true;
    sFocusLatched = false;
    sType = type;
    const bool thirdPerson = (type == CharacterRigType::ThirdPerson);

    CopyToBuffer(sName, sizeof(sName), thirdPerson ? "ThirdPersonPlayer" : "FirstPersonPlayer");
    CopyToBuffer(sScript, sizeof(sScript), thirdPerson ? "ThirdPersonController" : "FirstPersonController");
    sMeshRef = nullptr;
    sHeight = 1.8f;
    sFitMeshToHeight = false;
    sCreateScene = false;
    sError.clear();
    sScriptListDirty = true;

    // Default the output directory to where the asset browser is.
    sOutputDir[0] = '\0';
    if (EditorState* es = GetEditorState())
    {
        AssetDir* cur = es->GetAssetDirectory();
        std::string rel;
        if (cur != nullptr && !cur->mEngineDir && CharacterRigBuilder::ToProjectRelativeAssetPath(cur->mPath, rel))
        {
            CopyToBuffer(sOutputDir, sizeof(sOutputDir), rel);
        }
    }
}

void DrawCharacterCreatorDialogs()
{
    DrawCreateCharacterDialog();
}

#endif // EDITOR
