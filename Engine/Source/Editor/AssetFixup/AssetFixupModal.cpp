#include "AssetFixup/AssetFixupModal.h"

#if EDITOR

#include "Asset.h"
#include "AssetManager.h"
#include "Datum.h"
#include "Log.h"
#include "Property.h"
#include "Nodes/Node.h"
#include "Nodes/Widgets/Widget.h"
#include "EditorState.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

// Forward-declared here -- DrawAssetProperty lives in EditorImgui.cpp and is
// the same asset picker the inspector uses, so the modal stays visually
// consistent with the rest of the editor.
void DrawAssetProperty(Property& prop, uint32_t index, Object* owner, PropertyOwnerType ownerType);

static const char* SafeTypeName(TypeId id)
{
    const char* n = Asset::GetNameFromTypeId(id);
    return n ? n : "Asset";
}

AssetFixupModal* AssetFixupModal::Get()
{
    static AssetFixupModal sInstance;
    return &sInstance;
}

void AssetFixupModal::ScanWidgetTree(Node* root)
{
    if (root == nullptr)
        return;

    root->Traverse([this](Node* n) -> bool
    {
        if (n == nullptr || !n->IsWidget())
            return true;

        std::vector<Property> props;
        n->GatherProperties(props);

        for (Property& p : props)
        {
            if (p.mType != DatumType::Asset)
                continue;
            if (p.mExtra == nullptr)
                continue;
            TypeId expected = (TypeId)p.mExtra->GetInteger();
            if (expected == 0)
                continue;

            for (uint32_t i = 0; i < p.mCount; ++i)
            {
                Asset* bound = p.GetAsset(i);
                if (bound == nullptr)
                    continue;
                // `expected` is the factory TypeId stored in the property's
                // mExtra (set at GatherProperties time, e.g. Texture::GetStaticType()).
                // Asset::GetType() returns the same factory TypeId, so compare
                // TypeId-to-TypeId. The AssetRef::Get<T>() assertion also accepts
                // Is(T::ClassRuntimeId()) -- but RuntimeId lives in a different
                // ID space than TypeId, so we cannot apply that check here from
                // the TypeId alone. Polyphase keeps asset hierarchies flat in
                // practice, so the exact-type compare matches the realistic
                // surface area.
                if (bound->GetType() == expected)
                    continue;

                BrokenAssetRef row;
                row.mNode = ResolveWeakPtr<Node>(n);
                row.mNodeDisplayName = n->GetName();
                row.mPropName = p.mName;
                row.mExpectedType = expected;
                row.mExpectedTypeName = SafeTypeName(expected);
                row.mCurrentAsset = bound;
                row.mCurrentAssetName = bound->GetName();
                row.mCurrentTypeName = SafeTypeName(bound->GetType());
                mBrokenRefs.push_back(std::move(row));
            }
        }
        return true;
    });

    if (!mBrokenRefs.empty())
    {
        mModalRequested = true;
        LogWarning("AssetFixupModal: %zu widget asset reference(s) need fixing in scene tree.",
            mBrokenRefs.size());
    }
}

void AssetFixupModal::ApplyRow(BrokenAssetRef& row, Asset* newAsset)
{
    Node* node = row.mNode.Get();
    if (node == nullptr)
    {
        row.mResolved = true;
        return;
    }

    std::vector<Property> props;
    node->GatherProperties(props);
    for (Property& p : props)
    {
        if (p.mName == row.mPropName && p.mType == DatumType::Asset)
        {
            p.SetAsset(newAsset, 0);
            row.mResolved = true;

            // Mark the owning scene dirty so the user gets the unsaved-changes
            // prompt before exiting -- the property mutation we just made
            // wouldn't otherwise survive a close-without-save.
            EditorState* es = GetEditorState();
            if (es != nullptr)
            {
                EditScene* edit = es->GetEditScene();
                if (edit != nullptr)
                {
                    Asset* sceneAsset = edit->mSceneAsset.Get();
                    if (sceneAsset != nullptr)
                    {
                        sceneAsset->SetDirtyFlag();
                    }
                }
            }
            return;
        }
    }

    LogWarning("AssetFixupModal: prop '%s' not found on node '%s' at apply time -- node may have been destroyed.",
        row.mPropName.c_str(), row.mNodeDisplayName.c_str());
    row.mResolved = true;
}

void AssetFixupModal::Draw()
{
    if (mBrokenRefs.empty())
        return;

    if (mModalRequested && !ImGui::IsPopupOpen("Fix Broken Asset References"))
    {
        ImGui::OpenPopup("Fix Broken Asset References");
        mModalRequested = false;
    }

    if (ImGui::IsPopupOpen("Fix Broken Asset References"))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.6f, io.DisplaySize.y * 0.7f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    if (!ImGui::BeginPopupModal("Fix Broken Asset References", nullptr,
            ImGuiWindowFlags_NoCollapse))
    {
        return;
    }

    ImGui::TextWrapped("These widget asset slots are bound to assets of the wrong type "
        "(usually from a name collision during import). Pick a replacement of the right "
        "type, clear the slot, or skip and investigate manually.");
    ImGui::Spacing();

    if (ImGui::BeginChild("FixupList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true))
    {
        for (size_t r = 0; r < mBrokenRefs.size(); ++r)
        {
            BrokenAssetRef& row = mBrokenRefs[r];
            ImGui::PushID((int)r);

            ImGui::Separator();
            if (row.mResolved)
            {
                ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f),
                    "[Resolved] %s . %s", row.mNodeDisplayName.c_str(), row.mPropName.c_str());
            }
            else
            {
                ImGui::Text("Widget '%s' . property '%s'",
                    row.mNodeDisplayName.c_str(), row.mPropName.c_str());
                ImGui::TextDisabled("Expected %s, currently bound to %s '%s'.",
                    row.mExpectedTypeName.c_str(),
                    row.mCurrentTypeName.c_str(),
                    row.mCurrentAssetName.c_str());
                if (row.mNode.Get() == nullptr)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f),
                        "(Node no longer alive -- skip this row.)");
                }
                else
                {
                    Property pickerProp(DatumType::Asset, "Replacement", nullptr,
                        &row.mPickedReplacement, 1, nullptr, (int32_t)row.mExpectedType);
                    DrawAssetProperty(pickerProp, 0, nullptr, PropertyOwnerType::Count);

                    Asset* picked = row.mPickedReplacement.Get();
                    bool pickOk = (picked != nullptr) && picked->Is(row.mExpectedType);
                    if (!pickOk) ImGui::BeginDisabled();
                    if (ImGui::Button("Apply"))
                    {
                        ApplyRow(row, picked);
                    }
                    if (!pickOk) ImGui::EndDisabled();

                    ImGui::SameLine();
                    if (ImGui::Button("Clear"))
                    {
                        row.mPickedReplacement = (Asset*)nullptr;
                        ApplyRow(row, nullptr);
                    }
                }
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::Button("Apply All"))
    {
        for (BrokenAssetRef& row : mBrokenRefs)
        {
            if (row.mResolved) continue;
            Asset* picked = row.mPickedReplacement.Get();
            if (picked == nullptr || !picked->Is(row.mExpectedType))
                continue;
            ApplyRow(row, picked);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Skip All"))
    {
        mBrokenRefs.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Close"))
    {
        mBrokenRefs.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

#endif
