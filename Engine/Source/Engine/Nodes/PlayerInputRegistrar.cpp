#include "Nodes/PlayerInputRegistrar.h"
#include "Input/InputActionsAsset.h"
#include "Input/PlayerInputSystem.h"
#include "Log.h"

FORCE_LINK_DEF(PlayerInputRegistrar);
DEFINE_NODE(PlayerInputRegistrar, Node);

PlayerInputRegistrar::PlayerInputRegistrar()
{
    mName = "PlayerInputRegistrar";
}

void PlayerInputRegistrar::Start()
{
    Node::Start();

    InputActionsAsset* asset = mInputActions.Get<InputActionsAsset>();
    if (asset == nullptr) return;

    PlayerInputSystem* sys = PlayerInputSystem::Get();
    if (sys == nullptr) return;

    // Wipe-and-replay — mirrors PlayerInput.LoadActions
    // (PlayerInput_Lua.cpp:265-281). `existing` is a const ref to the
    // system's internal vector; UnregisterAction shrinks it in place so the
    // empty() check eventually terminates.
    const std::vector<InputAction>& existing = sys->GetActions();
    while (!existing.empty())
    {
        sys->UnregisterAction(existing[0].category, existing[0].name);
    }
    for (const InputAction& action : asset->mActions)
    {
        sys->RegisterAction(action.category, action.name, action.trigger.mode);
        sys->SetTrigger(action.category, action.name, action.trigger);
        for (const InputActionBinding& binding : action.bindings)
        {
            sys->AddBinding(action.category, action.name, binding);
        }
    }

    LogDebug("PlayerInputRegistrar: loaded %d actions from '%s'",
             (int)asset->mActions.size(), asset->GetName().c_str());
}

void PlayerInputRegistrar::GatherProperties(std::vector<Property>& outProps)
{
    Node::GatherProperties(outProps);

    SCOPED_CATEGORY("Player Input");

    outProps.push_back(Property(DatumType::Asset, "Input Actions", this,
        &mInputActions, 1, nullptr,
        int32_t(InputActionsAsset::GetStaticType())));
}

const char* PlayerInputRegistrar::GetTypeName() const
{
    return "PlayerInputRegistrar";
}

void PlayerInputRegistrar::SetInputActions(InputActionsAsset* asset)
{
    mInputActions = asset;
}

InputActionsAsset* PlayerInputRegistrar::GetInputActions() const
{
    return mInputActions.Get<InputActionsAsset>();
}
