#pragma once

#include "PolyphaseAPI.h"
#include "SmartPointer.h"
#include "ScriptFunc.h"
#include "Datum.h"

#include <string>
#include <unordered_map>

class Node;

typedef Datum(*SignalBusHandlerFP)(Node* listener, const std::vector<Datum>& args);
typedef void(*SignalBusHandlerVoidFP)(Node* listener, const std::vector<Datum>& args);

struct SignalBusHandlerFunc
{
    SignalBusHandlerFP mFuncPointer = nullptr;
    SignalBusHandlerVoidFP mVoidFuncPointer = nullptr;
    mutable ScriptFunc mScriptFunc;
};

class SignalBusChannel
{
public:

    std::vector<Datum> Emit(const std::vector<Datum>& args);
    void Connect(Node* node, SignalBusHandlerFP func);
    void Connect(Node* node, SignalBusHandlerVoidFP func);
    void Connect(Node* node, const ScriptFunc& func);
    void Disconnect(Node* node);
    void Clear();

private:

    void CleanupDeadConnections();

    std::unordered_map<NodePtrWeak, SignalBusHandlerFunc> mConnectionMap;
    std::vector<std::pair<NodePtrWeak, SignalBusHandlerFunc>> mPendingConnects;
    std::vector<NodePtrWeak> mPendingDisconnects;
    bool mEmitting = false;
};

class POLYPHASE_API SignalBus
{
public:

    std::vector<Datum> Emit(const std::string& name, const std::vector<Datum>& args = {});
    void Subscribe(const std::string& name, Node* listener, SignalBusHandlerFP func);
    void Subscribe(const std::string& name, Node* listener, SignalBusHandlerVoidFP func);
    void Subscribe(const std::string& name, Node* listener, const ScriptFunc& func);
    void Unsubscribe(const std::string& name, Node* listener);
    void Clear();

private:

    std::unordered_map<std::string, SignalBusChannel> mSignalMap;
};

POLYPHASE_API SignalBus* GetSignalBus();
