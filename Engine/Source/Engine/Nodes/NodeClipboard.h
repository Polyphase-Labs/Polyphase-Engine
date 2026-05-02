#pragma once

#include <vector>

#include "SmartPointer.h"

class Node;

// In-process clipboard for scene-hierarchy copy/paste. Holds deep-cloned
// snapshots of selected nodes so the source nodes can be edited, deleted, or
// shelved (when switching the active scene) without invalidating the
// clipboard. Paste re-clones the snapshots, so each paste produces fresh
// nodes with new persistent UUIDs.
namespace NodeClipboard
{
    void Copy(const std::vector<Node*>& sourceNodes);
    bool HasContent();
    const std::vector<NodePtr>& GetContents();
    void Clear();
}
