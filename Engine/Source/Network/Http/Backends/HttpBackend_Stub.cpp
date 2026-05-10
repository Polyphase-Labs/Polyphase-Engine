// Fallback HTTP backend for platforms that don't yet have a native impl
// wired up. Compiled when none of the platform-specific backends apply.
//
// To extend support to a new platform, add a HttpBackend_<Platform>.cpp file
// with the appropriate `#if PLATFORM_*` guard and implement the backend
// against the platform's HTTP/TLS facility.

#if !PLATFORM_WINDOWS && !PLATFORM_LINUX && !PLATFORM_3DS && !PLATFORM_DOLPHIN

#include "Network/Http/Backends/HttpBackend.h"

namespace
{
    class StubBackend : public HttpBackend
    {
    public:
        bool        Initialize()                                                        override { return false; }
        void        Shutdown()                                                          override {}
        bool        IsAvailable()                                                 const override { return false; }
        const char* GetMissingDependencyMessage()                                 const override
        {
            return "HTTP not implemented on this platform.";
        }

        void PerformRequest(const HttpRequest&,
                            std::atomic<bool>&,
                            HttpResponse& outResponse) override
        {
            outResponse.SetError(HttpError::Unavailable,
                "HTTP not implemented on this platform.");
        }
    };
}

std::unique_ptr<HttpBackend> CreatePlatformHttpBackend()
{
    return std::unique_ptr<HttpBackend>(new StubBackend());
}

#endif
