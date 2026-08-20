#include "Nodes/3D/Node3d.h"

#include "AssetManager.h"
#include "Nodes/Node.h"
#include "World.h"
#include "Renderer.h"
#include "Maths.h"
#include "NetworkManager.h"
#include "Assets/SkeletalMesh.h"
#include "Script.h"

#include "Nodes/3D/SkeletalMesh3d.h"

#if EDITOR
#include "Gizmos.h"
#include "ActionManager.h"
#include "imgui.h"
#endif

FORCE_LINK_DEF(Node3D);
DEFINE_NODE(Node3D, Node);

bool HandleTransformPropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    OCT_ASSERT(prop != nullptr);
    Node3D* transformComp = static_cast<Node3D*>(prop->mOwner);
    bool success = false;

    if (prop->mName == "Rotation")
    {
        transformComp->SetRotation(*((glm::vec3*)(newValue)));
        success = true;
    }
    else if (prop->mName == "Position")
    {
        transformComp->SetPosition(*((glm::vec3*)(newValue)));
        success = true;
    }
    else if (prop->mName == "Scale")
    {
        transformComp->SetScale(*((glm::vec3*)(newValue)));
        success = true;
    }

    transformComp->MarkTransformDirty();

    return success;
}

bool HandleAttachSocketPropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    OCT_ASSERT(prop != nullptr);
    Node3D* node3d = static_cast<Node3D*>(prop->mOwner);

    // Returning true tells Datum we've written the value ourselves -- which we
    // have, via SetAttachSocket(), so the resolve cache gets invalidated and the
    // transform re-dirtied. Letting Datum blind-write the string would leave a
    // stale bone index behind.
    node3d->SetAttachSocket(((const std::string*)newValue)->c_str());

    return true;
}

bool Node3D::OnRep_RootPosition(Datum* datum, uint32_t index, const void* newValue)
{
    Node3D* node3d = (Node3D*)datum->mOwner;
    OCT_ASSERT(node3d != nullptr);

    glm::vec3* newPos = (glm::vec3*) newValue;
    node3d->SetPosition(*newPos);

    return true;
}

bool Node3D::OnRep_RootRotation(Datum* datum, uint32_t index, const void* newValue)
{
    Node3D* node3d = (Node3D*)datum->mOwner;
    OCT_ASSERT(node3d != nullptr);

    glm::vec3* newRot = (glm::vec3*) newValue;
    node3d->SetRotation(*newRot);

    return true;
}

bool Node3D::OnRep_RootScale(Datum* datum, uint32_t index, const void* newValue)
{
    Node3D* node3d = (Node3D*)datum->mOwner;
    OCT_ASSERT(node3d != nullptr);

    glm::vec3* newScale = (glm::vec3*) newValue;
    node3d->SetScale(*newScale);

    return true;
}

Node3D::Node3D() :
    mPosition(0,0,0),
    mRotationEuler(0,0,0),
    mScale(1,1,1),
    mRotationQuat({0, 0, 0}),
    mTransform(1.0f),
    mParentBoneIndex(-1),
    mInheritTransform(true),
    mTransformDirty(true)
{
    mName = "Transform";
}

Node3D::~Node3D()
{

}

void Node3D::Create()
{
    Node::Create();
}

void Node3D::Destroy()
{
#if DEBUG_DRAW_ENABLED
    Renderer* renderer = Renderer::Get();
    if (renderer)
    {
        Renderer::Get()->RemoveDebugDrawsForNode(this);
    }
#endif

    Node::Destroy();
}

void Node3D::Tick(float deltaTime)
{
    Node::Tick(deltaTime);
}

const char* Node3D::GetTypeName() const
{
    return "Node3D";
}

void Node3D::GatherProperties(std::vector<Property>& outProps)
{
    Node::GatherProperties(outProps);

    SCOPED_CATEGORY("3D");

    outProps.push_back(Property(DatumType::Vector, "Position", this, &mPosition, 1, HandleTransformPropChange));
    outProps.push_back(Property(DatumType::Vector, "Rotation", this, &mRotationEuler, 1, HandleTransformPropChange));
    outProps.push_back(Property(DatumType::Vector, "Scale", this, &mScale, 1, HandleTransformPropChange));
    outProps.push_back(Property(DatumType::Bool, "Inherit Transform", this, &mInheritTransform));
    outProps.push_back(Property(DatumType::String, "Attach Socket", this, &mAttachSocket, 1, HandleAttachSocketPropChange));
}

void Node3D::GatherReplicatedData(std::vector<NetDatum>& outData)
{
    Node::GatherReplicatedData(outData);

    if (mReplicateTransform)
    {
        outData.push_back(NetDatum(DatumType::Vector, this, &mPosition, 1, OnRep_RootPosition));
        outData.push_back(NetDatum(DatumType::Vector, this, &mRotationEuler, 1, OnRep_RootRotation));
        outData.push_back(NetDatum(DatumType::Vector, this, &mScale, 1, OnRep_RootScale));
    }
}

bool Node3D::IsNode3D() const
{
    return true;
}

void Node3D::AttachToBone(SkeletalMesh3D* parent, const char* boneName, bool keepWorldTransform, int32_t childIndex)
{
    int32_t parentBoneIndex = parent->FindBoneIndex(boneName);
    AttachToBone(parent, parentBoneIndex, keepWorldTransform, childIndex);
}

void Node3D::AttachToBone(SkeletalMesh3D* parent, int32_t boneIndex, bool keepWorldTransform, int32_t childIndex)
{
    glm::mat4 origWorldTransform;
    if (keepWorldTransform)
    {
        origWorldTransform = GetTransform();
    }

    Attach(parent, keepWorldTransform, childIndex);

    mParentBoneIndex = boneIndex;
    mAttachSocketIndex = -1;

    // Record the bone NAME so the attachment round-trips through a scene save and
    // survives a mesh reimport that reorders bones. The index above stays as the
    // already-resolved cache, so this costs nothing at runtime.
    SkeletalMesh* mesh = (parent != nullptr) ? parent->GetSkeletalMesh() : nullptr;
    if (mesh != nullptr && boneIndex >= 0 && boneIndex < int32_t(mesh->GetNumBones()))
    {
        mAttachSocket = mesh->GetBone(boneIndex).mName;
    }
    else
    {
        mAttachSocket.clear();
    }

    mAttachResolved = true;

    if (keepWorldTransform)
    {
        SetTransform(origWorldTransform);
    }
}

void Node3D::AttachToSocket(SkeletalMesh3D* parent, const char* socketName, bool keepWorldTransform, int32_t childIndex)
{
    glm::mat4 origWorldTransform;
    if (keepWorldTransform)
    {
        origWorldTransform = GetTransform();
    }

    Attach(parent, keepWorldTransform, childIndex);

    mAttachSocket = (socketName != nullptr) ? socketName : "";
    InvalidateAttachSocket();

    if (keepWorldTransform)
    {
        SetTransform(origWorldTransform);
    }
}

void Node3D::SetAttachSocket(const char* socketName)
{
    const char* newName = (socketName != nullptr) ? socketName : "";

    if (mAttachSocket == newName)
    {
        return;
    }

    mAttachSocket = newName;
    InvalidateAttachSocket();
    MarkTransformDirty();
}

const std::string& Node3D::GetAttachSocket() const
{
    return mAttachSocket;
}

void Node3D::InvalidateAttachSocket()
{
    mAttachResolved = false;
    mParentBoneIndex = -1;
    mAttachSocketIndex = -1;
}

void Node3D::ResolveAttachSocket()
{
    if (mAttachResolved)
    {
        return;
    }

    mParentBoneIndex = -1;
    mAttachSocketIndex = -1;

    if (mAttachSocket.empty() || mParent == nullptr)
    {
        // Nothing to resolve, but don't latch mAttachResolved -- the parent may
        // not have had its mesh assigned yet during scene instantiation.
        mAttachResolved = mAttachSocket.empty();
        return;
    }

    SkeletalMesh3D* skComp = mParent->As<SkeletalMesh3D>();
    SkeletalMesh* mesh = (skComp != nullptr) ? skComp->GetSkeletalMesh() : nullptr;

    if (mesh == nullptr)
    {
        return;
    }

    // Sockets win over bones on a name collision -- a socket is the more
    // specific, deliberately authored thing.
    int32_t socketIndex = mesh->FindSocketIndex(mAttachSocket);

    if (socketIndex != -1)
    {
        mAttachSocketIndex = socketIndex;
        mParentBoneIndex = mesh->FindBoneIndex(mesh->GetSocket(socketIndex).mBoneName);

        if (mParentBoneIndex == -1)
        {
            LogWarning("Socket '%s' references missing bone '%s'",
                mAttachSocket.c_str(), mesh->GetSocket(socketIndex).mBoneName.c_str());
        }
    }
    else
    {
        mParentBoneIndex = mesh->FindBoneIndex(mAttachSocket);

        if (mParentBoneIndex == -1)
        {
            LogWarning("Node '%s' is attached to '%s', which is not a socket or bone on the parent mesh",
                GetName().c_str(), mAttachSocket.c_str());
        }
    }

    mAttachResolved = true;
}

void Node3D::MarkTransformDirty()
{
    mTransformDirty = true;

    // TODO-NODE: Consider propogating this to children nodes. 
    // It looks like Godot does it this way, and might remove some one-frame-delay bugs.
#if 0
    for (uint32_t i = 0; i < mChildren.size(); ++i)
    {
        if (mChildren[i]->IsNode3D())
        {
            static_cast<Node3D*>(mChildren[i])->MarkTransformDirty();
        }
    }
#endif
}

bool Node3D::IsTransformDirty() const
{
    return mTransformDirty;
}

void Node3D::UpdateTransform(bool updateChildren)
{
    // First we need to update parent transform if it's dirty.
    Node3D* parent = (mParent && mParent->IsNode3D()) ? static_cast<Node3D*>(mParent.Get()) : nullptr;

    if (parent != nullptr &&
        parent->mTransformDirty)
    {
        parent->UpdateTransform(false);
    }

    if (mTransformDirty)
    {
        // Update transform
        mTransform = glm::mat4(1);

        // Force uniform scale if the component has children.
        // Non-uniform scale was causing problems for children components because shear was 
        // getting introduced into the child transforms if the parent had any rotation.
        // Relevant Github issues:
        // https://github.com/BabylonJS/Babylon.js/issues/10579
        // https://github.com/mrdoob/three.js/issues/3845
        // https://github.com/armory3d/armory/issues/2211
        glm::vec3 scale = mScale;
        if (GetNumChildren() > 0)
        {
            scale = glm::vec3(mScale.x, mScale.x, mScale.x);
        }
        
        mTransform = glm::translate(mTransform, mPosition);
        mTransform *= glm::toMat4(mRotationQuat);
        mTransform = glm::scale(mTransform, scale);

        if (parent != nullptr && mInheritTransform)
        {
            // Concatenate parent transform with this transform
            mTransform = GetParentTransform() * mTransform;
        }

        // Recursively mark children dirty since their parent has updated.
        for (uint32_t i = 0; i < mChildren.size(); ++i)
        {
            Node3D* child3d = mChildren[i]->IsNode3D() ? static_cast<Node3D*>(mChildren[i].Get()) : nullptr;
            if (child3d)
            {
                child3d->MarkTransformDirty();
            }
        }

        // Cache off the euler angle rotation.
        mRotationEuler = GetRotationEuler();

        mTransformDirty = false;
    }

    // Recursively update child transforms.
    if (updateChildren)
    {
        for (uint32_t i = 0; i < mChildren.size(); ++i)
        {
            Node3D* child3d = mChildren[i]->IsNode3D() ? static_cast<Node3D*>(mChildren[i].Get()) : nullptr;
            if (child3d)
            {
                child3d->UpdateTransform(updateChildren);
            }
        }
    }
}

bool Node3D::CheckNetRelevance(Node* playerNode)
{
    if (mAlwaysRelevant || this == playerNode)
    {
        return true;
    }

    Node3D* player3D = playerNode->As<Node3D>();

    if (player3D)
    {
        glm::vec3 thisPos = GetWorldPosition();
        glm::vec3 playerPos = player3D->GetWorldPosition();

        float dist2 = glm::distance2(thisPos, playerPos);
        const float netRelDist2 = NetworkManager::Get()->GetRelevancyDistanceSquared();

        // TODO: Use slightly larger distance for checking irrelevance so we don't
        // spawn spawn/destroy messages for nodes just along the relevancy cusp.
        bool relevant = (dist2 < netRelDist2);
        return relevant;
    }
    else
    {
        // How do we handle the case where the player node is a 2D node, but
        // we need to check relevance for a 3D node? I say, just fallback to the 
        // parent class, which will return true
        return Node::CheckNetRelevance(playerNode);
    }
}

void Node3D::GatherProxyDraws(std::vector<DebugDraw>& inoutDraws)
{
#if DEBUG_DRAW_ENABLED

    // Bail out cleanly when SM_Cube isn't ready. Originally added because
    // BuildPhase1 packaging could call into here while the AssetManager was
    // mid-iteration; root cause turned out to be addon-DLL heap corruption
    // (fixed via NativeAddonManager force-rebuild + .meta sidecar). The
    // null-guard is cheap and remains as defense-in-depth.
    StaticMesh* cube = LoadAsset<StaticMesh>("SM_Cube");
    if (cube == nullptr)
        return;

    DebugDraw debugDraw;
    debugDraw.mMesh = cube;
    debugDraw.mNode = this;
    debugDraw.mColor = glm::vec4(1.0f, 0.25f, 0.25f, 1.0f);
    debugDraw.mTransform = glm::scale(GetTransform(), { 0.2f, 0.2f, 0.2f });
    inoutDraws.push_back(debugDraw);

#endif
}

#if EDITOR
void Node3D::OnDrawGizmos()
{
    Script* script = GetScript();
    if (script != nullptr && script->HasFunction("OnDrawGizmos"))
    {
        script->CallFunction("OnDrawGizmos");
    }
}

void Node3D::OnDrawGizmosSelected()
{
    Script* script = GetScript();
    if (script != nullptr && script->HasFunction("OnDrawGizmosSelected"))
    {
        script->CallFunction("OnDrawGizmosSelected");
    }

    // Draw this mesh's sockets as RGB axes so their authored offsets are visible
    // while placing props. Gizmos are immediate-mode and already gated on
    // selection by the renderer, so there's no lifetime bookkeeping here.
    SkeletalMesh3D* skComp = As<SkeletalMesh3D>();
    SkeletalMesh* mesh = (skComp != nullptr) ? skComp->GetSkeletalMesh() : nullptr;

    if (mesh != nullptr)
    {
        const float axisLength = 0.25f;

        for (uint32_t i = 0; i < mesh->GetNumSockets(); ++i)
        {
            glm::mat4 socketTransform = skComp->GetSocketTransform(mesh->GetSocket(i).mName);
            glm::vec3 origin = Maths::ExtractPosition(socketTransform);

            // Direction columns, normalized so socket scale doesn't change the
            // on-screen size of the marker.
            glm::vec3 axisX = Maths::SafeNormalize(glm::vec3(socketTransform[0]));
            glm::vec3 axisY = Maths::SafeNormalize(glm::vec3(socketTransform[1]));
            glm::vec3 axisZ = Maths::SafeNormalize(glm::vec3(socketTransform[2]));

            Gizmos::SetColor(glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
            Gizmos::DrawLine(origin, origin + axisX * axisLength);
            Gizmos::SetColor(glm::vec4(0.2f, 1.0f, 0.2f, 1.0f));
            Gizmos::DrawLine(origin, origin + axisY * axisLength);
            Gizmos::SetColor(glm::vec4(0.2f, 0.4f, 1.0f, 1.0f));
            Gizmos::DrawLine(origin, origin + axisZ * axisLength);
        }

        Gizmos::ResetState();
    }
}

bool Node3D::DrawCustomProperty(Property& prop)
{
    if (prop.mName != "Attach Socket")
        return false;

    // A String property gets no mEnumStrings combo path in the inspector, so the
    // socket/bone list has to be drawn here. Only meaningful under a skeletal
    // mesh parent, so hide the row entirely otherwise -- an always-present text
    // box on every Node3D would be noise.
    SkeletalMesh3D* parentSk = (mParent != nullptr) ? mParent->As<SkeletalMesh3D>() : nullptr;
    SkeletalMesh* mesh = (parentSk != nullptr) ? parentSk->GetSkeletalMesh() : nullptr;

    if (parentSk == nullptr)
    {
        return true;
    }

    ImGui::Text("Attach Socket");

    const char* preview = mAttachSocket.empty() ? "(None)" : mAttachSocket.c_str();

    // Route assignments through ActionManager so the change is undoable and
    // multi-select works. The property's change handler calls SetAttachSocket(),
    // which is what actually invalidates the resolve cache.
    std::string newSocket;
    bool changed = false;

    if (ImGui::BeginCombo("##AttachSocket", preview))
    {
        if (ImGui::Selectable("(None)", mAttachSocket.empty()))
        {
            newSocket = "";
            changed = true;
        }

        if (mesh != nullptr)
        {
            // Sockets first -- they're the intended attach targets. Bones stay
            // available for one-off placements that don't warrant a socket.
            if (mesh->GetNumSockets() > 0)
            {
                ImGui::SeparatorText("Sockets");

                for (uint32_t i = 0; i < mesh->GetNumSockets(); ++i)
                {
                    const std::string& name = mesh->GetSocket(i).mName;
                    if (ImGui::Selectable(name.c_str(), mAttachSocket == name))
                    {
                        newSocket = name;
                        changed = true;
                    }
                }
            }

            ImGui::SeparatorText("Bones");

            const std::vector<Bone>& bones = mesh->GetBones();
            for (uint32_t i = 0; i < bones.size(); ++i)
            {
                ImGui::PushID(int(i));
                if (ImGui::Selectable(bones[i].mName.c_str(), mAttachSocket == bones[i].mName))
                {
                    newSocket = bones[i].mName;
                    changed = true;
                }
                ImGui::PopID();
            }
        }

        ImGui::EndCombo();
    }

    if (changed)
    {
        ActionManager::Get()->EXE_EditProperty(this, PropertyOwnerType::Node, "Attach Socket", 0, newSocket);
    }

    return true;
}
#endif

bool Node3D::GetInheritTransform() const
{
    return mInheritTransform;
}

void Node3D::SetInheritTransform(bool inheritTransform)
{
    mInheritTransform = inheritTransform;
}

glm::vec3 Node3D::GetPosition() const
{
    return mPosition;
}

glm::vec3 Node3D::GetRotationEuler() const
{
    glm::vec3 eulerAngles = glm::eulerAngles(mRotationQuat) * RADIANS_TO_DEGREES;

    eulerAngles = EnforceEulerRange(eulerAngles);

    return eulerAngles;
}

glm::quat Node3D::GetRotationQuat() const
{
    return mRotationQuat;
}

glm::vec3 Node3D::GetScale() const
{
    return mScale;
}

glm::vec3& Node3D::GetPositionRef()
{
    return mPosition;
}

glm::vec3& Node3D::GetRotationEulerRef()
{
    return mRotationEuler;
}

glm::quat& Node3D::GetRotationQuatRef()
{
    return mRotationQuat;
}

glm::vec3& Node3D::GetScaleRef()
{
    return mScale;
}

const glm::mat4& Node3D::GetTransform()
{
    // TODO-NODE: I added this update transform check and made this method non-const.
    // Is this causing any bugs? Performance issues?
    if (mTransformDirty)
    {
        UpdateTransform(false);
    }

    return mTransform;
}

void Node3D::SetPosition(glm::vec3 position)
{
    mPosition = position;
    MarkTransformDirty();
}

void Node3D::SetRotation(glm::vec3 rotation)
{
    SetRotation(glm::quat(rotation * DEGREES_TO_RADIANS));
}

void Node3D::SetRotation(glm::quat quat)
{
    mRotationQuat = glm::normalize(quat);
    MarkTransformDirty();
}

void Node3D::SetScale(glm::vec3 scale)
{
    mScale = scale;
    MarkTransformDirty();
}

void Node3D::SetTransform(const glm::mat4& transform)
{
    mTransform = transform;

    // Update the relative transforms to match the new world transform.
    SetWorldPosition(Maths::ExtractPosition(transform));
    SetWorldScale(Maths::ExtractScale(transform));
    SetWorldRotation(Maths::ExtractRotation(transform));
    mRotationEuler = GetRotationEuler();

    mTransformDirty = false;

    for (uint32_t i = 0; i < mChildren.size(); ++i)
    {
        Node3D* child3d = mChildren[i]->IsNode3D() ? static_cast<Node3D*>(mChildren[i].Get()) : nullptr;
        if (child3d)
        {
            //child3d->UpdateTransform();
            child3d->MarkTransformDirty();
        }
    }
}

glm::vec3 Node3D::GetWorldPosition()
{
    UpdateTransform(false);
    return Maths::ExtractPosition(mTransform);
}

glm::vec3 Node3D::GetWorldRotationEuler()
{
    UpdateTransform(false);

    glm::vec3 eulerAngles = glm::eulerAngles(Maths::ExtractRotation(mTransform)) * RADIANS_TO_DEGREES;

    eulerAngles = EnforceEulerRange(eulerAngles);

    return eulerAngles;
}

glm::quat Node3D::GetWorldRotationQuat()
{
    UpdateTransform(false);
    return Maths::ExtractRotation(mTransform);
}

glm::vec3 Node3D::GetWorldScale()
{
    UpdateTransform(false);
    return Maths::ExtractScale(mTransform);
}

void Node3D::SetWorldPosition(glm::vec3 position)
{
    if (mParent != nullptr && mInheritTransform)
    {
        glm::mat4 invParentTrans = glm::inverse(GetParentTransform());
        glm::vec4 position4 = glm::vec4(position, 1.0f);
        glm::vec4 relPosition4 = invParentTrans * position4;
        SetPosition(glm::vec3(relPosition4.x, relPosition4.y, relPosition4.z));
    }
    else
    {
        SetPosition(position);
    }
}

void Node3D::SetWorldRotation(glm::vec3 rotation)
{
    glm::quat quat = glm::quat(rotation * DEGREES_TO_RADIANS);
    SetWorldRotation(quat);
}

void Node3D::SetWorldRotation(glm::quat rotation)
{
    glm::quat newRelativeRot = mRotationQuat;

    // Convert the world rotation to relative rotation
    if (mParent != nullptr && mParent->IsNode3D() && mInheritTransform)
    {
        // Derive the parent basis from the same source UpdateTransform and
        // SetWorldPosition use. The previous code multiplied the parent's world
        // rotation by GetBoneRotationQuat(), but that getter already bakes in the
        // mesh node's own world matrix -- so the character's rotation was applied
        // twice. Harmless at identity rotation, wrong as soon as it turns.
        glm::quat parentWorldRot = Maths::ExtractRotation(GetParentTransform());

        newRelativeRot = glm::inverse(parentWorldRot) * rotation;
    }
    else
    {
        // If no parent, then the world rotation is the relative rotation.
        newRelativeRot = rotation;
    }

    SetRotation(newRelativeRot);
}

void Node3D::SetWorldScale(glm::vec3 scale)
{
    if (mParent != nullptr && mParent->IsNode3D() && mInheritTransform)
    {
        // Must use GetParentTransform(), not the parent's world scale. When this
        // node is bone-attached the parent basis includes the bone matrix (which
        // carries the mesh's inverse root transform -- a 100x unit conversion on
        // Mixamo rigs). Dividing by the parent node's scale alone leaves that
        // factor baked into the relative scale, and since the gizmo re-applies
        // SetTransform every frame of a drag it compounds: the node explodes.
        // For a non-bone-attached node GetParentTransform() is exactly the
        // parent's world matrix, so this matches the previous behaviour.
        glm::vec3 parentScale = Maths::ExtractScale(GetParentTransform());
        glm::vec3 relScale;
        relScale.x = (parentScale.x != 0.0f) ? scale.x / parentScale.x : 0.0f;
        relScale.y = (parentScale.y != 0.0f) ? scale.y / parentScale.y : 0.0f;
        relScale.z = (parentScale.z != 0.0f) ? scale.z / parentScale.z : 0.0f;
        SetScale(relScale);
    }
    else
    {
        SetScale(scale);
    }
}

void Node3D::AddRotation(glm::quat rotation)
{
    SetRotation(rotation * mRotationQuat);
}

void Node3D::AddRotation(glm::vec3 rotation)
{
    glm::quat rotQuat = glm::quat(rotation * DEGREES_TO_RADIANS);
    AddRotation(rotQuat);
    //SetRotation(GetRotationEuler() + rotation);
}

void Node3D::AddWorldRotation(glm::quat rotation)
{
    // Get component's world rotation first
    glm::quat newWorldRot = GetWorldRotationQuat();

    // Add the world rotation to the component's world rotation (the new world rotation)
    newWorldRot = rotation * newWorldRot;

    SetWorldRotation(newWorldRot);
}

void Node3D::AddWorldRotation(glm::vec3 rotation)
{
    glm::quat rotQuat = glm::quat(rotation);
    AddWorldRotation(rotQuat);
}

void Node3D::RotateAround(glm::vec3 pivot, glm::vec3 axis, float degrees)
{
    // Work in world space
    UpdateTransform(false);

    glm::mat4 trans = mTransform;
    trans = glm::translate(trans, pivot);
    trans = glm::rotate(trans, degrees * DEGREES_TO_RADIANS, axis);
    trans = glm::translate(trans, -pivot);

    SetTransform(trans);
}

void Node3D::LookAt(glm::vec3 target, glm::vec3 up)
{
    glm::mat4 rotMat = glm::lookAt(GetWorldPosition(), target, up);
    glm::quat rotQuat = glm::conjugate(glm::toQuat(rotMat));
    SetWorldRotation(rotQuat);
}

AABB Node3D::GetAABB() const
{
    // No geometry of our own -- a degenerate box at our world origin.
    // Reads mTransform directly because this is const, so a dirty transform
    // yields last frame's position. Same contract as Primitive3D::GetBounds().
    glm::vec3 worldPos = glm::vec3(mTransform[3]);
    return AABB(worldPos, worldPos);
}

AABB Node3D::GetHierarchyAABB(bool includeSelf) const
{
    AABB retAABB = AABB::MakeInvalid();

    if (includeSelf && IsPrimitive3D())
    {
        AABB selfAABB = GetAABB();

        // Skip LARGE_BOUNDS placeholder boxes so one unbounded primitive
        // doesn't blow the whole hierarchy out to 10000 units.
        if (!selfAABB.IsLarge())
        {
            retAABB.Encapsulate(selfAABB);
        }
    }

    for (uint32_t i = 0; i < GetNumChildren(); ++i)
    {
        Node* child = GetChild(int32_t(i));

        if (child != nullptr && child->IsNode3D())
        {
            retAABB.Encapsulate(static_cast<Node3D*>(child)->GetHierarchyAABB(true));
        }
    }

    if (!retAABB.IsValid())
    {
        // Nothing in the subtree contributed any geometry.
        glm::vec3 worldPos = glm::vec3(mTransform[3]);
        retAABB = AABB(worldPos, worldPos);
    }

    return retAABB;
}

glm::vec3 Node3D::GetCachedEulerRotation() const
{
    return mRotationEuler;
}

glm::vec3 Node3D::GetForwardVector()
{
    if (mTransformDirty)
    {
        UpdateTransform(false);
    }

    glm::vec3 forwardVector = mTransform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
    forwardVector = Maths::SafeNormalize(forwardVector);
    return forwardVector;
}

glm::vec3 Node3D::GetRightVector()
{
    if (mTransformDirty)
    {
        UpdateTransform(false);
    }

    glm::vec3 rightVector = mTransform * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    rightVector = Maths::SafeNormalize(rightVector);
    return rightVector;
}

glm::vec3 Node3D::GetUpVector()
{
    if (mTransformDirty)
    {
        UpdateTransform(false);
    }

    glm::vec3 upVector = mTransform * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    upVector = Maths::SafeNormalize(upVector);
    return upVector;
}

glm::mat4 Node3D::GetParentTransform()
{
    glm::mat4 transform(1);

    ResolveAttachSocket();

    if (mParent != nullptr && mParent->IsNode3D())
    {
        Node3D* parent3d = static_cast<Node3D*>(mParent.Get());

        // Always seed from the parent's world transform. The bone term below is
        // an additional factor, not an alternative -- falling through to identity
        // when the bone can't be resolved would teleport this node to the origin.
        transform = parent3d->GetTransform();

        if (mParentBoneIndex != -1)
        {
            // As<> rather than an exact GetType() compare so SkeletalMesh3D
            // subclasses still drive their attached children.
            SkeletalMesh3D* skComp = mParent->As<SkeletalMesh3D>();
            SkeletalMesh* mesh = (skComp != nullptr) ? skComp->GetSkeletalMesh() : nullptr;

            if (mesh != nullptr &&
                mParentBoneIndex < int32_t(skComp->GetNumBones()))
            {
                transform = transform *
                    skComp->GetBoneTransform(mParentBoneIndex) *
                    mesh->GetBone(mParentBoneIndex).mInvOffsetMatrix;

                // A named socket adds a constant offset on top of the bone.
                if (mAttachSocketIndex >= 0 &&
                    mAttachSocketIndex < int32_t(mesh->GetNumSockets()))
                {
                    transform = transform * mesh->GetSocketLocalMatrix(mAttachSocketIndex);
                }
            }
        }
    }

    return transform;
}

int32_t Node3D::GetParentBoneIndex() const
{
    return mParentBoneIndex;
}

void Node3D::Attach(Node* parent, bool keepWorldTransform, int32_t index)
{
    // Can't attach to self.
    OCT_ASSERT(parent != this);
    if (parent == this)
    {
        return;
    }

    // Lock pointer so we don't delete this node if the parent is
    // the only one maintaining a shared pointer to it.
    NodePtr lockPtr = mSelf.Lock();

    if (keepWorldTransform && IsTransformDirty())
    {
        UpdateTransform(false);
    }

    // Detach from parent first
    if (mParent != nullptr)
    {
        if (keepWorldTransform)
        {
            glm::mat4 transform = GetTransform();
            mParent->RemoveChild(this);
            SetTransform(transform);
        }
        else
        {
            mParent->RemoveChild(this);
        }
    }

    // Reparenting drops any bone/socket attachment. AttachToBone / AttachToSocket
    // re-apply theirs immediately after calling this.
    mAttachSocket.clear();
    InvalidateAttachSocket();
    mAttachResolved = true;

    // Attach to new parent
    if (parent != nullptr)
    {
        if (keepWorldTransform)
        {
            glm::mat4 transform = GetTransform();
            parent->AddChild(this, index);
            SetTransform(transform);
        }
        else
        {
            parent->AddChild(this, index);
        }
    }
}

void Node3D::SetParent(Node* parent)
{
    Node::SetParent(parent);

    // The cached bone/socket indices belong to the old parent's mesh.
    InvalidateAttachSocket();

    MarkTransformDirty();
}
