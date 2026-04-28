#pragma once

#include "PolyphaseAPI.h"
#include "EngineTypes.h"
#include "ScriptFunc.h"
#include "Easing.h"

#include <glm/glm.hpp>
#include <vector>

enum class TweenTarget : uint8_t
{
    None,
    Position,
    Rotation,
    Scale,
    Color
};

struct TweenData
{
    int32_t id = 0;
    easing_functions easingType = EaseLinear;
    glm::vec4 startVal = glm::vec4(0.0f);
    glm::vec4 endVal = glm::vec4(0.0f);
    float duration = 1.0f;
    float elapsed = 0.0f;
    bool paused = false;
    bool finished = false;
    bool isScalar = false;
    uint8_t components = 4; // 1 for scalar, 3 for vec3, 4 for vec4/color/quat
    ScriptFunc onUpdate;
    ScriptFunc onComplete;
    Node* targetNode = nullptr;
    TweenTarget target = TweenTarget::None;
};

class POLYPHASE_API TweenManager
{
public:
    static TweenManager* Get();
    static void Create();
    static void Destroy();

    void Update(float deltaTime);

    int32_t CreateTween(const TweenData& data);
    void CancelTween(int32_t id);
    void CancelAllTweens();
    void PauseTween(int32_t id);
    void ResumeTween(int32_t id);

private:
    TweenData* FindTween(int32_t id);
    void ApplyToNode(TweenData& tween, const glm::vec4& value);

    std::vector<TweenData> mTweens;
    int32_t mNextId = 1;

    static TweenManager* sInstance;
};

TweenManager* GetTweenManager();
