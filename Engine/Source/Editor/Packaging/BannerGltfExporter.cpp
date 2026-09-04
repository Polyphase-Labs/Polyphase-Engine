#if EDITOR

#include "BannerGltfExporter.h"

#include "EngineTypes.h"
#include "Utilities.h"
#include "AssetManager.h"
#include "Assets/Scene.h"
#include "Assets/StaticMesh.h"
#include "Assets/Material.h"
#include "Assets/MaterialLite.h"
#include "Assets/Texture.h"
#include "Nodes/Node.h"
#include "Nodes/3D/Node3d.h"
#include "Nodes/3D/StaticMesh3d.h"
#include "Nodes/3D/SkeletalMesh3d.h"
#include "Nodes/3D/DirectionalLight3d.h"
#include "Graphics/GraphicsTypes.h"
#include "Vertex.h"
#include "Log.h"

#include <stb_image_resize2.h>
#include <stb_image_write.h>

#include "writer.h"
#include "stringbuffer.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

namespace BannerGltfExporter
{
    namespace
    {
        // pycgfx's banner-camera.gltf: perspective yfov 30 deg, aspect 5:3, at
        // (0, 1, 44.786) looking down -Z, znear 26.5. The visible half-height at
        // the z = 0 plane is 44.786 * tan(15 deg) ~= 12, so a bounding sphere of
        // radius 10 centred on (0, 1, 0) fills the frame with a little margin and
        // never crosses the near plane while spinning.
        constexpr float kLookAtY = 1.0f;
        constexpr float kTargetRadius = 10.0f;

        constexpr int kComponentFloat  = 5126;
        constexpr int kComponentUShort = 5123;
        constexpr int kComponentUInt   = 5125;
        constexpr int kTargetArrayBuffer = 34962;
        constexpr int kTargetElementArrayBuffer = 34963;

        struct Accessor
        {
            int mBufferView = 0;
            int mComponentType = kComponentFloat;
            int mCount = 0;
            const char* mType = "VEC3";
            bool mNormalized = false;
            bool mHasMinMax = false;
            std::vector<float> mMin;
            std::vector<float> mMax;
        };

        struct BufferView
        {
            size_t mOffset = 0;
            size_t mLength = 0;
            int mTarget = 0;
        };

        struct MaterialEntry
        {
            Material* mSource = nullptr;
            int mTextureIndex = -1;
            glm::vec4 mColor = glm::vec4(1.0f);
            BlendMode mBlendMode = BlendMode::Opaque;
            float mMaskCutoff = 0.5f;
            bool mDoubleSided = false;
        };

        struct MeshEntry
        {
            StaticMesh* mMesh = nullptr;
            int mMaterial = 0;
            int mPositionAccessor = -1;
            int mNormalAccessor = -1;
            int mUvAccessor = -1;
            int mColorAccessor = -1;
            int mIndexAccessor = -1;
        };

        struct NodeEntry
        {
            std::string mName;
            int mMesh = -1;
            glm::vec3 mTranslation = glm::vec3(0.0f);
            glm::quat mRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 mScale = glm::vec3(1.0f);
            std::vector<int> mChildren;
        };

        // Scene lighting handed to Standalone/3DS/banner_cgfx.py through the
        // glTF root "extras" (glTF itself has no core light objects and pycgfx
        // ignores KHR_lights_punctual).
        struct Lighting
        {
            bool mHasLight = false;
            glm::vec3 mDirection = glm::vec3(0.0f, 0.0f, -1.0f);   // travel direction, world space
            glm::vec3 mColor = glm::vec3(1.0f);
            bool mHasAmbient = false;
            glm::vec3 mAmbient = glm::vec3(0.1f);
        };

        struct Builder
        {
            Lighting mLighting;
            std::vector<uint8_t> mBin;
            std::vector<BufferView> mViews;
            std::vector<Accessor> mAccessors;
            std::vector<MaterialEntry> mMaterials;
            std::vector<MeshEntry> mMeshes;
            std::vector<NodeEntry> mNodes;
            std::vector<std::string> mImages;       // file names relative to the .gltf
            std::map<Texture*, int> mTextureIndices;
            std::map<std::pair<StaticMesh*, Material*>, int> mMeshIndices;
            std::map<Material*, int> mMaterialIndices;

            int AddView(const void* data, size_t bytes, int target)
            {
                while (mBin.size() % 4 != 0) mBin.push_back(0);
                BufferView view;
                view.mOffset = mBin.size();
                view.mLength = bytes;
                view.mTarget = target;
                mBin.insert(mBin.end(), (const uint8_t*)data, (const uint8_t*)data + bytes);
                mViews.push_back(view);
                return (int)mViews.size() - 1;
            }

            int AddAccessor(int view, int componentType, int count, const char* type, bool normalized = false)
            {
                Accessor acc;
                acc.mBufferView = view;
                acc.mComponentType = componentType;
                acc.mCount = count;
                acc.mType = type;
                acc.mNormalized = normalized;
                mAccessors.push_back(acc);
                return (int)mAccessors.size() - 1;
            }
        };

        uint32_t FloorPow2(uint32_t v)
        {
            uint32_t p = 1;
            while (p * 2 <= v) p *= 2;
            return p;
        }

        int AddTexture(Builder& b, Texture* texture, const Options& options)
        {
            if (texture == nullptr) return -1;

            auto it = b.mTextureIndices.find(texture);
            if (it != b.mTextureIndices.end()) return it->second;

            // In the editor the source pixels are always RGBA8, whatever the
            // console cook format (RGB565, LA4, ...) the asset is set to, so
            // only the buffer size is checked.
            const std::vector<uint8_t>& pixels = texture->GetPixels();
            const uint32_t w = texture->GetWidth();
            const uint32_t h = texture->GetHeight();
            if (pixels.size() != (size_t)w * h * 4 || w == 0 || h == 0)
            {
                LogWarning("Banner export: texture '%s' has no resident RGBA8 pixels; material will be untextured.", texture->GetName().c_str());
                b.mTextureIndices[texture] = -1;
                return -1;
            }

            // The whole CGFX has to stay under 512 KB, so textures are the
            // main thing to keep small: power-of-two and capped.
            const uint32_t maxSize = std::max(8u, FloorPow2(options.mMaxTextureSize));
            uint32_t dstW = std::max(8u, std::min(maxSize, FloorPow2(w)));
            uint32_t dstH = std::max(8u, std::min(maxSize, FloorPow2(h)));

            std::vector<uint8_t> out((size_t)dstW * dstH * 4);
            if (dstW == w && dstH == h)
            {
                out = pixels;
            }
            else
            {
                stbir_resize_uint8_srgb(pixels.data(), (int)w, (int)h, 0, out.data(), (int)dstW, (int)dstH, 0, STBIR_RGBA);
            }

            char fileName[64];
            std::snprintf(fileName, sizeof(fileName), "tex_%d.png", (int)b.mImages.size());
            std::string path = options.mOutputDir + fileName;
            if (!stbi_write_png(path.c_str(), (int)dstW, (int)dstH, 4, out.data(), (int)dstW * 4))
            {
                LogWarning("Banner export: failed to write %s", path.c_str());
                b.mTextureIndices[texture] = -1;
                return -1;
            }

            b.mImages.push_back(fileName);
            int index = (int)b.mImages.size() - 1;
            b.mTextureIndices[texture] = index;
            return index;
        }

        int AddMaterial(Builder& b, Material* material, const Options& options)
        {
            auto it = b.mMaterialIndices.find(material);
            if (it != b.mMaterialIndices.end()) return it->second;

            MaterialEntry entry;
            entry.mSource = material;

            if (material != nullptr)
            {
                Texture* texture = nullptr;
                MaterialLite* lite = Material::AsLite(material);
                if (lite != nullptr)
                {
                    texture = lite->GetTexture(0);
                    entry.mColor = lite->GetColor();
                }
                else
                {
                    for (ShaderParameter& param : material->GetParameters())
                    {
                        if (param.mType == ShaderParameterType::Texture && param.mTextureValue.Get() != nullptr)
                        {
                            texture = param.mTextureValue.Get<Texture>();
                            break;
                        }
                    }
                }

                entry.mTextureIndex = AddTexture(b, texture, options);
                entry.mBlendMode = material->GetBlendMode();
                entry.mMaskCutoff = material->GetMaskCutoff();
                entry.mDoubleSided = (material->GetCullMode() == CullMode::None);
            }

            b.mMaterials.push_back(entry);
            int index = (int)b.mMaterials.size() - 1;
            b.mMaterialIndices[material] = index;
            return index;
        }

        int AddMesh(Builder& b, StaticMesh* mesh, Material* material, const Options& options)
        {
            auto key = std::make_pair(mesh, material);
            auto it = b.mMeshIndices.find(key);
            if (it != b.mMeshIndices.end()) return it->second;

            const uint32_t numVerts = mesh->GetNumVertices();
            const uint32_t numIndices = mesh->GetNumIndices();
            const bool hasColor = mesh->HasVertexColor();
            const Vertex* verts = hasColor ? nullptr : mesh->GetVertices();
            const VertexColor* colorVerts = hasColor ? mesh->GetColorVertices() : nullptr;
            const IndexType* indices = mesh->GetIndices();

            if (numVerts == 0 || numIndices == 0 || indices == nullptr || (verts == nullptr && colorVerts == nullptr))
            {
                LogWarning("Banner export: mesh '%s' has no resident vertex data; skipped.", mesh->GetName().c_str());
                b.mMeshIndices[key] = -1;
                return -1;
            }

            std::vector<float> positions((size_t)numVerts * 3);
            std::vector<float> normals((size_t)numVerts * 3);
            std::vector<float> uvs((size_t)numVerts * 2);
            std::vector<uint8_t> colors(hasColor ? (size_t)numVerts * 4 : 0);

            glm::vec3 minP(FLT_MAX);
            glm::vec3 maxP(-FLT_MAX);

            for (uint32_t i = 0; i < numVerts; ++i)
            {
                glm::vec3 p = hasColor ? colorVerts[i].mPosition : verts[i].mPosition;
                glm::vec3 n = hasColor ? colorVerts[i].mNormal : verts[i].mNormal;
                glm::vec2 uv = hasColor ? colorVerts[i].mTexcoord0 : verts[i].mTexcoord0;

                float len = glm::length(n);
                n = (len > 0.0001f) ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);

                positions[i * 3 + 0] = p.x; positions[i * 3 + 1] = p.y; positions[i * 3 + 2] = p.z;
                normals[i * 3 + 0] = n.x;   normals[i * 3 + 1] = n.y;   normals[i * 3 + 2] = n.z;
                uvs[i * 2 + 0] = uv.x;      uvs[i * 2 + 1] = uv.y;

                minP = glm::min(minP, p);
                maxP = glm::max(maxP, p);

                if (hasColor)
                {
                    // Bytes are stored R, G, B, A in memory (ColorFloat4ToUint32).
                    std::memcpy(&colors[(size_t)i * 4], &colorVerts[i].mColor, 4);
                }
            }

            MeshEntry entry;
            entry.mMesh = mesh;
            entry.mMaterial = AddMaterial(b, material, options);

            int posView = b.AddView(positions.data(), positions.size() * sizeof(float), kTargetArrayBuffer);
            entry.mPositionAccessor = b.AddAccessor(posView, kComponentFloat, (int)numVerts, "VEC3");
            Accessor& posAcc = b.mAccessors[entry.mPositionAccessor];
            posAcc.mHasMinMax = true;
            posAcc.mMin = { minP.x, minP.y, minP.z };
            posAcc.mMax = { maxP.x, maxP.y, maxP.z };

            int nrmView = b.AddView(normals.data(), normals.size() * sizeof(float), kTargetArrayBuffer);
            entry.mNormalAccessor = b.AddAccessor(nrmView, kComponentFloat, (int)numVerts, "VEC3");

            int uvView = b.AddView(uvs.data(), uvs.size() * sizeof(float), kTargetArrayBuffer);
            entry.mUvAccessor = b.AddAccessor(uvView, kComponentFloat, (int)numVerts, "VEC2");

            if (hasColor)
            {
                int colView = b.AddView(colors.data(), colors.size(), kTargetArrayBuffer);
                entry.mColorAccessor = b.AddAccessor(colView, 5121 /* UNSIGNED_BYTE */, (int)numVerts, "VEC4", true);
            }

            // Indices: uint16 when they fit, which also halves the buffer.
            if (numVerts <= 65535)
            {
                std::vector<uint16_t> idx16(numIndices);
                for (uint32_t i = 0; i < numIndices; ++i) idx16[i] = (uint16_t)indices[i];
                int idxView = b.AddView(idx16.data(), idx16.size() * sizeof(uint16_t), kTargetElementArrayBuffer);
                entry.mIndexAccessor = b.AddAccessor(idxView, kComponentUShort, (int)numIndices, "SCALAR");
            }
            else
            {
                std::vector<uint32_t> idx32(numIndices);
                for (uint32_t i = 0; i < numIndices; ++i) idx32[i] = (uint32_t)indices[i];
                int idxView = b.AddView(idx32.data(), idx32.size() * sizeof(uint32_t), kTargetElementArrayBuffer);
                entry.mIndexAccessor = b.AddAccessor(idxView, kComponentUInt, (int)numIndices, "SCALAR");
            }

            b.mMeshes.push_back(entry);
            int index = (int)b.mMeshes.size() - 1;
            b.mMeshIndices[key] = index;
            return index;
        }

        void ExpandAabb(const AABB& local, const glm::mat4& transform, glm::vec3& outMin, glm::vec3& outMax)
        {
            for (int i = 0; i < 8; ++i)
            {
                glm::vec3 corner(
                    (i & 1) ? local.mMax.x : local.mMin.x,
                    (i & 2) ? local.mMax.y : local.mMin.y,
                    (i & 4) ? local.mMax.z : local.mMin.z);
                glm::vec3 world = glm::vec3(transform * glm::vec4(corner, 1.0f));
                outMin = glm::min(outMin, world);
                outMax = glm::max(outMax, world);
            }
        }

        using JsonWriter = rapidjson::Writer<rapidjson::StringBuffer>;

        void WriteVec(JsonWriter& w, const float* v, int n)
        {
            w.StartArray();
            for (int i = 0; i < n; ++i) w.Double(v[i]);
            w.EndArray();
        }

        std::string BuildJson(const Builder& b, const std::string& binName, bool hasAnimation,
                              int animInputAccessor, int animOutputAccessor, int spinNode)
        {
            rapidjson::StringBuffer sb;
            JsonWriter w(sb);

            w.StartObject();

            w.Key("asset");
            w.StartObject();
            w.Key("version"); w.String("2.0");
            w.Key("generator"); w.String("Polyphase BannerGltfExporter");
            w.EndObject();

            if (b.mLighting.mHasLight || b.mLighting.mHasAmbient)
            {
                w.Key("extras");
                w.StartObject();
                w.Key("polyphase");
                w.StartObject();
                if (b.mLighting.mHasLight)
                {
                    w.Key("lightDirection"); WriteVec(w, &b.mLighting.mDirection.x, 3);
                    w.Key("lightColor");     WriteVec(w, &b.mLighting.mColor.x, 3);
                }
                if (b.mLighting.mHasAmbient)
                {
                    w.Key("ambient"); WriteVec(w, &b.mLighting.mAmbient.x, 3);
                }
                w.EndObject();
                w.EndObject();
            }

            w.Key("scene"); w.Int(0);
            w.Key("scenes");
            w.StartArray();
            w.StartObject();
            w.Key("nodes"); w.StartArray(); w.Int(0); w.EndArray();
            w.EndObject();
            w.EndArray();

            w.Key("nodes");
            w.StartArray();
            for (const NodeEntry& node : b.mNodes)
            {
                w.StartObject();
                w.Key("name"); w.String(node.mName.c_str());
                if (node.mMesh >= 0) { w.Key("mesh"); w.Int(node.mMesh); }
                if (node.mTranslation != glm::vec3(0.0f))
                {
                    w.Key("translation"); WriteVec(w, &node.mTranslation.x, 3);
                }
                if (node.mRotation != glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
                {
                    const float q[4] = { node.mRotation.x, node.mRotation.y, node.mRotation.z, node.mRotation.w };
                    w.Key("rotation"); WriteVec(w, q, 4);
                }
                if (node.mScale != glm::vec3(1.0f))
                {
                    w.Key("scale"); WriteVec(w, &node.mScale.x, 3);
                }
                if (!node.mChildren.empty())
                {
                    w.Key("children");
                    w.StartArray();
                    for (int c : node.mChildren) w.Int(c);
                    w.EndArray();
                }
                w.EndObject();
            }
            w.EndArray();

            w.Key("meshes");
            w.StartArray();
            for (const MeshEntry& mesh : b.mMeshes)
            {
                w.StartObject();
                w.Key("name"); w.String(mesh.mMesh->GetName().c_str());
                w.Key("primitives");
                w.StartArray();
                w.StartObject();
                w.Key("attributes");
                w.StartObject();
                w.Key("POSITION"); w.Int(mesh.mPositionAccessor);
                w.Key("NORMAL"); w.Int(mesh.mNormalAccessor);
                w.Key("TEXCOORD_0"); w.Int(mesh.mUvAccessor);
                if (mesh.mColorAccessor >= 0) { w.Key("COLOR_0"); w.Int(mesh.mColorAccessor); }
                w.EndObject();
                w.Key("indices"); w.Int(mesh.mIndexAccessor);
                w.Key("material"); w.Int(mesh.mMaterial);
                w.Key("mode"); w.Int(4);
                w.EndObject();
                w.EndArray();
                w.EndObject();
            }
            w.EndArray();

            w.Key("materials");
            w.StartArray();
            for (const MaterialEntry& mat : b.mMaterials)
            {
                w.StartObject();
                w.Key("name"); w.String(mat.mSource != nullptr ? mat.mSource->GetName().c_str() : "Default");
                w.Key("pbrMetallicRoughness");
                w.StartObject();
                w.Key("baseColorFactor"); WriteVec(w, &mat.mColor.x, 4);
                if (mat.mTextureIndex >= 0)
                {
                    w.Key("baseColorTexture");
                    w.StartObject();
                    w.Key("index"); w.Int(mat.mTextureIndex);
                    w.Key("texCoord"); w.Int(0);
                    w.EndObject();
                }
                w.Key("metallicFactor"); w.Double(0.0);
                w.Key("roughnessFactor"); w.Double(1.0);
                w.EndObject();
                if (mat.mBlendMode == BlendMode::Translucent || mat.mBlendMode == BlendMode::Additive)
                {
                    w.Key("alphaMode"); w.String("BLEND");
                }
                else if (mat.mBlendMode == BlendMode::Masked)
                {
                    w.Key("alphaMode"); w.String("MASK");
                    w.Key("alphaCutoff"); w.Double(mat.mMaskCutoff);
                }
                if (mat.mDoubleSided) { w.Key("doubleSided"); w.Bool(true); }
                w.EndObject();
            }
            w.EndArray();

            if (!b.mImages.empty())
            {
                w.Key("images");
                w.StartArray();
                for (const std::string& img : b.mImages)
                {
                    w.StartObject();
                    w.Key("uri"); w.String(img.c_str());
                    w.EndObject();
                }
                w.EndArray();

                w.Key("samplers");
                w.StartArray();
                w.StartObject();
                w.Key("magFilter"); w.Int(9729);
                w.Key("minFilter"); w.Int(9729);
                w.Key("wrapS"); w.Int(10497);
                w.Key("wrapT"); w.Int(10497);
                w.EndObject();
                w.EndArray();

                w.Key("textures");
                w.StartArray();
                for (size_t i = 0; i < b.mImages.size(); ++i)
                {
                    w.StartObject();
                    w.Key("sampler"); w.Int(0);
                    w.Key("source"); w.Int((int)i);
                    w.EndObject();
                }
                w.EndArray();
            }

            if (hasAnimation)
            {
                w.Key("animations");
                w.StartArray();
                w.StartObject();
                w.Key("name"); w.String("Spin");
                w.Key("samplers");
                w.StartArray();
                w.StartObject();
                w.Key("input"); w.Int(animInputAccessor);
                w.Key("output"); w.Int(animOutputAccessor);
                w.Key("interpolation"); w.String("LINEAR");
                w.EndObject();
                w.EndArray();
                w.Key("channels");
                w.StartArray();
                w.StartObject();
                w.Key("sampler"); w.Int(0);
                w.Key("target");
                w.StartObject();
                w.Key("node"); w.Int(spinNode);
                w.Key("path"); w.String("rotation");
                w.EndObject();
                w.EndObject();
                w.EndArray();
                w.EndObject();
                w.EndArray();
            }

            w.Key("buffers");
            w.StartArray();
            w.StartObject();
            w.Key("uri"); w.String(binName.c_str());
            w.Key("byteLength"); w.Uint64((uint64_t)b.mBin.size());
            w.EndObject();
            w.EndArray();

            w.Key("bufferViews");
            w.StartArray();
            for (const BufferView& view : b.mViews)
            {
                w.StartObject();
                w.Key("buffer"); w.Int(0);
                w.Key("byteOffset"); w.Uint64((uint64_t)view.mOffset);
                w.Key("byteLength"); w.Uint64((uint64_t)view.mLength);
                if (view.mTarget != 0) { w.Key("target"); w.Int(view.mTarget); }
                w.EndObject();
            }
            w.EndArray();

            w.Key("accessors");
            w.StartArray();
            for (const Accessor& acc : b.mAccessors)
            {
                w.StartObject();
                w.Key("bufferView"); w.Int(acc.mBufferView);
                w.Key("componentType"); w.Int(acc.mComponentType);
                w.Key("count"); w.Int(acc.mCount);
                w.Key("type"); w.String(acc.mType);
                if (acc.mNormalized) { w.Key("normalized"); w.Bool(true); }
                if (acc.mHasMinMax)
                {
                    w.Key("min"); WriteVec(w, acc.mMin.data(), (int)acc.mMin.size());
                    w.Key("max"); WriteVec(w, acc.mMax.data(), (int)acc.mMax.size());
                }
                w.EndObject();
            }
            w.EndArray();

            w.EndObject();

            return sb.GetString();
        }

        bool WriteBinaryFile(const std::string& path, const void* data, size_t size)
        {
            FILE* f = fopen(path.c_str(), "wb");
            if (f == nullptr) return false;
            bool ok = (size == 0) || fwrite(data, 1, size, f) == size;
            fclose(f);
            return ok;
        }
    }

    std::string Export(const Options& options, std::string& outError)
    {
        outError.clear();

        Scene* scene = LoadAsset<Scene>(options.mSceneName);
        if (scene == nullptr)
        {
            outError = "scene asset '" + options.mSceneName + "' not found";
            return "";
        }

        NodePtr root = scene->Instantiate();
        if (root == nullptr)
        {
            outError = "scene '" + options.mSceneName + "' could not be instantiated";
            return "";
        }

        Builder b;

        // Wrapper nodes: origin (camera look-at point, tilted -90° about X)
        // -> spin (animated about its local Z) -> fit (+90° about X, scales
        // and centres the scene) -> meshes.
        //
        // pycgfx turns every rotation into Euler angles with X and Z from
        // atan2 (full range) but Y from asin (±90° only), so a turntable spin
        // about Y decomposes into X/Z flips past 90° and the model tumbles.
        // Spinning about local Z under a parent tilted -90° about X (local Z
        // = world +Y), with the fit node tilted back +90° so the geometry is
        // upright, keeps every key on a full-range axis. The tilts are folded
        // into the origin and fit nodes rather than being nodes of their own:
        // a textured, animated banner with two extra bones froze the HOME
        // Menu on hardware while the same content at this depth rendered.
        const glm::quat tilt   = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::quat untilt = glm::angleAxis(glm::radians( 90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        NodeEntry origin; origin.mName = "BannerOrigin"; origin.mTranslation = glm::vec3(0.0f, kLookAtY, 0.0f); origin.mRotation = tilt;
        NodeEntry spin;   spin.mName = "BannerSpin";
        NodeEntry fit;    fit.mName = "BannerFit";       fit.mRotation = untilt;
        b.mNodes.push_back(origin);
        b.mNodes.push_back(spin);
        b.mNodes.push_back(fit);
        const int kOriginNode = 0, kSpinNode = 1, kFitNode = 2;
        b.mNodes[kOriginNode].mChildren.push_back(kSpinNode);
        b.mNodes[kSpinNode].mChildren.push_back(kFitNode);

        glm::vec3 sceneMin(FLT_MAX);
        glm::vec3 sceneMax(-FLT_MAX);
        int skippedSkeletal = 0;

        // Scene ambient: the value the scene would hand the world. When the
        // scene doesn't override it the world default is used, which is the
        // same value the field is initialised with.
        {
            glm::vec4 ambient;
            scene->GetAmbientLightColor(ambient);
            b.mLighting.mHasAmbient = true;
            b.mLighting.mAmbient = glm::vec3(ambient);
        }

        root->Traverse([&](Node* node) -> bool
        {
            if (!node->IsVisible())
            {
                return false;
            }

            // First visible directional light drives the banner's single
            // CGFX light. Its forward vector is the direction light travels.
            if (DirectionalLight3D* light = node->As<DirectionalLight3D>())
            {
                if (!b.mLighting.mHasLight)
                {
                    glm::vec3 dir = light->GetDirection();
                    float len = glm::length(dir);
                    b.mLighting.mHasLight = len > 0.0001f;
                    b.mLighting.mDirection = b.mLighting.mHasLight ? dir / len : b.mLighting.mDirection;
                    b.mLighting.mColor = glm::vec3(light->GetColor()) * light->GetIntensity();
                }
                return true;
            }

            if (node->As<SkeletalMesh3D>() != nullptr)
            {
                ++skippedSkeletal;
                return true;
            }

            StaticMesh3D* meshNode = node->As<StaticMesh3D>();
            if (meshNode == nullptr)
            {
                return true;
            }

            StaticMesh* mesh = meshNode->GetStaticMesh();
            if (mesh == nullptr)
            {
                return true;
            }

            int meshIndex = AddMesh(b, mesh, meshNode->GetMaterial(), options);
            if (meshIndex < 0)
            {
                return true;
            }

            NodeEntry entry;
            entry.mName = node->GetName();
            entry.mMesh = meshIndex;
            entry.mTranslation = meshNode->GetWorldPosition();
            entry.mRotation = meshNode->GetWorldRotationQuat();
            entry.mScale = meshNode->GetWorldScale();
            b.mNodes.push_back(entry);
            b.mNodes[kFitNode].mChildren.push_back((int)b.mNodes.size() - 1);

            ExpandAabb(mesh->GetAABB(), meshNode->GetTransform(), sceneMin, sceneMax);
            return true;
        });

        root->Destroy();
        root = nullptr;

        if (skippedSkeletal > 0)
        {
            LogWarning("Banner export: %d skeletal mesh node(s) skipped (only static meshes are exported).", skippedSkeletal);
        }

        if (b.mNodes[kFitNode].mChildren.empty() || sceneMin.x > sceneMax.x)
        {
            outError = "scene contains no visible static meshes";
            return "";
        }

        // Fit the bounding sphere into the HOME Menu frustum.
        const glm::vec3 center = (sceneMin + sceneMax) * 0.5f;
        const float radius = std::max(glm::length(sceneMax - sceneMin) * 0.5f, 0.001f);
        // glTF node TRS is T*R*S, so the centring translation has to be
        // expressed in the fit node's rotated frame.
        const float scale = kTargetRadius / radius;
        b.mNodes[kFitNode].mScale = glm::vec3(scale);
        b.mNodes[kFitNode].mTranslation = untilt * (-center * scale);

        // Rotation animation about the spin node's local Z (= world Y, see
        // the tilt above). Keys are quaternions that pycgfx converts to an
        // Euler Z track and unwraps to the nearest angle key to key, so
        // consecutive keys must stay under 180° apart.
        //
        //  - Full spin (range >= 360): five keys per turn, 90° apart; 360° is
        //    written as (0,0,0,-1) so the last step also goes forward.
        //  - Sway (range < 360): one sine cycle between min and max sampled
        //    into 16 linear segments, so the reversals ease in and out. The
        //    speed is the average angular speed over the cycle.
        const float rotMin = std::min(options.mRotMinDeg, options.mRotMaxDeg);
        const float rotMax = std::max(options.mRotMinDeg, options.mRotMaxDeg);
        const float range = rotMax - rotMin;
        const float speed = std::fabs(options.mSpinDegreesPerSec);
        const bool fullSpin = range >= 360.0f;
        bool hasAnimation = options.mRotate && speed > 0.01f && (fullSpin || range > 0.01f);
        int animInput = -1;
        int animOutput = -1;
        if (hasAnimation)
        {
            // pycgfx writes key times as frame numbers (time * 60). The HOME
            // Menu froze on a 1 s sway sampled into 17 keys (3.75 frames
            // apart, fractional), while 5 keys 180 whole frames apart played
            // fine. So: every key lands on a whole frame, keys are at least
            // kMinFramesPerKey apart (fewer sine samples for short cycles),
            // and a cycle is never shorter than one second.
            const int kMinFramesPerKey = 15;
            const int kMinCycleFrames = 60;
            const int kMaxSegments = 16;

            std::vector<float> times;
            std::vector<float> quats;
            auto addKey = [&](int frame, float angleDeg)
            {
                const float angle = glm::radians(angleDeg);
                times.push_back((float)frame / 60.0f);
                quats.push_back(0.0f);
                quats.push_back(0.0f);
                quats.push_back(std::sin(angle * 0.5f));
                quats.push_back(std::cos(angle * 0.5f));
            };

            if (fullSpin)
            {
                const float dir = options.mSpinDegreesPerSec < 0.0f ? -1.0f : 1.0f;
                const int cycleFrames = std::max(kMinCycleFrames, (int)std::lround(360.0f / speed * 60.0f));
                for (int i = 0; i < 5; ++i)
                {
                    addKey((int)std::lround((float)cycleFrames * (float)i / 4.0f), rotMin + dir * 90.0f * (float)i);
                }
            }
            else
            {
                const float mid = 0.5f * (rotMin + rotMax);
                const float amp = 0.5f * range;
                const int cycleFrames = std::max(kMinCycleFrames, (int)std::lround(2.0f * range / speed * 60.0f));   // one there-and-back
                const int segments = std::max(2, std::min(kMaxSegments, cycleFrames / kMinFramesPerKey));
                for (int i = 0; i <= segments; ++i)
                {
                    const float phase = 2.0f * glm::pi<float>() * (float)i / (float)segments;
                    addKey((int)std::lround((float)cycleFrames * (float)i / (float)segments), mid + amp * std::sin(phase));
                }
            }

            const int keyCount = (int)times.size();
            int timeView = b.AddView(times.data(), times.size() * sizeof(float), 0);
            animInput = b.AddAccessor(timeView, kComponentFloat, keyCount, "SCALAR");
            b.mAccessors[animInput].mHasMinMax = true;
            b.mAccessors[animInput].mMin = { times.front() };
            b.mAccessors[animInput].mMax = { times.back() };

            int quatView = b.AddView(quats.data(), quats.size() * sizeof(float), 0);
            animOutput = b.AddAccessor(quatView, kComponentFloat, keyCount, "VEC4");
        }
        else if (options.mRotate == false || range < 360.0f)
        {
            // Static pose at the range's start (or at min for a disabled sway).
            b.mNodes[kSpinNode].mRotation = glm::angleAxis(glm::radians(rotMin), glm::vec3(0.0f, 0.0f, 1.0f));
        }

        const std::string gltfPath = options.mOutputDir + "banner.gltf";
        const std::string binPath = options.mOutputDir + "banner.bin";

        if (!WriteBinaryFile(binPath, b.mBin.data(), b.mBin.size()))
        {
            outError = "failed to write " + binPath;
            return "";
        }

        std::string json = BuildJson(b, "banner.bin", hasAnimation, animInput, animOutput, kSpinNode);
        if (!WriteBinaryFile(gltfPath, json.data(), json.size()))
        {
            outError = "failed to write " + gltfPath;
            return "";
        }

        LogDebug("Banner export: %d mesh node(s), %d material(s), %d texture(s), %zu KB of vertex data, %s -> %s",
                 (int)b.mNodes[kFitNode].mChildren.size(), (int)b.mMaterials.size(), (int)b.mImages.size(),
                 b.mBin.size() / 1024,
                 b.mLighting.mHasLight ? "directional light + ambient from scene" : "no directional light (pycgfx default headlight), ambient from scene",
                 gltfPath.c_str());
        return gltfPath;
    }
}

#endif /* EDITOR */
