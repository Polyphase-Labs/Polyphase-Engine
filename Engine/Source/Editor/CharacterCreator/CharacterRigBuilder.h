#pragma once

#if EDITOR

#include "SmartPointer.h"

#include <glm/glm.hpp>
#include <string>

class Node;
class Node3D;
class SkeletalMesh;
class AssetDir;
struct AssetStub;

/**
 * @file CharacterRigBuilder.h
 * @brief Builds ready-to-play first/third person character rigs.
 *
 * The rig is the node hierarchy the engine's built-in controllers
 * (Engine/Scripts/FirstPersonController.lua, ThirdPersonController.lua)
 * document at the top of their files:
 *
 *   <Name> (Capsule3D)            collision on, physics off, tag "Player"
 *     Camera (Camera3D)           first person: at eye height, Main Camera
 *     CameraPivot (Node3D)        third person: orbit pivot at chest height
 *       Camera (Camera3D)           behind the pivot, Main Camera
 *     Mesh (SkeletalMesh3D)       third person always; first person when a mesh is picked
 *     Controller (Node)           carries the script; node refs wired in
 *
 * Nothing here touches ImGui, so the same builder can serve the REST
 * controller or an addon later.
 */

enum class CharacterRigType : uint8_t
{
    FirstPerson,
    ThirdPerson
};

struct CharacterRigSpec
{
    CharacterRigType mType = CharacterRigType::FirstPerson;
    std::string mName = "Player";          // Root node name; also the Scene asset name
    float mHeight = 1.8f;                  // Total capsule height in metres (feet to top)
    SkeletalMesh* mSkeletalMesh = nullptr; // Optional for first person, expected for third person
    bool mFitMeshToHeight = false;         // Scale the mesh so its bind-pose box is mHeight tall (else scale 1)
    std::string mScriptFile;               // Script name as the Node "Script" property takes it
    bool mTagAsPlayer = true;
    bool mMainCamera = true;
};

// Numbers derived from the spec height. Shared by the builder and the
// dialog's preview text so both agree.
struct CharacterRigMetrics
{
    float mHeight = 0.0f;          // Clamped total height
    float mRadius = 0.0f;          // Capsule radius
    float mCapsuleSegment = 0.0f;  // Capsule3D::mHeight (cylinder part; total = segment + 2r)
    float mRootY = 0.0f;           // Root local Y that puts the feet on y = 0
    float mEyeLocalY = 0.0f;       // First person camera Y relative to the root
    float mPivotLocalY = 0.0f;     // Third person pivot Y relative to the root
    float mCameraDistance = 0.0f;  // Third person camera distance behind the pivot
    float mFollowCamHeight = 0.0f; // Third person follow-cam height
};

namespace CharacterRigBuilder
{
    CharacterRigMetrics ComputeMetrics(const CharacterRigSpec& spec);

    // Built detached from any world at the origin. Null + outError on failure
    // (script file not found / class table missing).
    SharedPtr<Node3D> BuildRig(const CharacterRigSpec& spec, std::string& outError);

    // Undoable: clones the rig into the open scene under parent (null = world
    // root), standing with its feet at feetPos, and selects it. One undo step.
    Node* SpawnRigIntoScene(const CharacterRigSpec& spec, Node* parent, glm::vec3 feetPos, std::string& outError);

    // Writes <spec.mName>.oct (uniquified on collision) into dir. Returns the stub.
    AssetStub* SaveRigAsScene(const CharacterRigSpec& spec, AssetDir* dir, std::string& outError);

    // Output-directory helpers. Paths may be absolute or relative to the
    // project's asset root; anything outside that root is refused. Missing
    // folders are created on disk and in the AssetDir tree.
    AssetDir* ResolveOrCreateOutputDir(const std::string& userPath, std::string& outError);
    // False when the path is outside the asset root. outRel has no leading or
    // trailing slash and is empty for the root itself.
    bool ToProjectRelativeAssetPath(const std::string& path, std::string& outRel);
    std::string GetProjectAssetRoot(); // absolute, forward slashes, trailing '/', "" if no project
}

#endif // EDITOR
