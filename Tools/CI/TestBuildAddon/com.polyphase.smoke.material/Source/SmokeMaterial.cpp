// Compile-only fixture. Each symbol referenced here was missing from the
// installed editor's import library before POLYPHASE_API was added to its
// declaration. If any of them lose their export annotation in the future,
// this TU fails to link and the release workflow stops before publish.

#include "SmokeMaterial.h"

#include "AssetManager.h"
#include "Engine.h"
#include "Renderer.h"
#include "Script.h"
#include "Assets/Scene.h"
#include "Nodes/Node.h"
#include "Nodes/3D/Capsule3d.h"

#if EDITOR
#include "Editor/EditorImgui.h"
#include "Editor/EditorState.h"
#include "Editor/EditorUtils.h"
#endif

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

    // Scene-authoring surface used by addons that build a node hierarchy in
    // memory and capture it into a Scene asset
    // (com.polyphase.engine.thirdperson.core). The volatile flag is never
    // set — the fixture only needs the linker to resolve the symbols, and
    // these calls have real side effects if they ever ran.
    static volatile bool sExerciseSceneAuthoring = false;
    if (sExerciseSceneAuthoring)
    {
        SharedPtr<Capsule3D> capsule = Node::Construct<Capsule3D>();
        capsule->SetRadius(0.4f);
        capsule->SetHeight(1.8f);

        if (Script* script = capsule->GetScript())
        {
            script->SetScriptProperties(std::vector<Property>());
        }

#if EDITOR
        EditorState* editorState = GetEditorState();
        AssetDir* dir = (editorState != nullptr) ? editorState->GetAssetDirectory() : nullptr;
        (void)EditorAddUniqueAsset("SmokeScene", dir, Scene::GetStaticType(), false);

        Property assetProp;
        DrawAssetProperty(assetProp, 0, nullptr, PropertyOwnerType::Count);
#endif
    }
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
