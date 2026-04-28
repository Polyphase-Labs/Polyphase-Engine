#pragma once

#include "Serial/SerialTypes.h"

#include <vector>

void SER_Initialize();
void SER_Shutdown();

std::vector<SerialPortInfo> SER_EnumeratePorts();

SerialNative* SER_Open(const char* portName, const SerialConfig& cfg);
void          SER_Close(SerialNative* native);

int32_t       SER_Write(SerialNative* native, const uint8_t* data, uint32_t size);
int32_t       SER_Read(SerialNative* native, uint8_t* buffer, uint32_t maxSize);
