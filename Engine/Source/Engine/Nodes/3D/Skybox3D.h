#pragma once

#include "Nodes/3D/StaticMesh3d.h"

class POLYPHASE_API Skybox3D : public StaticMesh3D
{
public:

    DECLARE_NODE(Skybox3D, StaticMesh3D);

    Skybox3D();
    ~Skybox3D();

    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

    virtual void Create() override;

    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void LoadStream(Stream& stream, Platform platform, uint32_t version) override;

    bool IsEnvMapEnabled() const { return mUseEnvMap; }
    void SetEnvMapEnabled(bool enable) { mUseEnvMap = enable; }

protected:

    bool mUseEnvMap = false;
};
