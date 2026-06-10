#if API_VULKAN

#include "Graphics/Graphics.h"
#include "Graphics/Vulkan/VulkanContext.h"
#include "Graphics/Vulkan/VulkanUtils.h"

#include "Maths.h"
#include "Engine.h"

extern VulkanContext* gVulkanContext;

void GFX_Initialize()
{
    // On Android, it's possible that GFX_Initialize() was already called.
    if (gVulkanContext == nullptr)
    {
        CreateVulkanContext();
        gVulkanContext->Initialize();
    }
}

void GFX_Shutdown()
{
    gVulkanContext->Destroy();
    DestroyVulkanContext();
}

void GFX_BeginFrame()
{
    gVulkanContext->BeginFrame();
}

void GFX_EndFrame()
{
    gVulkanContext->EndFrame();
}

void GFX_BeginScreen(uint32_t screenIndex)
{

}

void GFX_BeginView(uint32_t viewIndex)
{

}

bool GFX_ShouldCullLights()
{
    return true;
}

void GFX_BeginRenderPass(RenderPassId renderPassId)
{
    gVulkanContext->BeginRenderPass(renderPassId);
}

void GFX_EndRenderPass()
{
    gVulkanContext->EndRenderPass();
}

void GFX_SetPipelineState(PipelineConfig pipelineConfig)
{
    BindPipelineConfig(pipelineConfig);
}

void GFX_SetViewport(int32_t x, int32_t y, int32_t width, int32_t height, bool handlePrerotation)
{
    gVulkanContext->SetViewport(x, y, width, height, handlePrerotation, false);
}

void GFX_SetScissor(int32_t x, int32_t y, int32_t width, int32_t height, bool handlePrerotation)
{
    gVulkanContext->SetScissor(x, y, width, height, handlePrerotation, false);
}

glm::mat4 GFX_MakePerspectiveMatrix(float fovyDegrees, float aspectRatio, float zNear, float zFar)
{
    VkSurfaceTransformFlagBitsKHR preTransformFlag = gVulkanContext->GetPreTransformFlag();

    glm::mat4 preRotateMat = glm::mat4(1.0f);
    glm::vec3 rotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);

    if (preTransformFlag & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) {
        preRotateMat = glm::rotate(preRotateMat, glm::radians(90.0f), rotationAxis);
    }

    else if (preTransformFlag & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
        preRotateMat = glm::rotate(preRotateMat, glm::radians(270.0f), rotationAxis);
    }

    else if (preTransformFlag & VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR) {
        preRotateMat = glm::rotate(preRotateMat, glm::radians(180.0f), rotationAxis);
    }

    glm::mat4 perspMat = glm::perspectiveFov(glm::radians(fovyDegrees), aspectRatio, 1.0f, zNear, zFar);
    perspMat[1][1] *= -1.0f;
    perspMat = preRotateMat * perspMat;

    return perspMat;
}

glm::mat4 GFX_MakeOrthographicMatrix(float left, float right, float bottom, float top, float zNear, float zFar)
{
    VkSurfaceTransformFlagBitsKHR preTransformFlag = gVulkanContext->GetPreTransformFlag();

    glm::mat4 preRotateMat = glm::mat4(1.0f);
    glm::vec3 rotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);

    if (preTransformFlag & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) {
        preRotateMat = glm::rotate(preRotateMat, glm::radians(90.0f), rotationAxis);
    }

    else if (preTransformFlag & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
        preRotateMat = glm::rotate(preRotateMat, glm::radians(270.0f), rotationAxis);
    }

    else if (preTransformFlag & VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR) {
        preRotateMat = glm::rotate(preRotateMat, glm::radians(180.0f), rotationAxis);
    }

    glm::mat4 orthoMat = glm::ortho(left, right, bottom, top, zNear, zFar);
    orthoMat[1][1] *= -1.0f;
    orthoMat = preRotateMat * orthoMat;

    return orthoMat;
}

void GFX_SetFog(const FogSettings& fogSettings)
{

}

void GFX_Draw(void* data)
{
    //gVulkanContext->Draw();
}

void GFX_DrawLines(const std::vector<Line>& lines)
{
    gVulkanContext->DrawLines(lines);
}

void GFX_DrawSplats(const GaussianSplatInstance* instances, uint32_t count,
                    const glm::vec3& cameraRight, const glm::vec3& cameraUp)
{
    gVulkanContext->DrawSplats(instances, count, cameraRight, cameraUp);
}

#include "Graphics/Vulkan/GraphicsVulkanAddon.h"
#include "Graphics/GraphicsConstants.h"
#include "Renderer.h"

bool GFX_GetVulkanAddonHandles(GfxVulkanAddonHandles& out)
{
    if (gVulkanContext == nullptr) return false;

    out.mDevice               = gVulkanContext->GetDevice();
    out.mPhysicalDevice       = gVulkanContext->GetPhysicalDevice();
    out.mCurrentCommandBuffer = gVulkanContext->GetCommandBuffer();
    out.mCurrentRenderPass    = gVulkanContext->GetCurrentRenderPass();
    out.mGraphicsQueue        = gVulkanContext->GetGraphicsQueue();

    // Scene viewport rectangle the engine has set with GFX_SetViewport right
    // before the custom render pass fires. The actual rendering area is a
    // sub-region of the swapchain — for editor viewport renders it's the
    // editor panel rectangle, for game preview it's (0,0,640,480) into the
    // game-preview render target, for PIE Full Screen it's the full window.
    // Resolution scale is baked in.
    //
    // GetSceneWidth/Height returns swapchain-creation-time dimensions, which
    // are wrong here. We hand the full (x, y, w, h) to the addon so it can
    // re-apply the correct viewport with vkCmdSetViewport when binding its
    // own pipeline.
    if (Renderer* r = Renderer::Get())
    {
        glm::uvec4 svp = r->GetSceneViewport(-1);
        out.mSceneViewportX      = svp.x;
        out.mSceneViewportY      = svp.y;
        out.mSceneViewportWidth  = svp.z;
        out.mSceneViewportHeight = svp.w;
        out.mViewportWidth       = svp.z;
        out.mViewportHeight      = svp.w;
    }
    else
    {
        out.mSceneViewportWidth  = gVulkanContext->GetSceneWidth();
        out.mSceneViewportHeight = gVulkanContext->GetSceneHeight();
        out.mViewportWidth       = out.mSceneViewportWidth;
        out.mViewportHeight      = out.mSceneViewportHeight;
    }

    out.mCurrentFrameIndex    = uint32_t(gVulkanContext->GetFrameIndex());
    out.mMaxFramesInFlight    = MAX_FRAMES;
    return true;
}

void GFX_DrawFullscreen()
{
    gVulkanContext->DrawFullscreen();
}

void GFX_ResizeWindow()
{
    // Just wait until the out-of-date error when presenting or acquiring swapchain image?
#if 0
    if (gVulkanContext != nullptr)
    {
        gVulkanContext->RecreateSwapchain(false);
    }
#endif
}

void GFX_Reset()
{
    if (gVulkanContext != nullptr)
    {
        gVulkanContext->RecreateSwapchain(true);
    }
}

Node3D* GFX_ProcessHitCheck(World* world, int32_t x, int32_t y, uint32_t* outInstance)
{
#if EDITOR
    return gVulkanContext->ProcessHitCheck(world, x, y, outInstance);
#else
    return nullptr;
#endif
}

uint32_t GFX_GetNumViews()
{
    return 1;
}

void GFX_SetFrameRate(int32_t frameRate)
{

}

void GFX_PathTrace()
{
    if (gVulkanContext->IsRayTracingSupported())
    {
        gVulkanContext->GetRayTracer()->PathTraceWorld();
    }
}

void GFX_BeginLightBake()
{
    if (gVulkanContext->IsRayTracingSupported())
    {
        gVulkanContext->GetRayTracer()->BeginLightBake();
    }
    else
    {
        LogError("GPU cannot bake light because maxPerStageDescriptorSampledImages < %d", PATH_TRACE_MAX_TEXTURES);
    }
}

void GFX_UpdateLightBake()
{
    if (gVulkanContext->IsRayTracingSupported())
    {
        gVulkanContext->GetRayTracer()->UpdateLightBake();
    }
}

void GFX_EndLightBake()
{
    if (gVulkanContext->IsRayTracingSupported())
    {
        gVulkanContext->GetRayTracer()->EndLightBake();
    }
}

bool GFX_IsLightBakeInProgress()
{
    bool ret = false;

    if (gVulkanContext->IsRayTracingSupported())
    {
        ret = gVulkanContext->GetRayTracer()->IsLightBakeInProgress();
    }

    return ret;
}

float GFX_GetLightBakeProgress()
{
    float ret = 0.0f;

    if (gVulkanContext->IsRayTracingSupported())
    {
        ret = gVulkanContext->GetRayTracer()->GetLightBakeProgress();
    }
    
    return ret;
}

void GFX_EnableMaterials(bool enable)
{
    // This function is used to signal whether draws should bind their material pipelines.
    // Otherwise, just use the current pipeline.
    // This is kinda hacky.
    gVulkanContext->EnableMaterials(enable);
}

void GFX_BeginGpuTimestamp(const char* name)
{
    gVulkanContext->BeginGpuTimestamp(name);
}

void GFX_EndGpuTimestamp(const char* name)
{
    gVulkanContext->EndGpuTimestamp(name);
}

void GFX_CreateTextureResource(Texture* texture, std::vector<uint8_t>& data)
{
    if (IsHeadless()) return;
    CreateTextureResource(texture, data.data());
}

void GFX_DestroyTextureResource(Texture* texture)
{
    if (IsHeadless()) return;
    DestroyTextureResource(texture);
}

void GFX_UpdateTextureResourcePixels(Texture* /*texture*/, const uint8_t* /*src*/,
                                     uint32_t /*srcWidth*/, uint32_t /*srcHeight*/)
{
    // Vulkan path goes through mResource.mImage->Update directly in
    // Texture::UpdatePixels (#if API_VULKAN branch); this entry is never
    // called on Vulkan builds and exists only to keep the link clean.
}

void GFX_CreateMaterialResource(Material* material)
{
    if (IsHeadless()) return;
    CreateMaterialResource(material);
}

void GFX_DestroyMaterialResource(Material* material)
{
    if (IsHeadless()) return;
    DestroyMaterialResource(material);
}

void GFX_CreateStaticMeshResource(StaticMesh* staticMesh, bool hasColor, uint32_t numVertices, void* vertices, uint32_t numIndices, IndexType* indices)
{
    if (IsHeadless()) return;
    CreateStaticMeshResource(staticMesh, hasColor, numVertices, vertices, numIndices, indices);
}

void GFX_DestroyStaticMeshResource(StaticMesh* staticMesh)
{
    if (IsHeadless()) return;
    DestroyStaticMeshResource(staticMesh);
}

void GFX_CreateSkeletalMeshResource(SkeletalMesh* skeletalMesh, uint32_t numVertices, VertexSkinned* vertices, uint32_t numIndices, uint32_t* indices)
{
    if (IsHeadless()) return;
    CreateSkeletalMeshResource(skeletalMesh, numVertices, vertices, numIndices, indices);
}

void GFX_DestroySkeletalMeshResource(SkeletalMesh* skeletalMesh)
{
    if (IsHeadless()) return;
    DestroySkeletalMeshResource(skeletalMesh);
}

void GFX_CreateStaticMeshCompResource(StaticMesh3D* staticMeshComp)
{
    if (IsHeadless()) return;
}

void GFX_DestroyStaticMeshCompResource(StaticMesh3D* staticMeshComp)
{
    if (IsHeadless()) return;
    DestroyStaticMeshCompResource(staticMeshComp);
}

void GFX_UpdateStaticMeshCompResourceColors(StaticMesh3D* staticMeshComp)
{
    if (IsHeadless()) return;
    UpdateStaticMeshCompResourceColors(staticMeshComp);
}

void GFX_DrawStaticMeshComp(StaticMesh3D* staticMeshComp, StaticMesh* meshOverride)
{
    DrawStaticMeshComp(staticMeshComp, meshOverride);
}

void GFX_CreateSkeletalMeshCompResource(SkeletalMesh3D* skeletalMeshComp)
{
    if (IsHeadless()) return;
}

void GFX_DestroySkeletalMeshCompResource(SkeletalMesh3D* skeletalMeshComp)
{
    if (IsHeadless()) return;
    DestroySkeletalMeshCompResource(skeletalMeshComp);
}

void GFX_ReallocateSkeletalMeshCompVertexBuffer(SkeletalMesh3D* skeletalMeshComp, uint32_t numVertices)
{
    if (IsHeadless()) return;
    ReallocateSkeletalMeshCompVertexBuffer(skeletalMeshComp, numVertices);
}

void GFX_UpdateSkeletalMeshCompVertexBuffer(SkeletalMesh3D* skeletalMeshComp, const std::vector<Vertex>& skinnedVertices)
{
    if (IsHeadless()) return;
    UpdateSkeletalMeshCompVertexBuffer(skeletalMeshComp, skinnedVertices);
}

void GFX_DrawSkeletalMeshComp(SkeletalMesh3D* skeletalMeshComp)
{
    DrawSkeletalMeshComp(skeletalMeshComp);
}

bool GFX_IsCpuSkinningRequired(SkeletalMesh3D* skeletalMeshComp)
{
    return IsCpuSkinningRequired(skeletalMeshComp);
}

void GFX_DrawShadowMeshComp(ShadowMesh3D* shadowMeshComp)
{
    DrawShadowMeshComp(shadowMeshComp);
}

void GFX_DrawInstancedMeshComp(InstancedMesh3D* instancedMeshComp)
{
    DrawInstancedMeshComp(instancedMeshComp);
}

void GFX_CreateTextMeshCompResource(TextMesh3D* textMeshComp)
{
    if (IsHeadless()) return;
}

void GFX_DestroyTextMeshCompResource(TextMesh3D* textMeshComp)
{
    if (IsHeadless()) return;
    DestroyTextMeshCompResource(textMeshComp);
}

void GFX_UpdateTextMeshCompVertexBuffer(TextMesh3D* textMeshComp, const std::vector<Vertex>& vertices)
{
    if (IsHeadless()) return;
    UpdateTextMeshCompVertexBuffer(textMeshComp, vertices);
}

void GFX_DrawTextMeshComp(TextMesh3D* textMeshComp)
{
    DrawTextMeshComp(textMeshComp);
}

void GFX_CreateVoxel3DResource(Voxel3D* voxel)
{
    if (IsHeadless()) return;
    CreateVoxel3DResource(voxel);
}

void GFX_DestroyVoxel3DResource(Voxel3D* voxel)
{
    if (IsHeadless()) return;
    DestroyVoxel3DResource(voxel);
}

void GFX_UpdateVoxel3DResource(Voxel3D* voxel, const std::vector<VertexColor>& vertices, const std::vector<IndexType>& indices)
{
    if (IsHeadless()) return;
    UpdateVoxel3DResource(voxel, vertices, indices);
}

void GFX_DrawVoxel3D(Voxel3D* voxel)
{
    DrawVoxel3D(voxel);
}

void GFX_CreateTerrain3DResource(Terrain3D* terrain)
{
    if (IsHeadless()) return;
    CreateTerrain3DResource(terrain);
}

void GFX_DestroyTerrain3DResource(Terrain3D* terrain)
{
    if (IsHeadless()) return;
    DestroyTerrain3DResource(terrain);
}

void GFX_UpdateTerrain3DResource(Terrain3D* terrain, const std::vector<VertexColor>& vertices, const std::vector<IndexType>& indices)
{
    if (IsHeadless()) return;
    UpdateTerrain3DResource(terrain, vertices, indices);
}

void GFX_DrawTerrain3D(Terrain3D* terrain)
{
    DrawTerrain3D(terrain);
}

void GFX_CreateTileMap2DResource(TileMap2D* tileMap)
{
    if (IsHeadless()) return;
    CreateTileMap2DResource(tileMap);
}

void GFX_DestroyTileMap2DResource(TileMap2D* tileMap)
{
    if (IsHeadless()) return;
    DestroyTileMap2DResource(tileMap);
}

void GFX_UpdateTileMap2DResource(TileMap2D* tileMap, const std::vector<VertexColor>& vertices, const std::vector<IndexType>& indices)
{
    if (IsHeadless()) return;
    UpdateTileMap2DResource(tileMap, vertices, indices);
}

void GFX_DrawTileMap2D(TileMap2D* tileMap)
{
    DrawTileMap2D(tileMap);
}

void GFX_CreateParticleCompResource(Particle3D* particleComp)
{
    if (IsHeadless()) return;
}

void GFX_DestroyParticleCompResource(Particle3D* particleComp)
{
    if (IsHeadless()) return;
    DestroyParticleCompResource(particleComp);
}

void GFX_UpdateParticleCompVertexBuffer(Particle3D* particleComp, const std::vector<VertexParticle>& vertices)
{
    if (IsHeadless()) return;
    UpdateParticleCompVertexBuffer(particleComp, vertices);
}

void GFX_DrawParticleComp(Particle3D* particleComp)
{
    DrawParticleComp(particleComp);
}

void GFX_CreateQuadResource(Quad* quad)
{
    if (IsHeadless()) return;
    CreateQuadResource(quad);
}

void GFX_DestroyQuadResource(Quad* quad)
{
    if (IsHeadless()) return;
    DestroyQuadResource(quad);
}

void GFX_UpdateQuadResourceVertexData(Quad* quad)
{
    if (IsHeadless()) return;
    UpdateQuadResourceVertexData(quad);
}

void GFX_DrawQuad(Quad* quad)
{
    DrawQuad(quad);
}

void GFX_CreateQuadBorderResource(Quad* quad)
{
    if (IsHeadless()) return;
    CreateQuadBorderResource(quad);
}

void GFX_DestroyQuadBorderResource(Quad* quad)
{
    if (IsHeadless()) return;
    DestroyQuadBorderResource(quad);
}

void GFX_UpdateQuadBorderResourceVertexData(Quad* quad)
{
    if (IsHeadless()) return;
    UpdateQuadBorderResourceVertexData(quad);
}

void GFX_DrawQuadBorder(Quad* quad)
{
    DrawQuadBorder(quad);
}

void GFX_CreateTextResource(Text* text)
{
    if (IsHeadless()) return;
    CreateTextResource(text);
}

void GFX_DestroyTextResource(Text* text)
{
    if (IsHeadless()) return;
    DestroyTextResource(text);
}

void GFX_UpdateTextResourceVertexData(Text* text)
{
    if (IsHeadless()) return;
    UpdateTextResourceVertexData(text);
}

void GFX_DrawText(Text* text)
{
    DrawTextWidget(text);
}

void GFX_CreatePolyResource(Poly* poly)
{
    if (IsHeadless()) return;
    CreatePolyResource(poly);
}

void GFX_DestroyPolyResource(Poly* poly)
{
    if (IsHeadless()) return;
    DestroyPolyResource(poly);
}

void GFX_UpdatePolyResourceVertexData(Poly* poly)
{
    if (IsHeadless()) return;
    UpdatePolyResourceVertexData(poly);
}

void GFX_DrawPoly(Poly* poly)
{
    DrawPoly(poly);
}

void GFX_DrawStaticMesh(StaticMesh* mesh, Material* material, const glm::mat4& transform, glm::vec4 color)
{
    DrawStaticMesh(mesh, material, transform, color);
}

void GFX_RenderPostProcessPasses()
{
    gVulkanContext->RenderPostProcessChain();
}

#endif