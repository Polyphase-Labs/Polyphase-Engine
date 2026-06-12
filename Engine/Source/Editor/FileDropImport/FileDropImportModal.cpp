#include "FileDropImport/FileDropImportModal.h"

#if EDITOR

#include "ActionManager.h"
#include "Asset.h"
#include "AssetDir.h"
#include "AssetManager.h"
#include "EditorState.h"
#include "Log.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>

namespace
{
    // Mirror of the extension switch in HandleImportCallback /
    // ActionManager::ImportAsset (ActionManager.cpp:5726+, :5830+). Kept in
    // sync by hand for now -- if a third caller needs this we should lift it
    // into a shared AssetTypeFromExtension helper in ActionManager.h.
    struct AssetTypeProbe
    {
        std::string mTypeName;
        bool        mCanImportAsAsset = false;
        bool        mIsMeshExtension  = false;
    };

    AssetTypeProbe ProbeExtension(const std::string& extLower)
    {
        AssetTypeProbe out;
        if (extLower == ".png" || extLower == ".bmp" || extLower == ".jpeg" ||
            extLower == ".jpg" || extLower == ".tga")
        {
            out.mTypeName = "Texture";
            out.mCanImportAsAsset = true;
            return out;
        }
        if (extLower == ".dae" || extLower == ".fbx" || extLower == ".glb" ||
            extLower == ".gltf" || extLower == ".obj")
        {
            out.mTypeName = "Mesh";
            out.mCanImportAsAsset = true;
            out.mIsMeshExtension  = true;
            return out;
        }
        if (extLower == ".wav")
        {
            out.mTypeName = "Sound Wave";
            out.mCanImportAsAsset = true;
            return out;
        }
        if (extLower == ".ttf" || extLower == ".xml")
        {
            out.mTypeName = "Font";
            out.mCanImportAsAsset = true;
            return out;
        }

        // Addon-registered extensions (e.g. .mp4 -> VideoClip).
        TypeId addonType = AssetManager::Get()->LookupImportExtension(extLower);
        if (addonType != INVALID_TYPE_ID)
        {
            const char* name = Asset::GetNameFromTypeId(addonType);
            out.mTypeName = name ? name : "Addon Asset";
            out.mCanImportAsAsset = true;
            return out;
        }

        out.mTypeName = "(unknown - Loose only)";
        out.mCanImportAsAsset = false;
        return out;
    }

    void SplitFilenameAndExt(const std::string& path,
                             std::string& outFilename,
                             std::string& outExtLower)
    {
        size_t slashIdx = path.find_last_of("/\\");
        outFilename = (slashIdx == std::string::npos)
            ? path : path.substr(slashIdx + 1);
        size_t dotIdx = outFilename.find_last_of('.');
        outExtLower = (dotIdx == std::string::npos)
            ? "" : outFilename.substr(dotIdx);
        for (char& c : outExtLower)
            c = (char)std::tolower((unsigned char)c);
    }
}

FileDropImportModal* FileDropImportModal::Get()
{
    static FileDropImportModal sInstance;
    return &sInstance;
}

void FileDropImportModal::Reset()
{
    mRows.clear();
    mTargetDir = nullptr;
    mModalRequested = false;
}

void FileDropImportModal::EnqueueDroppedPaths(const std::vector<std::string>& paths)
{
    if (paths.empty())
        return;

    // Capture the target dir the first time the modal opens. Locking it in
    // means an artist navigating the Asset Browser mid-modal doesn't change
    // where the files land.
    //
    // Priority: the folder under the mouse cursor when the drop happened
    // (recorded by DrawAssetDirTree / DrawDirPicker each frame) wins over
    // the currently-open Asset Browser folder. This lets the user drop
    // directly onto a sibling/child folder without navigating into it first.
    if (mRows.empty())
    {
        EditorState* es = GetEditorState();
        AssetDir* hovered = es->mMouseHoveredAssetDir;
        mTargetDir = (hovered != nullptr) ? hovered : es->GetAssetDirectory();
    }

    if (mTargetDir == nullptr)
    {
        LogError("Cannot import dropped files: no project / asset directory open.");
        return;
    }
    if (mTargetDir->mEngineDir || mTargetDir->mAddonDir)
    {
        LogError("Cannot import dropped files into engine or addon directory '%s'.",
            mTargetDir->mPath.c_str());
        return;
    }

    for (const std::string& path : paths)
    {
        if (path.empty())
            continue;

        // Reject folders -- WM_DROPFILES delivers folder paths the same as
        // files. Detect via trailing slash from the WndProc normalization
        // (Windows drops are normalized, but folders never carry an extension).
        // Cheap-and-correct: check for an extension; treat extensionless drops
        // as folders or unknown blobs. The user can still rename + re-drop.
        std::string filename, extLower;
        SplitFilenameAndExt(path, filename, extLower);
        if (extLower.empty())
        {
            LogWarning("Drag-and-drop only supports files with an extension; "
                       "skipping '%s'.", path.c_str());
            continue;
        }

        AssetTypeProbe probe = ProbeExtension(extLower);

        PendingDrop row;
        row.mSourcePath        = path;
        row.mFilename          = filename;
        row.mExtension         = extLower;
        row.mDetectedTypeName  = probe.mTypeName;
        row.mCanImportAsAsset  = probe.mCanImportAsAsset;
        row.mIsMeshExtension   = probe.mIsMeshExtension;
        row.mChoice = probe.mCanImportAsAsset ? Choice::Asset : Choice::LooseFile;
        mRows.push_back(std::move(row));
    }

    if (!mRows.empty())
        mModalRequested = true;
}

void FileDropImportModal::ApplyRow(PendingDrop& row)
{
    // ActionManager::ImportAsset and the mesh-mode modal both read the
    // active Asset Browser folder via GetEditorState()->GetAssetDirectory()
    // at dispatch time -- not the modal-captured mTargetDir. Navigate the
    // browser to mTargetDir so the file lands where the user dropped it.
    // (ImportLooseFile takes targetDir directly, but the navigation is also
    // a nice UX cue: the user sees the file appear in the folder they
    // dropped onto.)
    if (mTargetDir != nullptr &&
        GetEditorState()->GetAssetDirectory() != mTargetDir)
    {
        GetEditorState()->SetAssetDirectory(mTargetDir, true);
    }

    switch (row.mChoice)
    {
    case Choice::Asset:
    {
        if (!row.mCanImportAsAsset)
        {
            // Defensive -- UI should have disabled the Asset button. Fall
            // through to LooseFile rather than silently dropping the file.
            ActionManager::Get()->ImportLooseFile(row.mSourcePath, mTargetDir);
            break;
        }
        if (row.mIsMeshExtension)
        {
            // Mesh files go through the import-mode modal (Scene / Multiple /
            // Single). Push to the existing queue; the modal at
            // EditorImgui.cpp:10561 picks them up on the next frame.
            GetEditorState()->mPendingMeshImportPaths.push_back(row.mSourcePath);
        }
        else
        {
            ActionManager::Get()->ImportAsset(row.mSourcePath);
        }
        break;
    }
    case Choice::LooseFile:
        ActionManager::Get()->ImportLooseFile(row.mSourcePath, mTargetDir);
        break;
    case Choice::Skip:
    case Choice::Unresolved:
        // Treat Unresolved as Skip when forced through (e.g. via Close).
        break;
    }

    row.mResolved = true;
}

void FileDropImportModal::Draw()
{
    if (mRows.empty())
        return;

    const char* kPopupId = "Import Dropped Files";

    if (mModalRequested && !ImGui::IsPopupOpen(kPopupId))
    {
        ImGui::OpenPopup(kPopupId);
        mModalRequested = false;
    }

    if (ImGui::IsPopupOpen(kPopupId))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.6f, io.DisplaySize.y * 0.7f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    if (!ImGui::BeginPopupModal(kPopupId, nullptr, ImGuiWindowFlags_NoCollapse))
        return;

    ImGui::TextWrapped(
        "You dropped %zu file%s on the editor. Pick how each one should land in "
        "the project:",
        mRows.size(), mRows.size() == 1 ? "" : "s");
    ImGui::Spacing();
    ImGui::TextDisabled("Asset: routes through the regular Import flow, converts to a .oct asset, "
                        "and fires the usual import dialogs (mesh mode, name clash, non-POT, etc).");
    ImGui::TextDisabled("Loose File: copies the file verbatim. Read at runtime via Lua "
                        "Stream:ReadFile or native SYS_AcquireFileData.");
    if (mTargetDir != nullptr)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Target folder: %s", mTargetDir->mPath.c_str());
    }
    ImGui::Spacing();

    if (ImGui::BeginChild("DropList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true))
    {
        for (size_t r = 0; r < mRows.size(); ++r)
        {
            PendingDrop& row = mRows[r];
            ImGui::PushID((int)r);
            ImGui::Separator();

            ImGui::Text("%s", row.mFilename.c_str());
            ImGui::TextDisabled("Type: %s", row.mDetectedTypeName.c_str());

            if (row.mResolved)
            {
                const char* tag =
                    row.mChoice == Choice::Asset      ? "[Imported as Asset]" :
                    row.mChoice == Choice::LooseFile  ? "[Imported as Loose File]" :
                                                        "[Skipped]";
                ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "%s", tag);
            }
            else
            {
                // Three-button choice row. Highlight the selected one.
                auto ChoiceButton = [&](const char* label, Choice c, bool enabled)
                {
                    bool selected = (row.mChoice == c);
                    if (!enabled) ImGui::BeginDisabled();
                    if (selected)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.25f, 0.55f, 0.95f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.62f, 1.00f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.20f, 0.45f, 0.85f, 1.0f));
                    }
                    if (ImGui::Button(label))
                        row.mChoice = c;
                    if (selected)
                        ImGui::PopStyleColor(3);
                    if (!enabled) ImGui::EndDisabled();
                };

                ChoiceButton("Asset",       Choice::Asset,     row.mCanImportAsAsset);
                if (!row.mCanImportAsAsset && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("No asset importer registered for %s.", row.mExtension.c_str());

                ImGui::SameLine();
                ChoiceButton("Loose File",  Choice::LooseFile, true);

                ImGui::SameLine();
                ChoiceButton("Skip",        Choice::Skip,      true);

                ImGui::SameLine();
                if (ImGui::Button("Apply"))
                {
                    ApplyRow(row);
                }
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::Button("Import All"))
    {
        int assetCount = 0;
        int looseCount = 0;
        int skipCount  = 0;
        std::string targetPath = (mTargetDir != nullptr) ? mTargetDir->mPath : std::string("?");

        for (PendingDrop& row : mRows)
        {
            if (row.mResolved) continue;
            if (row.mChoice == Choice::Unresolved)
                row.mChoice = row.mCanImportAsAsset ? Choice::Asset : Choice::LooseFile;
            switch (row.mChoice)
            {
            case Choice::Asset:     ++assetCount; break;
            case Choice::LooseFile: ++looseCount; break;
            case Choice::Skip:      ++skipCount;  break;
            default: break;
            }
            ApplyRow(row);
        }

        LogDebug("File drop import complete: %d as Asset, %d as Loose File, %d skipped (target: %s)",
            assetCount, looseCount, skipCount, targetPath.c_str());

        // Modal is done once every row is dispatched. Auto-close matches the
        // user's "finish and get out of my way" intent.
        mRows.clear();
        mTargetDir = nullptr;
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("All as Loose"))
    {
        for (PendingDrop& row : mRows)
        {
            if (row.mResolved) continue;
            row.mChoice = Choice::LooseFile;
            ApplyRow(row);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Skip All"))
    {
        for (PendingDrop& row : mRows)
        {
            if (row.mResolved) continue;
            row.mChoice = Choice::Skip;
            ApplyRow(row);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Close"))
    {
        // Anything still unresolved at Close is treated as Skip.
        for (PendingDrop& row : mRows)
        {
            if (row.mResolved) continue;
            row.mChoice = Choice::Skip;
            ApplyRow(row);
        }
        mRows.clear();
        mTargetDir = nullptr;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

#endif // EDITOR
