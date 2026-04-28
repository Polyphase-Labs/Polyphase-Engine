# Serial I/O Subsystem

Cross-platform RS-232 / USB-CDC serial support for Polyphase. Users can enumerate
ports, open multiple concurrent connections, send arbitrary byte payloads, and
opt into a background read loop that delivers incoming bytes to Lua scripts and
node-graph event nodes on the main thread.

---

## 1. Overview

**Supported platforms**

| Platform     | Backend                                         | Notes |
|--------------|-------------------------------------------------|-------|
| Windows      | Win32 `CreateFileA` + DCB, `SetupDiGetClassDevs` | First-class. Needs `setupapi.lib` (linked via `#pragma comment`). |
| Linux        | POSIX `open` + `termios`                         | First-class. Enumerates `/dev/ttyUSB*`, `/dev/ttyACM*`, `/dev/ttyS*`, `/dev/ttyAMA*`. |
| Android      | Stub                                             | `EnumeratePorts` returns `{}`; `Connect` returns `INVALID_SERIAL_HANDLE`. Single warn-once log. |
| Dolphin, 3DS | Stub                                             | Same as Android. |

**Two layers**

```
┌──────────────────────────────────────────────────────────────────┐
│   Lua scripts / Node-graph event nodes (main-thread dispatch)    │
└─────────────────────────┬────────────────────────────────────────┘
                          │
┌─────────────────────────▼────────────────────────────────────────┐
│   SerialManager (singleton)                                      │
│   - Handle table, per-handle RX buffer, per-handle read thread   │
│   - PreTickUpdate: drain RX, fire connect / message / disconnect │
└─────────────────────────┬────────────────────────────────────────┘
                          │
┌─────────────────────────▼────────────────────────────────────────┐
│   Serial/Serial.h (SER_* free functions, C-style)                │
│   - Platform-specific .cpp in Serial/{Windows,Linux,Stub}/       │
└──────────────────────────────────────────────────────────────────┘
```

The low-level `SER_*` layer only knows about OS handles and byte buffers.
`SerialManager` owns handle IDs, threading, and event dispatch.

---

## 2. Threading Model

```
┌──────────────────────────────────────────────────────┐
│                   Main thread                        │
│                                                      │
│   PreTickUpdate (called from Engine::Update)         │
│     ├─ Fire pending connect events                   │
│     ├─ Lock RX mutex, drain ≤64 KiB per port         │
│     ├─ Dispatch OnSerialMessage to scripts + graphs  │
│     └─ Close ports flagged as disconnected           │
└──────────────────────────────────────────────────────┘
                         ▲
                         │  mRxBuffer (guarded by mRxMutex)
                         │
┌────────────────────────┴─────────────────────────────┐
│     Read thread (one per port, spawned by            │
│     StartReceive via SYS_CreateThread)               │
│                                                      │
│     while (!stop)                                    │
│        n = SER_Read(...)                             │
│        if n<0 → set mDisconnected, exit              │
│        if n>0 → LockMutex; append; UnlockMutex       │
│        else    SYS_Sleep(2ms)                        │
└──────────────────────────────────────────────────────┘
```

**Guarantees**

- Callbacks always fire on the main thread, during `PreTickUpdate`.
- `SendMessage` is main-thread-only (no lock is taken; don't call from Lua inside
  a background thread — you shouldn't be in one anyway).
- `Disconnect` sets the stop flag, joins the read thread, closes the native
  handle, then fires the disconnect event. The handle becomes invalid
  immediately after `Disconnect` returns.
- Automatic disconnect: if `SER_Read` returns `-1` (device unplugged), the read
  thread flips `mDisconnected`. The next `PreTickUpdate` closes the port and
  fires `OnSerialDisconnect`.

---

## 3. Lifecycle

Hooks in `Engine/Source/Engine/Engine.cpp`:

| Phase              | Line   | Call                                             |
|--------------------|--------|--------------------------------------------------|
| Create             | ~L376  | `SerialManager::Create()`                        |
| Initialize         | ~L498  | `SerialManager::Get()->Initialize()`             |
| PreTickUpdate      | ~L697  | `SerialManager::Get()->PreTickUpdate(dt)`        |
| Shutdown           | ~L836  | `SerialManager::Get()->Shutdown()`               |
| Destroy            | ~L867  | `SerialManager::Destroy()`                       |
| ForceLinkage       | ~L246  | `FORCE_LINK_CALL(SerialGraphNodes)`              |

`Initialize` calls `SER_Initialize` (currently a no-op on all platforms).
`Shutdown` joins every read thread, closes every native handle, frees every
mutex, and finally calls `SER_Shutdown`.

---

## 4. Public C++ API

### `Engine/Source/Serial/SerialTypes.h`

```cpp
typedef uint32_t SerialHandle;
static const SerialHandle INVALID_SERIAL_HANDLE = 0;

enum class SerialParity : uint8_t { None, Odd, Even, Mark, Space };

struct SerialConfig {
    uint32_t     mBaudRate    = 9600;
    uint8_t      mDataBits    = 8;
    uint8_t      mStopBits    = 1;
    SerialParity mParity      = SerialParity::None;
    bool         mFlowControl = false;   // RTS/CTS
};

struct SerialPortInfo {
    std::string mPortName;     // "COM3", "/dev/ttyUSB0"
    std::string mDescription;  // Human-readable (Windows: "USB Serial Port (COM3)"; Linux: device basename).
};

struct SerialNative; // opaque; HANDLE on Windows, int fd on Linux.
```

### `Engine/Source/Serial/Serial.h` (low-level, platform-agnostic)

```cpp
void SER_Initialize();
void SER_Shutdown();

std::vector<SerialPortInfo> SER_EnumeratePorts();

SerialNative* SER_Open(const char* portName, const SerialConfig& cfg);
void          SER_Close(SerialNative* native);

int32_t SER_Write(SerialNative* native, const uint8_t* data, uint32_t size);
int32_t SER_Read (SerialNative* native, uint8_t* buffer, uint32_t maxSize);
```

`SER_Read` returns `n>0` (bytes), `0` (no data available, non-blocking), or `-1`
(fatal error; caller must close and re-open). `SER_Write` uses the same sign
convention and returns actual bytes written.

### `Engine/Source/Engine/SerialManager.h` (high-level)

```cpp
class SerialManager {
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

    int32_t      SendMessage(SerialHandle h, const uint8_t* data, uint32_t size);
    int32_t      SendMessage(SerialHandle h, const std::string& data);

    void         StartReceive(SerialHandle h);
    void         StopReceive (SerialHandle h);
    bool         IsReceiving (SerialHandle h) const;

    void SetScriptMessageCallback   (const ScriptFunc& f);
    void SetScriptConnectCallback   (const ScriptFunc& f);
    void SetScriptDisconnectCallback(const ScriptFunc& f);

    // Read during graph-node Evaluate():
    SerialHandle       GetCurrentEventHandle()   const;
    const std::string& GetCurrentEventPortName() const;
    const std::string& GetCurrentEventData()     const;
};
```

Handles are integers issued by the manager. `0` is reserved as invalid. A
handle is released immediately when `Disconnect` returns or when the manager
auto-closes a disconnected port — using a stale handle is safe; every method
that takes one returns `-1` / `false` if the handle is unknown.

---

## 5. Lua API

Global module `Serial`. Binds are registered in
`Engine/Source/LuaBindings/Serial_Lua.cpp`, called from `BindLuaInterface`
alongside `Network_Lua::Bind()`.

```lua
-- Enumerate
for i, p in ipairs(Serial.EnumeratePorts()) do
    print(i, p.name, p.description)
end

-- Connect (cfg table optional; defaults to 9600 8-N-1 no flow)
local h = Serial.Connect("COM3", {
    baud        = 115200,
    dataBits    = 8,
    stopBits    = 1,
    parity      = "none",   -- "none" | "odd" | "even" | "mark" | "space"
    flowControl = false,
})
if h == 0 then error("open failed") end

-- Send (Lua strings are binary-safe — null bytes OK)
local written = Serial.Send(h, "PING\n")
Serial.Send(h, string.char(0xAA, 0x55, 0x01, 0x02))

-- Receive (opt-in)
Serial.StartReceive(h)

Serial.SetMessageCallback(function(handle, data)
    -- data is a raw byte string.
    print(string.format("#%d got %d bytes", handle, #data))
end)

Serial.SetConnectCallback   (function(h, portName) print("connect",    h, portName) end)
Serial.SetDisconnectCallback(function(h)           print("disconnect", h)           end)

-- Stop / close
Serial.StopReceive(h)
Serial.Disconnect(h)
```

**Per-Script method.** If a Script component defines
`function script:OnSerialMessage(handle, data) ... end`, the manager invokes
it on every active script, every time a receive chunk arrives. No registration
required beyond defining the method — `Script` detects it via
`CheckIfFunctionExists` at load time (same path as `BeginOverlap`).

---

## 6. Node-Graph API

Registered in `SceneGraphDomain::RegisterNodeTypes`. All classes live in
`Engine/Source/Engine/NodeGraph/Nodes/SerialGraphNodes.{h,cpp}`.

| Class                          | Kind   | Category | Pins |
|--------------------------------|--------|----------|------|
| `SerialEnumeratePortsNode`     | Action | Serial   | In: flow · Out: flow, int count, string firstPort |
| `SerialConnectNode`            | Action | Serial   | In: flow, string portName, int baud · Out: flow, int handle, bool success |
| `SerialDisconnectNode`         | Action | Serial   | In: flow, int handle · Out: flow |
| `SerialSendMessageNode`        | Action | Serial   | In: flow, int handle, string data · Out: flow, int bytesWritten |
| `SerialStartReceiveNode`       | Action | Serial   | In: flow, int handle · Out: flow |
| `SerialStopReceiveNode`        | Action | Serial   | In: flow, int handle · Out: flow |
| `SerialMessageEventNode`       | Event  | Event    | Out: flow, int handle, string data |
| `SerialConnectedEventNode`     | Event  | Event    | Out: flow, int handle, string portName |
| `SerialDisconnectedEventNode`  | Event  | Event    | Out: flow, int handle |

Event nodes pull their payload from `SerialManager::GetCurrentEventXxx()`
during `Evaluate`, which the manager sets immediately before firing the event
on every active `NodeGraphPlayer` via `NodeGraphPlayer::FireNamedEvent(name)`.

---

## 7. Platform Notes

### Windows (`Serial/Windows/Serial_Windows.cpp`)

- Opens ports as `\\.\COMx` so numbers ≥ 10 work.
- DCB: explicit `ByteSize`, `StopBits`, `Parity`; `fBinary=TRUE`, `fNull=FALSE`.
- `COMMTIMEOUTS.ReadIntervalTimeout=MAXDWORD` + both total-timeouts 0 gives
  a non-blocking read that returns whatever's in the buffer immediately.
- Enumeration uses `SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, ...)` then reads
  `PortName` from the device's `Device Parameters` registry key, filtering out
  `LPTx`. Friendly name comes from `SPDRP_FRIENDLYNAME`.
- `setupapi.lib` is linked via `#pragma comment(lib, "setupapi.lib")` so no
  vcxproj change is needed.

### Linux (`Serial/Linux/Serial_Linux.cpp`)

- Opens with `O_RDWR | O_NOCTTY | O_NONBLOCK`.
- Raw-mode termios: `~(ICANON|ECHO|ISIG)`, `~(IXON|IXOFF)`, `~OPOST`.
- `VMIN=0 VTIME=0` → `read` returns immediately.
- Enumeration scans `/dev` for `ttyUSB*`, `ttyACM*`, `ttyS*`, `ttyAMA*`.
- `SER_Read` distinguishes "no data" (`EAGAIN` → 0) from "device gone" (other
  errno or `read==0` → -1).

### Android / Dolphin / 3DS (`Serial/Stub/Serial_Stub.cpp`)

- Everything no-ops. `SER_EnumeratePorts` returns `{}`, `SER_Open` returns
  `nullptr`, reads/writes return `-1`. A single `LogWarning` per process
  (`sWarned`) on first use — don't log per call.
- For real Android serial, replace this file with a JNI bridge to the Android
  USB Host API. Out of scope for this plan.

---

## 8. Gotchas

- **Per-frame cap.** `PreTickUpdate` drains at most `kMaxBytesPerFrame` (64 KiB)
  per port per frame. At 1 Mbaud ≈ 100 KB/s a single frame at 60fps sees ≈
  1.7 KB, so this cap is generous for normal traffic but prevents a runaway
  spammer from stalling the main thread with a 10 MB callback payload.
- **StopReceive keeps buffered bytes.** Stopping the read thread does not flush
  `mRxBuffer`; the next `StartReceive` picks up whatever accumulated in
  between (not useful in practice — there's nothing accumulating because the
  thread is gone — but buffered-but-not-yet-drained bytes do survive).
- **Lua strings are binary-safe.** `Serial.Send(h, "\0\xff\x01")` works
  verbatim. The callback receives a `string` whose `#` gives byte count even
  across null bytes.
- **Windows non-standard baud rates.** `SetCommState` accepts any baud in the
  DCB, but whether the device actually honors it depends on the driver. FTDI
  and CP210x handle everything up to ≥3 Mbaud; generic USB-CDC may cap at
  115200.
- **Linux baud rate fallback.** Only standard rates (1200, 2400, …, 921600) are
  mapped. Unknown rates fall back to `B9600`. Add a `termios2` path if you
  need arbitrary bauds.
- **Callbacks set globally, not per-handle.** `SetMessageCallback` installs
  one function that fires for *every* open handle. Inspect the `handle`
  argument to route.

---

## 9. Example Scripts

### Lua — Arduino echo test

```lua
local port = nil
for _, p in ipairs(Serial.EnumeratePorts()) do
    if p.description:find("Arduino") then port = p.name; break end
end
assert(port, "no Arduino found")

local h = Serial.Connect(port, { baud = 115200 })
Serial.SetMessageCallback(function(_, data)
    print("<<", data)
end)
Serial.StartReceive(h)

Serial.Send(h, "hello\n")

-- In script:Tick or wherever
-- when done:
--   Serial.StopReceive(h)
--   Serial.Disconnect(h)
```

### Node Graph — receive-only

```
[On Serial Connected] ──► [Debug Log: "connected to ${portName}"]

[On Serial Message] ──► [Debug Log: "got: ${data}"]

[Start Event] ──► [Serial Connect "COM3" 115200] ──► [Serial Start Receive (handle)]
```
