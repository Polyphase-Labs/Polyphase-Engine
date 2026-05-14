#pragma once

#include "Assets/MaterialBase.h"

class SmokeMaterial : public MaterialBase
{
public:

    DECLARE_ASSET(SmokeMaterial, MaterialBase);

    SmokeMaterial();
    virtual ~SmokeMaterial();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;
    virtual bool Import(const std::string& path, ImportOptions* options) override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual bool IsBase() const override;
};
