#if EDITOR

// WinSock2 must be included before Windows.h to avoid winsock.h conflict with ASIO
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Windows.h>
#endif

#include "ControllerServerRoutes.h"
#include "ControllerServer.h"

#include "Engine.h"
#include "World.h"
#include "Log.h"
#include "AssetManager.h"
#include "Assets/Scene.h"
#include "Nodes/Node.h"
#include "Nodes/3D/Node3d.h"
#include "Property.h"
#include "Script.h"
#include "EditorState.h"
#include "ActionManager.h"
#include "EditorUIHookManager.h"
#include "Addons/NativeAddonManager.h"
#include "DebugLog/DebugLogWindow.h"
#include "GamePreview/GamePreview.h"
#include "EditorScreenshot.h"

#include <chrono>

#include <cstdlib>
#include <vector>

#include <stb_image_write.h>
#include <stb_image_resize2.h>

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif

#include "crow.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// ---------------------------------------------------------------------------
// JSON Helpers
// ---------------------------------------------------------------------------

static crow::json::wvalue ErrorJson(const std::string& msg)
{
    crow::json::wvalue j;
    j["error"] = msg;
    return j;
}

static std::string Base64Encode(const uint8_t* data, size_t len)
{
    static const char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.resize(((len + 2) / 3) * 4);
    size_t o = 0;

    size_t i = 0;
    for (; i + 2 < len; i += 3)
    {
        uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | uint32_t(data[i + 2]);
        out[o++] = kTable[(v >> 18) & 0x3F];
        out[o++] = kTable[(v >> 12) & 0x3F];
        out[o++] = kTable[(v >>  6) & 0x3F];
        out[o++] = kTable[ v        & 0x3F];
    }
    if (i < len)
    {
        uint32_t v = uint32_t(data[i]) << 16;
        bool two = (i + 1 < len);
        if (two)
            v |= uint32_t(data[i + 1]) << 8;

        out[o++] = kTable[(v >> 18) & 0x3F];
        out[o++] = kTable[(v >> 12) & 0x3F];
        out[o++] = two ? kTable[(v >> 6) & 0x3F] : '=';
        out[o++] = '=';
    }

    return out;
}

static void PngWriteToVector(void* context, void* data, int size)
{
    auto* buf = static_cast<std::vector<uint8_t>*>(context);
    buf->insert(buf->end(),
                static_cast<uint8_t*>(data),
                static_cast<uint8_t*>(data) + size);
}

// Encode an RGBA buffer as a base64 PNG JSON response, optionally downscaling
// to targetWidth (preserves aspect; only resizes when smaller than native).
// rgba is mutated by resize.
static std::string EncodeScreenshotResponse(std::vector<uint8_t>& rgba,
                                            uint32_t w,
                                            uint32_t h,
                                            uint32_t targetWidth)
{
    if (targetWidth != 0 && targetWidth < w)
    {
        uint32_t newW = targetWidth;
        uint32_t newH = std::max<uint32_t>(1, (uint32_t)((uint64_t)h * newW / w));

        std::vector<uint8_t> resized(size_t(newW) * newH * 4);
        unsigned char* ok = stbir_resize_uint8_linear(
            rgba.data(), (int)w, (int)h, 0,
            resized.data(), (int)newW, (int)newH, 0,
            STBIR_RGBA);

        if (ok == nullptr)
            return ErrorJson("Failed to resize screenshot").dump();

        rgba.swap(resized);
        w = newW;
        h = newH;
    }

    std::vector<uint8_t> png;
    png.reserve(size_t(w) * h);
    int wr = stbi_write_png_to_func(
        &PngWriteToVector, &png,
        (int)w, (int)h, 4, rgba.data(), (int)(w * 4));

    if (!wr || png.empty())
        return ErrorJson("Failed to encode PNG").dump();

    std::string b64 = Base64Encode(png.data(), png.size());

    crow::json::wvalue j;
    j["format"] = "png";
    j["width"] = static_cast<int>(w);
    j["height"] = static_cast<int>(h);
    j["data"] = std::move(b64);
    return j.dump();
}

static crow::json::wvalue Vec3ToJson(const glm::vec3& v)
{
    crow::json::wvalue j;
    j[0] = v.x;
    j[1] = v.y;
    j[2] = v.z;
    return j;
}

static glm::vec3 JsonToVec3(const crow::json::rvalue& j)
{
    glm::vec3 v(0.0f);
    if (j.size() >= 3)
    {
        v.x = static_cast<float>(j[0].d());
        v.y = static_cast<float>(j[1].d());
        v.z = static_cast<float>(j[2].d());
    }
    return v;
}

static glm::vec2 JsonToVec2(const crow::json::rvalue& j)
{
    glm::vec2 v(0.0f);
    if (j.size() >= 2)
    {
        v.x = static_cast<float>(j[0].d());
        v.y = static_cast<float>(j[1].d());
    }
    return v;
}

static glm::vec4 JsonToColor(const crow::json::rvalue& j)
{
    glm::vec4 c(0.0f, 0.0f, 0.0f, 1.0f);
    if (j.size() >= 1) c.r = static_cast<float>(j[0].d());
    if (j.size() >= 2) c.g = static_cast<float>(j[1].d());
    if (j.size() >= 3) c.b = static_cast<float>(j[2].d());
    if (j.size() >= 4) c.a = static_cast<float>(j[3].d());
    return c;
}

// Convert a JSON value to a Datum matching the given Property's type.
// On failure, fills outError with a human-readable message and returns false.
// Handles the full settable type range — primitives, Vector/Vector2D/Color,
// Asset and all asset subtypes (Material, Scene, TileSet, TileMap, Timeline,
// NodeGraphAsset). Asset values are looked up by name from AssetManager;
// pass an empty string to clear an asset slot.
static bool BuildDatumFromJson(const Property& prop,
                               const crow::json::rvalue& jsonValue,
                               Datum& outDatum,
                               std::string& outError)
{
    DatumType type = prop.GetType();

    switch (type)
    {
    case DatumType::Integer:
        outDatum = Datum(static_cast<int32_t>(jsonValue.i()));
        return true;
    case DatumType::Short:
        outDatum = Datum(static_cast<int16_t>(jsonValue.i()));
        return true;
    case DatumType::Byte:
        outDatum = Datum(static_cast<uint8_t>(jsonValue.i()));
        return true;
    case DatumType::Float:
        outDatum = Datum(static_cast<float>(jsonValue.d()));
        return true;
    case DatumType::Bool:
        outDatum = Datum(jsonValue.b());
        return true;
    case DatumType::String:
        outDatum = Datum(std::string(jsonValue.s()));
        return true;
    case DatumType::Vector:
        outDatum = Datum(JsonToVec3(jsonValue));
        return true;
    case DatumType::Vector2D:
        outDatum = Datum(JsonToVec2(jsonValue));
        return true;
    case DatumType::Color:
        outDatum = Datum(JsonToColor(jsonValue));
        return true;
    case DatumType::Asset:
    case DatumType::Scene:
    case DatumType::Material:
    case DatumType::TileSet:
    case DatumType::TileMap:
    case DatumType::Timeline:
    case DatumType::NodeGraphAsset:
    {
        std::string assetName = jsonValue.s();
        Asset* asset = nullptr;
        if (!assetName.empty())
        {
            asset = AssetManager::Get()->LoadAsset(assetName);
            if (asset == nullptr)
            {
                outError = "Asset not found: " + assetName;
                return false;
            }
        }
        outDatum.SetType(type);
        outDatum.PushBack(AssetRef(asset));
        return true;
    }
    default:
        outError = "Unsupported property type for set";
        return false;
    }
}

static crow::json::wvalue DatumToJson(const Datum& datum, uint32_t index = 0)
{
    crow::json::wvalue j;
    switch (datum.GetType())
    {
    case DatumType::Integer:
        j = datum.GetInteger(index);
        break;
    case DatumType::Short:
        j = static_cast<int32_t>(datum.GetShort(index));
        break;
    case DatumType::Byte:
        j = static_cast<int32_t>(datum.GetByte(index));
        break;
    case DatumType::Float:
        j = datum.GetFloat(index);
        break;
    case DatumType::Bool:
        j = datum.GetBool(index);
        break;
    case DatumType::String:
        j = datum.GetString(index);
        break;
    case DatumType::Vector2D:
    {
        glm::vec2 v2 = datum.GetVector2D(index);
        j[0] = v2.x;
        j[1] = v2.y;
        break;
    }
    case DatumType::Vector:
    {
        glm::vec3 v3 = datum.GetVector(index);
        j[0] = v3.x;
        j[1] = v3.y;
        j[2] = v3.z;
        break;
    }
    case DatumType::Color:
    {
        glm::vec4 c = datum.GetColor(index);
        j[0] = c.r;
        j[1] = c.g;
        j[2] = c.b;
        j[3] = c.a;
        break;
    }
    case DatumType::Asset:
    case DatumType::Scene:
    case DatumType::Material:
    case DatumType::TileSet:
    case DatumType::TileMap:
    case DatumType::Timeline:
    case DatumType::NodeGraphAsset:
    {
        Asset* a = datum.GetAsset(index);
        j = a ? a->GetName() : "";
        break;
    }
    default:
        j = "(unsupported type)";
        break;
    }
    return j;
}

static crow::json::wvalue PropertyToJson(const Property& prop)
{
    crow::json::wvalue j;
    j["name"] = prop.mName;
    j["type"] = static_cast<int>(prop.GetType());

    uint32_t count = prop.GetCount();
    if (count == 1)
    {
        j["value"] = DatumToJson(prop, 0);
    }
    else
    {
        crow::json::wvalue arr;
        for (uint32_t i = 0; i < count; ++i)
        {
            arr[i] = DatumToJson(prop, i);
        }
        j["value"] = std::move(arr);
    }
    return j;
}

static crow::json::wvalue NodeToJson(Node* node)
{
    crow::json::wvalue j;
    if (node == nullptr)
    {
        return j;
    }

    j["name"] = node->GetName();
    j["type"] = node->GetTypeName();
    j["active"] = node->IsActive();
    j["visible"] = node->IsVisible();

    if (node->IsNode3D())
    {
        Node3D* n3d = static_cast<Node3D*>(node);
        j["position"] = Vec3ToJson(n3d->GetWorldPosition());
        j["rotation"] = Vec3ToJson(n3d->GetWorldRotationEuler());
        j["scale"] = Vec3ToJson(n3d->GetWorldScale());
    }

    return j;
}

static crow::json::wvalue HierarchyToJson(Node* node)
{
    crow::json::wvalue j = NodeToJson(node);
    if (node == nullptr)
    {
        return j;
    }

    const auto& children = node->GetChildren();
    if (!children.empty())
    {
        crow::json::wvalue childArr;
        for (uint32_t i = 0; i < children.size(); ++i)
        {
            childArr[i] = HierarchyToJson(children[i].Get());
        }
        j["children"] = std::move(childArr);
    }

    return j;
}

static Node* FindNodeByIdentifier(World* world, const std::string& identifier)
{
    if (world == nullptr)
    {
        return nullptr;
    }

    Node* node = world->FindNode(identifier);
    return node;
}

static void LogRequest(ControllerServer* server, const char* method, const char* path)
{
    if (server && server->GetLogRequests())
    {
        LogDebug("[ControllerServer] %s %s", method, path);
    }
}

// ---------------------------------------------------------------------------
// Route Registration
// ---------------------------------------------------------------------------

void RegisterRoutes(void* appPtr, ControllerServer* server)
{
    crow::SimpleApp& app = *static_cast<crow::SimpleApp*>(appPtr);
    // ------------------------------------------------------------------
    // GET /api/scene — Current scene info
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/scene").methods("GET"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "GET", "/api/scene");
        RETURN_IF_SHUTTING_DOWN(server);

        auto future = server->QueueCommand([]() -> std::string
        {
            crow::json::wvalue j;

            EditorState* editorState = GetEditorState();
            EditScene* editScene = editorState->GetEditScene();

            if (editScene && editScene->mSceneAsset.Get() != nullptr)
            {
                j["scene"] = editScene->mSceneAsset.Get()->GetName();
            }
            else
            {
                j["scene"] = "";
            }

            j["playing"] = editorState->mPlayInEditor;
            j["paused"] = editorState->mPaused;

            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // POST /api/scene/new — Create a new scene asset
    //   Body: { "name": string (required),
    //           "type": "3D" | "2D" (default "3D"),
    //           "createCamera": bool (default true, 3D only),
    //           "open": bool (default true) }
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/scene/new").methods("POST"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "POST", "/api/scene/new");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string body = req.body;
        auto future = server->QueueCommand([body]() -> std::string
        {
            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("name"))
            {
                return ErrorJson("Missing 'name' field").dump();
            }

            std::string sceneName = parsed["name"].s();
            if (sceneName.empty())
            {
                return ErrorJson("Scene name must be non-empty").dump();
            }

            int sceneTypeInt = 1; // 3D default
            if (parsed.has("type"))
            {
                std::string typeStr = parsed["type"].s();
                if (typeStr == "2D" || typeStr == "2d")
                    sceneTypeInt = 0;
                else if (typeStr == "3D" || typeStr == "3d")
                    sceneTypeInt = 1;
                else
                    return ErrorJson("Invalid 'type' — expected \"3D\" or \"2D\"").dump();
            }

            bool createSkybox = true;
            bool createCamera = true;
            if (parsed.has("createCamera"))
                createCamera = parsed["createCamera"].b();
            if (parsed.has("createSkybox"))
                createSkybox = parsed["createSkybox"].b();

            bool open = true;
            if (parsed.has("open"))
                open = parsed["open"].b();

            Scene* scene = ActionManager::Get()->CreateNewScene(sceneName.c_str(), sceneTypeInt, createCamera, createSkybox);
            if (scene == nullptr)
            {
                return ErrorJson("Failed to create scene: " + sceneName).dump();
            }

            if (open)
            {
                GetEditorState()->OpenEditScene(scene);
            }

            crow::json::wvalue j;
            j["success"] = true;
            j["scene"] = scene->GetName();
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // POST /api/scene/open — Open a scene by name
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/scene/open").methods("POST"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "POST", "/api/scene/open");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string body = req.body;
        auto future = server->QueueCommand([body]() -> std::string
        {
            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("name"))
            {
                return ErrorJson("Missing 'name' field").dump();
            }

            std::string sceneName = parsed["name"].s();
            Asset* asset = AssetManager::Get()->GetAsset(sceneName);
            Scene* scene = asset ? asset->As<Scene>() : nullptr;

            if (scene == nullptr)
            {
                return ErrorJson("Scene not found: " + sceneName).dump();
            }

            GetEditorState()->OpenEditScene(scene);

            crow::json::wvalue j;
            j["success"] = true;
            j["scene"] = sceneName;
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // POST /api/scene/save — Save current scene
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/scene/save").methods("POST"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "POST", "/api/scene/save");
        RETURN_IF_SHUTTING_DOWN(server);

        auto future = server->QueueCommand([]() -> std::string
        {
            ActionManager::Get()->SaveScene(false);

            crow::json::wvalue j;
            j["success"] = true;
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // GET /api/scene/hierarchy — Recursive node tree
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/scene/hierarchy").methods("GET"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "GET", "/api/scene/hierarchy");
        RETURN_IF_SHUTTING_DOWN(server);

        auto future = server->QueueCommand([]() -> std::string
        {
            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            Node* root = world->GetRootNode();
            crow::json::wvalue j = HierarchyToJson(root);
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // PUT /api/scene/hierarchy — Reparent a node
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/scene/hierarchy").methods("PUT"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "PUT", "/api/scene/hierarchy");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string body = req.body;
        auto future = server->QueueCommand([body]() -> std::string
        {
            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("node") || !parsed.has("parent"))
            {
                return ErrorJson("Missing 'node' or 'parent' field").dump();
            }

            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            std::string nodeName = parsed["node"].s();
            std::string parentName = parsed["parent"].s();

            Node* node = FindNodeByIdentifier(world, nodeName);
            Node* parent = FindNodeByIdentifier(world, parentName);

            if (node == nullptr)
            {
                return ErrorJson("Node not found: " + nodeName).dump();
            }
            if (parent == nullptr)
            {
                return ErrorJson("Parent not found: " + parentName).dump();
            }

            int32_t childIndex = -1;
            if (parsed.has("index"))
            {
                childIndex = static_cast<int32_t>(parsed["index"].i());
            }

            ActionManager::Get()->EXE_AttachNode(node, parent, childIndex, -1);

            crow::json::wvalue j;
            j["success"] = true;
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // GET /api/nodes/<name> — Node info
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/nodes/<string>").methods("GET"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "GET", "/api/nodes/<name>");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string nodeName = name;
        auto future = server->QueueCommand([nodeName]() -> std::string
        {
            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            Node* node = FindNodeByIdentifier(world, nodeName);
            if (node == nullptr)
            {
                return ErrorJson("Node not found: " + nodeName).dump();
            }

            return NodeToJson(node).dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // POST /api/nodes — Create a node
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/nodes").methods("POST"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "POST", "/api/nodes");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string body = req.body;
        auto future = server->QueueCommand([body]() -> std::string
        {
            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("type"))
            {
                return ErrorJson("Missing 'type' field").dump();
            }

            std::string typeName = parsed["type"].s();

            Node* newNode = ActionManager::Get()->EXE_SpawnNode(typeName.c_str());
            if (newNode == nullptr)
            {
                return ErrorJson("Failed to spawn node of type: " + typeName).dump();
            }

            if (parsed.has("name"))
            {
                newNode->SetName(parsed["name"].s());
            }

            if (parsed.has("parent"))
            {
                World* world = GetWorld(0);
                std::string parentName = parsed["parent"].s();
                Node* parent = FindNodeByIdentifier(world, parentName);
                if (parent != nullptr)
                {
                    ActionManager::Get()->EXE_AttachNode(newNode, parent, -1, -1);
                }
            }

            return NodeToJson(newNode).dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // DELETE /api/nodes/<name> — Delete a node
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/nodes/<string>/delete").methods("POST"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "POST", "/api/nodes/<name>/delete");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string nodeName = name;
        auto future = server->QueueCommand([nodeName]() -> std::string
        {
            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            Node* node = FindNodeByIdentifier(world, nodeName);
            if (node == nullptr)
            {
                return ErrorJson("Node not found: " + nodeName).dump();
            }

            ActionManager::Get()->EXE_DeleteNode(node);

            crow::json::wvalue j;
            j["success"] = true;
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // PUT /api/nodes/<name>/transform — Set full transform
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/nodes/<string>/transform").methods("PUT"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "PUT", "/api/nodes/<name>/transform");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string nodeName = name;
        std::string body = req.body;
        auto future = server->QueueCommand([nodeName, body]() -> std::string
        {
            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            Node* node = FindNodeByIdentifier(world, nodeName);
            if (node == nullptr)
            {
                return ErrorJson("Node not found: " + nodeName).dump();
            }
            if (!node->IsNode3D())
            {
                return ErrorJson("Node is not a 3D node").dump();
            }

            Node3D* n3d = static_cast<Node3D*>(node);
            auto parsed = crow::json::load(body);
            if (!parsed)
            {
                return ErrorJson("Invalid JSON body").dump();
            }

            if (parsed.has("position"))
            {
                glm::vec3 pos = JsonToVec3(parsed["position"]);
                ActionManager::Get()->EXE_SetWorldPosition(n3d, pos);
            }
            if (parsed.has("rotation"))
            {
                glm::vec3 rot = JsonToVec3(parsed["rotation"]);
                ActionManager::Get()->EXE_SetWorldRotation(n3d, glm::quat(glm::radians(rot)));
            }
            if (parsed.has("scale"))
            {
                glm::vec3 scl = JsonToVec3(parsed["scale"]);
                ActionManager::Get()->EXE_SetWorldScale(n3d, scl);
            }

            return NodeToJson(n3d).dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // PUT /api/nodes/<name>/move — Set position
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/nodes/<string>/move").methods("PUT"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "PUT", "/api/nodes/<name>/move");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string nodeName = name;
        std::string body = req.body;
        auto future = server->QueueCommand([nodeName, body]() -> std::string
        {
            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            Node* node = FindNodeByIdentifier(world, nodeName);
            if (node == nullptr || !node->IsNode3D())
            {
                return ErrorJson("3D Node not found: " + nodeName).dump();
            }

            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("position"))
            {
                return ErrorJson("Missing 'position' field").dump();
            }

            Node3D* n3d = static_cast<Node3D*>(node);
            glm::vec3 pos = JsonToVec3(parsed["position"]);
            ActionManager::Get()->EXE_SetWorldPosition(n3d, pos);

            return NodeToJson(n3d).dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // PUT /api/nodes/<name>/rotate — Set rotation
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/nodes/<string>/rotate").methods("PUT"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "PUT", "/api/nodes/<name>/rotate");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string nodeName = name;
        std::string body = req.body;
        auto future = server->QueueCommand([nodeName, body]() -> std::string
        {
            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            Node* node = FindNodeByIdentifier(world, nodeName);
            if (node == nullptr || !node->IsNode3D())
            {
                return ErrorJson("3D Node not found: " + nodeName).dump();
            }

            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("rotation"))
            {
                return ErrorJson("Missing 'rotation' field").dump();
            }

            Node3D* n3d = static_cast<Node3D*>(node);
            glm::vec3 rot = JsonToVec3(parsed["rotation"]);
            ActionManager::Get()->EXE_SetWorldRotation(n3d, glm::quat(glm::radians(rot)));

            return NodeToJson(n3d).dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // PUT /api/nodes/<name>/scale — Set scale
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/nodes/<string>/scale").methods("PUT"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "PUT", "/api/nodes/<name>/scale");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string nodeName = name;
        std::string body = req.body;
        auto future = server->QueueCommand([nodeName, body]() -> std::string
        {
            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            Node* node = FindNodeByIdentifier(world, nodeName);
            if (node == nullptr || !node->IsNode3D())
            {
                return ErrorJson("3D Node not found: " + nodeName).dump();
            }

            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("scale"))
            {
                return ErrorJson("Missing 'scale' field").dump();
            }

            Node3D* n3d = static_cast<Node3D*>(node);
            glm::vec3 scl = JsonToVec3(parsed["scale"]);
            ActionManager::Get()->EXE_SetWorldScale(n3d, scl);

            return NodeToJson(n3d).dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // PUT /api/nodes/<name>/visibility — Set visibility
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/nodes/<string>/visibility").methods("PUT"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "PUT", "/api/nodes/<name>/visibility");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string nodeName = name;
        std::string body = req.body;
        auto future = server->QueueCommand([nodeName, body]() -> std::string
        {
            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            Node* node = FindNodeByIdentifier(world, nodeName);
            if (node == nullptr)
            {
                return ErrorJson("Node not found: " + nodeName).dump();
            }

            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("visible"))
            {
                return ErrorJson("Missing 'visible' field").dump();
            }

            node->SetVisible(parsed["visible"].b());

            crow::json::wvalue j;
            j["success"] = true;
            j["visible"] = node->IsVisible();
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // GET /api/nodes/<name>/properties — All reflected properties
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/nodes/<string>/properties").methods("GET"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "GET", "/api/nodes/<name>/properties");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string nodeName = name;
        auto future = server->QueueCommand([nodeName]() -> std::string
        {
            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            Node* node = FindNodeByIdentifier(world, nodeName);
            if (node == nullptr)
            {
                return ErrorJson("Node not found: " + nodeName).dump();
            }

            std::vector<Property> props;
            node->GatherProperties(props);

            crow::json::wvalue j;
            crow::json::wvalue propArr;
            for (uint32_t i = 0; i < props.size(); ++i)
            {
                propArr[i] = PropertyToJson(props[i]);
            }
            j["properties"] = std::move(propArr);
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // PUT /api/nodes/<name>/properties — Set a property by name
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/nodes/<string>/properties").methods("PUT"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "PUT", "/api/nodes/<name>/properties");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string nodeName = name;
        std::string body = req.body;
        auto future = server->QueueCommand([nodeName, body]() -> std::string
        {
            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            Node* node = FindNodeByIdentifier(world, nodeName);
            if (node == nullptr)
            {
                return ErrorJson("Node not found: " + nodeName).dump();
            }

            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("name") || !parsed.has("value"))
            {
                return ErrorJson("Missing 'name' or 'value' field").dump();
            }

            std::string propName = parsed["name"].s();

            std::vector<Property> props;
            node->GatherProperties(props);

            for (auto& prop : props)
            {
                if (prop.mName == propName)
                {
                    Datum newValue;
                    std::string err;
                    if (!BuildDatumFromJson(prop, parsed["value"], newValue, err))
                    {
                        return ErrorJson(err).dump();
                    }

                    ActionManager::Get()->EXE_EditProperty(
                        node, PropertyOwnerType::Node, propName, 0, newValue);

                    crow::json::wvalue j;
                    j["success"] = true;
                    return j.dump();
                }
            }

            return ErrorJson("Property not found: " + propName).dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // GET /api/nodes/<name>/script-properties — Script fields
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/nodes/<string>/script-properties").methods("GET"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "GET", "/api/nodes/<name>/script-properties");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string nodeName = name;
        auto future = server->QueueCommand([nodeName]() -> std::string
        {
            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            Node* node = FindNodeByIdentifier(world, nodeName);
            if (node == nullptr)
            {
                return ErrorJson("Node not found: " + nodeName).dump();
            }

            Script* script = node->GetScript();
            if (script == nullptr)
            {
                crow::json::wvalue j;
                crow::json::wvalue emptyArr;
                j["properties"] = std::move(emptyArr);
                return j.dump();
            }

            const std::vector<Property>& scriptProps = script->GetScriptProperties();

            crow::json::wvalue j;
            crow::json::wvalue propArr;
            for (uint32_t i = 0; i < scriptProps.size(); ++i)
            {
                propArr[i] = PropertyToJson(scriptProps[i]);
            }
            j["properties"] = std::move(propArr);
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // PUT /api/nodes/<name>/script-properties — Set script field
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/nodes/<string>/script-properties").methods("PUT"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "PUT", "/api/nodes/<name>/script-properties");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string nodeName = name;
        std::string body = req.body;
        auto future = server->QueueCommand([nodeName, body]() -> std::string
        {
            World* world = GetWorld(0);
            if (world == nullptr)
            {
                return ErrorJson("No active world").dump();
            }

            Node* node = FindNodeByIdentifier(world, nodeName);
            if (node == nullptr)
            {
                return ErrorJson("Node not found: " + nodeName).dump();
            }

            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("name") || !parsed.has("value"))
            {
                return ErrorJson("Missing 'name' or 'value' field").dump();
            }

            std::string fieldName = parsed["name"].s();

            Script* script = node->GetScript();
            if (script == nullptr)
            {
                return ErrorJson("Node has no script").dump();
            }

            // Find the script property to determine type
            const std::vector<Property>& scriptProps = script->GetScriptProperties();
            for (const auto& prop : scriptProps)
            {
                if (prop.mName == fieldName)
                {
                    Datum newValue;
                    std::string err;
                    if (!BuildDatumFromJson(prop, parsed["value"], newValue, err))
                    {
                        return ErrorJson(err).dump();
                    }

                    // Route through EXE_EditProperty (not Script::SetField). The action
                    // re-gathers properties via Node::GatherProperties → Script::AppendScriptProperties,
                    // and the change handler updates BOTH the C++ Property cache (mScriptProps,
                    // which is what scene save reads) AND propagates to Lua via UploadDatum.
                    // Direct Script::SetField only updates Lua, leaving the cache stale —
                    // which is why REST-set values used to disappear after save / scene reload.
                    ActionManager::Get()->EXE_EditProperty(
                        node, PropertyOwnerType::Node, fieldName, 0, newValue);

                    crow::json::wvalue j;
                    j["success"] = true;
                    return j.dump();
                }
            }

            return ErrorJson("Script property not found: " + fieldName).dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // POST /api/play/start — Begin play in editor
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/play/start").methods("POST"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "POST", "/api/play/start");
        RETURN_IF_SHUTTING_DOWN(server);

        auto future = server->QueueCommand([]() -> std::string
        {
            EditorState* state = GetEditorState();
            if (!state->mPlayInEditor)
            {
                state->BeginPlayInEditor();
            }

            crow::json::wvalue j;
            j["success"] = true;
            j["playing"] = true;
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // POST /api/play/stop — End play in editor
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/play/stop").methods("POST"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "POST", "/api/play/stop");
        RETURN_IF_SHUTTING_DOWN(server);

        auto future = server->QueueCommand([]() -> std::string
        {
            EditorState* state = GetEditorState();
            if (state->mPlayInEditor)
            {
                state->EndPlayInEditor();
            }

            crow::json::wvalue j;
            j["success"] = true;
            j["playing"] = false;
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // POST /api/play/pause — Pause play
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/play/pause").methods("POST"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "POST", "/api/play/pause");
        RETURN_IF_SHUTTING_DOWN(server);

        auto future = server->QueueCommand([]() -> std::string
        {
            GetEditorState()->SetPlayInEditorPaused(true);

            crow::json::wvalue j;
            j["success"] = true;
            j["paused"] = true;
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // POST /api/play/resume — Resume play
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/play/resume").methods("POST"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "POST", "/api/play/resume");
        RETURN_IF_SHUTTING_DOWN(server);

        auto future = server->QueueCommand([]() -> std::string
        {
            GetEditorState()->SetPlayInEditorPaused(false);

            crow::json::wvalue j;
            j["success"] = true;
            j["paused"] = false;
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // POST /api/assets/import — Import an asset from disk path
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/assets/import").methods("POST"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "POST", "/api/assets/import");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string body = req.body;
        auto future = server->QueueCommand([body]() -> std::string
        {
            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("path"))
            {
                return ErrorJson("Missing 'path' field").dump();
            }

            std::string path = parsed["path"].s();
            Asset* imported = ActionManager::Get()->ImportAsset(path);

            if (imported == nullptr)
            {
                return ErrorJson("Failed to import asset: " + path).dump();
            }

            crow::json::wvalue j;
            j["success"] = true;
            j["name"] = imported->GetName();
            j["type"] = imported->GetTypeName();
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // GET /api/assets — List all registered assets
    //
    // Query params (all optional):
    //   type    — filter by asset class name (e.g. "MaterialLite", "Texture",
    //             "StaticMesh", "Scene"). Match is exact, case-sensitive.
    //   prefix  — case-sensitive substring match against asset name.
    //   engine  — "1" includes engine assets in the result (default excludes them
    //             so the LLM only sees project-level assets).
    //
    // Response: { "assets": [{name, type, path, uuid, engine}, ...] }
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/assets").methods("GET"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "GET", "/api/assets");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string typeFilter;
        std::string prefixFilter;
        bool includeEngine = false;
        if (const char* s = req.url_params.get("type"))    typeFilter = s;
        if (const char* s = req.url_params.get("prefix"))  prefixFilter = s;
        if (const char* s = req.url_params.get("engine"))  includeEngine = (std::atoi(s) != 0);

        auto future = server->QueueCommand([typeFilter, prefixFilter, includeEngine]() -> std::string
        {
            crow::json::wvalue j;
            crow::json::wvalue arr;
            uint32_t outIdx = 0;

            const auto& assetMap = AssetManager::Get()->GetAssetMap();
            for (const auto& kv : assetMap)
            {
                AssetStub* stub = kv.second;
                if (stub == nullptr) continue;
                if (!includeEngine && stub->mEngineAsset) continue;

                const char* typeName = Asset::GetNameFromTypeId(stub->mType);
                if (!typeFilter.empty() && typeFilter != typeName) continue;

                const std::string& name = kv.first;
                if (!prefixFilter.empty() && name.find(prefixFilter) == std::string::npos) continue;

                crow::json::wvalue item;
                item["name"] = name;
                item["type"] = typeName;
                item["path"] = stub->mPath;
                item["uuid"] = static_cast<int64_t>(stub->mUuid);
                item["engine"] = stub->mEngineAsset;
                arr[outIdx++] = std::move(item);
            }

            j["assets"] = std::move(arr);
            j["count"] = static_cast<int>(outIdx);
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // GET /api/assets/<name> — Asset summary (info, no properties)
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/assets/<string>").methods("GET"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "GET", "/api/assets/<name>");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string assetName = name;
        auto future = server->QueueCommand([assetName]() -> std::string
        {
            AssetStub* stub = AssetManager::Get()->GetAssetStub(assetName);
            if (stub == nullptr)
            {
                return ErrorJson("Asset not found: " + assetName).dump();
            }

            crow::json::wvalue j;
            j["name"] = assetName;
            j["type"] = Asset::GetNameFromTypeId(stub->mType);
            j["path"] = stub->mPath;
            j["uuid"] = static_cast<int64_t>(stub->mUuid);
            j["engine"] = stub->mEngineAsset;
            j["loaded"] = (stub->mAsset != nullptr);
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // GET /api/assets/<name>/properties — All reflected properties of an asset
    //
    // Loads the asset on demand. Same response shape as
    // /api/nodes/<name>/properties.
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/assets/<string>/properties").methods("GET"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "GET", "/api/assets/<name>/properties");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string assetName = name;
        auto future = server->QueueCommand([assetName]() -> std::string
        {
            Asset* asset = AssetManager::Get()->LoadAsset(assetName);
            if (asset == nullptr)
            {
                return ErrorJson("Asset not found: " + assetName).dump();
            }

            std::vector<Property> props;
            asset->GatherProperties(props);

            crow::json::wvalue j;
            crow::json::wvalue propArr;
            for (uint32_t i = 0; i < props.size(); ++i)
            {
                propArr[i] = PropertyToJson(props[i]);
            }
            j["properties"] = std::move(propArr);
            j["type"] = asset->GetTypeName();
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // PUT /api/assets/<name>/properties — Set a property by name
    //
    // Body: { "name": "Color", "value": [1.0, 1.0, 1.0, 1.0] }
    //
    // Goes through ActionManager::EXE_EditProperty so the asset is marked
    // dirty and undo/redo works. Asset is NOT automatically saved — call
    // POST /api/assets/<name>/save to persist.
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/assets/<string>/properties").methods("PUT"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "PUT", "/api/assets/<name>/properties");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string assetName = name;
        std::string body = req.body;
        auto future = server->QueueCommand([assetName, body]() -> std::string
        {
            Asset* asset = AssetManager::Get()->LoadAsset(assetName);
            if (asset == nullptr)
            {
                return ErrorJson("Asset not found: " + assetName).dump();
            }

            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("name") || !parsed.has("value"))
            {
                return ErrorJson("Missing 'name' or 'value' field").dump();
            }

            std::string propName = parsed["name"].s();

            std::vector<Property> props;
            asset->GatherProperties(props);

            for (auto& prop : props)
            {
                if (prop.mName == propName)
                {
                    Datum newValue;
                    std::string err;
                    if (!BuildDatumFromJson(prop, parsed["value"], newValue, err))
                    {
                        return ErrorJson(err).dump();
                    }

                    ActionManager::Get()->EXE_EditProperty(
                        asset, PropertyOwnerType::Asset, propName, 0, newValue);

                    crow::json::wvalue j;
                    j["success"] = true;
                    return j.dump();
                }
            }

            return ErrorJson("Property not found: " + propName).dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // POST /api/assets — Create and register a new asset
    //
    // Body: { "type": "MaterialLite", "name": "M_Background", "directory": "Materials" }
    //
    // The directory path is project-relative (e.g. "Materials" creates the
    // asset under <Project>/Materials). Returns the new asset's name + type
    // on success.
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/assets").methods("POST"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "POST", "/api/assets");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string body = req.body;
        auto future = server->QueueCommand([body]() -> std::string
        {
            auto parsed = crow::json::load(body);
            if (!parsed || !parsed.has("type") || !parsed.has("name") || !parsed.has("directory"))
            {
                return ErrorJson("Missing required field: 'type', 'name', or 'directory'").dump();
            }

            std::string typeStr = parsed["type"].s();
            std::string assetName = parsed["name"].s();
            std::string dirPath = parsed["directory"].s();

            TypeId typeId = Asset::GetTypeIdFromName(typeStr.c_str());
            if (typeId == INVALID_TYPE_ID)
            {
                return ErrorJson("Unknown asset type: " + typeStr).dump();
            }

            AssetDir* dir = AssetManager::Get()->GetAssetDirFromPath(dirPath);
            if (dir == nullptr)
            {
                return ErrorJson("Asset directory not found: " + dirPath).dump();
            }

            AssetStub* stub = AssetManager::Get()->CreateAndRegisterAsset(typeId, dir, assetName, false);
            if (stub == nullptr || stub->mAsset == nullptr)
            {
                return ErrorJson("Failed to create asset (name may already exist)").dump();
            }

            crow::json::wvalue j;
            j["success"] = true;
            j["name"] = stub->mAsset->GetName();
            j["type"] = stub->mAsset->GetTypeName();
            j["path"] = stub->mPath;
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // POST /api/assets/<name>/save — Persist a dirty asset to disk
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/assets/<string>/save").methods("POST"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "POST", "/api/assets/<name>/save");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string assetName = name;
        auto future = server->QueueCommand([assetName]() -> std::string
        {
            AssetStub* stub = AssetManager::Get()->GetAssetStub(assetName);
            if (stub == nullptr)
            {
                return ErrorJson("Asset not found: " + assetName).dump();
            }

            AssetManager::Get()->SaveAsset(assetName);

            crow::json::wvalue j;
            j["success"] = true;
            j["name"] = assetName;
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // POST /api/assets/<name>/delete — Purge an asset
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/assets/<string>/delete").methods("POST"_method)
    ([server](const crow::request& req, const std::string& name)
    {
        LogRequest(server, "POST", "/api/assets/<name>/delete");
        RETURN_IF_SHUTTING_DOWN(server);

        std::string assetName = name;
        auto future = server->QueueCommand([assetName]() -> std::string
        {
            if (!AssetManager::Get()->DoesAssetExist(assetName))
            {
                return ErrorJson("Asset not found: " + assetName).dump();
            }

            bool ok = AssetManager::Get()->PurgeAsset(assetName.c_str());
            if (!ok)
            {
                return ErrorJson("Failed to purge asset: " + assetName).dump();
            }

            crow::json::wvalue j;
            j["success"] = true;
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // GET /api/log — Tail editor debug log entries
    //
    // Query params (all optional):
    //   since        — return entries with seq > since (default 0 = all buffered)
    //   limit        — max entries to return (default 200, capped at 2048)
    //   minSeverity  — 0=Debug (all), 1=Warning+, 2=Error only (default 0)
    //
    // Response:
    //   { "entries": [{seq, severity, severityName, timestamp, message}, ...],
    //     "nextSeq": <latest seq>,
    //     "dropped": <bool — true if older entries were evicted from the ring> }
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/log").methods("GET"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "GET", "/api/log");
        RETURN_IF_SHUTTING_DOWN(server);

        uint64_t sinceSeq = 0;
        uint32_t limit = 200;
        uint32_t minSeverity = 0;

        if (const char* s = req.url_params.get("since"))
            sinceSeq = static_cast<uint64_t>(std::strtoull(s, nullptr, 10));
        if (const char* s = req.url_params.get("limit"))
        {
            int v = std::atoi(s);
            if (v < 1) v = 1;
            if (v > 2048) v = 2048;
            limit = static_cast<uint32_t>(v);
        }
        if (const char* s = req.url_params.get("minSeverity"))
        {
            int v = std::atoi(s);
            if (v < 0) v = 0;
            if (v > 2) v = 2;
            minSeverity = static_cast<uint32_t>(v);
        }

        auto future = server->QueueCommand([sinceSeq, limit, minSeverity]() -> std::string
        {
            DebugLogWindow* logWin = GetDebugLogWindow();
            if (logWin == nullptr)
            {
                return ErrorJson("Debug log unavailable").dump();
            }

            std::vector<DebugLogEntry> entries;
            uint64_t nextSeq = 0;
            logWin->GetEntriesSnapshot(sinceSeq, limit, entries, nextSeq);

            crow::json::wvalue j;
            crow::json::wvalue arr;
            uint32_t outIdx = 0;
            for (const auto& e : entries)
            {
                if (static_cast<uint32_t>(e.mSeverity) < minSeverity)
                    continue;

                const char* sevName = "Debug";
                switch (e.mSeverity)
                {
                case LogSeverity::Warning: sevName = "Warning"; break;
                case LogSeverity::Error:   sevName = "Error";   break;
                default:                   sevName = "Debug";   break;
                }

                crow::json::wvalue item;
                item["seq"] = static_cast<int64_t>(e.mSeq);
                item["severity"] = static_cast<int>(e.mSeverity);
                item["severityName"] = sevName;
                item["timestamp"] = e.mTimestamp;
                item["message"] = e.mMessage;
                arr[outIdx++] = std::move(item);
            }

            bool dropped = false;
            if (sinceSeq > 0 && !entries.empty() && entries.front().mSeq > sinceSeq + 1)
                dropped = true;

            j["entries"] = std::move(arr);
            j["nextSeq"] = static_cast<int64_t>(nextSeq);
            j["dropped"] = dropped;
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // GET /api/screenshot — PNG of the Game Preview viewport
    //
    // Query params (all optional):
    //   width  — downscale to this width (preserves aspect; only if < native)
    //
    // Response:
    //   { "format": "png", "width": <w>, "height": <h>, "data": "<base64>" }
    //
    // Requires Game Preview to be enabled and rendered at least once.
    // Vulkan-only (other backends return an error).
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/screenshot").methods("GET"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "GET", "/api/screenshot");
        RETURN_IF_SHUTTING_DOWN(server);

        uint32_t targetWidth = 0; // 0 = native
        if (const char* s = req.url_params.get("width"))
        {
            int v = std::atoi(s);
            if (v < 16) v = 16;
            if (v > 8192) v = 8192;
            targetWidth = static_cast<uint32_t>(v);
        }

        auto future = server->QueueCommand([targetWidth]() -> std::string
        {
            GamePreview* preview = GetGamePreview();
            if (preview == nullptr)
            {
                return ErrorJson("Game Preview unavailable").dump();
            }

            std::vector<uint8_t> rgba;
            uint32_t w = 0;
            uint32_t h = 0;
            if (!preview->CaptureScreenshotToMemory(rgba, w, h))
            {
                return ErrorJson("Game Preview not currently rendered (enable it in the editor)").dump();
            }

            return EncodeScreenshotResponse(rgba, w, h, targetWidth);
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // GET /api/screenshot/editor — PNG of the entire editor window
    //
    // Captures the swapchain image after the next frame is rendered, so the
    // result includes ImGui chrome (inspector, hierarchy, debug log) plus the
    // Game Preview viewport. Useful for inspecting UI/widget authoring or any
    // editor panel state — anything you'd see by glancing at the editor.
    //
    // Query params (all optional):
    //   width  — downscale to this width (preserves aspect; only if < native)
    //
    // Response: same shape as /api/screenshot.
    // Vulkan-only. Latency: one render frame (~16 ms typical).
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/screenshot/editor").methods("GET"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "GET", "/api/screenshot/editor");
        RETURN_IF_SHUTTING_DOWN(server);

        uint32_t targetWidth = 0;
        if (const char* s = req.url_params.get("width"))
        {
            int v = std::atoi(s);
            if (v < 16) v = 16;
            if (v > 8192) v = 8192;
            targetWidth = static_cast<uint32_t>(v);
        }

        // Capture happens at post-render time; not via QueueCommand. The
        // promise is fulfilled by ProcessPendingEditorScreenshots() during
        // the next Renderer::Render call, just before vkQueuePresent.
        auto promise = std::make_shared<std::promise<EditorScreenshotData>>();
        auto future = promise->get_future();
        RequestEditorScreenshot(promise);

        auto status = future.wait_for(std::chrono::seconds(2));
        if (status != std::future_status::ready)
        {
            return crow::response(200, "application/json",
                ErrorJson("Editor screenshot timed out (no render frame within 2s)").dump());
        }

        EditorScreenshotData shot = future.get();
        if (!shot.mOk)
        {
            std::string msg = shot.mError.empty() ? std::string("Editor screenshot failed") : shot.mError;
            return crow::response(200, "application/json", ErrorJson(msg).dump());
        }

        std::string result = EncodeScreenshotResponse(shot.mRgba, shot.mWidth, shot.mHeight, targetWidth);
        return crow::response(200, "application/json", result);
    });

    // ------------------------------------------------------------------
    // GET /api/preferences — Dump all preferences
    // ------------------------------------------------------------------
    CROW_ROUTE(app, "/api/preferences").methods("GET"_method)
    ([server](const crow::request& req)
    {
        LogRequest(server, "GET", "/api/preferences");
        RETURN_IF_SHUTTING_DOWN(server);

        auto future = server->QueueCommand([]() -> std::string
        {
            crow::json::wvalue j;
            j["controllerServer"]["enabled"] = true;
            j["controllerServer"]["running"] = ControllerServer::Get()->IsRunning();
            return j.dump();
        });

        return crow::response(200, "application/json", WaitForCommand(future));
    });

    // ------------------------------------------------------------------
    // Addon routes — Forward to registered addon callbacks
    // ------------------------------------------------------------------
    CROW_CATCHALL_ROUTE(app)
    ([server](const crow::request& req)
    {
        RETURN_IF_SHUTTING_DOWN(server);

        // Check addon routes
        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr != nullptr)
        {
            const auto& addonRoutes = hookMgr->GetControllerRoutes();
            for (const auto& route : addonRoutes)
            {
                if (route.mPath == req.url && route.mMethod == crow::method_name(req.method))
                {
                    LogRequest(server, route.mMethod.c_str(), route.mPath.c_str());

                    char responseBuffer[4096] = {};
                    route.mCallback(
                        route.mMethod.c_str(),
                        route.mPath.c_str(),
                        req.body.c_str(),
                        responseBuffer,
                        sizeof(responseBuffer),
                        route.mUserData);

                    return crow::response(200, "application/json", std::string(responseBuffer));
                }
            }
        }

        return crow::response(404, "application/json",
            ErrorJson("Unknown endpoint: " + req.url).dump());
    });
}

#endif
