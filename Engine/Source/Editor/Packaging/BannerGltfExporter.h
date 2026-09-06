#pragma once

#if EDITOR

#include <cstdint>
#include <string>

/**
 * Exports the static meshes of a Scene asset as a glTF 2.0 file laid out for
 * a Nintendo 3DS HOME Menu banner, ready for pycgfx to convert into CGFX.
 *
 * The scene is instantiated without a World, every StaticMesh3D is written
 * as a rigid glTF node with its world transform, textures become PNGs beside
 * the .gltf, and the whole thing is wrapped in a root that scales/centres it
 * into the HOME Menu camera's frustum (see pycgfx's banner-camera.gltf). An
 * optional Y-axis spin animation is emitted on that root.
 */
namespace BannerGltfExporter
{
    struct Options
    {
        std::string mSceneName;     // Scene asset name
        std::string mOutputDir;     // directory for banner.gltf/.bin/tex_*.png (trailing slash)
        bool mRotate = true;                // false = static model
        float mSpinDegreesPerSec = 30.0f;   // angular speed (0 = static); negative reverses a full spin
        float mRotMinDeg = 0.0f;            // rotation range about the vertical axis. A span of
        float mRotMaxDeg = 360.0f;          // 360 or more = continuous spin, less = sway between the two
        uint32_t mMaxTextureSize = 256;     // textures are downscaled to POT <= this
    };

    /** Returns the path of the written .gltf, or "" with outError filled. */
    std::string Export(const Options& options, std::string& outError);
}

#endif /* EDITOR */
