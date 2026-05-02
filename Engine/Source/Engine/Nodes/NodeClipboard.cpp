#include "Nodes/NodeClipboard.h"
#include "Nodes/Node.h"

namespace
{
    std::vector<NodePtr> sClipboard;

    bool IsAncestor(Node* ancestor, Node* candidate)
    {
        for (Node* p = candidate->GetParent(); p != nullptr; p = p->GetParent())
        {
            if (p == ancestor)
                return true;
        }
        return false;
    }
}

void NodeClipboard::Copy(const std::vector<Node*>& sourceNodes)
{
    sClipboard.clear();

    if (sourceNodes.empty())
        return;

    // Drop entries whose ancestor is also in the selection so we don't clone
    // the same subtree twice. Mirrors RemoveRedundantDescendants() in
    // EditorUtils, replicated locally so this file stays out of #if EDITOR.
    std::vector<Node*> trimmed;
    trimmed.reserve(sourceNodes.size());
    for (Node* node : sourceNodes)
    {
        if (node == nullptr)
            continue;

        bool redundant = false;
        for (Node* other : sourceNodes)
        {
            if (other != nullptr && other != node && IsAncestor(other, node))
            {
                redundant = true;
                break;
            }
        }

        if (!redundant)
        {
            trimmed.push_back(node);
        }
    }

    sClipboard.reserve(trimmed.size());
    for (Node* node : trimmed)
    {
        NodePtr clone = node->Clone(true /*recurse*/, true /*instantiateLinkedScene*/, true /*resolveNodePaths*/);
        if (clone != nullptr)
        {
            sClipboard.push_back(clone);
        }
    }
}

bool NodeClipboard::HasContent()
{
    return !sClipboard.empty();
}

const std::vector<NodePtr>& NodeClipboard::GetContents()
{
    return sClipboard;
}

void NodeClipboard::Clear()
{
    sClipboard.clear();
}
