#include "SerialManager.h"

#include "Engine.h"
#include "Log.h"
#include "Assertion.h"
#include "System/System.h"
#include "World.h"
#include "Script.h"
#include "Nodes/Node.h"
#include "Nodes/NodeGraphPlayer.h"

#ifdef SendMessage
#undef SendMessage
#endif

static const uint32_t kReadBufferSize    = 4096;
static const uint32_t kMaxBytesPerFrame  = 65536;
static const uint32_t kReadThreadSleepMs = 2;

namespace
{
    struct SerialReadCtx
    {
        SerialNative*         mNative    = nullptr;
        std::vector<uint8_t>* mRxBuffer  = nullptr;
        MutexObject*          mRxMutex   = nullptr;
        std::atomic<bool>*    mStopFlag  = nullptr;
        std::atomic<bool>*    mDeadFlag  = nullptr;
    };

    ThreadFuncRet SerialReadThreadMain(void* arg)
    {
        SerialReadCtx* ctx = reinterpret_cast<SerialReadCtx*>(arg);
        uint8_t localBuffer[kReadBufferSize];

        while (!ctx->mStopFlag->load())
        {
            int32_t n = SER_Read(ctx->mNative, localBuffer, kReadBufferSize);
            if (n < 0)
            {
                ctx->mDeadFlag->store(true);
                break;
            }

            if (n > 0)
            {
                SYS_LockMutex(ctx->mRxMutex);
                ctx->mRxBuffer->insert(ctx->mRxBuffer->end(), localBuffer, localBuffer + n);
                SYS_UnlockMutex(ctx->mRxMutex);
            }
            else
            {
                SYS_Sleep(kReadThreadSleepMs);
            }
        }

        delete ctx;
        THREAD_RETURN();
    }
}

SerialManager* SerialManager::sInstance = nullptr;

void SerialManager::Create()
{
    if (sInstance == nullptr)
        sInstance = new SerialManager();
}

void SerialManager::Destroy()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

SerialManager* SerialManager::Get()
{
    return sInstance;
}

SerialManager::SerialManager()
{
}

SerialManager::~SerialManager()
{
}

void SerialManager::Initialize()
{
    SER_Initialize();
}

void SerialManager::Shutdown()
{
    for (uint32_t i = 0; i < mPorts.size(); ++i)
    {
        Port* p = mPorts[i];
        if (p == nullptr)
            continue;

        JoinAndDestroyReadThread(p);

        if (p->mNative != nullptr)
        {
            SER_Close(p->mNative);
            p->mNative = nullptr;
        }
        if (p->mRxMutex != nullptr)
        {
            SYS_DestroyMutex((MutexObject*)p->mRxMutex);
            p->mRxMutex = nullptr;
        }
        delete p;
    }
    mPorts.clear();

    SER_Shutdown();
}

std::vector<SerialPortInfo> SerialManager::EnumeratePorts()
{
    return SER_EnumeratePorts();
}

SerialHandle SerialManager::Connect(const char* portName, const SerialConfig& cfg)
{
    if (portName == nullptr || portName[0] == '\0')
        return INVALID_SERIAL_HANDLE;

    SerialNative* native = SER_Open(portName, cfg);
    if (native == nullptr)
        return INVALID_SERIAL_HANDLE;

    Port* port           = new Port();
    port->mHandle        = mNextHandle++;
    port->mPortName      = portName;
    port->mNative        = native;
    port->mRxMutex       = SYS_CreateMutex();
    port->mConnectPending = true;

    mPorts.push_back(port);
    return port->mHandle;
}

void SerialManager::Disconnect(SerialHandle handle)
{
    for (uint32_t i = 0; i < mPorts.size(); ++i)
    {
        Port* p = mPorts[i];
        if (p == nullptr || p->mHandle != handle)
            continue;

        JoinAndDestroyReadThread(p);

        if (p->mNative != nullptr)
        {
            SER_Close(p->mNative);
            p->mNative = nullptr;
        }

        const SerialHandle disconnectedHandle = p->mHandle;

        if (p->mRxMutex != nullptr)
        {
            SYS_DestroyMutex((MutexObject*)p->mRxMutex);
            p->mRxMutex = nullptr;
        }

        delete p;
        mPorts.erase(mPorts.begin() + i);

        DispatchDisconnect(disconnectedHandle);
        return;
    }
}

bool SerialManager::IsConnected(SerialHandle handle) const
{
    const Port* p = FindPort(handle);
    return (p != nullptr && p->mNative != nullptr);
}

int32_t SerialManager::SendMessage(SerialHandle handle, const uint8_t* data, uint32_t size)
{
    Port* p = FindPort(handle);
    if (p == nullptr || p->mNative == nullptr)
        return -1;

    return SER_Write(p->mNative, data, size);
}

int32_t SerialManager::SendMessage(SerialHandle handle, const std::string& data)
{
    return SendMessage(handle, reinterpret_cast<const uint8_t*>(data.data()), (uint32_t)data.size());
}

void SerialManager::StartReceive(SerialHandle handle)
{
    Port* p = FindPort(handle);
    if (p == nullptr || p->mNative == nullptr || p->mReceiving)
        return;

    p->mShouldStop.store(false);
    p->mDisconnected.store(false);

    SerialReadCtx* ctx = new SerialReadCtx();
    ctx->mNative   = p->mNative;
    ctx->mRxBuffer = &p->mRxBuffer;
    ctx->mRxMutex  = (MutexObject*)p->mRxMutex;
    ctx->mStopFlag = &p->mShouldStop;
    ctx->mDeadFlag = &p->mDisconnected;

    p->mReadThread = (void*)SYS_CreateThread(SerialReadThreadMain, ctx);
    if (p->mReadThread == nullptr)
    {
        delete ctx;
        return;
    }
    p->mReceiving = true;
}

void SerialManager::StopReceive(SerialHandle handle)
{
    Port* p = FindPort(handle);
    if (p == nullptr || !p->mReceiving)
        return;

    JoinAndDestroyReadThread(p);
}

bool SerialManager::IsReceiving(SerialHandle handle) const
{
    const Port* p = FindPort(handle);
    return (p != nullptr && p->mReceiving);
}

void SerialManager::JoinAndDestroyReadThread(Port* port)
{
    if (port == nullptr)
        return;

    if (port->mReadThread == nullptr)
    {
        port->mReceiving = false;
        return;
    }

    port->mShouldStop.store(true);
    SYS_JoinThread((ThreadObject*)port->mReadThread);
    SYS_DestroyThread((ThreadObject*)port->mReadThread);
    port->mReadThread = nullptr;
    port->mReceiving  = false;
    port->mShouldStop.store(false);
}

SerialManager::Port* SerialManager::FindPort(SerialHandle handle)
{
    for (uint32_t i = 0; i < mPorts.size(); ++i)
    {
        if (mPorts[i] != nullptr && mPorts[i]->mHandle == handle)
            return mPorts[i];
    }
    return nullptr;
}

const SerialManager::Port* SerialManager::FindPort(SerialHandle handle) const
{
    for (uint32_t i = 0; i < mPorts.size(); ++i)
    {
        if (mPorts[i] != nullptr && mPorts[i]->mHandle == handle)
            return mPorts[i];
    }
    return nullptr;
}

void SerialManager::PreTickUpdate(float /*deltaTime*/)
{
    // Fire pending connect events.
    for (uint32_t i = 0; i < mPorts.size(); ++i)
    {
        Port* p = mPorts[i];
        if (p != nullptr && p->mConnectPending)
        {
            p->mConnectPending = false;
            DispatchConnect(p->mHandle, p->mPortName);
        }
    }

    // Drain receive buffers and fire message events.
    for (uint32_t i = 0; i < mPorts.size(); ++i)
    {
        Port* p = mPorts[i];
        if (p == nullptr || !p->mReceiving || p->mRxMutex == nullptr)
            continue;

        std::vector<uint8_t> frameBuffer;

        SYS_LockMutex((MutexObject*)p->mRxMutex);
        if (!p->mRxBuffer.empty())
        {
            const uint32_t take = (p->mRxBuffer.size() > kMaxBytesPerFrame)
                                  ? kMaxBytesPerFrame
                                  : (uint32_t)p->mRxBuffer.size();
            frameBuffer.assign(p->mRxBuffer.begin(), p->mRxBuffer.begin() + take);
            p->mRxBuffer.erase(p->mRxBuffer.begin(), p->mRxBuffer.begin() + take);
        }
        SYS_UnlockMutex((MutexObject*)p->mRxMutex);

        if (!frameBuffer.empty())
        {
            DispatchMessage(p->mHandle, frameBuffer.data(), (uint32_t)frameBuffer.size());
        }
    }

    // Handle OS-level disconnects observed by the read thread.
    for (int32_t i = (int32_t)mPorts.size() - 1; i >= 0; --i)
    {
        Port* p = mPorts[i];
        if (p == nullptr)
            continue;

        if (p->mDisconnected.load())
        {
            const SerialHandle h = p->mHandle;
            JoinAndDestroyReadThread(p);
            if (p->mNative != nullptr)
            {
                SER_Close(p->mNative);
                p->mNative = nullptr;
            }
            if (p->mRxMutex != nullptr)
            {
                SYS_DestroyMutex((MutexObject*)p->mRxMutex);
                p->mRxMutex = nullptr;
            }
            delete p;
            mPorts.erase(mPorts.begin() + i);
            DispatchDisconnect(h);
        }
    }
}

void SerialManager::DispatchMessage(SerialHandle handle, const uint8_t* data, uint32_t size)
{
    mCurrentEventHandle = handle;
    mCurrentEventData.assign(reinterpret_cast<const char*>(data), size);
    mCurrentEventPortName.clear();

    if (mScriptOnMessage.IsValid())
    {
        Datum params[2];
        params[0] = Datum((int32_t)handle);
        params[1] = Datum(mCurrentEventData);
        mScriptOnMessage.Call(2, params);
    }

    const int32_t numWorlds = GetNumWorlds();
    for (int32_t wi = 0; wi < numWorlds; ++wi)
    {
        World* w = GetWorld(wi);
        if (w == nullptr)
            continue;

        // Fire per-Script OnSerialMessage method.
        std::vector<Node*> nodes;
        w->FindNodes<Node>(nodes);
        for (uint32_t i = 0; i < nodes.size(); ++i)
        {
            if (nodes[i] == nullptr)
                continue;
            Script* s = nodes[i]->GetScript();
            if (s != nullptr)
                s->OnSerialMessage(handle, mCurrentEventData);
        }

        // Fire node-graph event.
        std::vector<NodeGraphPlayer*> players;
        w->FindNodes<NodeGraphPlayer>(players);
        for (uint32_t i = 0; i < players.size(); ++i)
        {
            NodeGraphPlayer* p = players[i];
            if (p != nullptr && p->IsPlaying())
                p->FireNamedEvent("SerialMessage");
        }
    }
}

void SerialManager::DispatchConnect(SerialHandle handle, const std::string& portName)
{
    mCurrentEventHandle   = handle;
    mCurrentEventPortName = portName;
    mCurrentEventData.clear();

    if (mScriptOnConnect.IsValid())
    {
        Datum params[2];
        params[0] = Datum((int32_t)handle);
        params[1] = Datum(portName);
        mScriptOnConnect.Call(2, params);
    }

    const int32_t numWorlds = GetNumWorlds();
    for (int32_t wi = 0; wi < numWorlds; ++wi)
    {
        World* w = GetWorld(wi);
        if (w == nullptr)
            continue;

        std::vector<NodeGraphPlayer*> players;
        w->FindNodes<NodeGraphPlayer>(players);
        for (uint32_t i = 0; i < players.size(); ++i)
        {
            NodeGraphPlayer* p = players[i];
            if (p != nullptr && p->IsPlaying())
                p->FireNamedEvent("SerialConnected");
        }
    }
}

void SerialManager::DispatchDisconnect(SerialHandle handle)
{
    mCurrentEventHandle = handle;
    mCurrentEventData.clear();

    if (mScriptOnDisconnect.IsValid())
    {
        Datum params[1];
        params[0] = Datum((int32_t)handle);
        mScriptOnDisconnect.Call(1, params);
    }

    const int32_t numWorlds = GetNumWorlds();
    for (int32_t wi = 0; wi < numWorlds; ++wi)
    {
        World* w = GetWorld(wi);
        if (w == nullptr)
            continue;

        std::vector<NodeGraphPlayer*> players;
        w->FindNodes<NodeGraphPlayer>(players);
        for (uint32_t i = 0; i < players.size(); ++i)
        {
            NodeGraphPlayer* p = players[i];
            if (p != nullptr && p->IsPlaying())
                p->FireNamedEvent("SerialDisconnected");
        }
    }
}
