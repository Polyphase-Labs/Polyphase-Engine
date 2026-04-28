#pragma once

#include "Asset.h"
#include "Input/PlayerInputSystem.h"

class InputActionsAsset : public Asset
{
public:

    DECLARE_ASSET(InputActionsAsset, Asset);

    InputActionsAsset();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;

    std::vector<InputAction> mActions;
};
