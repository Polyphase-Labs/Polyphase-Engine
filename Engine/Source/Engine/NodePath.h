#pragma once

#include <string>
#include <vector>
#include "Datum.h"
#include "SmartPointer.h"

class Object;
class Node;
class Datum;
class Property;

struct PendingNodePath
{
    WeakPtr<Node> mNode;
    std::string mPropName;
    Datum mPath;
};

std::string FindRelativeNodePath(Node* src, Node* dst);
void ResolveNodePaths(Node* node, bool recurseChildren);
// boundary: if non-null, a "../" token that would step the cursor *above*
// this node aborts the resolve (returns nullptr). FindRelativeNodePath
// refuses to emit cross-boundary paths when both endpoints share a
// sub-root; ResolveNodePath's boundary check is the symmetric guard so a
// stale or addon-affected "../" can't wander out of the sub-scene.
Node* ResolveNodePath(Node* src, const std::string& path, Node* boundary = nullptr);
void ResolvePendingNodePaths(std::vector<PendingNodePath>& pending);
void ResolveAllNodePathsRecursive(Node* node);
void RecordNodePaths(Node* node, std::vector<Property>& props);
