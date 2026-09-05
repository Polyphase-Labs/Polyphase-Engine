#pragma once

#include <string>
#include <string.h>
#include <float.h>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

#include "Constants.h"
#include "Maths.h"
#include "SmartPointer.h"

#include "System/SystemTypes.h"
#include "Graphics/GraphicsTypes.h"
#include "Input/InputTypes.h"

#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>

#if LUA_ENABLED
#include <Lua/lua.hpp>
#endif

class Level;
class Primitive3D;
class Light3D;
class Node3D;
class Node;

// Platform enum moved here for use in EngineConfig
enum class Platform : int
{
    Windows,
    Linux,
    Android,
    GameCube,
    Wii,
    N3DS,
    // NOTE: the literal token `PSP` cannot be used here as an enum value
    // because PSPSDK predefines `PSP=1` as a command-line macro for every TU
    // it compiles — leaking into a Polyphase build that links against any
    // PSP addon. The expansion `Platform::1` is a syntax error. `Psp` is
    // unaffected and `GetPlatformString` still returns "PSP" for display.
    Psp,

    Count
};

// Bitmask form of Platform for per-asset platform-mask flags.
// Values must mirror Platform's index order: PlatformBit::X == (1u << int(Platform::X)).
enum PlatformBit : uint32_t
{
    PlatformBit_Windows  = 1u << int(Platform::Windows),
    PlatformBit_Linux    = 1u << int(Platform::Linux),
    PlatformBit_Android  = 1u << int(Platform::Android),
    PlatformBit_GameCube = 1u << int(Platform::GameCube),
    PlatformBit_Wii      = 1u << int(Platform::Wii),
    PlatformBit_N3DS     = 1u << int(Platform::N3DS),
    PlatformBit_Psp      = 1u << int(Platform::Psp),

    PlatformBit_All      = (1u << int(Platform::Count)) - 1u,
};

inline uint32_t GetPlatformBit(Platform p)
{
    return (p >= Platform::Windows && p < Platform::Count) ? (1u << int(p)) : 0u;
}

class StaticMesh;
class Material;

typedef uint32_t TypeId;
typedef uint32_t NetId;
typedef uint32_t NodeId;
typedef uint64_t RuntimeId;

// Bullet Types
class btDynamicsWorld;
struct btDbvtBroadphase;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;
class btRigidBody;
class btCollisionShape;
class btBvhTriangleMeshShape;
class btTriangleIndexVertexArray;
struct btTriangleInfoMap;

enum class CollisionShape : uint32_t
{
    Empty,
    Box,
    Sphere,
    Capsule,
    TriangleMesh,
    ScaledTriangleMesh,
    ConvexHull,
    Compound,

    Num
};

enum class ShadingModel : uint32_t
{
    Unlit,
    Lit,
    Toon,

    Count
};

enum class BlendMode : uint32_t
{
    Opaque,
    Masked,
    Translucent,
    Additive,

    Count
};

enum class VertexColorMode : uint32_t
{
    None,
    Modulate,
    TextureBlend,

    Count
};

enum class TevMode : uint32_t
{
    Replace,
    Modulate,
    Decal,
    Add,
    SignedAdd,
    Subtract,
    Interpolate,
    Pass,

    Count
};

enum class CullMode : uint8_t
{
    None,
    Back,
    Front,

    Count
};

enum class PropertyOwnerType
{
    Node,
    Asset,
    Global,
    Count
};

enum class LightType : uint8_t
{
    Point,
    Spot,
    Directional,

    Count
};

enum class LightingDomain : uint8_t
{
    Static,
    Dynamic,
    All,

    Count
};

enum class PostProcessPassId : uint8_t
{
    Blur,
    Fxaa,
    Tonemap,

    Count
};

enum class SkyType : uint8_t
{
    None,
    ColoredSky,
    TexturedSky,
    RayTracedSky,

    Count
};

struct Bounds
{
    glm::vec3 mCenter = { };
    float mRadius = 1.0f;
};

// Axis-aligned bounding box. Kept separate from Bounds on purpose -- Bounds is
// embedded by value in DrawData and streamed through the per-frame cull loops,
// so it must stay small.
struct AABB
{
    glm::vec3 mMin = { -0.5f, -0.5f, -0.5f };
    glm::vec3 mMax = { 0.5f, 0.5f, 0.5f };

    AABB() = default;
    AABB(glm::vec3 min, glm::vec3 max) : mMin(min), mMax(max) { }

    // An inverted box. This is the identity element for Encapsulate() folds.
    static AABB MakeInvalid()
    {
        return AABB(glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX));
    }

    static AABB MakeFromCenterExtents(glm::vec3 center, glm::vec3 extents)
    {
        return AABB(center - extents, center + extents);
    }

    // Matches the LARGE_BOUNDS sphere used as the "never cull me" default.
    static AABB MakeLarge()
    {
        return AABB(glm::vec3(-LARGE_BOUNDS), glm::vec3(LARGE_BOUNDS));
    }

    bool IsValid() const
    {
        return mMin.x <= mMax.x && mMin.y <= mMax.y && mMin.z <= mMax.z;
    }

    // True when this is (approximately) the LARGE_BOUNDS placeholder box, so
    // callers like editor focus-on-selection can ignore it.
    bool IsLarge() const
    {
        return (mMax.x - mMin.x) >= (LARGE_BOUNDS - 1.0f);
    }

    glm::vec3 GetCenter() const { return (mMin + mMax) * 0.5f; }
    glm::vec3 GetExtents() const { return (mMax - mMin) * 0.5f; }
    glm::vec3 GetSize() const { return mMax - mMin; }
    float GetRadius() const { return glm::length(GetExtents()); }
    float GetVolume() const { glm::vec3 size = GetSize(); return size.x * size.y * size.z; }

    bool Contains(glm::vec3 point) const
    {
        return point.x >= mMin.x && point.x <= mMax.x
            && point.y >= mMin.y && point.y <= mMax.y
            && point.z >= mMin.z && point.z <= mMax.z;
    }

    bool Contains(const AABB& other) const
    {
        return Contains(other.mMin) && Contains(other.mMax);
    }

    bool Intersects(const AABB& other) const
    {
        return mMin.x <= other.mMax.x && mMax.x >= other.mMin.x
            && mMin.y <= other.mMax.y && mMax.y >= other.mMin.y
            && mMin.z <= other.mMax.z && mMax.z >= other.mMin.z;
    }

    bool Intersects(glm::vec3 sphereCenter, float sphereRadius) const
    {
        glm::vec3 closest = glm::clamp(sphereCenter, mMin, mMax);
        glm::vec3 delta = closest - sphereCenter;
        return glm::dot(delta, delta) <= (sphereRadius * sphereRadius);
    }

    void Encapsulate(glm::vec3 point)
    {
        mMin = glm::min(mMin, point);
        mMax = glm::max(mMax, point);
    }

    void Encapsulate(const AABB& other)
    {
        if (!other.IsValid())
            return;

        mMin = glm::min(mMin, other.mMin);
        mMax = glm::max(mMax, other.mMax);
    }

    void Expand(float amount) { mMin -= amount; mMax += amount; }
    void Expand(glm::vec3 amount) { mMin -= amount; mMax += amount; }

    // Refit this box after applying an arbitrary transform. Equivalent to
    // transforming all 8 corners and refitting, but does it in 9 mul/add.
    AABB Transform(const glm::mat4& mat) const
    {
        if (!IsValid())
            return *this;

        glm::vec3 center = GetCenter();
        glm::vec3 extents = GetExtents();

        glm::vec3 newCenter = glm::vec3(mat * glm::vec4(center, 1.0f));
        glm::vec3 newExtents;

        for (int32_t i = 0; i < 3; ++i)
        {
            newExtents[i] =
                glm::abs(mat[0][i]) * extents.x +
                glm::abs(mat[1][i]) * extents.y +
                glm::abs(mat[2][i]) * extents.z;
        }

        return AABB(newCenter - newExtents, newCenter + newExtents);
    }

    Bounds ToBounds() const
    {
        Bounds retBounds;
        retBounds.mCenter = GetCenter();
        retBounds.mRadius = GetRadius();
        return retBounds;
    }
};

struct DrawData
{
    Node* mNode;
    Material* mMaterial;
    BlendMode mBlendMode;
    glm::vec3 mPosition;
    Bounds mBounds;
    int32_t mSortPriority;
    float mDistance2;
    TypeId mNodeType;
    bool mDepthless;
    uint32_t mOcclusionSlot; // 0 = not an occludee, occludee index + 1, or kDynamicOccludee

    // Occludee with no baked slot (moved, spawned, or animated): tested
    // against the coarse dynamic table by its current bounds.
    static const uint32_t kDynamicOccludee = 0xFFFFFFFFu;
};

struct LightData
{
    LightType mType;
    LightingDomain mDomain;
    uint8_t mLightingChannels;
    glm::vec3 mPosition;
    glm::vec4 mColor;
    glm::vec3 mDirection;
    float mRadius;
    float mIntensity;
    float mInnerConeAngle; // Spot only, half angle in degrees.
    float mOuterConeAngle; // Spot only, half angle in degrees.
};

struct DebugDraw
{
    StaticMesh* mMesh = nullptr;
    Material* mMaterial = nullptr;
    Node3D* mNode = nullptr;
    glm::mat4 mTransform = glm::mat4(1);
    glm::vec4 mColor = { 0.25f, 0.25f, 1.0f, 1.0f };
    float mLife = 0.0f;
};

struct PrimitivePair
{
    Primitive3D* mPrimitiveA = nullptr;
    Primitive3D* mPrimitiveB = nullptr;

    PrimitivePair() :
        mPrimitiveA(nullptr),
        mPrimitiveB(nullptr)
    {

    }

    PrimitivePair(Primitive3D* compA, Primitive3D* compB)
    {
        mPrimitiveA = compA;
        mPrimitiveB = compB;
    }

    size_t operator()(const PrimitivePair& pairToHash) const
    {
        size_t hash = (size_t)pairToHash.mPrimitiveA + (size_t)pairToHash.mPrimitiveB;
        return hash;
    }

    bool operator==(const PrimitivePair& other) const
    {
        return (mPrimitiveA == other.mPrimitiveA) &&
            (mPrimitiveB == other.mPrimitiveB);
    }

    PrimitivePair(const PrimitivePair& other)
    {
        mPrimitiveA = other.mPrimitiveA;
        mPrimitiveB = other.mPrimitiveB;
    }

    PrimitivePair& operator=(const PrimitivePair& other)
    {
        mPrimitiveA = other.mPrimitiveA;
        mPrimitiveB = other.mPrimitiveB;
        return *this;
    }

    PrimitivePair& operator=(PrimitivePair&& other)
    {
        mPrimitiveA = other.mPrimitiveA;
        mPrimitiveB = other.mPrimitiveB;
        return *this;
    }
};

struct EngineConfig
{
    EngineConfig()
    {

    }

    std::string mProjectName;

    std::string mDefaultScene = "";
    std::string mDefaultEditorScene = "";
    std::string mDefaultLoadingScene = "";
    float mLoadingMinDisplaySeconds = 0.0f;
    float mLoadingTimeoutSeconds = 0.0f;
    uint32_t mGameCode = 0;
    uint32_t mVersion = 0;
    int32_t mWindowWidth = DEFAULT_WINDOW_WIDTH;
    int32_t mWindowHeight = DEFAULT_WINDOW_HEIGHT;

    bool mFullscreen = false;
    bool mValidateGraphics = false;
    bool mLinearColorSpace = false;
    bool mPackageForSteam = false;
    bool mUseAssetRegistry = false;
    bool mLogging = true;
    bool mLogToFile = false;
    bool mScriptHotReload = false;
    bool mCSharpScripting = false;

    int32_t mLqMaxTextureSize = 0;
    bool mLqEnableMipMaps = true;

    std::string mProjectPath;
    std::string mCurrentFont;
    std::string mWorkingDirectory;
    std::string mEditorFont = "Default";

    struct EmbeddedFile* mEmbeddedAssets = nullptr;
    uint32_t mEmbeddedAssetCount = 0;
    struct EmbeddedFile* mEmbeddedScripts = nullptr;
    uint32_t mEmbeddedScriptCount = 0;
    const char* mEmbeddedConfig = nullptr;
    uint32_t mEmbeddedConfigSize = 0;

    float mEditorInterfaceScale = 1.0f;
    int32_t mColorScale = 2;

    std::string mIconPath;

    // Headless mode configuration
    bool mHeadless = false;
    Platform mBuildPlatform = Platform::Count;  // Count = no build requested
    bool mBuildEmbedded = false;
    std::string mBuildTargetId;                 // -build <targetId> (e.g. polyphase.n3ds.cia); empty = platform build

    // Set when the editor was launched with --addon-recovery=<pid>. Value is
    // the PID of the prior editor process whose sentinel JSON we should look
    // for. EditorMain reads this on startup and triggers the addon-recovery
    // wipe before the first OpenProject. 0 = not in recovery mode.
    uint64_t mAddonRecoveryOldPid = 0;
};

enum class ConsoleMode
{
    Off,
    Overlay,
    Full
};

struct EngineState
{
    uint32_t mWindowWidth = DEFAULT_WINDOW_WIDTH;
    uint32_t mWindowHeight = DEFAULT_WINDOW_HEIGHT;
    uint32_t mSecondWindowWidth = DEFAULT_WINDOW_WIDTH;
    uint32_t mSecondWindowHeight = DEFAULT_WINDOW_HEIGHT;
    uint32_t mGameCode = 0;
    uint32_t mVersion = 0;
    uint32_t mFrameNumber = 0;
    std::string mProjectPath;
    std::string mIOAssetPath;
    std::string mProjectDirectory;
    std::string mProjectName;
    std::string mAssetDirectory;
    std::string mSolutionPath;
    int32_t mArgC = 0;
    char** mArgV = nullptr;
    float mGameDeltaTime = 0.0f;
    float mRealDeltaTime = 0.0f;
    float mGameElapsedTime = 0.0f;
    float mRealElapsedTime = 0.0f;
    float mTimeDilation = 1.0f;
    float mAspectRatioScale = 1.0f;
    bool mPaused = false;
    bool mFrameStep = false;
    bool mInitialized = false;
    bool mSuspended = false;
    FILE* mLogFile = nullptr;

#if LUA_ENABLED
    lua_State* mLua = nullptr;
#endif
    
    bool mConsoleMode = false;
    bool mQuit = false;
    bool mWindowMinimized = false;
    bool mStandalone = false;

    SystemState mSystem;
    GraphicsState mGraphics;
    InputState mInput;
};

struct RayTestResult
{
    glm::vec3 mStart = {};
    glm::vec3 mEnd = {};
    Primitive3D* mHitNode = {};
    glm::vec3 mHitNormal = {};
    glm::vec3 mHitPosition = {};
    float mHitFraction = 0.0f;
};

struct RayTestMultiResult
{
    glm::vec3 mStart = {};
    glm::vec3 mEnd = {};
    uint32_t mNumHits = 0;
    std::vector<Primitive3D*> mHitNodes;
    std::vector<glm::vec3> mHitNormals;
    std::vector<glm::vec3> mHitPositions;
    std::vector<float> mHitFractions;
};

struct SweepTestResult
{
    glm::vec3 mStart = {};
    glm::vec3 mEnd = {};
    Primitive3D* mHitNode = {};
    glm::vec3 mHitNormal = {};
    glm::vec3 mHitPosition = {};
    float mHitFraction = 0.0f;
};

struct IgnoreRayResultCallback : btCollisionWorld::ClosestRayResultCallback
{
    IgnoreRayResultCallback(const btVector3& rayFromWorld, const btVector3& rayToWorld);
    virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult & rayResult, bool normalInWorldSpace) override;

    uint32_t mNumIgnoreObjects = 0;
    btCollisionObject** mIgnoreObjects = nullptr;
    bool mIgnorePureOverlap = true;
};

struct IgnoreConvexResultCallback : btCollisionWorld::ClosestConvexResultCallback
{
    IgnoreConvexResultCallback(
        const btVector3& convexFromWorld, 
        const btVector3& convexToWorld);
    virtual bool needsCollision(btBroadphaseProxy* proxy0) const;

    uint32_t mNumIgnoreObjects = 0;
    btCollisionObject** mIgnoreObjects = nullptr;
};

enum class FogDensityFunc : uint8_t
{
    Linear,
    Exponential,

    Count
};

struct FogSettings
{
    bool mEnabled = false;
    glm::vec4 mColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    FogDensityFunc mDensityFunc = FogDensityFunc::Linear;
    float mNear = 0.0f;
    float mFar = 100.0f;
};

enum CollisionGroup
{
    ColGroup0 = 1,
    ColGroup1 = 2,
    ColGroup2 = 4,
    ColGroup3 = 8,
    ColGroup4 = 16,
    ColGroup5 = 32,
    ColGroup6 = 64,
    ColGroup7 = 128,

    ColGroupAll = -1
};

enum class AttenuationFunc
{
    Constant,
    Linear,

    Count
};

enum class NetStatus
{
    Local,
    Connecting,
    Client,
    Server,
    
    Count
};

typedef uint8_t NetHostId;

struct NetHost
{
    uint32_t mIpAddress = 0;
    uint16_t mPort = 0;
    NetHostId mId = INVALID_HOST_ID;
    uint64_t mOnlineId = 0;
};

struct ReliablePacket
{
    ReliablePacket(uint16_t seqNum, const char* data, uint32_t size);

    float mTimeSinceSend = 0.0f;
    uint32_t mNumSends = 0;

    std::vector<char> mData;
    uint16_t mSeq = 0;
};

struct NetHostProfile
{
    static const uint32_t sSendBufferSize = 512;

    NetHost mHost;
    float mPing = 0.0f;
    float mTimeSinceLastMsg = 0.0f;
    std::unordered_set<NetId> mRelevantNetIds;
    std::vector<char> mSendBuffer;
    std::vector<char> mReliableSendBuffer;
    std::vector<ReliablePacket> mOutgoingPackets;
    std::vector<ReliablePacket> mIncomingPackets;
    uint16_t mOutgoingReliableSeq = 0;
    uint16_t mIncomingReliableSeq = 0;
    uint16_t mOutgoingUnreliableSeq = 0;
    uint16_t mIncomingUnreliableSeq = 0;
    WeakPtr<Node> mPawn;
    bool mReady = true;
};

typedef NetHostProfile NetClient;
typedef NetHostProfile NetServer;

struct FadingLight
{
    // mNode should only be used for comparisons!! If deleted, we want to fade it out, not crash.
    Light3D* mComponent = nullptr;
    LightData mData = {};
    glm::vec4 mColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    float mAlpha = 0.0f;

    FadingLight(Light3D* comp) : mComponent(comp) {}
};
