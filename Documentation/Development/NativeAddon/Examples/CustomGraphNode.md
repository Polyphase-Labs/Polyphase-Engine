# Custom Graph Node

Add a visual-scripting node from a native addon. The node appears in the right-click *Add Node* menu of any domain you register it with (Material, Shader, Procedural, Animation, FSM, SceneGraph, or your own).

## Outcome

- A `GraphNode` subclass with input/output pins and an `Evaluate()` body.
- Auto-registered with one or more `GraphDomain`s via the engine's `REGISTER_GRAPH_NODE` macro — no engine source changes.
- Picked up the moment your addon's DLL loads.

## Files to add

```
Packages/com.example.myaddon/
    Source/
        GraphNodes/
            ClampedAddNode.h
            ClampedAddNode.cpp
        MyAddon.cpp     (existing entry point)
```

## Header

```cpp
// Source/GraphNodes/ClampedAddNode.h
#pragma once

#include "NodeGraph/GraphNode.h"

class ClampedAddNode : public GraphNode
{
public:
    DECLARE_GRAPH_NODE(ClampedAddNode, GraphNode);

    virtual void SetupPins() override;
    virtual void Evaluate() override;

    virtual const char* GetNodeTypeName() const override { return "Clamped Add"; }
    virtual const char* GetNodeCategory() const override { return "Math";        }
    virtual glm::vec4   GetNodeColor()    const override;
};
```

## Source

```cpp
// Source/GraphNodes/ClampedAddNode.cpp
#include "GraphNodes/ClampedAddNode.h"
#include "Maths.h"

#include <algorithm>

FORCE_LINK_DEF(ClampedAddNode);
DEFINE_GRAPH_NODE(ClampedAddNode);

// Register with a single domain. Use REGISTER_GRAPH_NODE_MULTI(...) to register
// the same node with several domains at once. Either macro auto-registers at
// static init time when the addon DLL loads — no extra plumbing needed.
//
// Defined in Engine/Source/Engine/NodeGraph/GraphNode.h:
//   REGISTER_GRAPH_NODE(Class, TypeName, Category, DomainName, Color)
//   REGISTER_GRAPH_NODE_MULTI(Class, TypeName, Category, Color, "Domain1", "Domain2", ...)
REGISTER_GRAPH_NODE(
    ClampedAddNode,
    "Clamped Add",
    "Math",
    "SceneGraph",
    glm::vec4(0.4f, 0.6f, 0.2f, 1.0f));

static const glm::vec4 kColor = glm::vec4(0.4f, 0.6f, 0.2f, 1.0f);

void ClampedAddNode::SetupPins()
{
    AddInputPin("A",   DatumType::Float, Datum(0.0f));
    AddInputPin("B",   DatumType::Float, Datum(0.0f));
    AddInputPin("Min", DatumType::Float, Datum(0.0f));
    AddInputPin("Max", DatumType::Float, Datum(1.0f));
    AddOutputPin("Result", DatumType::Float);
}

void ClampedAddNode::Evaluate()
{
    float a    = GetInputValue(0).GetFloat();
    float b    = GetInputValue(1).GetFloat();
    float lo   = GetInputValue(2).GetFloat();
    float hi   = GetInputValue(3).GetFloat();
    float sum  = a + b;
    float clamped = std::min(std::max(sum, lo), hi);
    SetOutputValue(0, Datum(clamped));
}

glm::vec4 ClampedAddNode::GetNodeColor() const { return kColor; }
```

## Wire into the addon

In your existing `MyAddon.cpp`, force-link the translation unit so the static registrar runs:

```cpp
#include "GraphNodes/ClampedAddNode.h"

static int OnLoad(PolyphaseEngineAPI* api)
{
    FORCE_LINK_CALL(ClampedAddNode);
    return 0;
}
```

That's all — the `REGISTER_GRAPH_NODE(...)` line in the .cpp file does the actual domain registration.

## Registering with multiple domains

If your node makes sense in several visual-scripting contexts (e.g. the standard "Input" nodes the engine ships register with every domain), use the `_MULTI` variant:

```cpp
REGISTER_GRAPH_NODE_MULTI(
    ClampedAddNode,
    "Clamped Add",
    "Math",
    glm::vec4(0.4f, 0.6f, 0.2f, 1.0f),
    "Material", "Shader", "Procedural", "Animation", "FSM", "SceneGraph");
```

Domain names are looked up by string. Unknown domain names are silently skipped — typos are not flagged.

## Registering with a custom domain

If your addon ships its own `GraphDomain` subclass (rare), name it in the `DomainName` argument. The engine creates the domain on first registration if it doesn't exist yet, so the order between domain implementation and node registration is forgiving.

## Verification

1. Reload addons (**Tools → Addons → Reload Native Addons**).
2. Open a node graph (e.g. on a Script component or a Material).
3. Right-click on empty graph space → *Math → Clamped Add*.
4. Wire `A`/`B` to two values, `Min`/`Max` to constants, and check the `Result` pin updates as you tweak inputs.

## See also

- `Engine/Source/Engine/NodeGraph/GraphNode.h` — `DECLARE_GRAPH_NODE`, `DEFINE_GRAPH_NODE`, `REGISTER_GRAPH_NODE`, `REGISTER_GRAPH_NODE_MULTI`.
- `Engine/Source/Engine/NodeGraph/Nodes/MathNodes.cpp` — pin-setup and `Evaluate` reference.
- `Engine/Source/Engine/NodeGraph/Domains/` — the built-in domains and the categories they expose.
- `.llm/NodeGraph.md` — node-graph architecture overview.
