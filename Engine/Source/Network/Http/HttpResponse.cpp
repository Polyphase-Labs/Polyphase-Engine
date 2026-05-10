#include "Network/Http/HttpResponse.h"

#include "Stream.h"
#include "Log.h"
#include "Assets/Texture.h"
#include "Assets/SoundWave.h"

const std::string& HttpResponse::GetHeader(const std::string& name) const
{
    static const std::string sEmpty;
    auto it = mHeaders.find(name);
    return (it != mHeaders.end()) ? it->second : sEmpty;
}

bool HttpResponse::HasHeader(const std::string& name) const
{
    return mHeaders.find(name) != mHeaders.end();
}

std::string HttpResponse::GetBodyAsString() const
{
    if (mBody.empty()) return std::string();
    return std::string(reinterpret_cast<const char*>(mBody.data()), mBody.size());
}

Stream HttpResponse::GetStream() const
{
    return Stream(reinterpret_cast<const char*>(mBody.data()), static_cast<uint32_t>(mBody.size()));
}

Texture* HttpResponse::GetTexture() const
{
    if (mBody.empty())
    {
        return nullptr;
    }

    Texture* tex = new Texture();
    if (!Texture::LoadFromMemory(mBody.data(), mBody.size(), *tex))
    {
        delete tex;
        return nullptr;
    }
    return tex;
}

SoundWave* HttpResponse::GetSoundWave() const
{
    if (mBody.empty())
    {
        return nullptr;
    }

    SoundWave* snd = new SoundWave();
    const char* hint = nullptr;
    auto ct = mHeaders.find("Content-Type");
    if (ct != mHeaders.end())
    {
        const std::string& v = ct->second;
        if      (v.find("audio/wav")    != std::string::npos) hint = "wav";
        else if (v.find("audio/x-wav")  != std::string::npos) hint = "wav";
        else if (v.find("audio/wave")   != std::string::npos) hint = "wav";
        else if (v.find("audio/ogg")    != std::string::npos) hint = "ogg";
        else if (v.find("audio/vorbis") != std::string::npos) hint = "ogg";
    }

    if (!SoundWave::LoadFromMemory(mBody.data(), mBody.size(), hint, *snd))
    {
        delete snd;
        return nullptr;
    }
    return snd;
}
