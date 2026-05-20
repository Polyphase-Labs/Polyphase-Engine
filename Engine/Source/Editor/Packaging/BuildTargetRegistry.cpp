#if EDITOR

#include "BuildTargetRegistry.h"

#include <algorithm>
#include <cstring>

static void CopyStringField(std::string& owned, const char*& descField, const char* src)
{
    owned = (src != nullptr) ? src : "";
    descField = owned.c_str();
}

void BuildTargetRegistry::Register(HookId hookId, const PolyphaseBuildTargetDesc* desc, bool isBuiltIn)
{
    if (desc == nullptr || desc->targetId == nullptr || desc->targetId[0] == '\0')
    {
        return;
    }

    // Find existing entry by id and replace it in place — keeps iterator
    // order stable across hot-reloads so the dropdown doesn't shuffle.
    RegisteredBuildTarget* slot = nullptr;
    for (RegisteredBuildTarget& t : mTargets)
    {
        if (t.mTargetId == desc->targetId)
        {
            slot = &t;
            break;
        }
    }

    if (slot == nullptr)
    {
        mTargets.emplace_back();
        slot = &mTargets.back();
    }

    slot->mHookId    = hookId;
    slot->mIsBuiltIn = isBuiltIn;

    // Bit-copy the descriptor first (preserves function pointers, flags,
    // basePlatform, reserved fields), then re-point its const char* slots
    // at our owned strings.
    slot->mDesc = *desc;

    CopyStringField(slot->mTargetId,        slot->mDesc.targetId,        desc->targetId);
    CopyStringField(slot->mDisplayName,     slot->mDesc.displayName,     desc->displayName);
    CopyStringField(slot->mIconText,        slot->mDesc.iconText,        desc->iconText);
    CopyStringField(slot->mCategory,        slot->mDesc.category,        desc->category);
    CopyStringField(slot->mBinaryExtension, slot->mDesc.binaryExtension, desc->binaryExtension);
    CopyStringField(slot->mPlatformExtensionDir, slot->mDesc.platformExtensionDir, desc->platformExtensionDir);
}

void BuildTargetRegistry::Unregister(HookId hookId, const char* targetId)
{
    if (targetId == nullptr) return;

    mTargets.erase(std::remove_if(mTargets.begin(), mTargets.end(),
        [hookId, targetId](const RegisteredBuildTarget& t) {
            return t.mHookId == hookId && t.mTargetId == targetId;
        }), mTargets.end());
}

void BuildTargetRegistry::RemoveAllForHook(HookId hookId)
{
    mTargets.erase(std::remove_if(mTargets.begin(), mTargets.end(),
        [hookId](const RegisteredBuildTarget& t) {
            // Never reap built-ins. They're registered with HookId 0 anyway,
            // but the explicit guard means a malicious addon that registers
            // with HookId 0 still can't blow away the built-in targets.
            return !t.mIsBuiltIn && t.mHookId == hookId;
        }), mTargets.end());
}

const RegisteredBuildTarget* BuildTargetRegistry::Find(const char* targetId) const
{
    if (targetId == nullptr) return nullptr;

    for (const RegisteredBuildTarget& t : mTargets)
    {
        if (t.mTargetId == targetId)
        {
            return &t;
        }
    }
    return nullptr;
}

const RegisteredBuildTarget* BuildTargetRegistry::FindBuiltInByPlatform(Platform platform) const
{
    for (const RegisteredBuildTarget& t : mTargets)
    {
        if (t.mIsBuiltIn && t.mDesc.basePlatform == static_cast<int32_t>(platform))
        {
            return &t;
        }
    }
    return nullptr;
}

void BuildTargetRegistry::Clear()
{
    mTargets.clear();
}

#endif /* EDITOR */
