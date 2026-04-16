# Serial Communication

The Serial system provides RS-232 / USB-CDC serial port communication for talking to external hardware like Arduino, microcontrollers, sensors, and other serial devices. You can enumerate available ports, open connections with configurable baud rates and line settings, send binary data, and receive incoming bytes via callbacks.

## Platform Support

| Platform         | Support    | Notes |
|------------------|------------|-------|
| Windows          | Full       | Enumerates COM ports via Windows Setup API |
| Linux            | Full       | Enumerates `/dev/ttyUSB*`, `/dev/ttyACM*`, `/dev/ttyS*`, `/dev/ttyAMA*` |
| Android          | Stub       | All operations return failure. No ports enumerated. |
| Dolphin / 3DS    | Stub       | Same as Android |

## Scrip Example

Create a ```Scripts/SerialGameManager.lua```
```lua
SerialGameManager = {}

SerialGameManager.instance = nil

function SerialGameManager:Create()
	self.connection = nil
end

function SerialGameManager:Start()

	self.connection = Serial.Connect("COM3", {
    baud        = 115200,
    dataBits    = 8,
    stopBits    = 1,
    parity      = "none",  
    flowControl = false,
})
if self.connection == 0 then error("open failed") end
	Log.Debug("SerialGameManager:Start() Connected to Serial Port : " .. tostring(self.connection))

end


function SerialGameManager:Tick(deltaTime)

	if Input.IsKeyJustDown(Key.Space) then
		if self.connection then
			Log.Debug("SerialGameManager:Tick() Sending 'blink(5)' Command to Serial Device")
		local written = Serial.Send(self.connection, "blink(5)\n")
		end
	end
end


function SerialGameManager:Destroy()

if self.connection then
Serial.StopReceive(self.connection)
Serial.Disconnect(self.connection)
self.connection = nil
end
end


```
## Reference Example

```lua
-- Find an Arduino
local port = nil
for _, p in ipairs(Serial.EnumeratePorts()) do
    if p.description:find("Arduino") then port = p.name; break end
end
assert(port, "no Arduino found")

-- Connect at 115200 baud
local h = Serial.Connect(port, { baud = 115200 })

-- Listen for incoming data
Serial.SetMessageCallback(function(handle, data)
    Log.Debug("Received: " .. data)
end)
Serial.StartReceive(h)

-- Send a command
Serial.Send(h, "hello\n")

-- When done
Serial.StopReceive(h)
Serial.Disconnect(h)
```

## Lua API Reference

### Serial.EnumeratePorts()

Returns a list of available serial ports on the system.

```lua
local ports = Serial.EnumeratePorts()
for i, p in ipairs(ports) do
    Log.Debug(i .. ": " .. p.name .. " - " .. p.description)
end
```

Each entry has:

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | System port name (`"COM3"`, `"/dev/ttyUSB0"`) |
| `description` | string | Human-readable label (`"USB Serial Port (COM3)"`) |

### Serial.Connect(portName, [config])

Opens a serial port and returns a handle. Returns `0` on failure.

| Parameter | Type | Description |
|-----------|------|-------------|
| portName | string | Port name from `EnumeratePorts()` |
| config | table | Optional configuration (defaults below) |

**Config fields:**

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `baud` | number | `9600` | Baud rate |
| `dataBits` | number | `8` | Data bits (5-8) |
| `stopBits` | number | `1` | Stop bits (1 or 2) |
| `parity` | string | `"none"` | `"none"`, `"odd"`, `"even"`, `"mark"`, `"space"` |
| `flowControl` | boolean | `false` | Enable RTS/CTS hardware flow control |

```lua
local h = Serial.Connect("COM3", {
    baud        = 115200,
    dataBits    = 8,
    stopBits    = 1,
    parity      = "none",
    flowControl = false,
})
if h == 0 then
    Log.Error("Failed to open COM3")
end
```

### Serial.Disconnect(handle)

Closes a serial port. Stops any active receive loop and releases the handle.

```lua
Serial.Disconnect(h)
```

### Serial.Send(handle, data)

Sends data to an open serial port. Returns the number of bytes written, or `-1` on error. Lua strings are binary-safe, so null bytes and raw binary data work.

```lua
-- Send text
Serial.Send(h, "PING\n")

-- Send raw bytes
Serial.Send(h, string.char(0xAA, 0x55, 0x01, 0x02))
```

### Serial.StartReceive(handle)

Starts a background read loop for the port. Incoming data is delivered to your message callback on the main thread each frame.

```lua
Serial.StartReceive(h)
```

### Serial.StopReceive(handle)

Stops the background read loop. Data already buffered but not yet delivered will still be dispatched.

```lua
Serial.StopReceive(h)
```

### Callbacks

```lua
-- Called when data arrives on any port with an active receive loop.
-- handle: which port the data came from
-- data: raw byte string (use #data for byte count)
Serial.SetMessageCallback(function(handle, data)
    Log.Debug(string.format("Port #%d: %d bytes", handle, #data))
end)

-- Called when a port is successfully connected
Serial.SetConnectCallback(function(handle, portName)
    Log.Debug("Connected to " .. portName)
end)

-- Called when a port disconnects (manual or device unplugged)
Serial.SetDisconnectCallback(function(handle)
    Log.Debug("Port #" .. handle .. " disconnected")
end)
```

Callbacks are global, not per-handle. Use the `handle` argument to identify which port the event belongs to.

### Per-Script Callback

If a Script component defines `OnSerialMessage`, it will be called automatically on every active script when data arrives. No registration needed.

```lua
function MyScript:OnSerialMessage(handle, data)
    Log.Debug("Got " .. #data .. " bytes from port #" .. handle)
end
```

## Node Graph Nodes

### Action Nodes (Serial category)

| Node | Inputs | Outputs |
|------|--------|---------|
| **Serial Enumerate Ports** | flow | flow, int count, string firstPort |
| **Serial Connect** | flow, string portName, int baud | flow, int handle, bool success |
| **Serial Disconnect** | flow, int handle | flow |
| **Serial Send Message** | flow, int handle, string data | flow, int bytesWritten |
| **Serial Start Receive** | flow, int handle | flow |
| **Serial Stop Receive** | flow, int handle | flow |

### Event Nodes (Event category)

| Node | Outputs |
|------|---------|
| **On Serial Message** | flow, int handle, string data |
| **On Serial Connected** | flow, int handle, string portName |
| **On Serial Disconnected** | flow, int handle |

### Node Graph Example

```
[Start Event] --> [Serial Connect "COM3" 115200] --> [Serial Start Receive (handle)]

[On Serial Connected]    --> [Debug Log: "connected to ${portName}"]
[On Serial Message]      --> [Debug Log: "got: ${data}"]
[On Serial Disconnected] --> [Debug Log: "disconnected"]
```

## Things to Know

- **Callbacks are global.** `SetMessageCallback` installs one function for all ports. Check the `handle` argument to route messages from different devices.
- **Binary-safe strings.** `Serial.Send(h, "\0\xff\x01")` sends exactly those bytes. The callback `data` string preserves null bytes; use `#data` for byte count.
- **Auto-disconnect.** If a device is physically unplugged, the disconnect callback fires automatically on the next frame.
- **Non-standard baud rates.** On Windows, whether unusual baud rates work depends on the USB-serial driver (FTDI and CP210x support up to 3+ Mbaud; generic USB-CDC may cap at 115200). On Linux, only standard rates (1200, 2400, ..., 921600) are mapped; unknown rates fall back to 9600.

## Examples

### Arduino Echo Test

```lua
local port = nil
for _, p in ipairs(Serial.EnumeratePorts()) do
    if p.description:find("Arduino") then port = p.name; break end
end
assert(port, "no Arduino found")

local h = Serial.Connect(port, { baud = 115200 })
Serial.SetMessageCallback(function(_, data)
    Log.Debug("<< " .. data)
end)
Serial.StartReceive(h)
Serial.Send(h, "hello\n")
```

### Multi-Device Routing

```lua
local sensors = {}
local display = nil

for _, p in ipairs(Serial.EnumeratePorts()) do
    if p.description:find("Sensor") then
        local h = Serial.Connect(p.name, { baud = 9600 })
        Serial.StartReceive(h)
        sensors[h] = p.name
    elseif p.description:find("Display") then
        display = Serial.Connect(p.name, { baud = 115200 })
    end
end

Serial.SetMessageCallback(function(handle, data)
    if sensors[handle] then
        Log.Debug("Sensor " .. sensors[handle] .. ": " .. data)
        if display then
            Serial.Send(display, data)  -- Forward to display
        end
    end
end)
```

### Command-Response Protocol

```lua
local h = Serial.Connect("COM4", { baud = 57600 })

Serial.SetMessageCallback(function(_, data)
    local value = tonumber(data)
    if value then
        Log.Debug("Temperature: " .. value .. " C")
    end
end)
Serial.StartReceive(h)

-- Request a reading every tick (throttle as needed)
function MyScript:Tick()
    Serial.Send(h, "READ_TEMP\n")
end
```
