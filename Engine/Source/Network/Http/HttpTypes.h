#pragma once

#include <stdint.h>

#include "PolyphaseAPI.h"

enum class HttpVerb : uint8_t
{
    Get,
    Post,
    Put,
    Patch,
    Delete,
    Head,
    Options,

    Count
};

enum class HttpError : uint8_t
{
    None,
    NotInitialized,
    Unavailable,
    InvalidUrl,
    Network,
    Tls,
    Timeout,
    TooLarge,
    Cancelled,
    BadResponse,
    Unknown,

    Count
};

POLYPHASE_API const char* HttpVerbToString(HttpVerb verb);
POLYPHASE_API HttpVerb    HttpVerbFromString(const char* s);
POLYPHASE_API const char* HttpErrorToString(HttpError err);
POLYPHASE_API bool        HttpStatusIsSuccess(int statusCode);
POLYPHASE_API bool        HttpStatusIsRedirect(int statusCode);
