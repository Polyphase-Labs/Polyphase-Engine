#if PLATFORM_WINDOWS

// Include engine headers FIRST so SystemTypes.h can pull <Windows.h> through
// the engine's canonical include path. Including <Windows.h> locally before
// engine headers on Windows 10 SDK 10.0.26100+ can produce
//   C2371 'DWORD': redefinition; different basic types
// when subsequent engine headers re-enter SystemTypes.h. Engine-first ordering
// matches Network_Windows.cpp's convention (it puts <WinSock2.h>/<Windows.h>
// before engine headers only because WinSock2 *must* be first; we have no
// such constraint here).
#include "Serial/Serial.h"
#include "Log.h"

#include <Windows.h>
#include <SetupAPI.h>

#pragma comment(lib, "setupapi.lib")

// GUID_DEVCLASS_PORTS from devguid.h: {4D36E978-E325-11CE-BFC1-08002BE10318}
static const GUID kGuidDevClassPorts = { 0x4d36e978, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };

struct SerialNative
{
    HANDLE mHandle = INVALID_HANDLE_VALUE;
};

void SER_Initialize()
{
}

void SER_Shutdown()
{
}

std::vector<SerialPortInfo> SER_EnumeratePorts()
{
    std::vector<SerialPortInfo> ports;

    HDEVINFO devInfo = SetupDiGetClassDevsA(&kGuidDevClassPorts, nullptr, nullptr, DIGCF_PRESENT);
    if (devInfo == INVALID_HANDLE_VALUE)
    {
        return ports;
    }

    SP_DEVINFO_DATA devInfoData = {};
    devInfoData.cbSize = sizeof(devInfoData);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devInfoData); ++i)
    {
        HKEY devKey = SetupDiOpenDevRegKey(devInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (devKey == INVALID_HANDLE_VALUE)
            continue;

        char portName[256] = {};
        DWORD portNameSize = sizeof(portName);
        DWORD type = 0;
        LONG result = RegQueryValueExA(devKey, "PortName", nullptr, &type, (LPBYTE)portName, &portNameSize);
        RegCloseKey(devKey);

        if (result != ERROR_SUCCESS || type != REG_SZ)
            continue;

        // Only keep COMx entries (filter out LPTx)
        if (strncmp(portName, "COM", 3) != 0)
            continue;

        char friendlyName[512] = {};
        DWORD friendlySize = sizeof(friendlyName);
        SetupDiGetDeviceRegistryPropertyA(devInfo, &devInfoData, SPDRP_FRIENDLYNAME, nullptr,
                                          (PBYTE)friendlyName, friendlySize, nullptr);

        SerialPortInfo info;
        info.mPortName = portName;
        info.mDescription = friendlyName;
        ports.push_back(info);
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return ports;
}

SerialNative* SER_Open(const char* portName, const SerialConfig& cfg)
{
    // Use "\\.\COMx" form to support port numbers >= 10.
    char fullName[64] = {};
    snprintf(fullName, sizeof(fullName), "\\\\.\\%s", portName);

    HANDLE h = CreateFileA(fullName,
                           GENERIC_READ | GENERIC_WRITE,
                           0,
                           nullptr,
                           OPEN_EXISTING,
                           0,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        LogWarning("SER_Open: failed to open %s (error %lu)", portName, GetLastError());
        return nullptr;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb))
    {
        CloseHandle(h);
        LogWarning("SER_Open: GetCommState failed for %s", portName);
        return nullptr;
    }

    dcb.BaudRate = cfg.mBaudRate;
    dcb.ByteSize = cfg.mDataBits;
    dcb.StopBits = (cfg.mStopBits == 2) ? TWOSTOPBITS : ONESTOPBIT;
    switch (cfg.mParity)
    {
    case SerialParity::None:  dcb.Parity = NOPARITY;    dcb.fParity = FALSE; break;
    case SerialParity::Odd:   dcb.Parity = ODDPARITY;   dcb.fParity = TRUE;  break;
    case SerialParity::Even:  dcb.Parity = EVENPARITY;  dcb.fParity = TRUE;  break;
    case SerialParity::Mark:  dcb.Parity = MARKPARITY;  dcb.fParity = TRUE;  break;
    case SerialParity::Space: dcb.Parity = SPACEPARITY; dcb.fParity = TRUE;  break;
    }

    if (cfg.mFlowControl)
    {
        dcb.fOutxCtsFlow = TRUE;
        dcb.fRtsControl  = RTS_CONTROL_HANDSHAKE;
    }
    else
    {
        dcb.fOutxCtsFlow = FALSE;
        dcb.fRtsControl  = RTS_CONTROL_ENABLE;
    }
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl  = DTR_CONTROL_ENABLE;
    dcb.fBinary      = TRUE;
    dcb.fNull        = FALSE;

    if (!SetCommState(h, &dcb))
    {
        CloseHandle(h);
        LogWarning("SER_Open: SetCommState failed for %s", portName);
        return nullptr;
    }

    // Non-blocking read: return immediately with whatever bytes are available.
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout         = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.ReadTotalTimeoutConstant    = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant   = 0;
    SetCommTimeouts(h, &timeouts);

    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

    SerialNative* native = new SerialNative();
    native->mHandle = h;
    return native;
}

void SER_Close(SerialNative* native)
{
    if (native == nullptr)
        return;
    if (native->mHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(native->mHandle);
    }
    delete native;
}

int32_t SER_Write(SerialNative* native, const uint8_t* data, uint32_t size)
{
    if (native == nullptr || native->mHandle == INVALID_HANDLE_VALUE)
        return -1;

    DWORD written = 0;
    if (!WriteFile(native->mHandle, data, size, &written, nullptr))
    {
        return -1;
    }
    return (int32_t)written;
}

int32_t SER_Read(SerialNative* native, uint8_t* buffer, uint32_t maxSize)
{
    if (native == nullptr || native->mHandle == INVALID_HANDLE_VALUE)
        return -1;

    DWORD read = 0;
    if (!ReadFile(native->mHandle, buffer, maxSize, &read, nullptr))
    {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING || err == ERROR_MORE_DATA)
            return 0;
        return -1;
    }
    return (int32_t)read;
}

#endif
