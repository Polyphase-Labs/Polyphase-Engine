#pragma once

#include "EngineTypes.h"

void ExecCommon(const char* cmd, std::string* output);

// Detached fire-and-forget process spawn — see SYS_ExecDetached doc.
void ExecCommonDetached(const char* cmd);
