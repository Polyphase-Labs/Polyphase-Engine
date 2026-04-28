#include "TweenManager.h"
#include "Datum.h"
#include "Log.h"

#include "Nodes/3D/Node3d.h"
#include "Nodes/Widgets/Widget.h"

TweenManager* TweenManager::sInstance = nullptr;

TweenManager* TweenManager::Get()
{
    return sInstance;
}

void TweenManager::Create()
{
    if (sInstance == nullptr)
    {
        sInstance = new TweenManager();
    }
}

void TweenManager::Destroy()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

TweenManager* GetTweenManager()
{
    return TweenManager::Get();
}

void TweenManager::Update(float deltaTime)
{
    // Process tweens with index-based loop so newly added tweens
    // (e.g. chained from onComplete) get their first update this frame.
    for (uint32_t i = 0; i < mTweens.size(); ++i)
    {
        TweenData& tween = mTweens[i];

        if (tween.paused || tween.finished)
            continue;

        tween.elapsed += deltaTime;

        float rawT = (tween.duration > 0.0f) ? (tween.elapsed / tween.duration) : 1.0f;
        if (rawT > 1.0f) rawT = 1.0f;

        bool finished = (rawT >= 1.0f);

        // Apply easing
        float easedT = (float)rawT;
        if (tween.easingType != EaseLinear)
        {
            easingFunction fn = getEasingFunction(tween.easingType);
            if (fn != nullptr)
            {
                easedT = (float)fn((double)rawT);
            }
        }

        // Lerp value
        glm::vec4 value = tween.startVal + (tween.endVal - tween.startVal) * easedT;

        // Apply to node target if set
        if (tween.target != TweenTarget::None && tween.targetNode != nullptr)
        {
            ApplyToNode(tween, value);
        }

        // Call onUpdate callback
        if (tween.onUpdate.IsValid())
        {
            if (tween.isScalar)
            {
                Datum params[3];
                params[0] = Datum(value.x);
                params[1] = Datum(rawT);
                params[2] = Datum(finished);
                tween.onUpdate.Call(3, params);
            }
            else
            {
                Datum params[3];
                params[0] = Datum(value);
                params[1] = Datum(rawT);
                params[2] = Datum(finished);
                tween.onUpdate.Call(3, params);
            }
        }

        if (finished)
        {
            tween.finished = true;

            if (tween.onComplete.IsValid())
            {
                tween.onComplete.Call();
            }
        }
    }

    // Remove finished tweens
    for (int32_t i = (int32_t)mTweens.size() - 1; i >= 0; --i)
    {
        if (mTweens[i].finished)
        {
            mTweens.erase(mTweens.begin() + i);
        }
    }
}

int32_t TweenManager::CreateTween(const TweenData& data)
{
    TweenData tween = data;
    tween.id = mNextId++;
    tween.elapsed = 0.0f;
    tween.finished = false;
    mTweens.push_back(tween);
    return tween.id;
}

void TweenManager::CancelTween(int32_t id)
{
    for (int32_t i = (int32_t)mTweens.size() - 1; i >= 0; --i)
    {
        if (mTweens[i].id == id)
        {
            mTweens.erase(mTweens.begin() + i);
            return;
        }
    }
}

void TweenManager::CancelAllTweens()
{
    mTweens.clear();
}

void TweenManager::PauseTween(int32_t id)
{
    TweenData* tween = FindTween(id);
    if (tween != nullptr)
    {
        tween->paused = true;
    }
}

void TweenManager::ResumeTween(int32_t id)
{
    TweenData* tween = FindTween(id);
    if (tween != nullptr)
    {
        tween->paused = false;
    }
}

TweenData* TweenManager::FindTween(int32_t id)
{
    for (uint32_t i = 0; i < mTweens.size(); ++i)
    {
        if (mTweens[i].id == id)
        {
            return &mTweens[i];
        }
    }
    return nullptr;
}

void TweenManager::ApplyToNode(TweenData& tween, const glm::vec4& value)
{
    Node* node = tween.targetNode;
    if (node == nullptr)
        return;

    switch (tween.target)
    {
    case TweenTarget::Position:
    {
        Node3D* node3d = node->As<Node3D>();
        if (node3d != nullptr)
        {
            node3d->SetPosition(glm::vec3(value));
        }
        else
        {
            Widget* widget = node->As<Widget>();
            if (widget != nullptr)
                widget->SetPosition(glm::vec2(value));
        }
        break;
    }
    case TweenTarget::Rotation:
    {
        Node3D* node3d = node->As<Node3D>();
        if (node3d != nullptr)
        {
            node3d->SetRotation(glm::vec3(value));
        }
        else
        {
            Widget* widget = node->As<Widget>();
            if (widget != nullptr)
                widget->SetRotation(value.x);
        }
        break;
    }
    case TweenTarget::Scale:
    {
        Node3D* node3d = node->As<Node3D>();
        if (node3d != nullptr)
        {
            node3d->SetScale(glm::vec3(value));
        }
        else
        {
            Widget* widget = node->As<Widget>();
            if (widget != nullptr)
                widget->SetScale(glm::vec2(value));
        }
        break;
    }
    case TweenTarget::Color:
    {
        Widget* widget = node->As<Widget>();
        if (widget != nullptr)
        {
            widget->SetColor(value);
        }
        break;
    }
    default:
        break;
    }
}
