#include "NodeGraph/GraphTypes.h"

bool AreGraphPinTypesCompatible(DatumType outputType, DatumType inputType)
{
    if (outputType == inputType)
    {
        return true;
    }

    // Float <-> Integer
    if ((outputType == DatumType::Float && inputType == DatumType::Integer) ||
        (outputType == DatumType::Integer && inputType == DatumType::Float))
    {
        return true;
    }

    // Vector <-> Color (vec3 <-> vec4)
    if ((outputType == DatumType::Vector && inputType == DatumType::Color) ||
        (outputType == DatumType::Color && inputType == DatumType::Vector))
    {
        return true;
    }

    // Float -> Vector2D/Vector/Color (scalar expansion)
    if (outputType == DatumType::Float &&
        (inputType == DatumType::Vector2D || inputType == DatumType::Vector || inputType == DatumType::Color))
    {
        return true;
    }

    // Node hierarchy: Node3D -> Node, Widget -> Node, Audio3D -> Node3D -> Node
    if (outputType == DatumType::Node3D  && inputType == DatumType::Node) return true;
    if (outputType == DatumType::Audio3D && inputType == DatumType::Node3D) return true;
    if (outputType == DatumType::Audio3D && inputType == DatumType::Node) return true;
    if (outputType == DatumType::Widget  && inputType == DatumType::Node) return true;
    if (outputType == DatumType::Text    && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::Text    && inputType == DatumType::Node) return true;
    if (outputType == DatumType::Quad    && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::Quad    && inputType == DatumType::Node) return true;

    // Spline3D -> Node3D -> Node
    if (outputType == DatumType::Spline3D && inputType == DatumType::Node3D) return true;
    if (outputType == DatumType::Spline3D && inputType == DatumType::Node) return true;

    // Widget subtypes -> Widget -> Node
    // SpinBox, Window, InputField, ProgressBar, CheckBox, ListViewWidget,
    // ListViewItemWidget, ArrayWidget, Button, Slider, LineEdit, Canvas -> Widget
    if (outputType == DatumType::SpinBox            && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::SpinBox            && inputType == DatumType::Node) return true;
    if (outputType == DatumType::Window             && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::Window             && inputType == DatumType::Node) return true;
    if (outputType == DatumType::InputField         && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::InputField         && inputType == DatumType::Node) return true;
    if (outputType == DatumType::ProgressBar        && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::ProgressBar        && inputType == DatumType::Node) return true;
    if (outputType == DatumType::CheckBox           && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::CheckBox           && inputType == DatumType::Node) return true;
    if (outputType == DatumType::ListViewWidget     && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::ListViewWidget     && inputType == DatumType::Node) return true;
    if (outputType == DatumType::ListViewItemWidget && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::ListViewItemWidget && inputType == DatumType::Node) return true;
    if (outputType == DatumType::ArrayWidget        && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::ArrayWidget        && inputType == DatumType::Node) return true;
    if (outputType == DatumType::Button             && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::Button             && inputType == DatumType::Node) return true;
    if (outputType == DatumType::Slider             && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::Slider             && inputType == DatumType::Node) return true;
    if (outputType == DatumType::LineEdit           && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::LineEdit           && inputType == DatumType::Node) return true;
    if (outputType == DatumType::Canvas             && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::Canvas             && inputType == DatumType::Node) return true;
    if (outputType == DatumType::ComboBox           && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::ComboBox           && inputType == DatumType::Node) return true;

    // DialogWindow -> Window -> Widget -> Node
    if (outputType == DatumType::DialogWindow       && inputType == DatumType::Window) return true;
    if (outputType == DatumType::DialogWindow       && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::DialogWindow       && inputType == DatumType::Node) return true;

    // DebugResourcesWidget -> Canvas -> Widget -> Node
    if (outputType == DatumType::DebugResourcesWidget && inputType == DatumType::Canvas) return true;
    if (outputType == DatumType::DebugResourcesWidget && inputType == DatumType::Widget) return true;
    if (outputType == DatumType::DebugResourcesWidget && inputType == DatumType::Node) return true;

    // Node3D subtypes -> Node3D -> Node
    // Voxel3D, Terrain3D, TileMap2D, Camera3D, DirectionalLight3D, Box3D, Particle3D -> Node3D
    if (outputType == DatumType::Voxel3D            && inputType == DatumType::Node3D) return true;
    if (outputType == DatumType::Voxel3D            && inputType == DatumType::Node) return true;
    if (outputType == DatumType::Terrain3D          && inputType == DatumType::Node3D) return true;
    if (outputType == DatumType::Terrain3D          && inputType == DatumType::Node) return true;
    if (outputType == DatumType::TileMap2D          && inputType == DatumType::Node3D) return true;
    if (outputType == DatumType::TileMap2D          && inputType == DatumType::Node) return true;
    if (outputType == DatumType::Camera3D           && inputType == DatumType::Node3D) return true;
    if (outputType == DatumType::Camera3D           && inputType == DatumType::Node) return true;
    if (outputType == DatumType::DirectionalLight3D && inputType == DatumType::Node3D) return true;
    if (outputType == DatumType::DirectionalLight3D && inputType == DatumType::Node) return true;
    if (outputType == DatumType::Box3D              && inputType == DatumType::Node3D) return true;
    if (outputType == DatumType::Box3D              && inputType == DatumType::Node) return true;
    if (outputType == DatumType::Particle3D         && inputType == DatumType::Node3D) return true;
    if (outputType == DatumType::Particle3D         && inputType == DatumType::Node) return true;

    // NavMesh3D -> Box3D -> Node3D -> Node
    if (outputType == DatumType::NavMesh3D          && inputType == DatumType::Box3D) return true;
    if (outputType == DatumType::NavMesh3D          && inputType == DatumType::Node3D) return true;
    if (outputType == DatumType::NavMesh3D          && inputType == DatumType::Node) return true;

    // Node subtypes -> Node
    if (outputType == DatumType::TimelinePlayer     && inputType == DatumType::Node) return true;
    if (outputType == DatumType::NodeGraphPlayer    && inputType == DatumType::Node) return true;

    // Asset subtypes -> Asset
    if (outputType == DatumType::Scene              && inputType == DatumType::Asset) return true;
    if (outputType == DatumType::Material           && inputType == DatumType::Asset) return true;
    if (outputType == DatumType::TileSet            && inputType == DatumType::Asset) return true;
    if (outputType == DatumType::TileMap            && inputType == DatumType::Asset) return true;
    if (outputType == DatumType::Timeline           && inputType == DatumType::Asset) return true;
    if (outputType == DatumType::NodeGraphAsset     && inputType == DatumType::Asset) return true;

    return false;
}

const char* GetDatumTypeName(DatumType type)
{
    switch (type)
    {
    case DatumType::Integer:  return "Integer";
    case DatumType::Float:    return "Float";
    case DatumType::Bool:     return "Bool";
    case DatumType::String:   return "String";
    case DatumType::Vector2D: return "Vector2D";
    case DatumType::Vector:   return "Vector";
    case DatumType::Color:    return "Color";
    case DatumType::Asset:    return "Asset";
    case DatumType::Node:     return "Node";
    case DatumType::Node3D:   return "Node3D";
    case DatumType::Widget:   return "Widget";
    case DatumType::Text:     return "Text";
    case DatumType::Quad:     return "Quad";
    case DatumType::Audio3D:  return "Audio3D";
    case DatumType::Scene:    return "Scene";
    case DatumType::PointCloud: return "PointCloud";
    case DatumType::Spline3D: return "Spline3D";
    case DatumType::Execution: return "Exec";
    // Widget subtypes
    case DatumType::SpinBox:            return "SpinBox";
    case DatumType::Window:             return "Window";
    case DatumType::DialogWindow:       return "DialogWindow";
    case DatumType::InputField:         return "InputField";
    case DatumType::ProgressBar:        return "ProgressBar";
    case DatumType::CheckBox:           return "CheckBox";
    case DatumType::ListViewWidget:     return "ListViewWidget";
    case DatumType::ListViewItemWidget: return "ListViewItemWidget";
    case DatumType::DebugResourcesWidget: return "DebugResourcesWidget";
    case DatumType::ArrayWidget:        return "ArrayWidget";
    case DatumType::Button:             return "Button";
    case DatumType::Slider:             return "Slider";
    case DatumType::LineEdit:           return "LineEdit";
    case DatumType::Canvas:             return "Canvas";
    case DatumType::ComboBox:           return "ComboBox";
    // Node3D subtypes
    case DatumType::Voxel3D:            return "Voxel3D";
    case DatumType::Terrain3D:          return "Terrain3D";
    case DatumType::TileMap2D:          return "TileMap2D";
    case DatumType::NavMesh3D:          return "NavMesh3D";
    case DatumType::Camera3D:           return "Camera3D";
    case DatumType::DirectionalLight3D: return "DirectionalLight3D";
    case DatumType::Box3D:              return "Box3D";
    case DatumType::Particle3D:         return "Particle3D";
    // Node subtypes
    case DatumType::TimelinePlayer:     return "TimelinePlayer";
    case DatumType::NodeGraphPlayer:    return "NodeGraphPlayer";
    // Asset subtypes
    case DatumType::Material:           return "Material";
    case DatumType::TileSet:            return "TileSet";
    case DatumType::TileMap:            return "TileMap";
    case DatumType::Timeline:           return "Timeline";
    case DatumType::NodeGraphAsset:     return "NodeGraphAsset";
    default:                  return "Unknown";
    }
}

glm::vec4 GetDatumTypeColor(DatumType type)
{
    switch (type)
    {
    case DatumType::Integer:  return glm::vec4(0.2f, 0.8f, 0.2f, 1.0f);
    case DatumType::Float:    return glm::vec4(0.5f, 0.9f, 0.5f, 1.0f);
    case DatumType::Bool:     return glm::vec4(0.9f, 0.2f, 0.2f, 1.0f);
    case DatumType::String:   return glm::vec4(0.9f, 0.2f, 0.9f, 1.0f);
    case DatumType::Vector2D: return glm::vec4(0.9f, 0.8f, 0.1f, 1.0f);
    case DatumType::Vector:   return glm::vec4(0.9f, 0.9f, 0.1f, 1.0f);
    case DatumType::Color:    return glm::vec4(0.1f, 0.5f, 0.9f, 1.0f);
    case DatumType::Asset:    return glm::vec4(0.3f, 0.3f, 0.9f, 1.0f);
    case DatumType::Node:     return glm::vec4(0.1f, 0.6f, 0.6f, 1.0f);
    case DatumType::Node3D:   return glm::vec4(0.0f, 0.8f, 0.7f, 1.0f);
    case DatumType::Widget:   return glm::vec4(0.6f, 0.2f, 0.8f, 1.0f);
    case DatumType::Text:     return glm::vec4(0.8f, 0.3f, 0.9f, 1.0f);
    case DatumType::Quad:     return glm::vec4(0.9f, 0.5f, 0.2f, 1.0f);
    case DatumType::Audio3D:  return glm::vec4(0.2f, 0.8f, 0.3f, 1.0f);
    case DatumType::Scene:    return glm::vec4(0.3f, 0.3f, 0.9f, 1.0f);
    case DatumType::PointCloud: return glm::vec4(0.9f, 0.55f, 0.1f, 1.0f);
    case DatumType::Spline3D: return glm::vec4(0.3f, 0.7f, 0.9f, 1.0f);
    case DatumType::Execution: return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    // Widget subtypes (purple family)
    case DatumType::SpinBox:            return glm::vec4(0.65f, 0.25f, 0.85f, 1.0f);
    case DatumType::Window:             return glm::vec4(0.55f, 0.15f, 0.75f, 1.0f);
    case DatumType::DialogWindow:       return glm::vec4(0.50f, 0.10f, 0.70f, 1.0f);
    case DatumType::InputField:         return glm::vec4(0.70f, 0.30f, 0.80f, 1.0f);
    case DatumType::ProgressBar:        return glm::vec4(0.75f, 0.25f, 0.75f, 1.0f);
    case DatumType::CheckBox:           return glm::vec4(0.60f, 0.20f, 0.90f, 1.0f);
    case DatumType::ListViewWidget:     return glm::vec4(0.55f, 0.25f, 0.80f, 1.0f);
    case DatumType::ListViewItemWidget: return glm::vec4(0.50f, 0.30f, 0.85f, 1.0f);
    case DatumType::DebugResourcesWidget: return glm::vec4(0.45f, 0.20f, 0.75f, 1.0f);
    case DatumType::ArrayWidget:        return glm::vec4(0.70f, 0.20f, 0.85f, 1.0f);
    case DatumType::Button:             return glm::vec4(0.75f, 0.30f, 0.90f, 1.0f);
    case DatumType::Slider:             return glm::vec4(0.65f, 0.30f, 0.80f, 1.0f);
    case DatumType::LineEdit:           return glm::vec4(0.60f, 0.25f, 0.85f, 1.0f);
    case DatumType::Canvas:             return glm::vec4(0.55f, 0.20f, 0.85f, 1.0f);
    case DatumType::ComboBox:           return glm::vec4(0.58f, 0.22f, 0.82f, 1.0f);
    // Node3D subtypes (cyan/teal family)
    case DatumType::Voxel3D:            return glm::vec4(0.0f, 0.75f, 0.65f, 1.0f);
    case DatumType::Terrain3D:          return glm::vec4(0.0f, 0.70f, 0.60f, 1.0f);
    case DatumType::TileMap2D:          return glm::vec4(0.0f, 0.85f, 0.75f, 1.0f);
    case DatumType::NavMesh3D:          return glm::vec4(0.1f, 0.75f, 0.70f, 1.0f);
    case DatumType::Camera3D:           return glm::vec4(0.1f, 0.85f, 0.80f, 1.0f);
    case DatumType::DirectionalLight3D: return glm::vec4(0.1f, 0.90f, 0.65f, 1.0f);
    case DatumType::Box3D:              return glm::vec4(0.0f, 0.80f, 0.80f, 1.0f);
    case DatumType::Particle3D:         return glm::vec4(0.0f, 0.85f, 0.60f, 1.0f);
    // Node subtypes (teal)
    case DatumType::TimelinePlayer:     return glm::vec4(0.15f, 0.65f, 0.65f, 1.0f);
    case DatumType::NodeGraphPlayer:    return glm::vec4(0.15f, 0.60f, 0.70f, 1.0f);
    // Asset subtypes (blue family)
    case DatumType::Material:           return glm::vec4(0.25f, 0.25f, 0.85f, 1.0f);
    case DatumType::TileSet:            return glm::vec4(0.35f, 0.30f, 0.90f, 1.0f);
    case DatumType::TileMap:            return glm::vec4(0.30f, 0.35f, 0.85f, 1.0f);
    case DatumType::Timeline:           return glm::vec4(0.25f, 0.30f, 0.90f, 1.0f);
    case DatumType::NodeGraphAsset:     return glm::vec4(0.35f, 0.25f, 0.85f, 1.0f);
    default:                  return glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
    }
}
