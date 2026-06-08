#pragma once

#include "Engine.h"

#if EDITOR

#include "AssetRef.h"
#include "Nodes/Node.h"

#include <string>
#include <vector>

// Post-load scan that catches widgets whose Asset-typed properties were
// bound to the wrong-type asset (e.g. a Texture slot pointing at a
// StaticMesh because the user imported a mesh with a name that collided
// with an existing texture). The import-clash modal in EditorImgui prevents
// new collisions, but already-saved scenes need this fixup. Surfaced as a
// modal at scene-open with a per-row picker so the user can rebind to a
// valid asset, clear the slot, or skip and investigate.
class AssetFixupModal
{
public:

    struct BrokenAssetRef
    {
        WeakPtr<Node> mNode;
        std::string mNodeDisplayName;     // captured at scan time so the row still has a label after teardown
        std::string mPropName;
        TypeId mExpectedType = 0;
        std::string mExpectedTypeName;
        Asset* mCurrentAsset = nullptr;
        std::string mCurrentAssetName;
        std::string mCurrentTypeName;
        AssetRef mPickedReplacement;
        bool mResolved = false;           // true once Apply/Clear has fired -- row sticks around for the user to read until Close
    };

    static AssetFixupModal* Get();

    // Walk the freshly-instantiated widget subtree and enqueue any
    // Asset-typed widget property whose binding fails its type filter
    // (mAsset non-null but doesn't pass Is(expectedType)).
    void ScanWidgetTree(Node* root);

    bool HasPending() const { return !mBrokenRefs.empty(); }

    // Call from EditorImguiDraw every frame.
    void Draw();

private:

    AssetFixupModal() = default;

    void ApplyRow(BrokenAssetRef& row, Asset* newAsset);

    std::vector<BrokenAssetRef> mBrokenRefs;
    bool mModalRequested = false;
};

#endif
