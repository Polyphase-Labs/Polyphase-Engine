#pragma once

#include "Engine.h"

#if EDITOR

#include <string>
#include <vector>

class AssetDir;

// Modal that fires when the OS drops files on the editor window. Per-row, the
// user picks how the file is imported:
//
//   Asset     -- routes through ActionManager::ImportAsset (so name-clash,
//                non-POT, mesh-mode, and addon-extension flows still fire).
//   LooseFile -- routes through ActionManager::ImportLooseFile (verbatim copy
//                into the current asset directory; readable from Lua via
//                Stream:ReadFile and from native code via SYS_AcquireFileData).
//   Skip      -- discards the row, no side effects.
//
// The Asset choice is disabled for files with no recognized asset-importer
// extension; those default to LooseFile.
class FileDropImportModal
{
public:

    enum class Choice
    {
        Unresolved,
        Asset,
        LooseFile,
        Skip,
    };

    struct PendingDrop
    {
        std::string mSourcePath;        // absolute path from the drop
        std::string mFilename;          // basename with extension
        std::string mExtension;         // lower-cased, includes leading '.'
        std::string mDetectedTypeName;  // "Texture" / "Mesh" / addon name / "(unknown)"
        bool        mCanImportAsAsset = false;
        bool        mIsMeshExtension = false;   // queue via mPendingMeshImportPaths
        Choice      mChoice = Choice::Unresolved;
        bool        mResolved = false;
    };

    static FileDropImportModal* Get();

    // Called once per frame from the editor main loop with whatever the OS
    // produced this frame. No-op if `paths` is empty.
    void EnqueueDroppedPaths(const std::vector<std::string>& paths);

    bool HasPending() const { return !mRows.empty(); }

    void Reset();

    void Draw();

private:

    FileDropImportModal() = default;

    void ApplyRow(PendingDrop& row);

    std::vector<PendingDrop> mRows;
    AssetDir* mTargetDir = nullptr;     // captured when the modal first opens
    bool      mModalRequested = false;
};

#endif // EDITOR
