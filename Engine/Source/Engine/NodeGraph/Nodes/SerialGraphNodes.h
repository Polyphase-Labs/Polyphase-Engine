#pragma once

#include "NodeGraph/GraphNode.h"

// =============================================================================
// Serial Action Nodes
// =============================================================================

class SerialEnumeratePortsNode : public GraphNode
{
public:
    DECLARE_GRAPH_NODE(SerialEnumeratePortsNode, GraphNode);
    virtual void SetupPins() override;
    virtual void Evaluate() override;
    virtual const char* GetNodeTypeName() const override { return "Serial Enumerate Ports"; }
    virtual const char* GetNodeCategory() const override { return "Serial"; }
    virtual glm::vec4   GetNodeColor() const override;
};

class SerialConnectNode : public GraphNode
{
public:
    DECLARE_GRAPH_NODE(SerialConnectNode, GraphNode);
    virtual void SetupPins() override;
    virtual void Evaluate() override;
    virtual const char* GetNodeTypeName() const override { return "Serial Connect"; }
    virtual const char* GetNodeCategory() const override { return "Serial"; }
    virtual glm::vec4   GetNodeColor() const override;
};

class SerialDisconnectNode : public GraphNode
{
public:
    DECLARE_GRAPH_NODE(SerialDisconnectNode, GraphNode);
    virtual void SetupPins() override;
    virtual void Evaluate() override;
    virtual const char* GetNodeTypeName() const override { return "Serial Disconnect"; }
    virtual const char* GetNodeCategory() const override { return "Serial"; }
    virtual glm::vec4   GetNodeColor() const override;
};

class SerialSendMessageNode : public GraphNode
{
public:
    DECLARE_GRAPH_NODE(SerialSendMessageNode, GraphNode);
    virtual void SetupPins() override;
    virtual void Evaluate() override;
    virtual const char* GetNodeTypeName() const override { return "Serial Send Message"; }
    virtual const char* GetNodeCategory() const override { return "Serial"; }
    virtual glm::vec4   GetNodeColor() const override;
};

class SerialStartReceiveNode : public GraphNode
{
public:
    DECLARE_GRAPH_NODE(SerialStartReceiveNode, GraphNode);
    virtual void SetupPins() override;
    virtual void Evaluate() override;
    virtual const char* GetNodeTypeName() const override { return "Serial Start Receive"; }
    virtual const char* GetNodeCategory() const override { return "Serial"; }
    virtual glm::vec4   GetNodeColor() const override;
};

class SerialStopReceiveNode : public GraphNode
{
public:
    DECLARE_GRAPH_NODE(SerialStopReceiveNode, GraphNode);
    virtual void SetupPins() override;
    virtual void Evaluate() override;
    virtual const char* GetNodeTypeName() const override { return "Serial Stop Receive"; }
    virtual const char* GetNodeCategory() const override { return "Serial"; }
    virtual glm::vec4   GetNodeColor() const override;
};

// =============================================================================
// Serial Event Nodes
// =============================================================================

class SerialMessageEventNode : public GraphNode
{
public:
    DECLARE_GRAPH_NODE(SerialMessageEventNode, GraphNode);
    virtual void SetupPins() override;
    virtual void Evaluate() override;
    virtual bool IsEventNode() const override { return true; }
    virtual const char* GetEventName() const override { return "SerialMessage"; }
    virtual const char* GetNodeTypeName() const override { return "On Serial Message"; }
    virtual const char* GetNodeCategory() const override { return "Event"; }
    virtual glm::vec4   GetNodeColor() const override;
};

class SerialConnectedEventNode : public GraphNode
{
public:
    DECLARE_GRAPH_NODE(SerialConnectedEventNode, GraphNode);
    virtual void SetupPins() override;
    virtual void Evaluate() override;
    virtual bool IsEventNode() const override { return true; }
    virtual const char* GetEventName() const override { return "SerialConnected"; }
    virtual const char* GetNodeTypeName() const override { return "On Serial Connected"; }
    virtual const char* GetNodeCategory() const override { return "Event"; }
    virtual glm::vec4   GetNodeColor() const override;
};

class SerialDisconnectedEventNode : public GraphNode
{
public:
    DECLARE_GRAPH_NODE(SerialDisconnectedEventNode, GraphNode);
    virtual void SetupPins() override;
    virtual void Evaluate() override;
    virtual bool IsEventNode() const override { return true; }
    virtual const char* GetEventName() const override { return "SerialDisconnected"; }
    virtual const char* GetNodeTypeName() const override { return "On Serial Disconnected"; }
    virtual const char* GetNodeCategory() const override { return "Event"; }
    virtual glm::vec4   GetNodeColor() const override;
};
