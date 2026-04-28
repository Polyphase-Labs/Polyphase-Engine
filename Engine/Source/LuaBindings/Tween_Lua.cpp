#include "LuaBindings/Tween_Lua.h"
#include "LuaBindings/LuaUtils.h"
#include "LuaBindings/Vector_Lua.h"
#include "LuaBindings/Node_Lua.h"
#include "LuaBindings/Node3d_Lua.h"

#include "TweenManager.h"
#include "Nodes/3D/Node3d.h"
#include "Nodes/Widgets/Widget.h"

#if LUA_ENABLED

// Tween.Value(easing, from, to, duration, onUpdate, [onComplete])
int Tween_Lua::Value(lua_State* L)
{
    int32_t easingType = CHECK_INTEGER(L, 1);
    float from = CHECK_NUMBER(L, 2);
    float to = CHECK_NUMBER(L, 3);
    float duration = CHECK_NUMBER(L, 4);
    CHECK_FUNCTION(L, 5);
    ScriptFunc onUpdate(L, 5);

    ScriptFunc onComplete;
    if (lua_isfunction(L, 6))
    {
        onComplete = ScriptFunc(L, 6);
    }

    TweenData data;
    data.easingType = (easing_functions)easingType;
    data.startVal = glm::vec4(from, 0.0f, 0.0f, 0.0f);
    data.endVal = glm::vec4(to, 0.0f, 0.0f, 0.0f);
    data.duration = duration;
    data.isScalar = true;
    data.components = 1;
    data.onUpdate = onUpdate;
    data.onComplete = onComplete;

    int32_t id = GetTweenManager()->CreateTween(data);
    lua_pushinteger(L, id);
    return 1;
}

// Tween.Vector(easing, from, to, duration, onUpdate, [onComplete])
int Tween_Lua::Vector(lua_State* L)
{
    int32_t easingType = CHECK_INTEGER(L, 1);
    glm::vec4 from = CHECK_VECTOR(L, 2);
    glm::vec4 to = CHECK_VECTOR(L, 3);
    float duration = CHECK_NUMBER(L, 4);
    CHECK_FUNCTION(L, 5);
    ScriptFunc onUpdate(L, 5);

    ScriptFunc onComplete;
    if (lua_isfunction(L, 6))
    {
        onComplete = ScriptFunc(L, 6);
    }

    TweenData data;
    data.easingType = (easing_functions)easingType;
    data.startVal = from;
    data.endVal = to;
    data.duration = duration;
    data.isScalar = false;
    data.components = 4;
    data.onUpdate = onUpdate;
    data.onComplete = onComplete;

    int32_t id = GetTweenManager()->CreateTween(data);
    lua_pushinteger(L, id);
    return 1;
}

// Tween.Quaternion(easing, from, to, duration, onUpdate, [onComplete])
int Tween_Lua::Quaternion(lua_State* L)
{
    // Quaternions are stored as vec4 in Lua, same as Vector tween
    return Vector(L);
}

// Tween.Position(node, easing, to, duration, [onComplete])
int Tween_Lua::Position(lua_State* L)
{
    Node* node = CHECK_NODE(L, 1);
    int32_t easingType = CHECK_INTEGER(L, 2);
    glm::vec4 to = CHECK_VECTOR(L, 3);
    float duration = CHECK_NUMBER(L, 4);

    ScriptFunc onComplete;
    if (lua_isfunction(L, 5))
    {
        onComplete = ScriptFunc(L, 5);
    }

    glm::vec4 fromVal(0.0f);
    Node3D* node3d = node->As<Node3D>();
    Widget* widget = node->As<Widget>();
    if (node3d != nullptr)
        fromVal = glm::vec4(node3d->GetPosition(), 0.0f);
    else if (widget != nullptr)
        fromVal = glm::vec4(widget->GetPosition(), 0.0f, 0.0f);

    TweenData data;
    data.easingType = (easing_functions)easingType;
    data.startVal = fromVal;
    data.endVal = to;
    data.duration = duration;
    data.isScalar = false;
    data.components = node3d ? 3 : 2;
    data.onComplete = onComplete;
    data.targetNode = node;
    data.target = TweenTarget::Position;

    int32_t id = GetTweenManager()->CreateTween(data);
    lua_pushinteger(L, id);
    return 1;
}

// Tween.Rotation(node, easing, to, duration, [onComplete])
int Tween_Lua::Rotation(lua_State* L)
{
    Node* node = CHECK_NODE(L, 1);
    int32_t easingType = CHECK_INTEGER(L, 2);
    glm::vec4 to = CHECK_VECTOR(L, 3);
    float duration = CHECK_NUMBER(L, 4);

    ScriptFunc onComplete;
    if (lua_isfunction(L, 5))
    {
        onComplete = ScriptFunc(L, 5);
    }

    glm::vec4 fromVal(0.0f);
    Node3D* node3d = node->As<Node3D>();
    Widget* widget = node->As<Widget>();
    if (node3d != nullptr)
        fromVal = glm::vec4(node3d->GetRotationEuler(), 0.0f);
    else if (widget != nullptr)
        fromVal = glm::vec4(widget->GetRotation(), 0.0f, 0.0f, 0.0f);

    TweenData data;
    data.easingType = (easing_functions)easingType;
    data.startVal = fromVal;
    data.endVal = to;
    data.duration = duration;
    data.isScalar = false;
    data.components = node3d ? 3 : 1;
    data.onComplete = onComplete;
    data.targetNode = node;
    data.target = TweenTarget::Rotation;

    int32_t id = GetTweenManager()->CreateTween(data);
    lua_pushinteger(L, id);
    return 1;
}

// Tween.Scale(node, easing, to, duration, [onComplete])
int Tween_Lua::Scale(lua_State* L)
{
    Node* node = CHECK_NODE(L, 1);
    int32_t easingType = CHECK_INTEGER(L, 2);
    glm::vec4 to = CHECK_VECTOR(L, 3);
    float duration = CHECK_NUMBER(L, 4);

    ScriptFunc onComplete;
    if (lua_isfunction(L, 5))
    {
        onComplete = ScriptFunc(L, 5);
    }

    glm::vec4 fromVal(1.0f);
    Node3D* node3d = node->As<Node3D>();
    Widget* widget = node->As<Widget>();
    if (node3d != nullptr)
        fromVal = glm::vec4(node3d->GetScale(), 0.0f);
    else if (widget != nullptr)
        fromVal = glm::vec4(widget->GetScale(), 0.0f, 0.0f);

    TweenData data;
    data.easingType = (easing_functions)easingType;
    data.startVal = fromVal;
    data.endVal = to;
    data.duration = duration;
    data.isScalar = false;
    data.components = node3d ? 3 : 2;
    data.onComplete = onComplete;
    data.targetNode = node;
    data.target = TweenTarget::Scale;

    int32_t id = GetTweenManager()->CreateTween(data);
    lua_pushinteger(L, id);
    return 1;
}

// Tween.Color(node, easing, to, duration, [onComplete])
int Tween_Lua::Color(lua_State* L)
{
    Node* node = CHECK_NODE(L, 1);
    int32_t easingType = CHECK_INTEGER(L, 2);
    glm::vec4 to = CHECK_VECTOR(L, 3);
    float duration = CHECK_NUMBER(L, 4);

    ScriptFunc onComplete;
    if (lua_isfunction(L, 5))
    {
        onComplete = ScriptFunc(L, 5);
    }

    Widget* widget = node->As<Widget>();
    glm::vec4 from4 = widget ? widget->GetColor() : glm::vec4(1.0f);

    TweenData data;
    data.easingType = (easing_functions)easingType;
    data.startVal = from4;
    data.endVal = to;
    data.duration = duration;
    data.isScalar = false;
    data.components = 4;
    data.onComplete = onComplete;
    data.targetNode = node;
    data.target = TweenTarget::Color;

    int32_t id = GetTweenManager()->CreateTween(data);
    lua_pushinteger(L, id);
    return 1;
}

int Tween_Lua::Cancel(lua_State* L)
{
    int32_t id = CHECK_INTEGER(L, 1);
    GetTweenManager()->CancelTween(id);
    return 0;
}

int Tween_Lua::CancelAll(lua_State* L)
{
    GetTweenManager()->CancelAllTweens();
    return 0;
}

int Tween_Lua::Pause(lua_State* L)
{
    int32_t id = CHECK_INTEGER(L, 1);
    GetTweenManager()->PauseTween(id);
    return 0;
}

int Tween_Lua::Resume(lua_State* L)
{
    int32_t id = CHECK_INTEGER(L, 1);
    GetTweenManager()->ResumeTween(id);
    return 0;
}

void Tween_Lua::Bind()
{
    lua_State* L = GetLua();

    lua_newtable(L);
    int tableIdx = lua_gettop(L);

    REGISTER_TABLE_FUNC(L, tableIdx, Value);
    REGISTER_TABLE_FUNC(L, tableIdx, Vector);
    REGISTER_TABLE_FUNC(L, tableIdx, Quaternion);
    REGISTER_TABLE_FUNC(L, tableIdx, Position);
    REGISTER_TABLE_FUNC(L, tableIdx, Rotation);
    REGISTER_TABLE_FUNC(L, tableIdx, Scale);
    REGISTER_TABLE_FUNC(L, tableIdx, Color);
    REGISTER_TABLE_FUNC(L, tableIdx, Cancel);
    REGISTER_TABLE_FUNC(L, tableIdx, CancelAll);
    REGISTER_TABLE_FUNC(L, tableIdx, Pause);
    REGISTER_TABLE_FUNC(L, tableIdx, Resume);

    lua_setglobal(L, TWEEN_LUA_NAME);
    OCT_ASSERT(lua_gettop(L) == 0);
}

#endif
