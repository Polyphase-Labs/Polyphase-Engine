#pragma once

#include "Nodes/3D/Node3d.h"
#include "Animation/SpriteAnimPlayback.h"
#include "AssetRef.h"

#include "glm/glm.hpp"

#include <string>
#include <vector>

class Texture;
class Material;
class SpriteAnimation;

// Drives a connected Material's texture parameters from a SpriteAnimator-style
// playback. Per-channel toggles let the user route the current frame to any of
// the three standard texture slots (Diffuse / Alpha / Emission). The atlas UV
// rect is also pushed as a vec4 parameter so atlas-mode animations work for
// shaders that sample the atlas via UV transform.
//
// Common pattern: place this next to a StaticMesh3D that uses the same Material.
// The animator updates the material each frame, and any meshes sharing that
// material will animate.
class POLYPHASE_API AnimatedSprite3D : public Node3D
{
public:

    DECLARE_NODE(AnimatedSprite3D, Node3D);

    AnimatedSprite3D();
    virtual ~AnimatedSprite3D();

    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Start() override;
    virtual void Tick(float deltaTime) override;
    virtual void EditorTick(float deltaTime) override;

    // Playback API mirrors SpriteAnimator.
    void Play();
    void Pause();
    void Stop();
    void PlayAnimation(const std::string& name);
    void SetFrame(int32_t frameIndex);
    bool AnimateTo(int32_t targetFrame, bool pauseOnFinished, const ScriptFunc& onFinished);
    bool AnimateToProgress(float progress, bool pauseOnFinished, const ScriptFunc& onFinished);
    void CancelAnimateTo() { mPlayback.CancelAnimateTo(); }
    void SetSpeed(float speed);
    float GetSpeed() const { return mPlayback.mPlaybackSpeed; }
    bool IsPlaying() const { return mPlayback.mPlaying; }

    void SetAutoPlay(bool v) { mPlayback.mAutoPlay = v; }
    bool GetAutoPlay() const { return mPlayback.mAutoPlay; }
    void SetLoopOverride(bool v) { mPlayback.mLoopOverride = v; }
    bool GetLoopOverride() const { return mPlayback.mLoopOverride; }
    void SetDefaultAnimation(const std::string& name) { mPlayback.mDefaultAnimation = name; }
    const std::string& GetDefaultAnimation() const { return mPlayback.mDefaultAnimation; }

    void AddAnimation(SpriteAnimation* asset)              { mPlayback.AddAnimationAsset(asset); }
    void AddAnimation(const std::string& path)             { mPlayback.AddAnimationByPath(path); }
    void CreateAnimation(const std::string& name)          { mPlayback.CreateAnimation(name); }
    void CreateAnimation(const std::string& name, const std::vector<Texture*>& frames)
                                                            { mPlayback.CreateAnimation(name, frames); }
    void AddImage(const std::string& name, Texture* tex)   { mPlayback.AddImage(name, tex); }
    void AddImage(const std::string& name, const std::string& path)
                                                            { mPlayback.AddImage(name, path); }
    void AddImages(const std::string& name, const std::vector<std::string>& paths)
                                                            { mPlayback.AddImages(name, paths); }
    void RemoveAnimation(const std::string& name)          { mPlayback.RemoveAnimation(name); }
    bool HasAnimation(const std::string& name) const       { return mPlayback.HasAnimation(name); }

    float GetProgress() const                              { return mPlayback.GetProgress(); }
    Texture* GetCurrentTexture() const                     { return mPlayback.GetCurrentTexture(); }
    int32_t GetCurrentFrameIndex() const                   { return mPlayback.mCurrentFrame; }
    const std::string& GetCurrentAnimationName() const     { return mPlayback.mCurrentName; }

    // Material targeting
    void SetMaterial(Material* mat);
    Material* GetMaterial() const;
    void SetAffectDiffuse(bool v)  { mAffectDiffuse  = v; }
    void SetAffectAlpha(bool v)    { mAffectAlpha    = v; }
    void SetAffectEmission(bool v) { mAffectEmission = v; }
    bool GetAffectDiffuse() const  { return mAffectDiffuse; }
    bool GetAffectAlpha() const    { return mAffectAlpha; }
    bool GetAffectEmission() const { return mAffectEmission; }

    bool GetEditorPreview() const { return mEditorPreview; }
    void SetEditorPreview(bool v) { mEditorPreview = v; }

protected:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    void ApplyCurrentFrameToMaterial();

    SpriteAnimPlayback mPlayback;

    // Target material + which texture slots the animation drives.
    AssetRef    mMaterial;
    bool        mAffectDiffuse  = true;
    bool        mAffectAlpha    = false;
    bool        mAffectEmission = false;

    // For MaterialLite — indexed texture slots (0..3). MaterialLite doesn't
    // expose named texture parameters; it has fixed slot indices that the
    // shader samples in order. Defaults match a typical layout where slot 0
    // is the base color, slot 1 the alpha mask, slot 2 the emissive.
    int32_t     mDiffuseSlot  = 0;
    int32_t     mAlphaSlot    = 1;
    int32_t     mEmissionSlot = 2;

    // For atlas-mode SpriteAnimations on MaterialLite: which UV map (0 or 1)
    // to push the per-frame UV scale/offset into. MaterialLite supports up
    // to two independent UV maps so static and animated textures can coexist
    // on the same material — assign the animated texture slot(s) to this UV
    // map in MaterialLite's "UV Map N" property and any static slots to the
    // other UV map.
    int32_t     mAtlasUvMap   = 0;

    // For full Material with custom-shader named params. Used only when the
    // target Material is NOT a MaterialLite. Adjust to match whatever names
    // your custom shader exposes.
    std::string mDiffuseParamName  = "DiffuseMap";
    std::string mAlphaParamName    = "AlphaMap";
    std::string mEmissionParamName = "EmissionMap";

    // If true, also push the current UV rect (u0, v0, u1, v1) as a Vector
    // parameter so atlas-mode shaders can sample the right cell. Only fires
    // for full Materials — MaterialLite has no custom parameters.
    bool        mPushUVRect       = true;
    std::string mUVRectParamName  = "AnimUVRect";

    // Editor-only preview controls.
    bool mEditorPreview     = false;
    bool mEditorPlayButton  = false;
    bool mEditorStopButton  = false;
};
