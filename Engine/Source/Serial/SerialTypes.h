#pragma once

#include <stdint.h>
#include <string>

typedef uint32_t SerialHandle;
static const SerialHandle INVALID_SERIAL_HANDLE = 0;

enum class SerialParity : uint8_t
{
    None,
    Odd,
    Even,
    Mark,
    Space
};

struct SerialConfig
{
    uint32_t     mBaudRate    = 9600;
    uint8_t      mDataBits    = 8;
    uint8_t      mStopBits    = 1;
    SerialParity mParity      = SerialParity::None;
    bool         mFlowControl = false;
};

struct SerialPortInfo
{
    std::string mPortName;
    std::string mDescription;
};

struct SerialNative;
