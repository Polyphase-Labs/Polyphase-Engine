#if EDITOR

#include "CharacterRigBuilder.h"

#include "Nodes/Node.h"
#include "Nodes/3D/Node3d.h"
#include "Nodes/3D/Capsule3d.h"
#include "Nodes/3D/Camera3d.h"
#include "Nodes/3D/SkeletalMesh3d.h"
#include "Assets/SkeletalMesh.h"
#include "Assets/Scene.h"
#include "Script.h"
#include "Property.h"
#include "AssetManager.h"
#include "AssetDir.h"
#include "Engine.h"
#include "EngineTypes.h"
#include "World.h"
#include "Log.h"
#include "System/System.h"

#include "ActionManager.h"
#include "EditorState.h"
#include "EditorUtils.h"

#include <algorithm>
#include <cctype>

namespace
{
    std::string NormalizeSlashes(std::string s)
    {
        for (char& c : s)
        {
            if (c == '\\') c = '/';
        }
        return s;
    }

    // Paths on Windows compare case-insensitively; the asset root came from
    // the project file and the user's path from a folder dialog or typing.
    bool PrefixEqualsNoCase(const std::string& s, const std::string& prefix)
    {
        if (s.size() < prefix.size()) return false;
        for (size_t i = 0; i < prefix.size(); ++i)
        {
            if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)prefix[i])) return false;
        }
        return true;
    }

    std::string StripLuaExtension(const std::string& name)
    {
        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".lua") == 0)
        {
            return name.substr(0, name.size() - 4);
        }
        return name;
    }

    bool ScriptFileExists(const std::string& scriptFile)
    {
        std::string name = StripLuaExtension(scriptFile);
        const std::string& projectDir = GetEngineState()->mProjectDirectory;
        if (!projectDir.empty())
        {
            std::string p = projectDir + "Scripts/" + name + ".lua";
            if (SYS_DoesFileExist(p.c_str(), false)) return true;
        }
        std::string engineScripts = GetEngineContentDir("Scripts/");
        if (!engineScripts.empty() && engineScripts.back() != '/' && engineScripts.back() != '\\')
        {
            engineScripts += '/';
        }
        std::string e = engineScripts + name + ".lua";
        return SYS_DoesFileExist(e.c_str(), false);
    }

    Property* FindScriptProperty(Script* script, const char* name)
    {
        std::vector<Property>& props = script->GetScriptProperties();
        for (Property& prop : props)
        {
            if (prop.mName == name) return &prop;
        }
        return nullptr;
    }

    void SetNodeProp(Script* script, const char* name, Node* target)
    {
        Property* prop = FindScriptProperty(script, name);
        if (prop != nullptr && target != nullptr)
        {
            prop->SetNode(target->GetSelfPtr());
        }
    }

    void SetFloatProp(Script* script, const char* name, float value)
    {
        Property* prop = FindScriptProperty(script, name);
        if (prop != nullptr)
        {
            prop->SetFloat(value);
        }
    }
}

namespace CharacterRigBuilder
{

CharacterRigMetrics ComputeMetrics(const CharacterRigSpec& spec)
{
    CharacterRigMetrics m;
    const float h = glm::clamp(spec.mHeight, 0.3f, 10.0f);
    m.mHeight = h;
    // ~shoulder half-width; the 0.3-0.4 m capsule most controllers assume,
    // clamped so giants and toddlers still get a sane collider.
    m.mRadius = glm::clamp(0.2f * h, 0.1f, 0.6f);
    // Bullet's capsule height is the cylinder segment only (Capsule3d.cpp).
    m.mCapsuleSegment = glm::max(h - 2.0f * m.mRadius, 0.05f);
    // Capsule origin is its centre; feet land on y = 0.
    m.mRootY = 0.5f * h;
    // Eye at 0.9 H keeps the camera near plane inside the capsule top.
    m.mEyeLocalY = 0.9f * h - m.mRootY;
    // Chest-height orbit pivot.
    m.mPivotLocalY = 0.75f * h - m.mRootY;
    // Script defaults (10 / 4) assume its large demo world; scale to the body.
    m.mCameraDistance = 2.5f * h;
    m.mFollowCamHeight = 1.5f * h;
    return m;
}

SharedPtr<Node3D> BuildRig(const CharacterRigSpec& spec, std::string& outError)
{
    outError.clear();
    if (spec.mName.empty())
    {
        outError = "Name is required.";
        return nullptr;
    }
    if (spec.mScriptFile.empty())
    {
        outError = "Script is required.";
        return nullptr;
    }

    const CharacterRigMetrics m = ComputeMetrics(spec);
    const bool thirdPerson = (spec.mType == CharacterRigType::ThirdPerson);

    SharedPtr<Capsule3D> capsule = Node::Construct<Capsule3D>();
    if (!capsule)
    {
        outError = "Failed to construct the capsule node.";
        return nullptr;
    }
    capsule->SetName(spec.mName);
    capsule->SetHeight(m.mCapsuleSegment);
    capsule->SetRadius(m.mRadius);
    // The controllers move by SweepToWorldPosition + SetWorldPosition, so the
    // capsule is a collider, not a rigid body. Environment geometry sits on
    // ColGroup1 (editor convention); ColGroupAll keeps that in the mask.
    capsule->EnableCollision(true);
    capsule->EnablePhysics(false);
    capsule->EnableOverlaps(true);
    capsule->SetCollisionGroup((uint8_t)ColGroup0);
    capsule->SetCollisionMask((uint8_t)ColGroupAll);
    if (spec.mTagAsPlayer)
    {
        capsule->AddTag("Player");
    }

    Camera3D* camera = nullptr;
    Node3D* pivot = nullptr;
    if (thirdPerson)
    {
        pivot = capsule->CreateChild<Node3D>("CameraPivot");
        pivot->SetPosition(glm::vec3(0.0f, m.mPivotLocalY, 0.0f));
        camera = pivot->CreateChild<Camera3D>("Camera");
        camera->SetPosition(glm::vec3(0.0f, 0.0f, m.mCameraDistance));
    }
    else
    {
        camera = capsule->CreateChild<Camera3D>("Camera");
        camera->SetPosition(glm::vec3(0.0f, m.mEyeLocalY, 0.0f));
    }
    camera->SetIsMainCamera(spec.mMainCamera);

    // The third person script dereferences self.mesh every tick, so the node
    // always exists for that rig even with no asset picked yet.
    SkeletalMesh3D* meshNode = nullptr;
    if (thirdPerson || spec.mSkeletalMesh != nullptr)
    {
        meshNode = capsule->CreateChild<SkeletalMesh3D>("Mesh");
        float scale = 1.0f;
        float minY = 0.0f;
        if (spec.mSkeletalMesh != nullptr)
        {
            meshNode->SetSkeletalMesh(spec.mSkeletalMesh);
            // Raw bind-pose box (GetAABB() is padded by Bounds Scale, which
            // would both mis-scale and float the mesh). Characters are
            // usually authored with the origin at the feet, so minY is ~0
            // and the mesh lands on the capsule bottom; a centred origin is
            // handled the same way through minY.
            AABB box = spec.mSkeletalMesh->GetBindPoseAABB();
            float extent = box.mMax.y - box.mMin.y;
            if (extent > 1e-4f)
            {
                minY = box.mMin.y;
                if (spec.mFitMeshToHeight)
                {
                    scale = m.mHeight / extent;
                }
            }
        }
        meshNode->SetScale(glm::vec3(scale));
        // Feet on the capsule bottom.
        meshNode->SetPosition(glm::vec3(0.0f, -m.mRootY - minY * scale, 0.0f));

        if (spec.mSkeletalMesh != nullptr && spec.mSkeletalMesh->GetAnimation("Idle") != nullptr)
        {
            std::vector<Property> props;
            meshNode->GatherProperties(props);
            for (Property& prop : props)
            {
                if (prop.mName == "Default Animation")
                {
                    prop.SetString("Idle");
                    break;
                }
            }
        }
    }

    // SetScriptFile instantiates the Lua class immediately (even off-world),
    // which is what populates the script's property list.
    Node* controller = capsule->CreateChild<Node>("Controller");
    controller->SetScriptFile(spec.mScriptFile);
    Script* script = controller->GetScript();
    if (script == nullptr || !script->IsActive())
    {
        outError = ScriptFileExists(spec.mScriptFile)
            ? "Script '" + spec.mScriptFile + "' failed to load. It must define a table named after the file."
            : "Script '" + spec.mScriptFile + "' was not found in <Project>/Scripts or Engine/Scripts.";
        return nullptr;
    }

    // Both controllers auto-resolve collider/camera in Start(), but explicit
    // refs survive renames. cameraPivot / mesh are only auto-resolved never.
    SetNodeProp(script, "collider", capsule.Get());
    SetNodeProp(script, "camera", camera);
    if (thirdPerson)
    {
        SetNodeProp(script, "cameraPivot", pivot);
        SetNodeProp(script, "mesh", meshNode);
        SetFloatProp(script, "cameraDistance", m.mCameraDistance);
        SetFloatProp(script, "followCamHeight", m.mFollowCamHeight);
    }

    return PtrStaticCast<Node3D>(capsule);
}

Node* SpawnRigIntoScene(const CharacterRigSpec& spec, Node* parent, glm::vec3 feetPos, std::string& outError)
{
    outError.clear();
    ActionManager* am = ActionManager::Get();
    if (am == nullptr)
    {
        outError = "Editor is not ready.";
        return nullptr;
    }
    if (parent != nullptr && parent->IsSceneLinked())
    {
        outError = "Cannot add nodes inside an instanced scene. Pick a parent outside it or unlink the scene first.";
        return nullptr;
    }

    // Build (and fail) before opening an action group so a bad script never
    // leaves a half-made undo entry.
    SharedPtr<Node3D> rig = BuildRig(spec, outError);
    if (!rig)
    {
        return nullptr;
    }
    const CharacterRigMetrics m = ComputeMetrics(spec);
    rig->SetPosition(feetPos + glm::vec3(0.0f, m.mRootY, 0.0f));

    World* world = GetWorld(0);
    const bool hadMainCamera = (world != nullptr && world->GetMainCamera() != nullptr);

    am->BeginActionGroup("Create Character");
    // Deep clone: script node refs are remapped to the clone's own children.
    Node* spawned = am->EXE_SpawnNode(rig.Get());
    if (spawned == nullptr)
    {
        am->EndActionGroup();
        outError = "Failed to spawn the character into the scene.";
        return nullptr;
    }

    world = GetWorld(0); // EXE_SpawnNode may have opened a scene
    Node* target = parent ? parent : (world ? world->GetRootNode() : nullptr);
    if (target != nullptr && target != spawned)
    {
        am->EXE_AttachNode(spawned, target, -1, -1);
    }
    else if (world != nullptr)
    {
        world->SetRootNode(spawned);
    }
    am->EndActionGroup();

    GetEditorState()->SetSelectedNode(spawned);

    if (hadMainCamera && spec.mMainCamera)
    {
        LogWarning("Scene already had a main camera; '%s' also flags its camera as Main. The first one found wins at Play.",
                   spec.mName.c_str());
    }
    return spawned;
}

AssetStub* SaveRigAsScene(const CharacterRigSpec& spec, AssetDir* dir, std::string& outError)
{
    outError.clear();
    if (dir == nullptr)
    {
        outError = "No output directory.";
        return nullptr;
    }
    if (dir->mEngineDir)
    {
        outError = "Cannot write into the engine's asset directory.";
        return nullptr;
    }

    // Build first so a script failure never leaves an empty .oct behind.
    SharedPtr<Node3D> rig = BuildRig(spec, outError);
    if (!rig)
    {
        return nullptr;
    }
    rig->SetPosition(glm::vec3(0.0f, ComputeMetrics(spec).mRootY, 0.0f));

    // Already Create()'d and saved once by CreateAndRegisterAsset; never
    // call Create() on it again.
    AssetStub* stub = EditorAddUniqueAsset(spec.mName.c_str(), dir, Scene::GetStaticType(), true);
    if (stub == nullptr || stub->mAsset == nullptr)
    {
        outError = "Failed to create the Scene asset in " + dir->mPath;
        return nullptr;
    }

    Scene* scene = stub->mAsset->As<Scene>();
    if (scene == nullptr)
    {
        outError = "Asset '" + stub->mName + "' is not a Scene.";
        return nullptr;
    }
    scene->Capture(rig.Get());
    AssetManager::Get()->SaveAsset(*stub);
    return stub;
}

std::string GetProjectAssetRoot()
{
    AssetManager* am = AssetManager::Get();
    AssetDir* projDir = am ? am->FindProjectDirectory() : nullptr;
    if (projDir == nullptr)
    {
        return std::string();
    }
    std::string root = NormalizeSlashes(projDir->mPath);
    if (!root.empty() && root.back() != '/')
    {
        root += '/';
    }
    return root;
}

bool ToProjectRelativeAssetPath(const std::string& path, std::string& outRel)
{
    outRel.clear();
    std::string root = GetProjectAssetRoot();
    if (root.empty())
    {
        return false;
    }
    std::string p = NormalizeSlashes(path);
    if (!p.empty() && p.back() != '/')
    {
        p += '/';
    }
    if (!PrefixEqualsNoCase(p, root))
    {
        return false;
    }
    std::string rel = p.substr(root.size());
    while (!rel.empty() && rel.back() == '/') rel.pop_back();
    while (!rel.empty() && rel.front() == '/') rel.erase(rel.begin());
    outRel = rel;
    return true;
}

AssetDir* ResolveOrCreateOutputDir(const std::string& userPath, std::string& outError)
{
    outError.clear();
    AssetManager* am = AssetManager::Get();
    AssetDir* projDir = am ? am->FindProjectDirectory() : nullptr;
    if (projDir == nullptr)
    {
        outError = "No project is open.";
        return nullptr;
    }
    const std::string root = GetProjectAssetRoot();

    std::string p = NormalizeSlashes(userPath);
    while (!p.empty() && std::isspace((unsigned char)p.front())) p.erase(p.begin());
    while (!p.empty() && std::isspace((unsigned char)p.back())) p.pop_back();
    if (p.compare(0, 2, "./") == 0) p.erase(0, 2);

    const bool absolute = (!p.empty() && p[0] == '/') || (p.size() > 1 && p[1] == ':');
    std::string abs = absolute ? p : root + p;

    std::string rel;
    if (!ToProjectRelativeAssetPath(abs, rel))
    {
        outError = "Output directory must be inside the project's asset folder (" + root + ").";
        return nullptr;
    }
    if (rel.empty())
    {
        return projDir;
    }

    if (!CreateDirectoryRecursive(root + rel))
    {
        outError = "Could not create directory " + root + rel;
        return nullptr;
    }

    // AssetDir::CreateSubdirectory only adds the in-memory node; the disk
    // folder above already exists.
    AssetDir* cur = projDir;
    size_t start = 0;
    while (start < rel.size())
    {
        size_t end = rel.find('/', start);
        if (end == std::string::npos) end = rel.size();
        std::string part = rel.substr(start, end - start);
        start = end + 1;
        if (part.empty()) continue;

        AssetDir* next = cur->GetSubdirectory(part);
        if (next == nullptr)
        {
            next = cur->CreateSubdirectory(part);
        }
        if (next == nullptr)
        {
            outError = "Could not register asset directory " + part;
            return nullptr;
        }
        cur = next;
    }
    return cur;
}

} // namespace CharacterRigBuilder

#endif // EDITOR
