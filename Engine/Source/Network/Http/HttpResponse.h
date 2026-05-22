#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "PolyphaseAPI.h"
#include "Network/Http/HttpTypes.h"
#include "Network/Http/HttpRequest.h"

class Stream;
class Texture;
class SoundWave;

class POLYPHASE_API HttpResponse
{
public:
    // Out-of-line so the engine DLL is the single owner of the class's
    // construction/destruction/copy/move code paths. Forces every consumer
    // through one canonical implementation regardless of the TU's view of
    // std::string's ABI.
    HttpResponse();
    ~HttpResponse();
    HttpResponse(const HttpResponse&);
    HttpResponse(HttpResponse&&) noexcept;
    HttpResponse& operator=(const HttpResponse&);
    HttpResponse& operator=(HttpResponse&&) noexcept;

    int                  GetStatus()      const { return mStatusCode;   }
    HttpError            GetError()       const { return mError;        }
    const std::string&   GetErrorMessage()const { return mErrorMessage; }
    const HttpHeaderMap& GetHeaders()     const { return mHeaders;      }
    const std::vector<uint8_t>& GetBody() const { return mBody;         }
    const std::string&   GetFinalUrl()    const { return mFinalUrl;     }

    bool IsSuccess() const { return HttpStatusIsSuccess(mStatusCode) && mError == HttpError::None; }

    // Header lookup (case-insensitive). Returns "" when missing.
    const std::string& GetHeader(const std::string& name) const;
    bool               HasHeader(const std::string& name) const;

    // Body-as-string view. Doesn't copy; reinterprets the byte buffer as a
    // std::string_view-equivalent (returned by value as std::string for ABI
    // simplicity). Use GetBody() for raw byte access.
    std::string GetBodyAsString() const;

    // Wraps the body as a Stream without copying. The Stream's lifetime must
    // not exceed this HttpResponse's lifetime.
    Stream GetStream() const;

    // Decode the body as a PNG/JPG/TGA texture. Returns nullptr on failure.
    // The returned Texture is allocated via the engine factory; callers own
    // the lifetime via AssetRef in normal use.
    Texture*   GetTexture() const;

    // Decode the body as a WAV or OGG sound. Format detected via magic bytes.
    // Returns nullptr on failure.
    SoundWave* GetSoundWave() const;

    // Mutators (used by HttpClient internals).
    void SetStatus(int code)                                { mStatusCode = code; }
    void SetError(HttpError err, std::string msg = {})      { mError = err; mErrorMessage = std::move(msg); }
    void SetFinalUrl(std::string url)                       { mFinalUrl = std::move(url); }
    HttpHeaderMap&        MutableHeaders()                  { return mHeaders; }
    std::vector<uint8_t>& MutableBody()                     { return mBody; }

private:
    int                  mStatusCode = 0;
    HttpError            mError      = HttpError::None;
    std::string          mErrorMessage;
    HttpHeaderMap        mHeaders;
    std::vector<uint8_t> mBody;
    std::string          mFinalUrl;
};
