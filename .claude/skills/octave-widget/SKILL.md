# Octave Widget Generation Skill

Generate new UI widgets for the Octave Engine following established patterns.

## Widget File Structure

Each widget requires 4-6 files:

```
Engine/Source/Engine/Nodes/Widgets/
  - MyWidget.h
  - MyWidget.cpp

Engine/Source/LuaBindings/
  - MyWidget_Lua.h
  - MyWidget_Lua.cpp
```

## Widget Header Template (MyWidget.h)

```cpp
#pragma once

#include "Nodes/Widgets/Widget.h"
#include "AssetRef.h"  // If using asset references

class Quad;
class Text;
// Forward declare child widget types

class OCTAVE_API MyWidget : public Widget
{
public:

    DECLARE_NODE(MyWidget, Widget);

    virtual void Create() override;
    virtual void Tick(float deltaTime) override;
    virtual void PreRender() override;
    virtual void GatherProperties(std::vector<Property>& props) override;

    // Public API methods
    void SetSomething(float value);
    float GetSomething() const;

protected:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    // Properties (exposed in editor)
    float mSomething = 0.0f;

    // Transient children (internal UI, not serialized)
    Quad* mBackground = nullptr;
    Text* mLabel = nullptr;
};
```

## Widget Implementation Template (MyWidget.cpp)

```cpp
#include "Nodes/Widgets/MyWidget.h"
#include "Nodes/Widgets/Quad.h"
#include "Nodes/Widgets/Text.h"

FORCE_LINK_DEF(MyWidget);
DEFINE_NODE(MyWidget, Widget);

bool MyWidget::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    OCT_ASSERT(prop->mOwner != nullptr);
    MyWidget* widget = static_cast<MyWidget*>(prop->mOwner);

    bool success = false;

    if (prop->mName == "Something")
    {
        widget->SetSomething(*static_cast<const float*>(newValue));
        success = true;
    }

    return success;
}

void MyWidget::Create()
{
    Widget::Create();
    SetName("MyWidget");

    // Enable scissor clipping if needed
    // mUseScissor = true;

    // Set default size
    SetDimensions(200.0f, 100.0f);

    // Create transient children
    mBackground = CreateChild<Quad>("Background");
    mBackground->SetTransient(true);
    mBackground->SetAnchorMode(AnchorMode::FullStretch);
    mBackground->SetMargins(0.0f, 0.0f, 0.0f, 0.0f);
#if EDITOR
    mBackground->mHiddenInTree = true;
#endif

    mLabel = CreateChild<Text>("Label");
    mLabel->SetTransient(true);
    mLabel->SetAnchorMode(AnchorMode::Mid);
#if EDITOR
    mLabel->mHiddenInTree = true;
#endif

    MarkDirty();
}

void MyWidget::Tick(float deltaTime)
{
    Widget::Tick(deltaTime);

    // Handle input, update state
}

void MyWidget::PreRender()
{
    Widget::PreRender();

    // Update layout, apply property changes
}

void MyWidget::GatherProperties(std::vector<Property>& props)
{
    Widget::GatherProperties(props);

    SCOPED_CATEGORY("MyWidget");

    props.push_back(Property(DatumType::Float, "Something", this, &mSomething, 1, HandlePropChange));
}

void MyWidget::SetSomething(float value)
{
    mSomething = value;
    MarkDirty();
}

float MyWidget::GetSomething() const
{
    return mSomething;
}
```

## Lua Binding Header Template (MyWidget_Lua.h)

```cpp
#pragma once

#include "EngineTypes.h"

#if LUA_ENABLED

#include "LuaBindings/LuaUtils.h"

#define MYWIDGET_LUA_NAME "MyWidget"
#define MYWIDGET_LUA_FLAG "cfMyWidget"
#define CHECK_MYWIDGET(L, arg) static_cast<MyWidget*>(CheckNodeLuaType(L, arg, MYWIDGET_LUA_NAME, MYWIDGET_LUA_FLAG))

struct MyWidget_Lua
{
    static int SetSomething(lua_State* L);
    static int GetSomething(lua_State* L);

    static void Bind();
};

#endif
```

## Lua Binding Implementation Template (MyWidget_Lua.cpp)

```cpp
#include "LuaBindings/MyWidget_Lua.h"

#if LUA_ENABLED

#include "LuaBindings/Widget_Lua.h"
#include "LuaBindings/Node_Lua.h"
#include "LuaBindings/LuaTypeCheck.h"

#include "Nodes/Widgets/MyWidget.h"

int MyWidget_Lua::SetSomething(lua_State* L)
{
    MyWidget* widget = CHECK_MYWIDGET(L, 1);
    float value = CHECK_NUMBER(L, 2);

    widget->SetSomething(value);
    return 0;
}

int MyWidget_Lua::GetSomething(lua_State* L)
{
    MyWidget* widget = CHECK_MYWIDGET(L, 1);
    lua_pushnumber(L, widget->GetSomething());
    return 1;
}

void MyWidget_Lua::Bind()
{
    lua_State* L = GetLua();
    int mtIndex = CreateClassMetatable(
        MYWIDGET_LUA_NAME,
        MYWIDGET_LUA_FLAG,
        WIDGET_LUA_NAME);

    Node_Lua::BindCommon(L, mtIndex);

    REGISTER_TABLE_FUNC(L, mtIndex, SetSomething);
    REGISTER_TABLE_FUNC(L, mtIndex, GetSomething);

    lua_pop(L, 1);
}

#endif
```

## Integration Steps

### 1. Engine.cpp - Add FORCE_LINK_CALL

```cpp
// In ForceLink() function, after other widgets:
FORCE_LINK_CALL(MyWidget);
```

### 2. LuaBindings.cpp - Add include and Bind call

```cpp
// Add include at top:
#include "LuaBindings/MyWidget_Lua.h"

// In BindLua() function:
MyWidget_Lua::Bind();
```

### 3. Engine.vcxproj - Add source files

```xml
<!-- In ClCompile section: -->
<ClCompile Include="Source\Engine\Nodes\Widgets\MyWidget.cpp" />
<ClCompile Include="Source\LuaBindings\MyWidget_Lua.cpp" />

<!-- In ClInclude section: -->
<ClInclude Include="Source\Engine\Nodes\Widgets\MyWidget.h" />
<ClInclude Include="Source\LuaBindings\MyWidget_Lua.h" />
```

### 4. Engine.vcxproj.filters - Add filter entries

```xml
<!-- For .cpp files: -->
<ClCompile Include="Source\Engine\Nodes\Widgets\MyWidget.cpp">
  <Filter>Source Files\Engine\Nodes\Widgets</Filter>
</ClCompile>
<ClCompile Include="Source\LuaBindings\MyWidget_Lua.cpp">
  <Filter>Source Files\LuaBindings</Filter>
</ClCompile>

<!-- For .h files: -->
<ClInclude Include="Source\Engine\Nodes\Widgets\MyWidget.h">
  <Filter>Source Files\Engine\Nodes\Widgets</Filter>
</ClInclude>
<ClInclude Include="Source\LuaBindings\MyWidget_Lua.h">
  <Filter>Source Files\LuaBindings</Filter>
</ClInclude>
```

## Key Patterns

### Transient Children
Internal UI elements that shouldn't be serialized:
```cpp
mChild = CreateChild<Quad>("Child");
mChild->SetTransient(true);
#if EDITOR
mChild->mHiddenInTree = true;
#endif
```

### Signals and Callbacks (Dual Pattern)
Always emit both signal and call Lua function:
```cpp
EmitSignal("Activated", { this });
CallFunction("OnActivated", { this });
```

### Property Change Handling
Use HandlePropChange for editor property updates:
```cpp
props.push_back(Property(DatumType::Float, "Value", this, &mValue, 1, HandlePropChange));
```

### Lazy Container Creation
Create containers on first access (like Window pattern):
```cpp
void MyWidget::EnsureContainers()
{
    if (mContainer != nullptr) return;

    mContainer = CreateChild<ScrollContainer>("Container");
    mContainer->SetTransient(true);
    // ... configure
}
```

### Common Property Types
```cpp
DatumType::Bool      // bool
DatumType::Integer   // int32_t
DatumType::Float     // float
DatumType::String    // std::string
DatumType::Vector2D  // glm::vec2
DatumType::Color     // glm::vec4
DatumType::Asset     // AssetRef (add type filter as extra param)
DatumType::Byte      // uint8_t (for enums, add strings array)
```

### Common Lua Type Checks
```cpp
CHECK_NUMBER(L, arg)    // float
CHECK_INTEGER(L, arg)   // int
CHECK_STRING(L, arg)    // const char*
CHECK_BOOLEAN(L, arg)   // bool
CHECK_VECTOR(L, arg)    // glm::vec4
CHECK_WIDGET(L, arg)    // Widget*
```

## Reference Files

- **Simple widget**: `Button.h/.cpp`
- **Composite widget**: `Window.h/.cpp`
- **Container widget**: `ScrollContainer.h/.cpp`
- **Data-driven widget**: `ListViewWidget.h/.cpp`
- **Lua bindings**: `Window_Lua.h/.cpp`
