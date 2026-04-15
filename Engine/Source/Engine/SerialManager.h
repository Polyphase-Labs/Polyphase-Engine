#pragma once

#include "EngineTypes.h"
#include "ScriptFunc.h"
#include "Serial/Serial.h"

#include <atomic>
#include <string>
#include <vector>

// Conflict with winuser.h's SendMessage macro.
#ifdef SendMessage
#undef SendMessage
#endif

class SerialManager
{
public:

    static void           Create();
    static void           Destroy();
    static SerialManager* Get();

    void Initialize();
    void Shutdown();
    void PreTickUpdate(float deltaTime);

    std::vector<SerialPortInfo> EnumeratePorts();

    SerialHandle Connect(const char* portName, const SerialConfig& cfg);
    void         Disconnect(SerialHandle handle);
    bool         IsConnected(SerialHandle handle) const;

    int32_t      SendMessage(SerialHandle handle, const uint8_t* data, uint32_t size);
    int32_t      SendMessage(SerialHandle handle, const std::string& data);

    void         StartReceive(SerialHandle handle);
    void         StopReceive(SerialHandle handle);
    bool         IsReceiving(SerialHandle handle) const;

    void         SetScriptMessageCallback(const ScriptFunc& func)    { mScriptOnMessage = func; }
    void         SetScriptConnectCallback(const ScriptFunc& func)    { mScriptOnConnect = func; }
    void         SetScriptDisconnectCallback(const ScriptFunc& func) { mScriptOnDisconnect = func; }

    // Current-event accessors used by graph nodes during Evaluate().
    SerialHandle       GetCurrentEventHandle() const   { return mCurrentEventHandle; }
    const std::string& GetCurrentEventPortName() const { return mCurrentEventPortName; }
    const std::string& GetCurrentEventData() const     { return mCurrentEventData; }

private:

    struct Port
    {
        SerialHandle          mHandle      = INVALID_SERIAL_HANDLE;
        std::string           mPortName;
        SerialNative*         mNative      = nullptr;
        std::atomic<bool>     mShouldStop  { false };
        std::atomic<bool>     mDisconnected{ false };
        bool                  mReceiving   = false;
        bool                  mConnectPending    = true;
        void*                 mReadThread  = nullptr;   // ThreadObject* — opaque to avoid pulling <Windows.h> into this header.
        void*                 mRxMutex     = nullptr;   // MutexObject*
        std::vector<uint8_t>  mRxBuffer;
    };

    static SerialManager* sInstance;

    SerialManager();
    ~SerialManager();

    Port*       FindPort(SerialHandle handle);
    const Port* FindPort(SerialHandle handle) const;

    void        JoinAndDestroyReadThread(Port* port);
    void        DispatchMessage(SerialHandle handle, const uint8_t* data, uint32_t size);
    void        DispatchConnect(SerialHandle handle, const std::string& portName);
    void        DispatchDisconnect(SerialHandle handle);

    std::vector<Port*> mPorts;
    SerialHandle       mNextHandle = 1;

    ScriptFunc mScriptOnMessage;
    ScriptFunc mScriptOnConnect;
    ScriptFunc mScriptOnDisconnect;

    // Populated during DispatchXxx so graph event nodes can read the payload.
    SerialHandle mCurrentEventHandle = INVALID_SERIAL_HANDLE;
    std::string  mCurrentEventPortName;
    std::string  mCurrentEventData;
};
