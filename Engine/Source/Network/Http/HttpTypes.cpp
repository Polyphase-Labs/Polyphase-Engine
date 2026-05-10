#include "Network/Http/HttpTypes.h"

#include <string.h>

const char* HttpVerbToString(HttpVerb verb)
{
    switch (verb)
    {
    case HttpVerb::Get:     return "GET";
    case HttpVerb::Post:    return "POST";
    case HttpVerb::Put:     return "PUT";
    case HttpVerb::Patch:   return "PATCH";
    case HttpVerb::Delete:  return "DELETE";
    case HttpVerb::Head:    return "HEAD";
    case HttpVerb::Options: return "OPTIONS";
    default:                return "GET";
    }
}

HttpVerb HttpVerbFromString(const char* s)
{
    if (s == nullptr) return HttpVerb::Get;
    if (strcmp(s, "GET")     == 0) return HttpVerb::Get;
    if (strcmp(s, "POST")    == 0) return HttpVerb::Post;
    if (strcmp(s, "PUT")     == 0) return HttpVerb::Put;
    if (strcmp(s, "PATCH")   == 0) return HttpVerb::Patch;
    if (strcmp(s, "DELETE")  == 0) return HttpVerb::Delete;
    if (strcmp(s, "HEAD")    == 0) return HttpVerb::Head;
    if (strcmp(s, "OPTIONS") == 0) return HttpVerb::Options;
    return HttpVerb::Get;
}

const char* HttpErrorToString(HttpError err)
{
    switch (err)
    {
    case HttpError::None:           return "None";
    case HttpError::NotInitialized: return "NotInitialized";
    case HttpError::Unavailable:    return "Unavailable";
    case HttpError::InvalidUrl:     return "InvalidUrl";
    case HttpError::Network:        return "Network";
    case HttpError::Tls:            return "Tls";
    case HttpError::Timeout:        return "Timeout";
    case HttpError::TooLarge:       return "TooLarge";
    case HttpError::Cancelled:      return "Cancelled";
    case HttpError::BadResponse:    return "BadResponse";
    case HttpError::Unknown:        return "Unknown";
    default:                        return "Unknown";
    }
}

bool HttpStatusIsSuccess(int statusCode)
{
    return statusCode >= 200 && statusCode < 300;
}

bool HttpStatusIsRedirect(int statusCode)
{
    return statusCode >= 300 && statusCode < 400 && statusCode != 304;
}
