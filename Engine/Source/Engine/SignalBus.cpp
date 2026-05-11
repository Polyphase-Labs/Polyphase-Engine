#include "SignalBus.h"

#include "Nodes/Node.h"

SignalBus gSignalBus;

SignalBus* GetSignalBus()
{
    return &gSignalBus;
}

std::vector<Datum> SignalBusChannel::Emit(const std::vector<Datum>& args)
{
    std::vector<Datum> retVals;

    mEmitting = true;

    for (auto it = mConnectionMap.begin(); it != mConnectionMap.end();)
    {
        Node* node = it->first.Get<Node>();
        if (node == nullptr)
        {
            it = mConnectionMap.erase(it);
        }
        else
        {
            Datum ret;
            bool hasRet = false;

            if (it->second.mFuncPointer != nullptr)
            {
                SignalBusHandlerFP handler = it->second.mFuncPointer;
                ret = handler(node, args);
                hasRet = ret.IsValid();
            }
            else if (it->second.mVoidFuncPointer != nullptr)
            {
                SignalBusHandlerVoidFP handler = it->second.mVoidFuncPointer;
                handler(node, args);
            }

            if (it->second.mScriptFunc.IsValid())
            {
                std::vector<Datum> selfArgs;
                selfArgs.reserve(args.size() + 1);
                selfArgs.push_back(node);

                if (args.size() > 0)
                {
                    selfArgs.insert(selfArgs.end(), args.begin(), args.end());
                }

                ret = it->second.mScriptFunc.CallR((uint32_t)selfArgs.size(), selfArgs.data());
                hasRet = ret.IsValid();
            }

            if (hasRet)
            {
                retVals.push_back(ret);
            }

            ++it;
        }
    }

    mEmitting = false;

    for (uint32_t i = 0; i < mPendingDisconnects.size(); ++i)
    {
        mConnectionMap.erase(mPendingDisconnects[i]);
    }
    mPendingDisconnects.clear();

    for (uint32_t i = 0; i < mPendingConnects.size(); ++i)
    {
        mConnectionMap[mPendingConnects[i].first] = mPendingConnects[i].second;
    }
    mPendingConnects.clear();

    return retVals;
}

void SignalBusChannel::Connect(Node* node, SignalBusHandlerFP func)
{
    if (node != nullptr)
    {
        CleanupDeadConnections();
        NodePtrWeak nodePtrWeak = ResolveWeakPtr(node);

        SignalBusHandlerFunc handler;
        handler.mFuncPointer = func;

        if (mEmitting)
        {
            mPendingConnects.push_back({ nodePtrWeak, handler });
        }
        else
        {
            mConnectionMap[nodePtrWeak] = handler;
        }
    }
}

void SignalBusChannel::Connect(Node* node, SignalBusHandlerVoidFP func)
{
    if (node != nullptr)
    {
        CleanupDeadConnections();
        NodePtrWeak nodePtrWeak = ResolveWeakPtr(node);

        SignalBusHandlerFunc handler;
        handler.mVoidFuncPointer = func;

        if (mEmitting)
        {
            mPendingConnects.push_back({ nodePtrWeak, handler });
        }
        else
        {
            mConnectionMap[nodePtrWeak] = handler;
        }
    }
}

void SignalBusChannel::Connect(Node* node, const ScriptFunc& func)
{
    if (node != nullptr)
    {
        CleanupDeadConnections();
        NodePtrWeak nodePtrWeak = ResolveWeakPtr(node);

        SignalBusHandlerFunc handler;
        handler.mScriptFunc = func;

        if (mEmitting)
        {
            mPendingConnects.push_back({ nodePtrWeak, handler });
        }
        else
        {
            mConnectionMap[nodePtrWeak] = handler;
        }
    }
}

void SignalBusChannel::Disconnect(Node* node)
{
    if (node != nullptr)
    {
        NodePtrWeak nodePtrWeak = ResolveWeakPtr(node);

        if (mEmitting)
        {
            mPendingDisconnects.push_back(nodePtrWeak);
        }
        else
        {
            mConnectionMap.erase(nodePtrWeak);
        }
    }
}

void SignalBusChannel::Clear()
{
    mConnectionMap.clear();
    mPendingConnects.clear();
    mPendingDisconnects.clear();
    mEmitting = false;
}

void SignalBusChannel::CleanupDeadConnections()
{
    if (mEmitting)
    {
        return;
    }

    for (auto it = mConnectionMap.begin(); it != mConnectionMap.end();)
    {
        Node* node = it->first.Get<Node>();
        if (node == nullptr)
        {
            it = mConnectionMap.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::vector<Datum> SignalBus::Emit(const std::string& name, const std::vector<Datum>& args)
{
    std::vector<Datum> retVals;

    if (mSignalMap.size() > 0)
    {
        auto it = mSignalMap.find(name);
        if (it != mSignalMap.end())
        {
            retVals = it->second.Emit(args);
        }
    }

    return retVals;
}

void SignalBus::Subscribe(const std::string& name, Node* listener, SignalBusHandlerFP func)
{
    mSignalMap[name].Connect(listener, func);
}

void SignalBus::Subscribe(const std::string& name, Node* listener, SignalBusHandlerVoidFP func)
{
    mSignalMap[name].Connect(listener, func);
}

void SignalBus::Subscribe(const std::string& name, Node* listener, const ScriptFunc& func)
{
    mSignalMap[name].Connect(listener, func);
}

void SignalBus::Unsubscribe(const std::string& name, Node* listener)
{
    auto it = mSignalMap.find(name);
    if (it != mSignalMap.end())
    {
        it->second.Disconnect(listener);
    }
}

void SignalBus::Clear()
{
    for (auto it = mSignalMap.begin(); it != mSignalMap.end(); ++it)
    {
        it->second.Clear();
    }

    mSignalMap.clear();
}
