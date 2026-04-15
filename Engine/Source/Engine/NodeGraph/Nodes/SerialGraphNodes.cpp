#include "NodeGraph/Nodes/SerialGraphNodes.h"

#include "SerialManager.h"
#include "Engine.h"

FORCE_LINK_DEF(SerialGraphNodes);

static const glm::vec4 kSerialActionColor = glm::vec4(0.2f, 0.5f, 0.7f, 1.0f);
static const glm::vec4 kSerialEventColor  = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f);

// =============================================================================
// SerialEnumeratePortsNode
// =============================================================================
DEFINE_GRAPH_NODE(SerialEnumeratePortsNode);

void SerialEnumeratePortsNode::SetupPins()
{
    AddInputPin("Exec", DatumType::Execution);
    AddOutputPin("Exec", DatumType::Execution);
    AddOutputPin("Count", DatumType::Integer);
    AddOutputPin("First Port", DatumType::String);
}

void SerialEnumeratePortsNode::Evaluate()
{
    SerialManager* mgr = SerialManager::Get();
    if (mgr == nullptr)
    {
        SetOutputValue(1, Datum((int32_t)0));
        SetOutputValue(2, Datum(std::string("")));
        return;
    }

    std::vector<SerialPortInfo> ports = mgr->EnumeratePorts();
    SetOutputValue(1, Datum((int32_t)ports.size()));
    SetOutputValue(2, Datum(ports.empty() ? std::string("") : ports[0].mPortName));
}

glm::vec4 SerialEnumeratePortsNode::GetNodeColor() const { return kSerialActionColor; }

// =============================================================================
// SerialConnectNode
// =============================================================================
DEFINE_GRAPH_NODE(SerialConnectNode);

void SerialConnectNode::SetupPins()
{
    AddInputPin("Exec", DatumType::Execution);
    AddInputPin("Port Name", DatumType::String, Datum(std::string("")));
    AddInputPin("Baud Rate", DatumType::Integer, Datum((int32_t)9600));
    AddOutputPin("Exec", DatumType::Execution);
    AddOutputPin("Handle", DatumType::Integer);
    AddOutputPin("Success", DatumType::Bool);
}

void SerialConnectNode::Evaluate()
{
    SerialManager* mgr = SerialManager::Get();
    SetOutputValue(1, Datum((int32_t)INVALID_SERIAL_HANDLE));
    SetOutputValue(2, Datum(false));
    if (mgr == nullptr)
        return;

    const std::string portName = GetInputValue(1).GetString();
    const int32_t baud = GetInputValue(2).GetInteger();

    SerialConfig cfg;
    cfg.mBaudRate = (baud > 0) ? (uint32_t)baud : 9600;

    SerialHandle h = mgr->Connect(portName.c_str(), cfg);
    SetOutputValue(1, Datum((int32_t)h));
    SetOutputValue(2, Datum(h != INVALID_SERIAL_HANDLE));
}

glm::vec4 SerialConnectNode::GetNodeColor() const { return kSerialActionColor; }

// =============================================================================
// SerialDisconnectNode
// =============================================================================
DEFINE_GRAPH_NODE(SerialDisconnectNode);

void SerialDisconnectNode::SetupPins()
{
    AddInputPin("Exec", DatumType::Execution);
    AddInputPin("Handle", DatumType::Integer, Datum((int32_t)0));
    AddOutputPin("Exec", DatumType::Execution);
}

void SerialDisconnectNode::Evaluate()
{
    SerialManager* mgr = SerialManager::Get();
    if (mgr == nullptr)
        return;
    const SerialHandle h = (SerialHandle)GetInputValue(1).GetInteger();
    mgr->Disconnect(h);
}

glm::vec4 SerialDisconnectNode::GetNodeColor() const { return kSerialActionColor; }

// =============================================================================
// SerialSendMessageNode
// =============================================================================
DEFINE_GRAPH_NODE(SerialSendMessageNode);

void SerialSendMessageNode::SetupPins()
{
    AddInputPin("Exec", DatumType::Execution);
    AddInputPin("Handle", DatumType::Integer, Datum((int32_t)0));
    AddInputPin("Data", DatumType::String, Datum(std::string("")));
    AddOutputPin("Exec", DatumType::Execution);
    AddOutputPin("Bytes Written", DatumType::Integer);
}

void SerialSendMessageNode::Evaluate()
{
    SerialManager* mgr = SerialManager::Get();
    SetOutputValue(1, Datum((int32_t)-1));
    if (mgr == nullptr)
        return;

    const SerialHandle h = (SerialHandle)GetInputValue(1).GetInteger();
    const std::string data = GetInputValue(2).GetString();

    const int32_t written = mgr->SendMessage(h, data);
    SetOutputValue(1, Datum(written));
}

glm::vec4 SerialSendMessageNode::GetNodeColor() const { return kSerialActionColor; }

// =============================================================================
// SerialStartReceiveNode
// =============================================================================
DEFINE_GRAPH_NODE(SerialStartReceiveNode);

void SerialStartReceiveNode::SetupPins()
{
    AddInputPin("Exec", DatumType::Execution);
    AddInputPin("Handle", DatumType::Integer, Datum((int32_t)0));
    AddOutputPin("Exec", DatumType::Execution);
}

void SerialStartReceiveNode::Evaluate()
{
    SerialManager* mgr = SerialManager::Get();
    if (mgr == nullptr)
        return;
    const SerialHandle h = (SerialHandle)GetInputValue(1).GetInteger();
    mgr->StartReceive(h);
}

glm::vec4 SerialStartReceiveNode::GetNodeColor() const { return kSerialActionColor; }

// =============================================================================
// SerialStopReceiveNode
// =============================================================================
DEFINE_GRAPH_NODE(SerialStopReceiveNode);

void SerialStopReceiveNode::SetupPins()
{
    AddInputPin("Exec", DatumType::Execution);
    AddInputPin("Handle", DatumType::Integer, Datum((int32_t)0));
    AddOutputPin("Exec", DatumType::Execution);
}

void SerialStopReceiveNode::Evaluate()
{
    SerialManager* mgr = SerialManager::Get();
    if (mgr == nullptr)
        return;
    const SerialHandle h = (SerialHandle)GetInputValue(1).GetInteger();
    mgr->StopReceive(h);
}

glm::vec4 SerialStopReceiveNode::GetNodeColor() const { return kSerialActionColor; }

// =============================================================================
// SerialMessageEventNode
// =============================================================================
DEFINE_GRAPH_NODE(SerialMessageEventNode);

void SerialMessageEventNode::SetupPins()
{
    AddOutputPin("Exec", DatumType::Execution);
    AddOutputPin("Handle", DatumType::Integer);
    AddOutputPin("Data", DatumType::String);
}

void SerialMessageEventNode::Evaluate()
{
    SerialManager* mgr = SerialManager::Get();
    if (mgr == nullptr)
        return;
    SetOutputValue(1, Datum((int32_t)mgr->GetCurrentEventHandle()));
    SetOutputValue(2, Datum(mgr->GetCurrentEventData()));
}

glm::vec4 SerialMessageEventNode::GetNodeColor() const { return kSerialEventColor; }

// =============================================================================
// SerialConnectedEventNode
// =============================================================================
DEFINE_GRAPH_NODE(SerialConnectedEventNode);

void SerialConnectedEventNode::SetupPins()
{
    AddOutputPin("Exec", DatumType::Execution);
    AddOutputPin("Handle", DatumType::Integer);
    AddOutputPin("Port Name", DatumType::String);
}

void SerialConnectedEventNode::Evaluate()
{
    SerialManager* mgr = SerialManager::Get();
    if (mgr == nullptr)
        return;
    SetOutputValue(1, Datum((int32_t)mgr->GetCurrentEventHandle()));
    SetOutputValue(2, Datum(mgr->GetCurrentEventPortName()));
}

glm::vec4 SerialConnectedEventNode::GetNodeColor() const { return kSerialEventColor; }

// =============================================================================
// SerialDisconnectedEventNode
// =============================================================================
DEFINE_GRAPH_NODE(SerialDisconnectedEventNode);

void SerialDisconnectedEventNode::SetupPins()
{
    AddOutputPin("Exec", DatumType::Execution);
    AddOutputPin("Handle", DatumType::Integer);
}

void SerialDisconnectedEventNode::Evaluate()
{
    SerialManager* mgr = SerialManager::Get();
    if (mgr == nullptr)
        return;
    SetOutputValue(1, Datum((int32_t)mgr->GetCurrentEventHandle()));
}

glm::vec4 SerialDisconnectedEventNode::GetNodeColor() const { return kSerialEventColor; }
