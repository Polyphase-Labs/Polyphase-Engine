#pragma once

#include "Graphics/Vulkan/PostProcess/PostProcessPass.h"

struct FxaaUniforms
{
    float mEdgeThresholdMin = 0.0312f;
    float mEdgeThresholdMax = 0.125f;
    float mSubpixelQuality = 0.75f;
    int32_t mPad0 = 1337;
};

class FxaaPass : public PostProcessPass
{
public:

    virtual void Create() override;
    virtual void Destroy() override;

    virtual void Render(Image* input, Image* output) override;

    virtual void GatherProperties(std::vector<Property>& props) override;

protected:

    FxaaUniforms mUniforms;
};
