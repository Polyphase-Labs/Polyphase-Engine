#if PLATFORM_ANDROID || PLATFORM_DOLPHIN || PLATFORM_3DS

#include "Serial/Serial.h"
#include "Log.h"

struct SerialNative
{
    int mUnused = 0;
};

static bool sWarned = false;

static void WarnOnce(const char* what)
{
    if (!sWarned)
    {
        LogWarning("Serial %s is not supported on this platform.", what);
        sWarned = true;
    }
}

void SER_Initialize() {}
void SER_Shutdown() {}

std::vector<SerialPortInfo> SER_EnumeratePorts()
{
    WarnOnce("enumeration");
    return std::vector<SerialPortInfo>();
}

SerialNative* SER_Open(const char* /*portName*/, const SerialConfig& /*cfg*/)
{
    WarnOnce("open");
    return nullptr;
}

void SER_Close(SerialNative* native)
{
    if (native != nullptr)
        delete native;
}

int32_t SER_Write(SerialNative* /*native*/, const uint8_t* /*data*/, uint32_t /*size*/)
{
    return -1;
}

int32_t SER_Read(SerialNative* /*native*/, uint8_t* /*buffer*/, uint32_t /*maxSize*/)
{
    return -1;
}

#endif
