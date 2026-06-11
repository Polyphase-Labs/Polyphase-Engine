#pragma once

#include "Maths.h"

#include <cstdint>
#include <string>

enum class InterpMode : uint8_t
{
    Linear,
    Step,
    Cubic,

    Count
};

struct TransformKeyframe
{
    float       mTime       = 0.0f;
    glm::vec3   mPosition   = glm::vec3(0.0f);
    glm::quat   mRotation   = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3   mScale      = glm::vec3(1.0f);
    InterpMode  mInterpMode = InterpMode::Linear;
    std::string mSignal;

    static TransformKeyframe Lerp(const TransformKeyframe& a, const TransformKeyframe& b, float t)
    {
        if (a.mInterpMode == InterpMode::Step || t <= 0.0f)
        {
            return a;
        }

        if (t >= 1.0f)
        {
            return b;
        }

        TransformKeyframe out;
        out.mTime       = glm::mix(a.mTime, b.mTime, t);
        out.mPosition   = glm::mix(a.mPosition, b.mPosition, t);
        out.mRotation   = glm::slerp(a.mRotation, b.mRotation, t);
        out.mScale      = glm::mix(a.mScale, b.mScale, t);
        out.mInterpMode = a.mInterpMode;
        out.mSignal     = a.mSignal;
        return out;
    }
};

class Node;
void ApplyTransformKeyframeToNode(Node* target, const TransformKeyframe& kf);
