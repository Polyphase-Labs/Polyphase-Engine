// Compile-only fixture. Each symbol referenced here was missing from the
// installed editor's import library before POLYPHASE_API was added to its
// declaration. If any of them lose their export annotation in the future,
// this TU fails to link and the release workflow stops before publish.

#include "SmokeMaterial.h"

#include "AssetManager.h"
#include "Engine.h"
#include "Renderer.h"

DEFINE_ASSET(SmokeMaterial);

SmokeMaterial::SmokeMaterial() : MaterialBase() {}
SmokeMaterial::~SmokeMaterial() {}

void SmokeMaterial::LoadStream(Stream& stream, Platform platform)
{
    MaterialBase::LoadStream(stream, platform);
}

void SmokeMaterial::SaveStream(Stream& stream, Platform platform)
{
    MaterialBase::SaveStream(stream, platform);
}

void SmokeMaterial::Create()
{
    MaterialBase::Create();

    // Reference every singleton + free function that LNK2001'd previously.
    if (IsHeadless())
    {
        return;
    }

    if (AssetManager* am = AssetManager::Get())
    {
        am->RegisterTransientAsset(this);
    }

    (void)Renderer::Get();

    // Exercise the non-virtual MaterialBase setters/getters/MarkStale that
    // also lacked export annotations. These calls have no side effects in
    // a never-loaded fixture, but the linker still needs the symbols.
    MarkStale();
    SetBlendMode(GetBlendMode());
    SetMaskCutoff(GetMaskCutoff());
    (void)GetSortPriority();
    SetDepthTestDisabled(IsDepthTestDisabled());
    SetApplyFog(ShouldApplyFog());
    SetCullMode(GetCullMode());
}

void SmokeMaterial::Destroy()
{
    MaterialBase::Destroy();
}

bool SmokeMaterial::Import(const std::string& path, ImportOptions* options)
{
    return MaterialBase::Import(path, options);
}

void SmokeMaterial::GatherProperties(std::vector<Property>& outProps)
{
    MaterialBase::GatherProperties(outProps);
}

bool SmokeMaterial::IsBase() const
{
    return MaterialBase::IsBase();
}
