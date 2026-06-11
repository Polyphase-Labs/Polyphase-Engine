#pragma once

#include "PolyphaseAPI.h"
#include "SmartPointer.h"
#include "Datum.h"
#include "Nodes/Node.h"

#include <string>
#include <vector>

class World;

enum class LoadingState : uint8_t
{
    Idle,
    Loading,
    Closing,
};

class POLYPHASE_API LoadingMenu
{
public:

    void SetMenuScene(const std::string& sceneName);
    const std::string& GetMenuScene() const;

    bool Open(const std::string& targetSceneName, int32_t worldIndex = 0);
    void Close();
    void ForceClose();

    bool IsActive() const;
    LoadingState GetState() const;
    const std::string& GetTargetScene() const;

    void Update(float deltaTime, int32_t worldIndex);

    bool ShouldInterceptLoadScene() const;

private:

    static Datum OnLoadingFinishedSignal(Node* listener, const std::vector<Datum>& args);

    void NotifyFinished();
    std::string ResolveLoadingSceneName() const;
    void TeardownMenuRoot();

    std::string mMenuSceneOverride;
    std::string mActiveLoadingScene;
    std::string mTargetScene;
    NodePtr     mMenuRoot;
    int32_t     mWorldIndex = 0;
    LoadingState mState = LoadingState::Idle;
    float       mElapsed = 0.0f;
    bool        mFinishedSignalReceived = false;
    bool        mMinDisplayElapsedEmitted = false;
    bool        mCloseRequested = false;
    bool        mInternalLoad = false;
};

POLYPHASE_API LoadingMenu* GetLoadingMenu();
