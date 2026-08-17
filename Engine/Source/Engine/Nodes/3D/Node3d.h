#pragma once

#include "PolyphaseAPI.h"
#include "Nodes/Node.h"
#include "EngineTypes.h"

#include "Maths.h"

#include "AssetRef.h"

class SkeletalMesh3D;

class POLYPHASE_API Node3D : public Node
{
public:

    DECLARE_NODE(Node3D, Node);

    Node3D();
    virtual ~Node3D();

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Tick(float deltaTime) override;

    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual void GatherReplicatedData(std::vector<NetDatum>& outData);

    virtual bool IsNode3D() const override;

    void AttachToBone(SkeletalMesh3D* parent, const char* boneName, bool keepWorldTransform = false, int32_t childIndex = -1);
    void AttachToBone(SkeletalMesh3D* parent, int32_t boneIndex, bool keepWorldTransform = false, int32_t childIndex = -1);

    // Attach to a named socket (or, if no socket by that name exists, a bone of
    // that name) on `parent`. Use this for the initial attach; once attached,
    // prefer SetAttachSocket() to move between sockets on the SAME parent -- it
    // avoids the RemoveChild/AddChild round trip that reorders siblings.
    void AttachToSocket(SkeletalMesh3D* parent, const char* socketName, bool keepWorldTransform = false, int32_t childIndex = -1);
    void SetAttachSocket(const char* socketName);
    const std::string& GetAttachSocket() const;

    void MarkTransformDirty();
    bool IsTransformDirty() const;
    virtual void UpdateTransform(bool updateChildren);

    virtual bool CheckNetRelevance(Node* playerNode) override;

    virtual void GatherProxyDraws(std::vector<DebugDraw>& inoutDraws);

#if EDITOR
    virtual void OnDrawGizmos();
    virtual void OnDrawGizmosSelected();
    virtual bool DrawCustomProperty(Property& prop) override;
#endif

    glm::vec3 GetPosition() const;
    glm::vec3 GetRotationEuler() const;
    glm::quat GetRotationQuat() const;
    glm::vec3 GetScale() const;

    glm::vec3& GetPositionRef();
    glm::vec3& GetRotationEulerRef();
    glm::quat& GetRotationQuatRef();
    glm::vec3& GetScaleRef();

    const glm::mat4& GetTransform();

    void SetPosition(glm::vec3 position);
    void SetRotation(glm::vec3 rotation);
    void SetRotation(glm::quat quat);
    void SetScale(glm::vec3 scale);
    virtual void SetTransform(const glm::mat4& transform);

    glm::vec3 GetWorldPosition();
    glm::vec3 GetWorldRotationEuler();
    glm::quat GetWorldRotationQuat();
    glm::vec3 GetWorldScale();

    void SetWorldPosition(glm::vec3 position);
    void SetWorldRotation(glm::vec3 rotation);
    void SetWorldRotation(glm::quat rotation);
    void SetWorldScale(glm::vec3 scale);

    void AddRotation(glm::quat rotation);
    void AddRotation(glm::vec3 rotation);
    void AddWorldRotation(glm::quat rotation);
    void AddWorldRotation(glm::vec3 rotation);
    void RotateAround(glm::vec3 pivot, glm::vec3 axis, float degrees);

    void LookAt(glm::vec3 target, glm::vec3 up);

    // World-space bounding box of THIS node's own geometry. Node3D has no
    // geometry of its own, so the base implementation returns a degenerate
    // box at the node's world origin. Primitive3D overrides this.
    virtual AABB GetAABB() const;

    // This node unioned with every Node3D descendant's GetAABB(). Useful for
    // framing, selection and group operations.
    AABB GetHierarchyAABB(bool includeSelf = true) const;

    glm::vec3 GetCachedEulerRotation() const;

    glm::vec3 GetForwardVector();
    glm::vec3 GetRightVector();
    glm::vec3 GetUpVector();

    glm::mat4 GetParentTransform();
    int32_t GetParentBoneIndex() const;

    bool GetInheritTransform() const;
    void SetInheritTransform(bool inheritTransform);

    virtual void Attach(Node* parent, bool keepWorldTransform = false, int32_t index = -1) override;

    static bool OnRep_RootPosition(Datum* datum, uint32_t index, const void* newValue);
    static bool OnRep_RootRotation(Datum* datum, uint32_t index, const void* newValue);
    static bool OnRep_RootScale(Datum* datum, uint32_t index, const void* newValue);

protected:

    virtual void SetParent(Node* parent) override;

    glm::vec3 mPosition;
    glm::vec3 mRotationEuler;
    glm::vec3 mScale;
    
    glm::quat mRotationQuat;
    
    glm::mat4 mTransform;

    // Bone/socket attachment. mAttachSocket is the authoritative, serialized
    // value -- it names either a socket or a bare bone on the parent skeletal
    // mesh. The two indices are a lazily-resolved cache, rebuilt on demand in
    // ResolveAttachSocket() and invalidated whenever the name or parent changes.
    // Resolving by name (rather than storing an index) means a mesh reimport
    // that reorders bones can't silently move attached props.
    std::string mAttachSocket;
    int32_t mParentBoneIndex;
    int32_t mAttachSocketIndex = -1;
    bool mAttachResolved = false;

    bool mInheritTransform = true;

    bool mTransformDirty;

private:

    // Resolves mAttachSocket against the parent skeletal mesh into
    // mParentBoneIndex / mAttachSocketIndex. Cheap no-op once resolved.
    void ResolveAttachSocket();
    void InvalidateAttachSocket();
};