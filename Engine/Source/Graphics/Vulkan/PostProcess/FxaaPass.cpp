#include "FxaaPass.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "Graphics/Vulkan/VulkanContext.h"

void FxaaPass::Create()
{
    mName = "FXAA";
}

void FxaaPass::Destroy()
{

}

void FxaaPass::Render(Image* input, Image* output)
{
    PostProcessPass::Render(input, output);

    VulkanContext* context = GetVulkanContext();
    VkCommandBuffer cb = GetCommandBuffer();

    // Set render target
    RenderPassSetup rpSetup;
    rpSetup.mColorImages[0] = output;
    rpSetup.mLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    rpSetup.mStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    context->BeginVkRenderPass(rpSetup, true);

    // Bind vertex/index buffers for full screen quad.
    context->BindFullscreenVertexBuffer(cb);

    // Set vertex + fragment shader
    context->SetVertexShader("ScreenRect.vert");
    context->SetFragmentShader("Fxaa.frag");

    // Commit graphics pipeline
    context->CommitPipeline();

    // Bind descriptor set
    UniformBlock block = WriteUniformBlock(&mUniforms, sizeof(FxaaUniforms));

    DescriptorSet::Begin("FXAA DS")
        .WriteUniformBuffer(0, block)
        .WriteImage(1, input)
        .Build()
        .Bind(cb, 1);

    // Draw
    vkCmdDraw(cb, 4, 1, 0, 0);

    // End render pass
    context->EndVkRenderPass();
}

void FxaaPass::GatherProperties(std::vector<Property>& props)
{
    PostProcessPass::GatherProperties(props);
    props.push_back(Property(DatumType::Float, "FXAA Edge Threshold Min", nullptr, &mUniforms.mEdgeThresholdMin));
    props.push_back(Property(DatumType::Float, "FXAA Edge Threshold Max", nullptr, &mUniforms.mEdgeThresholdMax));
    props.push_back(Property(DatumType::Float, "FXAA Subpixel Quality", nullptr, &mUniforms.mSubpixelQuality));
}
