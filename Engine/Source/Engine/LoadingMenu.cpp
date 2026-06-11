#include "LoadingMenu.h"

#include "Engine.h"
#include "World.h"
#include "SignalBus.h"
#include "Log.h"
#include "AssetManager.h"
#include "Assets/Scene.h"
#include "Nodes/Node.h"
#include "Datum.h"

static LoadingMenu gLoadingMenu;

LoadingMenu* GetLoadingMenu()
{
    return &gLoadingMenu;
}

static const char* StateName(LoadingState s)
{
    switch (s)
    {
    case LoadingState::Idle:    return "Idle";
    case LoadingState::Loading: return "Loading";
    case LoadingState::Closing: return "Closing";
    }
    return "?";
}

void LoadingMenu::SetMenuScene(const std::string& sceneName)
{
    mMenuSceneOverride = sceneName;
}

const std::string& LoadingMenu::GetMenuScene() const
{
    return mMenuSceneOverride;
}

bool LoadingMenu::IsActive() const
{
    return mState != LoadingState::Idle;
}

LoadingState LoadingMenu::GetState() const
{
    return mState;
}

const std::string& LoadingMenu::GetTargetScene() const
{
    return mTargetScene;
}

std::string LoadingMenu::ResolveLoadingSceneName() const
{
    if (!mMenuSceneOverride.empty())
    {
        return mMenuSceneOverride;
    }
    return GetEngineConfig()->mDefaultLoadingScene;
}

bool LoadingMenu::ShouldInterceptLoadScene() const
{
    if (mInternalLoad)
    {
        return false;
    }
    if (mState != LoadingState::Idle)
    {
        return false;
    }
    return !ResolveLoadingSceneName().empty();
}

bool LoadingMenu::Open(const std::string& targetSceneName, int32_t worldIndex)
{
    if (mState != LoadingState::Idle)
    {
        LogWarning("LoadingMenu: ignoring Open('%s') while state=%s",
            targetSceneName.c_str(), StateName(mState));
        return false;
    }

    World* world = GetWorld(worldIndex);
    if (world == nullptr)
    {
        LogWarning("LoadingMenu: Open('%s') invalid world index %d",
            targetSceneName.c_str(), worldIndex);
        return false;
    }

    const bool headless = GetEngineConfig()->mHeadless;
    const std::string loadingSceneName = ResolveLoadingSceneName();

    if (headless || loadingSceneName.empty() || worldIndex != 0)
    {
        mInternalLoad = true;
        world->LoadScene(targetSceneName.c_str(), false);
        mInternalLoad = false;
        return false;
    }

    LogDebug("LoadingMenu: instantiating loading scene '%s'", loadingSceneName.c_str());

    Scene* scene = LoadAsset<Scene>(loadingSceneName);
    if (scene == nullptr)
    {
        LogWarning("LoadingMenu: loading scene '%s' not found; falling back to direct load",
            loadingSceneName.c_str());
        mInternalLoad = true;
        world->LoadScene(targetSceneName.c_str(), false);
        mInternalLoad = false;
        return false;
    }

    NodePtr menuRoot = scene->Instantiate();
    if (menuRoot == nullptr)
    {
        LogError("LoadingMenu: scene '%s' failed to instantiate", loadingSceneName.c_str());
        mInternalLoad = true;
        world->LoadScene(targetSceneName.c_str(), false);
        mInternalLoad = false;
        return false;
    }

    menuRoot->SetPersistent(true);

    Node* worldRoot = world->GetRootNode();
    if (worldRoot != nullptr)
    {
        worldRoot->AddChild(menuRoot.Get());
    }
    else
    {
        world->SetRootNode(menuRoot.Get());
    }

    GetSignalBus()->Subscribe("Loading.Finished", menuRoot.Get(), &LoadingMenu::OnLoadingFinishedSignal);

    mMenuRoot = menuRoot;
    mActiveLoadingScene = loadingSceneName;
    mTargetScene = targetSceneName;
    mWorldIndex = worldIndex;
    mElapsed = 0.0f;
    mFinishedSignalReceived = false;
    mMinDisplayElapsedEmitted = false;
    mCloseRequested = false;
    mState = LoadingState::Loading;

    mInternalLoad = true;
    world->LoadScene(targetSceneName.c_str(), false);
    mInternalLoad = false;

    LogDebug("LoadingMenu: opened (loading='%s', target='%s')",
        loadingSceneName.c_str(), targetSceneName.c_str());
    return true;
}

void LoadingMenu::Close()
{
    if (mState != LoadingState::Loading)
    {
        return;
    }

    mCloseRequested = true;

    const float minDisplay = GetEngineConfig()->mLoadingMinDisplaySeconds;
    if (mElapsed >= minDisplay)
    {
        mState = LoadingState::Closing;
    }
}

void LoadingMenu::ForceClose()
{
    if (mState == LoadingState::Idle)
    {
        return;
    }
    TeardownMenuRoot();
    mState = LoadingState::Idle;
}

void LoadingMenu::Update(float deltaTime, int32_t worldIndex)
{
    if (worldIndex != mWorldIndex)
    {
        return;
    }

    if (mState == LoadingState::Loading)
    {
        mElapsed += deltaTime;

        const float minDisplay = GetEngineConfig()->mLoadingMinDisplaySeconds;
        const float timeout = GetEngineConfig()->mLoadingTimeoutSeconds;

        if (!mMinDisplayElapsedEmitted && mElapsed >= minDisplay)
        {
            mMinDisplayElapsedEmitted = true;
            GetSignalBus()->Emit("Loading.MinDisplayElapsed", {});
        }

        if (timeout > 0.0f && !mFinishedSignalReceived && mElapsed > timeout)
        {
            LogWarning("LoadingMenu: target scene '%s' did not emit Loading.Finished within %.2fs; auto-closing",
                mTargetScene.c_str(), timeout);
            ForceClose();
            return;
        }

        if (mCloseRequested && mElapsed >= minDisplay)
        {
            mState = LoadingState::Closing;
        }
    }
    else if (mState == LoadingState::Closing)
    {
        TeardownMenuRoot();
        mState = LoadingState::Idle;
        LogDebug("LoadingMenu: closed");
    }
}

void LoadingMenu::TeardownMenuRoot()
{
    if (mMenuRoot != nullptr)
    {
        GetSignalBus()->Unsubscribe("Loading.Finished", mMenuRoot.Get());
        mMenuRoot->Detach();
        mMenuRoot.Reset();
    }

    mActiveLoadingScene.clear();
    mTargetScene.clear();
    mElapsed = 0.0f;
    mFinishedSignalReceived = false;
    mMinDisplayElapsedEmitted = false;
    mCloseRequested = false;
}

void LoadingMenu::NotifyFinished()
{
    if (mState != LoadingState::Loading)
    {
        return;
    }
    mFinishedSignalReceived = true;
}

Datum LoadingMenu::OnLoadingFinishedSignal(Node* /*listener*/, const std::vector<Datum>& /*args*/)
{
    GetLoadingMenu()->NotifyFinished();
    return Datum{};
}
