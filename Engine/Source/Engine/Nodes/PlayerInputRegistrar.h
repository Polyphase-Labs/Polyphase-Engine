#pragma once

#include "Nodes/Node.h"
#include "AssetRef.h"

class InputActionsAsset;

class PlayerInputRegistrar : public Node
{
public:

    DECLARE_NODE(PlayerInputRegistrar, Node);

    PlayerInputRegistrar();

    virtual void Start() override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual const char* GetTypeName() const override;

    void SetInputActions(InputActionsAsset* asset);
    InputActionsAsset* GetInputActions() const;

protected:

    AssetRef mInputActions;
};
