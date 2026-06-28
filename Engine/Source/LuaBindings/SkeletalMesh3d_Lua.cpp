#include "LuaBindings/SkeletalMesh3d_Lua.h"
#include "LuaBindings/Mesh3d_Lua.h"
#include "LuaBindings/Asset_Lua.h"
#include "LuaBindings/Material_Lua.h"
#include "LuaBindings/SkeletalMesh_Lua.h"
#include "LuaBindings/Vector_Lua.h"

#include "Asset.h"
#include "AssetManager.h"
#include "Assets/SkeletalMesh.h"
#include "Assets/BoneMaskAsset.h"

int SkeletalMesh3D_Lua::SetSkeletalMesh(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    SkeletalMesh* skMesh = nullptr;
    if (!lua_isnil(L, 2))
    {
        skMesh = CHECK_SKELETAL_MESH(L, 2);
    }

    comp->SetSkeletalMesh(skMesh);

    return 0;
}

int SkeletalMesh3D_Lua::GetSkeletalMesh(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);

    SkeletalMesh* skMesh = comp->GetSkeletalMesh();

    Asset_Lua::Create(L, skMesh);
    return 1;
}

int SkeletalMesh3D_Lua::PlayAnimation(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* animName = CHECK_STRING(L, 2);
    bool loop = false;
    float speed = 1.0f;
    float weight = 1.0f;
    int32_t slot = -1;

    if (lua_isinteger(L, 3))
    {
        // New signature, slot is the next parameter
        if (!lua_isnone(L, 3)) { slot = CHECK_INTEGER(L, 3); }
        if (!lua_isnone(L, 4)) { loop = CHECK_BOOLEAN(L, 4); }
        if (!lua_isnone(L, 5)) { speed = CHECK_NUMBER(L, 5); }
        if (!lua_isnone(L, 6)) { weight = CHECK_NUMBER(L, 6); }
    }
    else if (lua_isboolean(L, 3))
    {
        // Old signature, kept for backwards compatibility
        if (!lua_isnone(L, 3)) { loop = CHECK_BOOLEAN(L, 3); }
        if (!lua_isnone(L, 4)) { speed = CHECK_NUMBER(L, 4); }
        if (!lua_isnone(L, 5)) { weight = CHECK_NUMBER(L, 5); }
        if (!lua_isnone(L, 6)) { slot = CHECK_INTEGER(L, 6); }
    }

    comp->PlayAnimation(animName, loop, speed, weight, slot);

    return 0;
}

int SkeletalMesh3D_Lua::PlayAnimationMasked(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* animName = CHECK_STRING(L, 2);
    int32_t slot = CHECK_INTEGER(L, 3);

    BoneMaskAsset* mask = nullptr;
    if (!lua_isnil(L, 4))
    {
        Asset* asset = CHECK_ASSET(L, 4);
        mask = asset ? asset->As<BoneMaskAsset>() : nullptr;
    }

    bool loop = false;
    float speed = 1.0f;
    float weight = 1.0f;
    if (!lua_isnone(L, 5)) { loop = CHECK_BOOLEAN(L, 5); }
    if (!lua_isnone(L, 6)) { speed = CHECK_NUMBER(L, 6); }
    if (!lua_isnone(L, 7)) { weight = CHECK_NUMBER(L, 7); }

    comp->PlayAnimationMasked(animName, slot, mask, loop, speed, weight);

    return 0;
}

int SkeletalMesh3D_Lua::PlayAnimationAdditive(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* animName = CHECK_STRING(L, 2);
    int32_t slot = CHECK_INTEGER(L, 3);

    BoneMaskAsset* mask = nullptr;
    if (!lua_isnil(L, 4))
    {
        Asset* asset = CHECK_ASSET(L, 4);
        mask = asset ? asset->As<BoneMaskAsset>() : nullptr;
    }

    bool loop = false;
    float speed = 1.0f;
    float weight = 1.0f;
    if (!lua_isnone(L, 5)) { loop = CHECK_BOOLEAN(L, 5); }
    if (!lua_isnone(L, 6)) { speed = CHECK_NUMBER(L, 6); }
    if (!lua_isnone(L, 7)) { weight = CHECK_NUMBER(L, 7); }

    comp->PlayAnimationAdditive(animName, slot, mask, loop, speed, weight);
    return 0;
}

int SkeletalMesh3D_Lua::SetSlotLayerMode(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    int32_t slot = CHECK_INTEGER(L, 2);
    int32_t modeInt = CHECK_INTEGER(L, 3);

    AnimLayerMode mode = (modeInt == (int32_t)AnimLayerMode::Additive)
        ? AnimLayerMode::Additive
        : AnimLayerMode::Replace;

    comp->SetSlotLayerMode(slot, mode);
    return 0;
}

int SkeletalMesh3D_Lua::SetSlotMask(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    int32_t slot = CHECK_INTEGER(L, 2);

    BoneMaskAsset* mask = nullptr;
    if (!lua_isnil(L, 3))
    {
        Asset* asset = CHECK_ASSET(L, 3);
        mask = asset ? asset->As<BoneMaskAsset>() : nullptr;
    }

    comp->SetSlotMask(slot, mask);
    return 0;
}

int SkeletalMesh3D_Lua::SetSlotWeight(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    int32_t slot = CHECK_INTEGER(L, 2);
    float weight = CHECK_NUMBER(L, 3);

    comp->SetSlotWeight(slot, weight);
    return 0;
}

int SkeletalMesh3D_Lua::FadeSlotWeight(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    int32_t slot = CHECK_INTEGER(L, 2);
    float target = CHECK_NUMBER(L, 3);
    float seconds = CHECK_NUMBER(L, 4);

    comp->FadeSlotWeight(slot, target, seconds);
    return 0;
}

int SkeletalMesh3D_Lua::StopAnimation(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* animName = CHECK_STRING(L, 2);
    bool cancelQueued = false;

    if (!lua_isnone(L, 3)) { cancelQueued = CHECK_BOOLEAN(L, 3); }

    comp->StopAnimation(animName, cancelQueued);

    return 0;
}

int SkeletalMesh3D_Lua::StopAllAnimations(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    bool cancelQueued = false;

    if (!lua_isnone(L, 2)) { cancelQueued = CHECK_BOOLEAN(L, 2); }

    comp->StopAllAnimations(cancelQueued);

    return 0;
}

int SkeletalMesh3D_Lua::IsAnimationPlaying(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* animName = CHECK_STRING(L, 2);

    bool ret = comp->IsAnimationPlaying(animName);

    lua_pushboolean(L, ret);
    return 1;
}

int SkeletalMesh3D_Lua::QueueAnimation(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* animName = CHECK_STRING(L, 2);
    bool loop = false;
    const char* dependentAnimName = nullptr;

    float speed = 1.0f;
    float weight = 1.0f;
    int32_t slot = -1;


    if (lua_isstring(L, 3))
    {
        // New signature.
        if (!lua_isnone(L, 3) && !lua_isnil(L, 3)) { dependentAnimName = CHECK_STRING(L, 3); }
        if (!lua_isnone(L, 4)) { slot = CHECK_INTEGER(L, 4); }
        if (!lua_isnone(L, 5)) { loop = CHECK_BOOLEAN(L, 5) };
        if (!lua_isnone(L, 6)) { speed = CHECK_NUMBER(L, 6); }
        if (!lua_isnone(L, 7)) { weight = CHECK_NUMBER(L, 7); }

    }
    else if (lua_isboolean(L, 3))
    {
        // Old signature, kept for backwards compatibility.
        if (!lua_isnone(L, 3)) { loop = CHECK_BOOLEAN(L, 3) };
        if (!lua_isnone(L, 4) && !lua_isnil(L, 4)) { dependentAnimName = CHECK_STRING(L, 4); }
        if (!lua_isnone(L, 5)) { speed = CHECK_NUMBER(L, 5); }
        if (!lua_isnone(L, 6)) { weight = CHECK_NUMBER(L, 6); }
        if (!lua_isnone(L, 7)) { slot = CHECK_INTEGER(L, 7); }
    }

    comp->QueueAnimation(animName, loop, dependentAnimName, speed, weight, slot);

    return 0;
}

int SkeletalMesh3D_Lua::CancelQueuedAnimation(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* animName = CHECK_STRING(L, 2);

    comp->CancelQueuedAnimation(animName);

    return 0;
}

int SkeletalMesh3D_Lua::CancelAllQueuedAnimations(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);

    comp->CancelAllQueuedAnimations();

    return 0;
}

int SkeletalMesh3D_Lua::SetInheritPose(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    bool value = CHECK_BOOLEAN(L, 2);

    comp->SetInheritPose(value);

    return 0;
}

int SkeletalMesh3D_Lua::IsInheritPoseEnabled(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);

    bool ret = comp->IsInheritPoseEnabled();

    lua_pushboolean(L, ret);
    return 1;
}

int SkeletalMesh3D_Lua::ResetAnimation(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);

    comp->ResetAnimation();

    return 0;
}

int SkeletalMesh3D_Lua::GetAnimationSpeed(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);

    float ret = comp->GetAnimationSpeed();

    lua_pushnumber(L, ret);
    return 1;
}

int SkeletalMesh3D_Lua::SetAnimationSpeed(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    float speed = CHECK_NUMBER(L, 2);

    comp->SetAnimationSpeed(speed);

    return 0;
}

int SkeletalMesh3D_Lua::SetAnimationPaused(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    bool value = CHECK_BOOLEAN(L, 2);

    comp->SetAnimationPaused(value);

    return 0;
}

int SkeletalMesh3D_Lua::IsAnimationPaused(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);

    bool ret = comp->IsAnimationPaused();

    lua_pushboolean(L, ret);
    return 1;
}

int SkeletalMesh3D_Lua::GetBonePosition(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* boneName = CHECK_STRING(L, 2);

    glm::vec3 ret = comp->GetBonePosition(boneName);

    Vector_Lua::Create(L, ret);
    return 1;
}

int SkeletalMesh3D_Lua::GetBoneRotation(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* boneName = CHECK_STRING(L, 2);

    glm::vec3 ret = comp->GetBoneRotationEuler(boneName);

    Vector_Lua::Create(L, ret);
    return 1;
}

int SkeletalMesh3D_Lua::GetBoneScale(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* boneName = CHECK_STRING(L, 2);

    glm::vec3 ret = comp->GetBoneScale(boneName);

    Vector_Lua::Create(L, ret);
    return 1;
}

int SkeletalMesh3D_Lua::SetBonePosition(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* boneName = CHECK_STRING(L, 2);
    glm::vec3 value = CHECK_VECTOR(L, 3);

    int32_t boneIndex = comp->FindBoneIndex(boneName);
    comp->SetBonePosition(boneIndex, value);

    return 0;
}

int SkeletalMesh3D_Lua::SetBoneRotation(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* boneName = CHECK_STRING(L, 2);
    glm::vec3 value = CHECK_VECTOR(L, 3);

    int32_t boneIndex = comp->FindBoneIndex(boneName);
    comp->SetBoneRotation(boneIndex, value);

    return 0;
}

int SkeletalMesh3D_Lua::SetBoneScale(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* boneName = CHECK_STRING(L, 2);
    glm::vec3 value = CHECK_VECTOR(L, 3);

    int32_t boneIndex = comp->FindBoneIndex(boneName);
    comp->SetBoneScale(boneIndex, value);

    return 0;
}

int SkeletalMesh3D_Lua::GetNumBones(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);

    uint32_t numBones = comp->GetNumBones();

    lua_pushinteger(L, (int)numBones);
    return 1;
}

int SkeletalMesh3D_Lua::SetAnimEventHandler(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    CHECK_FUNCTION(L, 2);
    ScriptFunc func(L, 2);

    comp->SetScriptAnimEventHandler(func);

    return 0;
}

int SkeletalMesh3D_Lua::SetBoundsRadiusOverride(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    float radius = CHECK_NUMBER(L, 2);

    comp->SetBoundsRadiusOverride(radius);

    return 0;
}

int SkeletalMesh3D_Lua::GetBoundsRadiusOverride(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);

    float radius = comp->GetBoundsRadiusOverride();

    lua_pushnumber(L, radius);
    return 1;
}

int SkeletalMesh3D_Lua::GetNumMaterialSlots(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    lua_pushinteger(L, (int)comp->GetNumMaterialSlots());
    return 1;
}

int SkeletalMesh3D_Lua::GetMaterialSlot(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    int32_t slot = -1;

    if (lua_isinteger(L, 2))
    {
        slot = CHECK_INDEX(L, 2);
    }
    else if (lua_isstring(L, 2))
    {
        // CHECK_STRING is a two-statement macro — assign to a local first
        // so the semicolon doesn't land inside FindMaterialSlot's arg list.
        const char* slotName = CHECK_STRING(L, 2);
        slot = comp->FindMaterialSlot(slotName);
    }

    Material* mat = nullptr;
    if (slot >= 0)
    {
        mat = comp->GetMaterialSlot(uint32_t(slot));
    }

    Asset_Lua::Create(L, mat);
    return 1;
}

int SkeletalMesh3D_Lua::SetMaterialSlot(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    int32_t slot = -1;

    if (lua_isinteger(L, 2))
    {
        slot = CHECK_INDEX(L, 2);
    }
    else if (lua_isstring(L, 2))
    {
        // CHECK_STRING is a two-statement macro — assign to a local first
        // so the semicolon doesn't land inside FindMaterialSlot's arg list.
        const char* slotName = CHECK_STRING(L, 2);
        slot = comp->FindMaterialSlot(slotName);
    }

    Material* material = nullptr;
    if (!lua_isnil(L, 3)) { material = CHECK_MATERIAL(L, 3); }

    if (slot >= 0)
    {
        comp->SetMaterialSlot(uint32_t(slot), material);
    }
    return 0;
}

int SkeletalMesh3D_Lua::FindMaterialSlot(lua_State* L)
{
    SkeletalMesh3D* comp = CHECK_SKELETAL_MESH_3D(L, 1);
    const char* name = CHECK_STRING(L, 2);

    lua_pushinteger(L, 1 + comp->FindMaterialSlot(name));
    return 1;
}

void SkeletalMesh3D_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        SKELETAL_MESH_3D_LUA_NAME,
        SKELETAL_MESH_3D_LUA_FLAG,
        MESH_3D_LUA_NAME);

    Node_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, SetSkeletalMesh);

    REGISTER_TABLE_FUNC(L, mtIndex, GetSkeletalMesh);

    REGISTER_TABLE_FUNC(L, mtIndex, PlayAnimation);

    REGISTER_TABLE_FUNC(L, mtIndex, PlayAnimationMasked);

    REGISTER_TABLE_FUNC(L, mtIndex, PlayAnimationAdditive);

    REGISTER_TABLE_FUNC(L, mtIndex, SetSlotMask);

    REGISTER_TABLE_FUNC(L, mtIndex, SetSlotWeight);

    REGISTER_TABLE_FUNC(L, mtIndex, SetSlotLayerMode);

    REGISTER_TABLE_FUNC(L, mtIndex, FadeSlotWeight);

    REGISTER_TABLE_FUNC(L, mtIndex, StopAnimation);

    REGISTER_TABLE_FUNC(L, mtIndex, StopAllAnimations);

    REGISTER_TABLE_FUNC(L, mtIndex, IsAnimationPlaying);

    REGISTER_TABLE_FUNC(L, mtIndex, QueueAnimation);

    REGISTER_TABLE_FUNC(L, mtIndex, CancelQueuedAnimation);

    REGISTER_TABLE_FUNC(L, mtIndex, CancelAllQueuedAnimations);

    REGISTER_TABLE_FUNC(L, mtIndex, SetInheritPose);

    REGISTER_TABLE_FUNC(L, mtIndex, IsInheritPoseEnabled);

    REGISTER_TABLE_FUNC(L, mtIndex, ResetAnimation);

    REGISTER_TABLE_FUNC(L, mtIndex, GetAnimationSpeed);

    REGISTER_TABLE_FUNC(L, mtIndex, SetAnimationSpeed);

    REGISTER_TABLE_FUNC(L, mtIndex, SetAnimationPaused);

    REGISTER_TABLE_FUNC(L, mtIndex, IsAnimationPaused);

    REGISTER_TABLE_FUNC(L, mtIndex, GetBonePosition);

    REGISTER_TABLE_FUNC(L, mtIndex, GetBoneRotation);

    REGISTER_TABLE_FUNC(L, mtIndex, GetBoneScale);

    REGISTER_TABLE_FUNC(L, mtIndex, SetBonePosition);

    REGISTER_TABLE_FUNC(L, mtIndex, SetBoneRotation);

    REGISTER_TABLE_FUNC(L, mtIndex, SetBoneScale);

    REGISTER_TABLE_FUNC(L, mtIndex, GetNumBones);

    REGISTER_TABLE_FUNC(L, mtIndex, SetAnimEventHandler);

    REGISTER_TABLE_FUNC(L, mtIndex, SetBoundsRadiusOverride);

    REGISTER_TABLE_FUNC(L, mtIndex, GetBoundsRadiusOverride);

    REGISTER_TABLE_FUNC(L, mtIndex, GetNumMaterialSlots);

    REGISTER_TABLE_FUNC(L, mtIndex, GetMaterialSlot);

    REGISTER_TABLE_FUNC(L, mtIndex, SetMaterialSlot);

    REGISTER_TABLE_FUNC(L, mtIndex, FindMaterialSlot);

    lua_pop(L, 1);

    // Global enum table: AnimLayerMode.Replace / AnimLayerMode.Additive
    lua_newtable(L);
    int modeTbl = lua_gettop(L);
    lua_pushinteger(L, (int)AnimLayerMode::Replace);
    lua_setfield(L, modeTbl, "Replace");
    lua_pushinteger(L, (int)AnimLayerMode::Additive);
    lua_setfield(L, modeTbl, "Additive");
    lua_setglobal(L, "AnimLayerMode");

    OCT_ASSERT(lua_gettop(L) == 0);
}
