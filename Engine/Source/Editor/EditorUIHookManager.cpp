#if EDITOR

#include "EditorUIHookManager.h"
#include "EditorImageCache.h"
#include "EditorImgui.h"
#include "EditorState.h"
#include "Engine.h"
#include "Log.h"
#include "InputDevices.h"
#include "Renderer.h"
#include "World.h"
#include "Maths.h"
#include "Nodes/3D/Camera3d.h"
#include "Nodes/3D/Primitive3d.h"
#include "Plugins/ImGuiPluginContext.h"
#include "Profiling/ProfilingWindow.h"
#include "Packaging/BuiltInBuildTargets.h"
#include "System/System.h"

#include "imgui.h"
#include "imgui_dock.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <functional>

// ---------------------------------------------------------------------------
// Slash-path menu rendering
// ---------------------------------------------------------------------------
//
// Several hook APIs accept an `itemPath` string (e.g. AddMenuItem(menuPath,
// itemPath, …), AddCreateAssetItem(itemPath, …)). For convenience the path
// supports '/'-separated nesting so addons can write:
//
//     hooks->AddMenuItem(hookId, "Tools", "My Addon/Features/Toggle A", …);
//     hooks->AddCreateAssetItem(hookId, "WorldStream/World Sector", …);
//
// and the dispatcher emits the right nested ImGui::BeginMenu/EndMenu calls.
// Sibling items sharing a prefix (e.g. "My Addon/Features/Toggle A" and
// "My Addon/Features/Toggle B") merge under the same `My Addon > Features`
// submenu — the helper builds an in-place tree first so it doesn't create
// duplicate intermediate menus.
//
// Items without any '/' degrade to a single MenuItem call (zero behavioural
// change for the bulk of existing callers).
namespace
{
    struct SlashMenuNode
    {
        std::string mLabel;          // segment label at this level
        std::function<void()> mAction; // leaf-only; if null, this is a submenu
        std::string mShortcut;        // leaf-only
        bool mEnabled = true;        // leaf-only
        bool mIsSeparator = false;   // leaf-only
        std::vector<SlashMenuNode> mChildren;
    };

    std::vector<std::string> SplitSlashPath(const std::string& path)
    {
        std::vector<std::string> segs;
        size_t start = 0;
        for (size_t i = 0; i < path.size(); ++i)
        {
            if (path[i] == '/')
            {
                if (i > start) segs.emplace_back(path.substr(start, i - start));
                start = i + 1;
            }
        }
        if (start < path.size()) segs.emplace_back(path.substr(start));
        return segs;
    }

    // Insert a leaf into the tree under the given path. Intermediate segments
    // reuse existing submenu nodes whose label matches; if none match, a new
    // submenu node is appended at the end of the parent's children (preserving
    // registration order — siblings remain in the order they were inserted).
    void InsertSlashLeaf(SlashMenuNode& root,
                         const std::string& path,
                         std::function<void()> action,
                         const std::string& shortcut,
                         bool enabled)
    {
        const std::vector<std::string> segs = SplitSlashPath(path);
        if (segs.empty())
        {
            // Path was empty or all-slash. Render raw text as a leaf so the
            // entry isn't silently dropped — surfaces the bug to the author.
            SlashMenuNode leaf;
            leaf.mLabel = path;
            leaf.mAction = std::move(action);
            leaf.mShortcut = shortcut;
            leaf.mEnabled = enabled;
            root.mChildren.push_back(std::move(leaf));
            return;
        }

        SlashMenuNode* cur = &root;
        for (size_t i = 0; i + 1 < segs.size(); ++i)
        {
            SlashMenuNode* child = nullptr;
            // Match an existing submenu (not leaf, not separator) with same label.
            for (SlashMenuNode& c : cur->mChildren)
            {
                if (!c.mAction && !c.mIsSeparator && c.mLabel == segs[i])
                {
                    child = &c;
                    break;
                }
            }
            if (child == nullptr)
            {
                SlashMenuNode newNode;
                newNode.mLabel = segs[i];
                cur->mChildren.push_back(std::move(newNode));
                child = &cur->mChildren.back();
            }
            cur = child;
        }

        SlashMenuNode leaf;
        leaf.mLabel = segs.back();
        leaf.mAction = std::move(action);
        leaf.mShortcut = shortcut;
        leaf.mEnabled = enabled;
        cur->mChildren.push_back(std::move(leaf));
    }

    void InsertSlashSeparator(SlashMenuNode& root)
    {
        SlashMenuNode sep;
        sep.mIsSeparator = true;
        root.mChildren.push_back(std::move(sep));
    }

    void DrawSlashMenuTree(const SlashMenuNode& node)
    {
        for (const SlashMenuNode& child : node.mChildren)
        {
            if (child.mIsSeparator)
            {
                ImGui::Separator();
                continue;
            }
            if (child.mAction)
            {
                const char* shortcut = child.mShortcut.empty() ? nullptr : child.mShortcut.c_str();
                if (ImGui::MenuItem(child.mLabel.c_str(), shortcut, false, child.mEnabled))
                {
                    child.mAction();
                }
            }
            else
            {
                if (ImGui::BeginMenu(child.mLabel.c_str()))
                {
                    DrawSlashMenuTree(child);
                    ImGui::EndMenu();
                }
            }
        }
    }
}

void GetImGuiPluginContext(ImGuiPluginContext* outCtx)
{
    if (outCtx)
    {
        outCtx->context = ImGui::GetCurrentContext();
        ImGui::GetAllocatorFunctions(
            &outCtx->allocFunc,
            &outCtx->freeFunc,
            &outCtx->allocUserData
        );
    }
}

EditorUIHookManager* EditorUIHookManager::sInstance = nullptr;

// ===== Helper Functions Used by InitializeHooks =====

static void Hook_OpenWindow(const char* windowId)
{
    EditorUIHookManager* mgr = EditorUIHookManager::Get();
    if (mgr) mgr->OpenWindow(windowId ? windowId : "");
}

static void Hook_CloseWindow(const char* windowId)
{
    EditorUIHookManager* mgr = EditorUIHookManager::Get();
    if (mgr) mgr->CloseWindow(windowId ? windowId : "");
}

static bool Hook_IsWindowOpen(const char* windowId)
{
    EditorUIHookManager* mgr = EditorUIHookManager::Get();
    return mgr ? mgr->IsWindowOpen(windowId ? windowId : "") : false;
}

static void Hook_RemoveAllHooks(HookId hookId)
{
    EditorUIHookManager* mgr = EditorUIHookManager::Get();
    if (mgr) mgr->RemoveAllHooks(hookId);
}

// ===== EditorUIHookManager Implementation =====

void EditorUIHookManager::Create()
{
    if (sInstance == nullptr)
    {
        sInstance = new EditorUIHookManager();
    }
}

void EditorUIHookManager::Destroy()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

EditorUIHookManager* EditorUIHookManager::Get()
{
    return sInstance;
}

EditorUIHookManager::EditorUIHookManager()
{
    InitializeHooks();
    // Register the six built-in build targets so they appear in the same
    // registry as addon-provided ones. Addons hot-loaded later append.
    BuiltInBuildTargets::RegisterAll(mBuildTargets);
}

EditorUIHookManager::~EditorUIHookManager()
{
}

void EditorUIHookManager::InitializeHooks()
{
    // Set up the hooks struct with function pointers
    // These need to call back into this manager

    // For AddMenuItem, we need a static function that can access the manager
    mHooks.AddMenuItem = [](HookId hookId, const char* menuPath, const char* itemPath,
                            MenuCallback callback, void* userData, const char* shortcut) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredMenuItem item;
        item.mHookId = hookId;
        item.mMenuPath = menuPath ? menuPath : "";
        item.mItemPath = itemPath ? itemPath : "";
        item.mCallback = callback;
        item.mUserData = userData;
        item.mShortcut = shortcut ? shortcut : "";
        item.mIsSeparator = false;

        mgr->mMenuItems[item.mMenuPath].push_back(item);
    };

    mHooks.AddMenuSeparator = [](HookId hookId, const char* menuPath) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredMenuItem item;
        item.mHookId = hookId;
        item.mMenuPath = menuPath ? menuPath : "";
        item.mIsSeparator = true;

        mgr->mMenuItems[item.mMenuPath].push_back(item);
    };

    mHooks.RemoveMenuItem = [](HookId hookId, const char* menuPath, const char* itemPath) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string menu = menuPath ? menuPath : "";
        std::string path = itemPath ? itemPath : "";

        auto it = mgr->mMenuItems.find(menu);
        if (it != mgr->mMenuItems.end())
        {
            auto& items = it->second;
            items.erase(std::remove_if(items.begin(), items.end(),
                [hookId, &path](const RegisteredMenuItem& item) {
                    return item.mHookId == hookId && item.mItemPath == path;
                }), items.end());
        }
    };

    mHooks.RegisterWindow = [](HookId hookId, const char* windowName, const char* windowId,
                               WindowDrawCallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredWindow win;
        win.mHookId = hookId;
        win.mWindowName = windowName ? windowName : "";
        win.mWindowId = windowId ? windowId : "";
        win.mDrawFunc = drawFunc;
        win.mUserData = userData;
        win.mIsOpen = false;

        mgr->mWindows.push_back(win);
    };

    mHooks.UnregisterWindow = [](HookId hookId, const char* windowId) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string id = windowId ? windowId : "";
        mgr->mWindows.erase(std::remove_if(mgr->mWindows.begin(), mgr->mWindows.end(),
            [hookId, &id](const RegisteredWindow& win) {
                return win.mHookId == hookId && win.mWindowId == id;
            }), mgr->mWindows.end());
    };

    mHooks.OpenWindow = Hook_OpenWindow;
    mHooks.CloseWindow = Hook_CloseWindow;
    mHooks.IsWindowOpen = Hook_IsWindowOpen;

    mHooks.RegisterInspector = [](HookId hookId, const char* nodeTypeName,
                                  InspectorDrawCallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredInspector insp;
        insp.mHookId = hookId;
        insp.mNodeTypeName = nodeTypeName ? nodeTypeName : "";
        insp.mDrawFunc = drawFunc;
        insp.mUserData = userData;

        mgr->mInspectors.push_back(insp);
    };

    mHooks.UnregisterInspector = [](HookId hookId, const char* nodeTypeName) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string typeName = nodeTypeName ? nodeTypeName : "";
        mgr->mInspectors.erase(std::remove_if(mgr->mInspectors.begin(), mgr->mInspectors.end(),
            [hookId, &typeName](const RegisteredInspector& insp) {
                return insp.mHookId == hookId && insp.mNodeTypeName == typeName;
            }), mgr->mInspectors.end());
    };

    mHooks.AddNodeContextItem = [](HookId hookId, const char* itemPath,
                                   MenuCallback callback, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredContextItem ctx;
        ctx.mHookId = hookId;
        ctx.mItemPath = itemPath ? itemPath : "";
        ctx.mCallback = callback;
        ctx.mUserData = userData;
        ctx.mIsNodeContext = true;

        mgr->mContextItems.push_back(ctx);
    };

    mHooks.AddAssetContextItem = [](HookId hookId, const char* itemPath,
                                    const char* assetTypeFilter,
                                    MenuCallback callback, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredContextItem ctx;
        ctx.mHookId = hookId;
        ctx.mItemPath = itemPath ? itemPath : "";
        ctx.mAssetTypeFilter = assetTypeFilter ? assetTypeFilter : "*";
        ctx.mCallback = callback;
        ctx.mUserData = userData;
        ctx.mIsNodeContext = false;

        mgr->mContextItems.push_back(ctx);
    };

    mHooks.RemoveAllHooks = Hook_RemoveAllHooks;

    // ===== Top-Level Menus =====

    mHooks.AddTopLevelMenuItem = [](HookId hookId, const char* menuName,
                                    TopLevelMenuDrawCallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredTopLevelMenu menu;
        menu.mHookId = hookId;
        menu.mMenuName = menuName ? menuName : "";
        menu.mDrawFunc = drawFunc;
        menu.mUserData = userData;

        mgr->mTopLevelMenus.push_back(menu);
    };

    mHooks.RemoveTopLevelMenuItem = [](HookId hookId, const char* menuName) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string name = menuName ? menuName : "";
        mgr->mTopLevelMenus.erase(std::remove_if(mgr->mTopLevelMenus.begin(), mgr->mTopLevelMenus.end(),
            [hookId, &name](const RegisteredTopLevelMenu& menu) {
                return menu.mHookId == hookId && menu.mMenuName == name;
            }), mgr->mTopLevelMenus.end());
    };

    // ===== Toolbar =====

    mHooks.AddToolbarItem = [](HookId hookId, const char* itemName,
                               ToolbarDrawCallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredToolbarItem item;
        item.mHookId = hookId;
        item.mItemName = itemName ? itemName : "";
        item.mDrawFunc = drawFunc;
        item.mUserData = userData;

        mgr->mToolbarItems.push_back(item);
    };

    mHooks.RemoveToolbarItem = [](HookId hookId, const char* itemName) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string name = itemName ? itemName : "";
        mgr->mToolbarItems.erase(std::remove_if(mgr->mToolbarItems.begin(), mgr->mToolbarItems.end(),
            [hookId, &name](const RegisteredToolbarItem& item) {
                return item.mHookId == hookId && item.mItemName == name;
            }), mgr->mToolbarItems.end());
    };

    // ===== Project Lifecycle Events =====

    mHooks.RegisterOnProjectOpen = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnProjectOpen.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnProjectClose = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnProjectClose.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnProjectSave = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnProjectSave.push_back({hookId, cb, userData});
    };

    // ===== Scene Lifecycle Events =====

    mHooks.RegisterOnSceneOpen = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnSceneOpen.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnSceneClose = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnSceneClose.push_back({hookId, cb, userData});
    };

    // ===== Packaging/Build Events =====

    mHooks.RegisterOnPackageStarted = [](HookId hookId, PlatformEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnPackageStarted.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnPackageFinished = [](HookId hookId, PackageFinishedCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnPackageFinished.push_back({hookId, cb, userData});
    };

    // ===== Editor State Events =====

    mHooks.RegisterOnSelectionChanged = [](HookId hookId, EventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnSelectionChanged.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnPlayModeChanged = [](HookId hookId, PlayModeCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnPlayModeChanged.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnEditorShutdown = [](HookId hookId, EventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnEditorShutdown.push_back({hookId, cb, userData});
    };

    // ===== Asset Pipeline Events =====

    mHooks.RegisterOnAssetImported = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnAssetImported.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnAssetDeleted = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnAssetDeleted.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnAssetSaved = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnAssetSaved.push_back({hookId, cb, userData});
    };

    // ===== Asset Open Events =====

    mHooks.RegisterOnAssetOpen = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnAssetOpen.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnAssetOpened = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnAssetOpened.push_back({hookId, cb, userData});
    };

    // ===== Undo/Redo =====

    mHooks.RegisterOnUndoRedo = [](HookId hookId, EventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnUndoRedo.push_back({hookId, cb, userData});
    };

    // ===== Drag-and-Drop Events =====

    mHooks.RegisterOnAssetDropHierarchy = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnAssetDropHierarchy.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnAssetDropViewport = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnAssetDropViewport.push_back({hookId, cb, userData});
    };

    // ===== Batch 1: Menu Positioning & Top-Level Menu Control =====

    mHooks.AddTopLevelMenuItemEx = [](HookId hookId, const char* menuName,
                                       TopLevelMenuDrawCallback drawFunc, void* userData, int32_t position) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredTopLevelMenu menu;
        menu.mHookId = hookId;
        menu.mMenuName = menuName ? menuName : "";
        menu.mDrawFunc = drawFunc;
        menu.mUserData = userData;
        menu.mPosition = position;

        mgr->mTopLevelMenus.push_back(menu);
    };

    mHooks.AddMenuItemEx = [](HookId hookId, const char* menuPath, const char* itemPath,
                               MenuCallback callback, void* userData, const char* shortcut,
                               MenuValidationCallback validateFunc) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredMenuItemEx item;
        item.mHookId = hookId;
        item.mMenuPath = menuPath ? menuPath : "";
        item.mItemPath = itemPath ? itemPath : "";
        item.mCallback = callback;
        item.mUserData = userData;
        item.mShortcut = shortcut ? shortcut : "";
        item.mValidateFunc = validateFunc;

        mgr->mMenuItemsEx.push_back(item);
    };

    // ===== Batch 2: Create/Spawn Menu Extensions =====

    mHooks.AddNodeMenuItems = [](HookId hookId, const char* category,
                                  MenuSectionDrawCallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredNodeMenuItems item;
        item.mHookId = hookId;
        item.mCategory = category ? category : "";
        item.mDrawFunc = drawFunc;
        item.mUserData = userData;

        mgr->mNodeMenuItems.push_back(item);
    };

    mHooks.RemoveNodeMenuItems = [](HookId hookId, const char* category) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string cat = category ? category : "";
        mgr->mNodeMenuItems.erase(std::remove_if(mgr->mNodeMenuItems.begin(), mgr->mNodeMenuItems.end(),
            [hookId, &cat](const RegisteredNodeMenuItems& item) {
                return item.mHookId == hookId && item.mCategory == cat;
            }), mgr->mNodeMenuItems.end());
    };

    mHooks.AddCreateAssetItems = [](HookId hookId, MenuSectionDrawCallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredCreateAssetItems item;
        item.mHookId = hookId;
        item.mDrawFunc = drawFunc;
        item.mUserData = userData;

        mgr->mCreateAssetItems.push_back(item);
    };

    mHooks.RemoveCreateAssetItems = [](HookId hookId) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        mgr->mCreateAssetItems.erase(std::remove_if(mgr->mCreateAssetItems.begin(), mgr->mCreateAssetItems.end(),
            [hookId](const RegisteredCreateAssetItems& item) {
                return item.mHookId == hookId;
            }), mgr->mCreateAssetItems.end());
    };

    // Declarative singular variant: addon supplies (itemPath, callback). The
    // dispatcher splits itemPath on '/' into nested BeginMenu/EndMenu so an
    // addon can register "WorldStream/World Sector" without writing any
    // ImGui code. The callback-based AddCreateAssetItems above is still
    // supported for callers that want full ImGui control.
    mHooks.AddCreateAssetItem = [](HookId hookId, const char* itemPath,
                                   MenuCallback callback, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr || itemPath == nullptr || *itemPath == '\0') return;

        RegisteredCreateAssetItem item;
        item.mHookId   = hookId;
        item.mPath     = itemPath;
        item.mCallback = callback;
        item.mUserData = userData;
        mgr->mCreateAssetItemSingles.push_back(std::move(item));
    };

    // ===== Batch 11: Build Targets =====
    mHooks.RegisterBuildTarget = [](HookId hookId, const PolyphaseBuildTargetDesc* desc) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr || desc == nullptr) return;
        if (desc->apiVersion != POLYPHASE_BUILD_TARGET_API_VERSION)
        {
            LogWarning("RegisterBuildTarget: apiVersion mismatch (got %u, expected %u) for id '%s'",
                       desc->apiVersion, (unsigned)POLYPHASE_BUILD_TARGET_API_VERSION,
                       desc->targetId ? desc->targetId : "<null>");
            return;
        }
        mgr->mBuildTargets.Register(hookId, desc, /*isBuiltIn=*/ false);
    };

    mHooks.UnregisterBuildTarget = [](HookId hookId, const char* targetId) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mBuildTargets.Unregister(hookId, targetId);
    };

    mHooks.AddSpawnBasic3dItems = [](HookId hookId, MenuSectionDrawCallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredSpawnItems item;
        item.mHookId = hookId;
        item.mDrawFunc = drawFunc;
        item.mUserData = userData;

        mgr->mSpawnBasic3dItems.push_back(item);
    };

    mHooks.AddSpawnBasicWidgetItems = [](HookId hookId, MenuSectionDrawCallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredSpawnItems item;
        item.mHookId = hookId;
        item.mDrawFunc = drawFunc;
        item.mUserData = userData;

        mgr->mSpawnBasicWidgetItems.push_back(item);
    };

    // ===== Scene Type Registration =====

    mHooks.RegisterSceneType = [](HookId hookId, const char* typeName,
                                   SceneCreationCallback createFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredSceneType sceneType;
        sceneType.mHookId = hookId;
        sceneType.mTypeName = typeName ? typeName : "";
        sceneType.mCreateFunc = createFunc;
        sceneType.mUserData = userData;

        mgr->mSceneTypes.push_back(sceneType);
    };

    mHooks.UnregisterSceneType = [](HookId hookId, const char* typeName) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string name = typeName ? typeName : "";
        mgr->mSceneTypes.erase(std::remove_if(mgr->mSceneTypes.begin(), mgr->mSceneTypes.end(),
            [hookId, &name](const RegisteredSceneType& st) {
                return st.mHookId == hookId && st.mTypeName == name;
            }), mgr->mSceneTypes.end());
    };

    // ===== Batch 3: Viewport Context Menu & Overlay Drawing =====

    mHooks.AddViewportContextItem = [](HookId hookId, const char* itemPath,
                                        MenuCallback callback, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredViewportContextItem item;
        item.mHookId = hookId;
        item.mItemPath = itemPath ? itemPath : "";
        item.mCallback = callback;
        item.mUserData = userData;

        mgr->mViewportContextItems.push_back(item);
    };

    mHooks.RemoveViewportContextItem = [](HookId hookId, const char* itemPath) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string path = itemPath ? itemPath : "";
        mgr->mViewportContextItems.erase(std::remove_if(mgr->mViewportContextItems.begin(), mgr->mViewportContextItems.end(),
            [hookId, &path](const RegisteredViewportContextItem& item) {
                return item.mHookId == hookId && item.mItemPath == path;
            }), mgr->mViewportContextItems.end());
    };

    mHooks.RegisterViewportOverlay = [](HookId hookId, const char* overlayName,
                                         ViewportOverlayCallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredViewportOverlay overlay;
        overlay.mHookId = hookId;
        overlay.mOverlayName = overlayName ? overlayName : "";
        overlay.mDrawFunc = drawFunc;
        overlay.mUserData = userData;

        mgr->mViewportOverlays.push_back(overlay);
    };

    mHooks.UnregisterViewportOverlay = [](HookId hookId, const char* overlayName) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string name = overlayName ? overlayName : "";
        mgr->mViewportOverlays.erase(std::remove_if(mgr->mViewportOverlays.begin(), mgr->mViewportOverlays.end(),
            [hookId, &name](const RegisteredViewportOverlay& overlay) {
                return overlay.mHookId == hookId && overlay.mOverlayName == name;
            }), mgr->mViewportOverlays.end());
    };

    // ===== Batch 4: Custom Preferences/Settings Pages =====

    mHooks.RegisterPreferencesPanel = [](HookId hookId, const char* panelName, const char* panelCategory,
                                          PreferencesPanelDrawCallback drawFunc,
                                          PreferencesLoadCallback loadFunc, PreferencesSaveCallback saveFunc,
                                          void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredPreferencesPanel panel;
        panel.mHookId = hookId;
        panel.mPanelName = panelName ? panelName : "";
        panel.mPanelCategory = panelCategory ? panelCategory : "";
        panel.mDrawFunc = drawFunc;
        panel.mLoadFunc = loadFunc;
        panel.mSaveFunc = saveFunc;
        panel.mUserData = userData;

        mgr->mPreferencesPanels.push_back(panel);
    };

    mHooks.UnregisterPreferencesPanel = [](HookId hookId, const char* panelName) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string name = panelName ? panelName : "";
        mgr->mPreferencesPanels.erase(std::remove_if(mgr->mPreferencesPanels.begin(), mgr->mPreferencesPanels.end(),
            [hookId, &name](const RegisteredPreferencesPanel& panel) {
                return panel.mHookId == hookId && panel.mPanelName == name;
            }), mgr->mPreferencesPanels.end());
    };

    // ===== Batch 5: Custom Keyboard Shortcuts =====

    mHooks.RegisterShortcut = [](HookId hookId, const char* shortcutId, const char* displayName,
                                  const char* defaultBinding, ShortcutCallback callback, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredShortcut sc;
        sc.mHookId = hookId;
        sc.mShortcutId = shortcutId ? shortcutId : "";
        sc.mDisplayName = displayName ? displayName : "";
        sc.mDefaultBinding = defaultBinding ? defaultBinding : "";
        sc.mCallback = callback;
        sc.mUserData = userData;

        mgr->ParseKeyBinding(sc);
        mgr->mShortcuts.push_back(sc);
    };

    mHooks.UnregisterShortcut = [](HookId hookId, const char* shortcutId) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string id = shortcutId ? shortcutId : "";
        mgr->mShortcuts.erase(std::remove_if(mgr->mShortcuts.begin(), mgr->mShortcuts.end(),
            [hookId, &id](const RegisteredShortcut& sc) {
                return sc.mHookId == hookId && sc.mShortcutId == id;
            }), mgr->mShortcuts.end());
    };

    // ===== Batch 6: Property Drawer System =====

    mHooks.RegisterPropertyDrawer = [](HookId hookId, const char* propertyTypeName,
                                        PropertyDrawCallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredPropertyDrawer drawer;
        drawer.mHookId = hookId;
        drawer.mPropertyTypeName = propertyTypeName ? propertyTypeName : "";
        drawer.mDrawFunc = drawFunc;
        drawer.mUserData = userData;

        mgr->mPropertyDrawers.push_back(drawer);
    };

    mHooks.UnregisterPropertyDrawer = [](HookId hookId, const char* propertyTypeName) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string typeName = propertyTypeName ? propertyTypeName : "";
        mgr->mPropertyDrawers.erase(std::remove_if(mgr->mPropertyDrawers.begin(), mgr->mPropertyDrawers.end(),
            [hookId, &typeName](const RegisteredPropertyDrawer& drawer) {
                return drawer.mHookId == hookId && drawer.mPropertyTypeName == typeName;
            }), mgr->mPropertyDrawers.end());
    };

    // ===== Batch 7: Hierarchy & Asset Browser Extensions =====

    mHooks.RegisterHierarchyItemGUI = [](HookId hookId, HierarchyItemGUICallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredHierarchyItemGUI item;
        item.mHookId = hookId;
        item.mDrawFunc = drawFunc;
        item.mUserData = userData;

        mgr->mHierarchyItemGUI.push_back(item);
    };

    mHooks.UnregisterHierarchyItemGUI = [](HookId hookId) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        mgr->mHierarchyItemGUI.erase(std::remove_if(mgr->mHierarchyItemGUI.begin(), mgr->mHierarchyItemGUI.end(),
            [hookId](const RegisteredHierarchyItemGUI& item) {
                return item.mHookId == hookId;
            }), mgr->mHierarchyItemGUI.end());
    };

    mHooks.RegisterAssetItemGUI = [](HookId hookId, AssetItemGUICallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredAssetItemGUI item;
        item.mHookId = hookId;
        item.mDrawFunc = drawFunc;
        item.mUserData = userData;

        mgr->mAssetItemGUI.push_back(item);
    };

    mHooks.UnregisterAssetItemGUI = [](HookId hookId) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        mgr->mAssetItemGUI.erase(std::remove_if(mgr->mAssetItemGUI.begin(), mgr->mAssetItemGUI.end(),
            [hookId](const RegisteredAssetItemGUI& item) {
                return item.mHookId == hookId;
            }), mgr->mAssetItemGUI.end());
    };

    mHooks.RegisterOnHierarchyChanged = [](HookId hookId, HierarchyChangedCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnHierarchyChanged.push_back({hookId, cb, userData});
    };

    // ===== Batch 8: Additional Context Menus =====

    mHooks.AddSceneTabContextItem = [](HookId hookId, const char* itemPath,
                                        MenuCallback callback, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mSceneTabContextItems.push_back({hookId, itemPath ? itemPath : "", callback, userData});
    };

    mHooks.AddDebugLogContextItem = [](HookId hookId, const char* itemPath,
                                        MenuCallback callback, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mDebugLogContextItems.push_back({hookId, itemPath ? itemPath : "", callback, userData});
    };

    mHooks.AddImportMenuItem = [](HookId hookId, const char* itemPath,
                                   MenuCallback callback, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mImportMenuItems.push_back({hookId, itemPath ? itemPath : "", callback, userData});
    };

    mHooks.AddAddonsMenuItem = [](HookId hookId, const char* itemPath,
                                   MenuCallback callback, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mAddonsMenuItems.push_back({hookId, itemPath ? itemPath : "", callback, userData});
    };

    mHooks.AddPlayTarget = [](HookId hookId, const char* targetName, const char* iconText,
                               PlayTargetCallback callback, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredPlayTarget target;
        target.mHookId = hookId;
        target.mTargetName = targetName ? targetName : "";
        target.mIconText = iconText ? iconText : "";
        target.mCallback = callback;
        target.mUserData = userData;

        mgr->mPlayTargets.push_back(target);
    };

    mHooks.RemovePlayTarget = [](HookId hookId, const char* targetName) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string name = targetName ? targetName : "";
        mgr->mPlayTargets.erase(std::remove_if(mgr->mPlayTargets.begin(), mgr->mPlayTargets.end(),
            [hookId, &name](const RegisteredPlayTarget& target) {
                return target.mHookId == hookId && target.mTargetName == name;
            }), mgr->mPlayTargets.end());
    };

    // ===== Batch 9: Drag-Drop Enhancement & Asset Pipeline =====

    mHooks.RegisterDragDropHandler = [](HookId hookId, const char* targetArea,
                                         DragDropHandlerCallback handler, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredDragDropHandler h;
        h.mHookId = hookId;
        h.mTargetArea = targetArea ? targetArea : "";
        h.mHandler = handler;
        h.mUserData = userData;

        mgr->mDragDropHandlers.push_back(h);
    };

    mHooks.RegisterAssetImporter = [](HookId hookId, const char* extension,
                                       AssetImportCallback importFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredAssetImporter imp;
        imp.mHookId = hookId;
        imp.mExtension = extension ? extension : "";
        imp.mImportFunc = importFunc;
        imp.mUserData = userData;

        mgr->mAssetImporters.push_back(imp);
    };

    mHooks.UnregisterAssetImporter = [](HookId hookId, const char* extension) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string ext = extension ? extension : "";
        mgr->mAssetImporters.erase(std::remove_if(mgr->mAssetImporters.begin(), mgr->mAssetImporters.end(),
            [hookId, &ext](const RegisteredAssetImporter& imp) {
                return imp.mHookId == hookId && imp.mExtension == ext;
            }), mgr->mAssetImporters.end());
    };

    mHooks.RegisterOnPreAssetImport = [](HookId hookId, PreImportCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnPreAssetImport.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnPostAssetImport = [](HookId hookId, StringEventCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnPostAssetImport.push_back({hookId, cb, userData});
    };

    // ===== Batch 10: Build Pipeline & Editor State =====

    mHooks.RegisterOnPreBuild = [](HookId hookId, PreBuildCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnPreBuild.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnPostBuild = [](HookId hookId, PackageFinishedCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnPostBuild.push_back({hookId, cb, userData});
    };

    mHooks.RegisterOnEditorModeChanged = [](HookId hookId, EditorModeCallback cb, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;
        mgr->mOnEditorModeChanged.push_back({hookId, cb, userData});
    };

    // ===== Game Preview Resolution Presets =====

    mHooks.AddGamePreviewResolution = [](HookId hookId, const char* name, uint32_t width, uint32_t height) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredGamePreviewResolution preset;
        preset.mHookId = hookId;
        preset.mName = name ? name : "";
        preset.mWidth = width;
        preset.mHeight = height;

        mgr->mGamePreviewResolutions.push_back(preset);
    };

    mHooks.RemoveGamePreviewResolution = [](HookId hookId, const char* name) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string n = name ? name : "";
        mgr->mGamePreviewResolutions.erase(std::remove_if(mgr->mGamePreviewResolutions.begin(), mgr->mGamePreviewResolutions.end(),
            [hookId, &n](const RegisteredGamePreviewResolution& preset) {
                return preset.mHookId == hookId && preset.mName == n;
            }), mgr->mGamePreviewResolutions.end());
    };

    mHooks.RegisterGizmoTool = [](HookId hookId, const char* toolName, const char* iconText,
                                   const char* tooltip, GizmoToolDrawCallback drawFunc, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredGizmoTool tool;
        tool.mHookId = hookId;
        tool.mToolName = toolName ? toolName : "";
        tool.mIconText = iconText ? iconText : "";
        tool.mTooltip = tooltip ? tooltip : "";
        tool.mDrawFunc = drawFunc;
        tool.mUserData = userData;
        tool.mIsActive = false;

        mgr->mGizmoTools.push_back(tool);
    };

    mHooks.UnregisterGizmoTool = [](HookId hookId, const char* toolName) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string name = toolName ? toolName : "";
        mgr->mGizmoTools.erase(std::remove_if(mgr->mGizmoTools.begin(), mgr->mGizmoTools.end(),
            [hookId, &name](const RegisteredGizmoTool& tool) {
                return tool.mHookId == hookId && tool.mToolName == name;
            }), mgr->mGizmoTools.end());
    };

    // ===== Controller Server Extension Hooks =====

    mHooks.RegisterControllerRoute = [](HookId hookId, const char* method, const char* path, ControllerRouteCallback callback, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        RegisteredControllerRoute route;
        route.mHookId = hookId;
        route.mMethod = method ? method : "";
        route.mPath = path ? path : "";
        route.mCallback = callback;
        route.mUserData = userData;

        mgr->mControllerRoutes.push_back(route);
    };

    mHooks.UnregisterControllerRoute = [](HookId hookId, const char* path) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        std::string p = path ? path : "";
        mgr->mControllerRoutes.erase(std::remove_if(mgr->mControllerRoutes.begin(), mgr->mControllerRoutes.end(),
            [hookId, &p](const RegisteredControllerRoute& route) {
                return route.mHookId == hookId && route.mPath == p;
            }), mgr->mControllerRoutes.end());
    };

    mHooks.RegisterOnControllerServerStateChanged = [](HookId hookId, ControllerServerEventCallback callback, void* userData) {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (mgr == nullptr) return;

        mgr->mOnControllerServerStateChanged.push_back({hookId, callback, userData});
    };

    // ===== Profiling Window Extension Hooks =====

    mHooks.RegisterProfilingStat = [](HookId hookId, const char* statName, const char* category, float maxValue, bool showAsBar) {
        ProfilingWindow* profiling = GetProfilingWindow();
        if (profiling != nullptr)
        {
            profiling->RegisterCustomStat(hookId, statName, category, maxValue, showAsBar);
        }
    };

    mHooks.UnregisterProfilingStat = [](HookId hookId, const char* statName) {
        ProfilingWindow* profiling = GetProfilingWindow();
        if (profiling != nullptr)
        {
            profiling->UnregisterCustomStat(hookId, statName);
        }
    };

    mHooks.SetProfilingStatValue = [](const char* statName, float value) {
        ProfilingWindow* profiling = GetProfilingWindow();
        if (profiling != nullptr)
        {
            profiling->SetCustomStatValue(statName, value);
        }
    };

    mHooks.RegisterProfilingSection = [](HookId hookId, const char* sectionName, void (*drawFunc)(void*), void* userData) {
        ProfilingWindow* profiling = GetProfilingWindow();
        if (profiling != nullptr)
        {
            profiling->RegisterCustomSection(hookId, sectionName, drawFunc, userData);
        }
    };

    mHooks.UnregisterProfilingSection = [](HookId hookId, const char* sectionName) {
        ProfilingWindow* profiling = GetProfilingWindow();
        if (profiling != nullptr)
        {
            profiling->UnregisterCustomSection(hookId, sectionName);
        }
    };

    // ===== Batch 12: Viewport input polling for addons =====
    mHooks.Viewport_RaycastUnderMouse = [](float screenX, float screenY,
                                           float fallbackPlaneY,
                                           float* outHitX, float* outHitY, float* outHitZ,
                                           float* outNormalX, float* outNormalY, float* outNormalZ,
                                           void** outHitNode) -> bool
    {
        auto writeMiss = [&]() {
            if (outHitX) *outHitX = 0.0f;
            if (outHitY) *outHitY = 0.0f;
            if (outHitZ) *outHitZ = 0.0f;
            if (outNormalX) *outNormalX = 0.0f;
            if (outNormalY) *outNormalY = 0.0f;
            if (outNormalZ) *outNormalZ = 0.0f;
            if (outHitNode) *outHitNode = nullptr;
        };

        Camera3D* camera = (GetEditorState() != nullptr) ? GetEditorState()->GetEditorCamera() : nullptr;
        if (camera == nullptr)
        {
            writeMiss();
            return false;
        }

        float scale = GetEngineConfig()->mEditorInterfaceScale;
        if (scale == 0.0f) scale = 1.0f;
        int32_t rx = (int32_t)(screenX * scale);
        int32_t ry = (int32_t)(screenY * scale);

        // 1. Bullet ray-test (matches asset-drop precedent in EditorImgui.cpp:10085-10125).
        RayTestResult rayResult;
        camera->TraceScreenToWorld(rx, ry, ColGroupAll, rayResult);
        if (rayResult.mHitNode != nullptr)
        {
            if (outHitX) *outHitX = rayResult.mHitPosition.x;
            if (outHitY) *outHitY = rayResult.mHitPosition.y;
            if (outHitZ) *outHitZ = rayResult.mHitPosition.z;
            if (outNormalX) *outNormalX = rayResult.mHitNormal.x;
            if (outNormalY) *outNormalY = rayResult.mHitNormal.y;
            if (outNormalZ) *outNormalZ = rayResult.mHitNormal.z;
            if (outHitNode) *outHitNode = (void*)rayResult.mHitNode;
            return true;
        }

        // Compute the same ray geometry the asset-drop fallback uses (perspective only
        // produces a single origin; for ortho the ray slides with the screen point).
        glm::vec3 rayOrigin;
        glm::vec3 rayDir;
        glm::vec3 nearPoint = camera->ScreenToWorldPosition(rx, ry);
        if (camera->GetProjectionMode() == ProjectionMode::PERSPECTIVE)
        {
            rayOrigin = camera->GetWorldPosition();
            rayDir    = Maths::SafeNormalize(nearPoint - rayOrigin);
        }
        else
        {
            rayOrigin = nearPoint;
            rayDir    = Maths::SafeNormalize(camera->GetForwardVector());
        }

        // 2. GPU id-buffer pick + bounding-sphere intersect for non-collidable meshes.
        Node3D* hitNode = Renderer::Get()->ProcessHitCheck(GetWorld(0), rx, ry);
        if (hitNode != nullptr)
        {
            Primitive3D* prim = hitNode->As<Primitive3D>();
            if (prim != nullptr)
            {
                Bounds bounds = prim->GetBounds();
                glm::vec3 oc = rayOrigin - bounds.mCenter;
                float b = 2.0f * glm::dot(oc, rayDir);
                float c = glm::dot(oc, oc) - bounds.mRadius * bounds.mRadius;
                float discriminant = b * b - 4.0f * c;

                glm::vec3 hitPos;
                if (discriminant >= 0.0f)
                {
                    float t = (-b - sqrtf(discriminant)) / 2.0f;
                    hitPos = rayOrigin + rayDir * t;
                }
                else
                {
                    float t = -glm::dot(oc, rayDir);
                    hitPos = rayOrigin + rayDir * glm::max(t, 0.0f);
                }

                if (outHitX) *outHitX = hitPos.x;
                if (outHitY) *outHitY = hitPos.y;
                if (outHitZ) *outHitZ = hitPos.z;
                // No surface normal from the id buffer — return -rayDir as a cheap stand-in.
                glm::vec3 nrm = -rayDir;
                if (outNormalX) *outNormalX = nrm.x;
                if (outNormalY) *outNormalY = nrm.y;
                if (outNormalZ) *outNormalZ = nrm.z;
                if (outHitNode) *outHitNode = (void*)hitNode;
                return true;
            }
        }

        // 3. Optional ground-plane fallback at world Y=fallbackPlaneY.
        if (!std::isnan(fallbackPlaneY))
        {
            const float kEps = 1e-6f;
            if (fabsf(rayDir.y) > kEps)
            {
                float t = (fallbackPlaneY - rayOrigin.y) / rayDir.y;
                if (t >= 0.0f)
                {
                    glm::vec3 hitPos = rayOrigin + rayDir * t;
                    if (outHitX) *outHitX = hitPos.x;
                    if (outHitY) *outHitY = hitPos.y;
                    if (outHitZ) *outHitZ = hitPos.z;
                    if (outNormalX) *outNormalX = 0.0f;
                    if (outNormalY) *outNormalY = 1.0f;
                    if (outNormalZ) *outNormalZ = 0.0f;
                    if (outHitNode) *outHitNode = nullptr;
                    return true;
                }
            }
        }

        writeMiss();
        return false;
    };

    // ===== Batch 15 / v8: viewport math for plugin gizmos =====

    mHooks.Viewport_GetMouseWorldRay = [](float mx, float my,
                                           float* outOX, float* outOY, float* outOZ,
                                           float* outDX, float* outDY, float* outDZ)
    {
        auto writeNothing = [&]() {
            if (outOX) *outOX = 0.0f; if (outOY) *outOY = 0.0f; if (outOZ) *outOZ = 0.0f;
            if (outDX) *outDX = 0.0f; if (outDY) *outDY = 0.0f; if (outDZ) *outDZ = 1.0f;
        };
        Camera3D* camera = (GetEditorState() != nullptr) ? GetEditorState()->GetEditorCamera() : nullptr;
        if (camera == nullptr) { writeNothing(); return; }

        float scale = GetEngineConfig()->mEditorInterfaceScale;
        if (scale == 0.0f) scale = 1.0f;
        int32_t rx = (int32_t)(mx * scale);
        int32_t ry = (int32_t)(my * scale);

        // Mirror Viewport_RaycastUnderMouse's perspective/ortho ray
        // construction so both entries agree on coordinate semantics.
        glm::vec3 nearPoint = camera->ScreenToWorldPosition(rx, ry);
        glm::vec3 origin, dir;
        if (camera->GetProjectionMode() == ProjectionMode::PERSPECTIVE)
        {
            origin = camera->GetWorldPosition();
            dir    = Maths::SafeNormalize(nearPoint - origin);
        }
        else
        {
            origin = nearPoint;
            dir    = Maths::SafeNormalize(camera->GetForwardVector());
        }

        if (outOX) *outOX = origin.x; if (outOY) *outOY = origin.y; if (outOZ) *outOZ = origin.z;
        if (outDX) *outDX = dir.x;    if (outDY) *outDY = dir.y;    if (outDZ) *outDZ = dir.z;
    };

    mHooks.Viewport_WorldToScreen = [](float wx, float wy, float wz,
                                        float* outSx, float* outSy) -> int
    {
        if (outSx) *outSx = 0.0f;
        if (outSy) *outSy = 0.0f;
        Camera3D* camera = (GetEditorState() != nullptr) ? GetEditorState()->GetEditorCamera() : nullptr;
        if (camera == nullptr) return 0;

        // Camera3D::WorldToScreenPosition returns xy = device pixels,
        // z = depth (positive = in front of camera; negative = behind).
        glm::vec3 sp = camera->WorldToScreenPosition(glm::vec3(wx, wy, wz));

        // Convert device-pixel coords back to the window-space pixels
        // Viewport_GetMouseState uses (divide by interface scale).
        float scale = GetEngineConfig()->mEditorInterfaceScale;
        if (scale == 0.0f) scale = 1.0f;
        if (outSx) *outSx = sp.x / scale;
        if (outSy) *outSy = sp.y / scale;
        return (sp.z > 0.0f) ? 1 : 0;
    };

    // ===== Batch 16 / v9: shared editor image cache =====

    mHooks.EditorImage_Load = [](const char* absPath) -> void*
    {
        if (absPath == nullptr || absPath[0] == '\0')
        {
            return nullptr;
        }
        return (void*)EditorImageCache::Get(absPath);
    };

    mHooks.EditorImage_GetSize = [](const char* absPath, int* outWidth, int* outHeight) -> int
    {
        if (outWidth) *outWidth = 0;
        if (outHeight) *outHeight = 0;
        if (absPath == nullptr || absPath[0] == '\0')
        {
            return 0;
        }

        int32_t w = 0;
        int32_t h = 0;
        if (!EditorImageCache::GetSize(absPath, w, h))
        {
            return 0;
        }

        if (outWidth) *outWidth = (int)w;
        if (outHeight) *outHeight = (int)h;
        return 1;
    };

    mHooks.EditorImage_Invalidate = [](const char* absPath)
    {
        if (absPath == nullptr || absPath[0] == '\0')
        {
            return;
        }
        EditorImageCache::Invalidate(absPath);
    };

    mHooks.Viewport_GetMouseState = [](float* outViewportX, float* outViewportY,
                                       float* outViewportW, float* outViewportH,
                                       float* outMouseX,    float* outMouseY,
                                       int* outHovered, int* outLeftClicked,
                                       int* outLeftDown, int* outRightClicked)
    {
        EditorImguiGetViewportRect(outViewportX, outViewportY, outViewportW, outViewportH);

        ImVec2 mp = ImGui::GetIO().MousePos;
        if (outMouseX) *outMouseX = mp.x;
        if (outMouseY) *outMouseY = mp.y;

        const bool hovered = EditorImguiIsViewportHovered();
        if (outHovered) *outHovered = hovered ? 1 : 0;

        // Drag-vs-click: only count as a click when release lands inside the viewport AND
        // the cursor didn't travel more than 4px since press (otherwise it's an orbit/pan).
        const ImVec2 drag = ImGui::GetMouseDragDelta(0, 0.0f);
        const bool leftClicked = hovered && ImGui::IsMouseReleased(0)
                              && fabsf(drag.x) < 4.0f && fabsf(drag.y) < 4.0f;
        if (outLeftClicked) *outLeftClicked = leftClicked ? 1 : 0;

        if (outLeftDown)    *outLeftDown    = (hovered && ImGui::IsMouseDown(0))    ? 1 : 0;
        if (outRightClicked) *outRightClicked = (hovered && ImGui::IsMouseClicked(1)) ? 1 : 0;
    };

    // ===== Batch 13: Selection control =====

    mHooks.Viewport_SuppressNextSelectionClick = []()
    {
        // One-frame latch. Read-and-cleared in Viewport3D::HandleDefaultControls
        // — see EditorUIHooks.h for the full contract.
        if (EditorState* state = GetEditorState())
        {
            state->mSuppressNextSelectionClick = true;
        }
    };

    mHooks.Selection_Clear = []()
    {
        // Equivalent to Escape over an empty viewport. SetSelectedNode(nullptr)
        // fires the existing OnSelectionChanged callbacks so observer addons
        // (and the editor's own UI) react normally.
        if (EditorState* state = GetEditorState())
        {
            state->SetSelectedNode(nullptr);
        }
    };

    // ===== Batch 14: File dialogs + OS file-drop dispatch =====
    //
    // All three dialogs are thin wrappers around the existing SYS_*
    // platform layer (System_Windows.cpp / System_Linux.cpp / etc.).
    // The drag-drop subscription stores entries in mFileDropHandlers;
    // Engine.cpp's main loop calls DispatchFileDrop each frame after
    // SYS_DrainDroppedFiles.

    mHooks.ShowOpenFileDialog = [](const char* /*title*/,
                                   const char* /*filter*/,
                                   const char* /*initialDir*/,
                                   char* outPath, int outPathSize) -> int
    {
        std::vector<std::string> picks = SYS_OpenFileDialog();
        if (picks.empty()) return 0;
        if (outPath && outPathSize > 0)
            std::snprintf(outPath, outPathSize, "%s", picks.front().c_str());
        return 1;
    };

    mHooks.ShowSaveFileDialog = [](const char* /*title*/,
                                   const char* /*filter*/,
                                   const char* /*defaultName*/,
                                   char* outPath, int outPathSize) -> int
    {
        const std::string pick = SYS_SaveFileDialog();
        if (pick.empty()) return 0;
        if (outPath && outPathSize > 0)
            std::snprintf(outPath, outPathSize, "%s", pick.c_str());
        return 1;
    };

    mHooks.ShowSelectFolderDialog = [](const char* /*title*/,
                                       char* outPath, int outPathSize) -> int
    {
        const std::string pick = SYS_SelectFolderDialog();
        if (pick.empty()) return 0;
        if (outPath && outPathSize > 0)
            std::snprintf(outPath, outPathSize, "%s", pick.c_str());
        return 1;
    };

    mHooks.RegisterFileDropHandler = [](HookId hookId,
                                        EditorUIHooks::FileDropCallback cb,
                                        void* userData)
    {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (!mgr || !cb) return;
        // Re-registering the same hookId replaces — matches the
        // hot-reload-safe contract every other registration uses.
        for (auto& h : mgr->mFileDropHandlers)
        {
            if (h.mHookId == hookId)
            {
                h.mCallback = cb;
                h.mUserData = userData;
                return;
            }
        }
        RegisteredFileDropHandler h;
        h.mHookId   = hookId;
        h.mCallback = cb;
        h.mUserData = userData;
        mgr->mFileDropHandlers.push_back(h);
    };

    mHooks.UnregisterFileDropHandler = [](HookId hookId)
    {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (!mgr) return;
        auto& v = mgr->mFileDropHandlers;
        v.erase(std::remove_if(v.begin(), v.end(),
                               [hookId](const RegisteredFileDropHandler& h) {
                                   return h.mHookId == hookId;
                               }),
                v.end());
    };

    // ===== Batch 15: Viewport Mode dropdown =====

    mHooks.AddViewportMode = [](HookId hookId,
                                const char* modeId,
                                const char* displayName,
                                int32_t sortOrder,
                                EditorUIHooks::ViewportModeCanActivateCallback canActivate,
                                EditorUIHooks::ViewportModeActivateCallback onActivate,
                                EditorUIHooks::ViewportModeDeactivateCallback onDeactivate,
                                EditorUIHooks::ViewportModeTickCallback tick,
                                EditorUIHooks::ViewportModeDrawPanelCallback drawPanel,
                                void* userData)
    {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (!mgr) return;
        if (modeId == nullptr || modeId[0] == '\0') return;

        // Re-registering the same modeId replaces the entry's callbacks +
        // userData so a freshly-reloaded addon DLL swaps in fresh function
        // pointers without leaving stale ones behind. The hookId may shift
        // (different load instance), so update that too.
        for (RegisteredViewportMode& vm : mgr->mViewportModes)
        {
            if (vm.mModeId == modeId)
            {
                vm.mHookId = hookId;
                if (displayName) vm.mDisplayName = displayName;
                vm.mSortOrder = sortOrder;
                vm.mCanActivate = canActivate;
                vm.mOnActivate = onActivate;
                vm.mOnDeactivate = onDeactivate;
                vm.mTick = tick;
                vm.mDrawPanel = drawPanel;
                vm.mUserData = userData;
                return;
            }
        }

        RegisteredViewportMode vm;
        vm.mHookId = hookId;
        vm.mModeId = modeId;
        vm.mDisplayName = (displayName && displayName[0] != '\0') ? displayName : modeId;
        vm.mSortOrder = sortOrder;
        vm.mCanActivate = canActivate;
        vm.mOnActivate = onActivate;
        vm.mOnDeactivate = onDeactivate;
        vm.mTick = tick;
        vm.mDrawPanel = drawPanel;
        vm.mUserData = userData;
        mgr->mViewportModes.push_back(std::move(vm));
    };

    mHooks.RemoveViewportMode = [](HookId hookId, const char* modeId)
    {
        EditorUIHookManager* mgr = EditorUIHookManager::Get();
        if (!mgr || modeId == nullptr) return;

        // If the mode being removed is the currently active addon mode,
        // deactivate it via EditorState first so callbacks run + the id
        // gets cleared before the entry is erased.
        EditorState* es = GetEditorState();
        if (es != nullptr && es->HasActiveAddonViewportMode() &&
            es->GetActiveAddonViewportModeId() == modeId)
        {
            es->ClearActiveAddonViewportMode();
        }

        auto& v = mgr->mViewportModes;
        v.erase(std::remove_if(v.begin(), v.end(),
                               [hookId, modeId](const RegisteredViewportMode& vm) {
                                   return vm.mHookId == hookId && vm.mModeId == modeId;
                               }),
                v.end());
    };

    mHooks.GetActiveViewportMode = [](char* outBuffer, int outBufferSize) -> int
    {
        EditorState* es = GetEditorState();
        if (es == nullptr || !es->HasActiveAddonViewportMode())
            return 0;
        const std::string& id = es->GetActiveAddonViewportModeId();
        if (outBuffer == nullptr || outBufferSize <= 0) return 0;
        if ((int)id.size() + 1 > outBufferSize) return 0;
        std::snprintf(outBuffer, outBufferSize, "%s", id.c_str());
        return 1;
    };
}

const std::vector<RegisteredMenuItem>& EditorUIHookManager::GetMenuItems(const std::string& menuPath) const
{
    auto it = mMenuItems.find(menuPath);
    if (it != mMenuItems.end())
    {
        return it->second;
    }
    return mEmptyMenuItems;
}

void EditorUIHookManager::DrawMenuItems(const std::string& menuPath)
{
    // Build a single in-place tree from both the legacy mMenuItems entries
    // and the Ex entries for this menu. Slash-separated itemPaths become
    // nested submenus (e.g. "My Addon/Features/Toggle A"); sibling entries
    // with a shared prefix merge under the same parent submenu.
    SlashMenuNode root;

    auto it = mMenuItems.find(menuPath);
    if (it != mMenuItems.end())
    {
        for (const RegisteredMenuItem& item : it->second)
        {
            if (item.mIsSeparator)
            {
                InsertSlashSeparator(root);
                continue;
            }
            MenuCallback cb = item.mCallback;
            void* ud = item.mUserData;
            InsertSlashLeaf(root, item.mItemPath,
                            [cb, ud]() { if (cb) cb(ud); },
                            item.mShortcut,
                            /*enabled*/ true);
        }
    }

    for (const RegisteredMenuItemEx& item : mMenuItemsEx)
    {
        if (item.mMenuPath != menuPath) continue;

        bool enabled = true;
        if (item.mValidateFunc) enabled = item.mValidateFunc(item.mUserData);

        MenuCallback cb = item.mCallback;
        void* ud = item.mUserData;
        InsertSlashLeaf(root, item.mItemPath,
                        [cb, ud]() { if (cb) cb(ud); },
                        item.mShortcut,
                        enabled);
    }

    DrawSlashMenuTree(root);
}

void EditorUIHookManager::DrawWindows()
{
    for (RegisteredWindow& win : mWindows)
    {
        if (win.mIsOpen)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
            if (ImGui::BeginDock(win.mWindowName.c_str(), &win.mIsOpen))
            {
                if (win.mDrawFunc)
                {
                    win.mDrawFunc(win.mUserData);
                }
            }
            ImGui::EndDock();
            ImGui::PopStyleColor();
        }
    }
}

void EditorUIHookManager::OpenWindow(const std::string& windowId)
{
    for (RegisteredWindow& win : mWindows)
    {
        if (win.mWindowId == windowId)
        {
            win.mIsOpen = true;
            return;
        }
    }
}

void EditorUIHookManager::CloseWindow(const std::string& windowId)
{
    for (RegisteredWindow& win : mWindows)
    {
        if (win.mWindowId == windowId)
        {
            win.mIsOpen = false;
            return;
        }
    }
}

bool EditorUIHookManager::IsWindowOpen(const std::string& windowId) const
{
    for (const RegisteredWindow& win : mWindows)
    {
        if (win.mWindowId == windowId)
        {
            return win.mIsOpen;
        }
    }
    return false;
}

const RegisteredInspector* EditorUIHookManager::GetInspector(const std::string& nodeTypeName) const
{
    for (const RegisteredInspector& insp : mInspectors)
    {
        if (insp.mNodeTypeName == nodeTypeName)
        {
            return &insp;
        }
    }
    return nullptr;
}

bool EditorUIHookManager::DrawInspector(const std::string& nodeTypeName, void* node)
{
    const RegisteredInspector* insp = GetInspector(nodeTypeName);
    if (insp && insp->mDrawFunc)
    {
        insp->mDrawFunc(node, insp->mUserData);
        return true;
    }
    return false;
}

void EditorUIHookManager::DrawNodeContextItems()
{
    for (const RegisteredContextItem& ctx : mContextItems)
    {
        if (ctx.mIsNodeContext)
        {
            if (ImGui::MenuItem(ctx.mItemPath.c_str()))
            {
                if (ctx.mCallback)
                {
                    ctx.mCallback(ctx.mUserData);
                }
            }
        }
    }
}

void EditorUIHookManager::DrawAssetContextItems(const std::string& assetType)
{
    for (const RegisteredContextItem& ctx : mContextItems)
    {
        if (!ctx.mIsNodeContext)
        {
            // Check if this item applies to this asset type
            if (ctx.mAssetTypeFilter == "*" || ctx.mAssetTypeFilter == assetType)
            {
                if (ImGui::MenuItem(ctx.mItemPath.c_str()))
                {
                    if (ctx.mCallback)
                    {
                        ctx.mCallback(ctx.mUserData);
                    }
                }
            }
        }
    }
}

void EditorUIHookManager::RemoveAllHooks(HookId hookId)
{
    // Remove menu items
    for (auto& pair : mMenuItems)
    {
        pair.second.erase(std::remove_if(pair.second.begin(), pair.second.end(),
            [hookId](const RegisteredMenuItem& item) {
                return item.mHookId == hookId;
            }), pair.second.end());
    }

    // Remove windows
    mWindows.erase(std::remove_if(mWindows.begin(), mWindows.end(),
        [hookId](const RegisteredWindow& win) {
            return win.mHookId == hookId;
        }), mWindows.end());

    // Remove inspectors
    mInspectors.erase(std::remove_if(mInspectors.begin(), mInspectors.end(),
        [hookId](const RegisteredInspector& insp) {
            return insp.mHookId == hookId;
        }), mInspectors.end());

    // Remove context items
    mContextItems.erase(std::remove_if(mContextItems.begin(), mContextItems.end(),
        [hookId](const RegisteredContextItem& ctx) {
            return ctx.mHookId == hookId;
        }), mContextItems.end());

    // Remove top-level menus
    mTopLevelMenus.erase(std::remove_if(mTopLevelMenus.begin(), mTopLevelMenus.end(),
        [hookId](const RegisteredTopLevelMenu& menu) {
            return menu.mHookId == hookId;
        }), mTopLevelMenus.end());

    // Remove toolbar items
    mToolbarItems.erase(std::remove_if(mToolbarItems.begin(), mToolbarItems.end(),
        [hookId](const RegisteredToolbarItem& item) {
            return item.mHookId == hookId;
        }), mToolbarItems.end());

    // Remove event callbacks - helper lambda
    auto removeByHookId = [hookId](auto& vec) {
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [hookId](const auto& entry) {
                return entry.mHookId == hookId;
            }), vec.end());
    };

    removeByHookId(mOnProjectOpen);
    removeByHookId(mOnProjectClose);
    removeByHookId(mOnProjectSave);
    removeByHookId(mOnSceneOpen);
    removeByHookId(mOnSceneClose);
    removeByHookId(mOnPackageStarted);
    removeByHookId(mOnPackageFinished);
    removeByHookId(mOnSelectionChanged);
    removeByHookId(mOnPlayModeChanged);
    removeByHookId(mOnEditorShutdown);
    removeByHookId(mOnAssetImported);
    removeByHookId(mOnAssetDeleted);
    removeByHookId(mOnAssetSaved);
    removeByHookId(mOnAssetOpen);
    removeByHookId(mOnAssetOpened);
    removeByHookId(mOnUndoRedo);
    removeByHookId(mOnAssetDropHierarchy);
    removeByHookId(mOnAssetDropViewport);

    // New hook types
    removeByHookId(mMenuItemsEx);
    removeByHookId(mNodeMenuItems);
    removeByHookId(mSceneTypes);
    removeByHookId(mCreateAssetItems);
    removeByHookId(mCreateAssetItemSingles);
    removeByHookId(mSpawnBasic3dItems);
    removeByHookId(mSpawnBasicWidgetItems);
    removeByHookId(mViewportContextItems);
    removeByHookId(mViewportOverlays);
    removeByHookId(mPreferencesPanels);
    removeByHookId(mShortcuts);
    removeByHookId(mPropertyDrawers);
    removeByHookId(mHierarchyItemGUI);
    removeByHookId(mAssetItemGUI);
    removeByHookId(mOnHierarchyChanged);
    removeByHookId(mSceneTabContextItems);
    removeByHookId(mDebugLogContextItems);
    removeByHookId(mImportMenuItems);
    removeByHookId(mAddonsMenuItems);
    removeByHookId(mPlayTargets);
    removeByHookId(mDragDropHandlers);
    removeByHookId(mFileDropHandlers);
    removeByHookId(mAssetImporters);
    removeByHookId(mOnPreAssetImport);
    removeByHookId(mOnPostAssetImport);
    removeByHookId(mOnPreBuild);
    removeByHookId(mOnPostBuild);
    removeByHookId(mOnEditorModeChanged);
    removeByHookId(mGizmoTools);
    removeByHookId(mGamePreviewResolutions);
    removeByHookId(mControllerRoutes);
    removeByHookId(mOnControllerServerStateChanged);

    // Batch 15: Viewport modes. Hot-reload safety — if the currently active
    // addon mode is owned by the hook being torn down, deactivate it via
    // EditorState first so OnDeactivate fires while the function pointer is
    // still valid AND the id gets cleared. Then erase the registrations.
    {
        EditorState* es = GetEditorState();
        if (es != nullptr && es->HasActiveAddonViewportMode())
        {
            const std::string& activeId = es->GetActiveAddonViewportModeId();
            for (const RegisteredViewportMode& vm : mViewportModes)
            {
                if (vm.mHookId == hookId && vm.mModeId == activeId)
                {
                    es->ClearActiveAddonViewportMode();
                    break;
                }
            }
        }
        removeByHookId(mViewportModes);
    }

    // Batch 11: build-target registry. Critical for hot-reload safety —
    // descriptors hold function pointers into the addon DLL; leaving them
    // around after FreeLibrary makes the next PackagingWindow::Draw call
    // jump into freed code. RemoveAllForHook skips built-ins regardless.
    mBuildTargets.RemoveAllForHook(hookId);

    // Remove profiling hooks (managed by ProfilingWindow)
    ProfilingWindow* profiling = GetProfilingWindow();
    if (profiling != nullptr)
    {
        profiling->RemoveAllHooks(hookId);
    }

    // Batch 16: the shared EditorImageCache is intentionally NOT torn down
    // here. Entries are engine-lifetime and path-keyed, deliberately not
    // owner-keyed — see the ownership note in EditorUIHooks.h. Releasing on
    // unload would free descriptor sets mid-frame (ImGui_ImplVulkan_RemoveTexture
    // is immediate, unlike DestroyQueue) and would yank shared kit icons out
    // from under sibling addons that resolved the same path.
}

// ===== Batch 14: OS file-drop dispatch =====

bool EditorUIHookManager::DispatchFileDrop(const std::vector<std::string>& paths)
{
    if (paths.empty() || mFileDropHandlers.empty())
        return false;

    // Marshal to a const char* array. Stable for the callback's lifetime.
    std::vector<const char*> raw;
    raw.reserve(paths.size());
    for (const std::string& s : paths) raw.push_back(s.c_str());

    bool claimed = false;
    // Snapshot the handler list before dispatch so a callback that
    // unregisters itself (or another) mid-iteration can't invalidate
    // the iterator.
    std::vector<RegisteredFileDropHandler> snapshot = mFileDropHandlers;
    for (const RegisteredFileDropHandler& h : snapshot)
    {
        if (!h.mCallback) continue;
        if (h.mCallback((int)raw.size(), raw.data(), h.mUserData))
            claimed = true;
    }
    return claimed;
}

// ===== Batch 15: Viewport Mode dropdown =====

std::vector<const RegisteredViewportMode*> EditorUIHookManager::GetViewportModesSorted() const
{
    std::vector<const RegisteredViewportMode*> out;
    out.reserve(mViewportModes.size());
    for (const RegisteredViewportMode& vm : mViewportModes) out.push_back(&vm);
    std::stable_sort(out.begin(), out.end(),
        [](const RegisteredViewportMode* a, const RegisteredViewportMode* b) {
            return a->mSortOrder < b->mSortOrder;
        });
    return out;
}

const RegisteredViewportMode* EditorUIHookManager::FindViewportMode(const std::string& modeId) const
{
    for (const RegisteredViewportMode& vm : mViewportModes)
    {
        if (vm.mModeId == modeId) return &vm;
    }
    return nullptr;
}

void EditorUIHookManager::FireViewportModeActivate(const std::string& modeId)
{
    const RegisteredViewportMode* vm = FindViewportMode(modeId);
    if (vm != nullptr && vm->mOnActivate != nullptr)
    {
        vm->mOnActivate(vm->mUserData);
    }
}

void EditorUIHookManager::FireViewportModeDeactivate(const std::string& modeId)
{
    const RegisteredViewportMode* vm = FindViewportMode(modeId);
    if (vm != nullptr && vm->mOnDeactivate != nullptr)
    {
        vm->mOnDeactivate(vm->mUserData);
    }
}

bool EditorUIHookManager::CanActivateViewportMode(const std::string& modeId)
{
    const RegisteredViewportMode* vm = FindViewportMode(modeId);
    if (vm == nullptr) return false;
    if (vm->mCanActivate == nullptr) return true;
    return vm->mCanActivate(vm->mUserData);
}

void EditorUIHookManager::TickActiveViewportMode(float deltaTime)
{
    EditorState* es = GetEditorState();
    if (es == nullptr || !es->HasActiveAddonViewportMode()) return;
    const RegisteredViewportMode* vm = FindViewportMode(es->GetActiveAddonViewportModeId());
    if (vm != nullptr && vm->mTick != nullptr)
    {
        vm->mTick(deltaTime, vm->mUserData);
    }
}

void EditorUIHookManager::DrawActiveViewportModePanel()
{
    EditorState* es = GetEditorState();
    if (es == nullptr || !es->HasActiveAddonViewportMode()) return;
    const RegisteredViewportMode* vm = FindViewportMode(es->GetActiveAddonViewportModeId());
    if (vm != nullptr && vm->mDrawPanel != nullptr)
    {
        vm->mDrawPanel(vm->mUserData);
    }
}

// ===== Top-Level Menus and Toolbar Drawing =====

void EditorUIHookManager::DrawTopLevelMenus()
{
    // Draw only menus with position == -1 (legacy append behavior)
    for (const RegisteredTopLevelMenu& menu : mTopLevelMenus)
    {
        if (menu.mPosition != -1) continue;

        if (ImGui::BeginMenu(menu.mMenuName.c_str()))
        {
            if (menu.mDrawFunc)
            {
                menu.mDrawFunc(menu.mUserData);
            }
            ImGui::EndMenu();
        }
    }
}

void EditorUIHookManager::DrawTopLevelMenusAtPosition(int32_t builtinPosition)
{
    for (const RegisteredTopLevelMenu& menu : mTopLevelMenus)
    {
        if (menu.mPosition != builtinPosition) continue;

        if (ImGui::BeginMenu(menu.mMenuName.c_str()))
        {
            if (menu.mDrawFunc)
            {
                menu.mDrawFunc(menu.mUserData);
            }
            ImGui::EndMenu();
        }
    }
}

void EditorUIHookManager::DrawToolbarItems()
{
    for (const RegisteredToolbarItem& item : mToolbarItems)
    {
        ImGui::SameLine();
        if (item.mDrawFunc)
        {
            item.mDrawFunc(item.mUserData);
        }
    }
}

// ===== Batch 2: Create/Spawn Menu Extensions =====

void EditorUIHookManager::DrawNodeMenuItems(const char* category, void* parentNode)
{
    for (const RegisteredNodeMenuItems& item : mNodeMenuItems)
    {
        if (item.mCategory == category && item.mDrawFunc)
        {
            ImGui::Separator();
            item.mDrawFunc(parentNode, item.mUserData);
        }
    }
}

void EditorUIHookManager::DrawCustomNodeCategories(void* parentNode)
{
    // Collect unique custom category names (not built-in ones)
    std::vector<std::string> customCategories;
    for (const RegisteredNodeMenuItems& item : mNodeMenuItems)
    {
        if (item.mCategory != "3D" && item.mCategory != "Widget" &&
            item.mCategory != "Scene" && item.mCategory != "Other")
        {
            bool found = false;
            for (const std::string& cat : customCategories)
            {
                if (cat == item.mCategory) { found = true; break; }
            }
            if (!found) customCategories.push_back(item.mCategory);
        }
    }

    for (const std::string& category : customCategories)
    {
        if (ImGui::BeginMenu(category.c_str()))
        {
            for (const RegisteredNodeMenuItems& item : mNodeMenuItems)
            {
                if (item.mCategory == category && item.mDrawFunc)
                {
                    item.mDrawFunc(parentNode, item.mUserData);
                }
            }
            ImGui::EndMenu();
        }
    }
}

void EditorUIHookManager::DrawCreateAssetItems()
{
    // Callback-style entries (AddCreateAssetItems): the addon owns the ImGui
    // drawing inside the menu. Kept for callers that need full control.
    for (const RegisteredCreateAssetItems& item : mCreateAssetItems)
    {
        if (item.mDrawFunc)
        {
            ImGui::Separator();
            item.mDrawFunc(nullptr, item.mUserData);
        }
    }

    // Declarative entries (AddCreateAssetItem singular): we build a slash
    // path tree across all addons so e.g. "WorldStream/World Sector" and
    // "WorldStream/Manifest" collapse into the same WorldStream submenu.
    if (!mCreateAssetItemSingles.empty())
    {
        ImGui::Separator();

        SlashMenuNode root;
        for (const RegisteredCreateAssetItem& item : mCreateAssetItemSingles)
        {
            MenuCallback cb = item.mCallback;
            void* ud = item.mUserData;
            InsertSlashLeaf(root, item.mPath,
                            [cb, ud]() { if (cb) cb(ud); },
                            /*shortcut*/ "",
                            /*enabled*/ true);
        }
        DrawSlashMenuTree(root);
    }
}

void EditorUIHookManager::DrawSpawnBasic3dItems(void* parentNode)
{
    for (const RegisteredSpawnItems& item : mSpawnBasic3dItems)
    {
        if (item.mDrawFunc)
        {
            ImGui::Separator();
            item.mDrawFunc(parentNode, item.mUserData);
        }
    }
}

void EditorUIHookManager::DrawSpawnBasicWidgetItems(void* parentNode)
{
    for (const RegisteredSpawnItems& item : mSpawnBasicWidgetItems)
    {
        if (item.mDrawFunc)
        {
            ImGui::Separator();
            item.mDrawFunc(parentNode, item.mUserData);
        }
    }
}

// ===== Scene Type Registration =====

void EditorUIHookManager::FireSceneCreation(const std::string& typeName, const char* sceneName, void* rootNode)
{
    for (const RegisteredSceneType& st : mSceneTypes)
    {
        if (st.mTypeName == typeName && st.mCreateFunc)
        {
            st.mCreateFunc(sceneName, rootNode, st.mUserData);
            return;
        }
    }
}

// ===== Batch 3: Viewport Context Menu & Overlay Drawing =====

void EditorUIHookManager::DrawViewportContextItems()
{
    for (const RegisteredViewportContextItem& item : mViewportContextItems)
    {
        if (ImGui::MenuItem(item.mItemPath.c_str()))
        {
            if (item.mCallback)
            {
                item.mCallback(item.mUserData);
            }
        }
    }
}

void EditorUIHookManager::DrawViewportOverlays(float viewportX, float viewportY, float viewportW, float viewportH)
{
    for (const RegisteredViewportOverlay& overlay : mViewportOverlays)
    {
        if (overlay.mDrawFunc)
        {
            overlay.mDrawFunc(viewportX, viewportY, viewportW, viewportH, overlay.mUserData);
        }
    }
}

bool EditorUIHookManager::HasViewportContextItems() const
{
    return !mViewportContextItems.empty();
}

// ===== Batch 4: Preferences =====

void EditorUIHookManager::LoadAddonPreferences()
{
    for (const RegisteredPreferencesPanel& panel : mPreferencesPanels)
    {
        if (panel.mLoadFunc)
        {
            panel.mLoadFunc(panel.mUserData);
        }
    }
}

void EditorUIHookManager::SaveAddonPreferences()
{
    for (const RegisteredPreferencesPanel& panel : mPreferencesPanels)
    {
        if (panel.mSaveFunc)
        {
            panel.mSaveFunc(panel.mUserData);
        }
    }
}

// ===== Batch 5: Keyboard Shortcuts =====

void EditorUIHookManager::ParseKeyBinding(RegisteredShortcut& shortcut)
{
    // Parse "Ctrl+Shift+Alt+X" format
    std::string binding = shortcut.mDefaultBinding;
    shortcut.mCtrl = false;
    shortcut.mShift = false;
    shortcut.mAlt = false;
    shortcut.mKeyCode = -1;

    // Convert to uppercase for comparison
    std::string upper = binding;
    for (char& c : upper) c = (char)toupper((unsigned char)c);

    // Check modifiers
    if (upper.find("CTRL+") != std::string::npos) shortcut.mCtrl = true;
    if (upper.find("SHIFT+") != std::string::npos) shortcut.mShift = true;
    if (upper.find("ALT+") != std::string::npos) shortcut.mAlt = true;

    // Get the key (last part after the last '+')
    size_t lastPlus = upper.rfind('+');
    std::string key = (lastPlus != std::string::npos) ? upper.substr(lastPlus + 1) : upper;

    // Map common key names to POLYPHASE_KEY_* constants from InputTypes.h
    if (key.length() == 1 && key[0] >= 'A' && key[0] <= 'Z')
    {
        shortcut.mKeyCode = POLYPHASE_KEY_A + (key[0] - 'A');
    }
    else if (key.length() == 1 && key[0] >= '0' && key[0] <= '9')
    {
        shortcut.mKeyCode = POLYPHASE_KEY_0 + (key[0] - '0');
    }
    else if (key == "F1")  shortcut.mKeyCode = POLYPHASE_KEY_F1;
    else if (key == "F2")  shortcut.mKeyCode = POLYPHASE_KEY_F2;
    else if (key == "F3")  shortcut.mKeyCode = POLYPHASE_KEY_F3;
    else if (key == "F4")  shortcut.mKeyCode = POLYPHASE_KEY_F4;
    else if (key == "F5")  shortcut.mKeyCode = POLYPHASE_KEY_F5;
    else if (key == "F6")  shortcut.mKeyCode = POLYPHASE_KEY_F6;
    else if (key == "F7")  shortcut.mKeyCode = POLYPHASE_KEY_F7;
    else if (key == "F8")  shortcut.mKeyCode = POLYPHASE_KEY_F8;
    else if (key == "F9")  shortcut.mKeyCode = POLYPHASE_KEY_F9;
    else if (key == "F10") shortcut.mKeyCode = POLYPHASE_KEY_F10;
    else if (key == "F11") shortcut.mKeyCode = POLYPHASE_KEY_F11;
    else if (key == "F12") shortcut.mKeyCode = POLYPHASE_KEY_F12;
    else if (key == "SPACE") shortcut.mKeyCode = POLYPHASE_KEY_SPACE;
    else if (key == "ENTER" || key == "RETURN") shortcut.mKeyCode = POLYPHASE_KEY_ENTER;
    else if (key == "ESCAPE" || key == "ESC") shortcut.mKeyCode = POLYPHASE_KEY_ESCAPE;
    else if (key == "TAB") shortcut.mKeyCode = POLYPHASE_KEY_TAB;
    else if (key == "DELETE" || key == "DEL") shortcut.mKeyCode = POLYPHASE_KEY_DELETE;
    else if (key == "BACKSPACE") shortcut.mKeyCode = POLYPHASE_KEY_BACKSPACE;
}

void EditorUIHookManager::ProcessShortcuts()
{
    // Don't process shortcuts when typing in text fields
    if (ImGui::GetIO().WantTextInput) return;

    for (const RegisteredShortcut& sc : mShortcuts)
    {
        if (sc.mKeyCode < 0 || sc.mCallback == nullptr) continue;

        // Check modifier key state using engine's input system
        bool ctrlMatch = sc.mCtrl ? IsControlDown() : !IsControlDown();
        bool shiftMatch = sc.mShift ? IsShiftDown() : !IsShiftDown();
        bool altMatch = sc.mAlt ? IsAltDown() : !IsAltDown();

        if (ctrlMatch && shiftMatch && altMatch && IsKeyJustDown(sc.mKeyCode))
        {
            sc.mCallback(sc.mUserData);
        }
    }
}

// ===== Batch 6: Property Drawers =====

bool EditorUIHookManager::DrawPropertyDrawer(const char* propertyTypeName, const char* propertyName,
                                              void* propertyOwner, int32_t propertyType)
{
    for (const RegisteredPropertyDrawer& drawer : mPropertyDrawers)
    {
        if (drawer.mPropertyTypeName == propertyTypeName && drawer.mDrawFunc)
        {
            if (drawer.mDrawFunc(propertyName, propertyOwner, propertyType, drawer.mUserData))
            {
                return true;
            }
        }
    }
    return false;
}

// ===== Batch 7: Hierarchy & Asset Browser Extensions =====

void EditorUIHookManager::DrawHierarchyItemGUI(void* node, float rowX, float rowY, float rowW, float rowH)
{
    for (const RegisteredHierarchyItemGUI& item : mHierarchyItemGUI)
    {
        if (item.mDrawFunc)
        {
            item.mDrawFunc(node, rowX, rowY, rowW, rowH, item.mUserData);
        }
    }
}

void EditorUIHookManager::DrawAssetItemGUI(const char* assetName, const char* assetType,
                                            float rowX, float rowY, float rowW, float rowH)
{
    for (const RegisteredAssetItemGUI& item : mAssetItemGUI)
    {
        if (item.mDrawFunc)
        {
            item.mDrawFunc(assetName, assetType, rowX, rowY, rowW, rowH, item.mUserData);
        }
    }
}

void EditorUIHookManager::FireOnHierarchyChanged(int32_t changeType, void* node)
{
    for (const auto& entry : mOnHierarchyChanged)
    {
        if (entry.mCallback) entry.mCallback(changeType, node, entry.mUserData);
    }
}

// ===== Batch 8: Additional Context Menus =====

void EditorUIHookManager::DrawSceneTabContextItems()
{
    for (const RegisteredSimpleContextItem& item : mSceneTabContextItems)
    {
        if (ImGui::MenuItem(item.mItemPath.c_str()))
        {
            if (item.mCallback) item.mCallback(item.mUserData);
        }
    }
}

void EditorUIHookManager::DrawDebugLogContextItems()
{
    for (const RegisteredSimpleContextItem& item : mDebugLogContextItems)
    {
        if (ImGui::MenuItem(item.mItemPath.c_str()))
        {
            if (item.mCallback) item.mCallback(item.mUserData);
        }
    }
}

void EditorUIHookManager::DrawImportMenuItems()
{
    for (const RegisteredSimpleContextItem& item : mImportMenuItems)
    {
        if (ImGui::MenuItem(item.mItemPath.c_str()))
        {
            if (item.mCallback) item.mCallback(item.mUserData);
        }
    }
}

void EditorUIHookManager::DrawAddonsMenuItems()
{
    for (const RegisteredSimpleContextItem& item : mAddonsMenuItems)
    {
        if (ImGui::MenuItem(item.mItemPath.c_str()))
        {
            if (item.mCallback) item.mCallback(item.mUserData);
        }
    }
}

void EditorUIHookManager::DrawPlayTargets()
{
    for (const RegisteredPlayTarget& target : mPlayTargets)
    {
        if (ImGui::Selectable(target.mTargetName.c_str()))
        {
            if (target.mCallback) target.mCallback(target.mUserData);
        }
    }
}

bool EditorUIHookManager::HasPlayTargets() const
{
    return !mPlayTargets.empty();
}

// ===== Batch 9: Drag-Drop & Asset Pipeline =====

bool EditorUIHookManager::HandleDragDrop(const char* targetArea, const char* payloadType,
                                          const void* payloadData, int32_t payloadSize)
{
    for (const RegisteredDragDropHandler& handler : mDragDropHandlers)
    {
        if (handler.mTargetArea == targetArea && handler.mHandler)
        {
            if (handler.mHandler(payloadType, payloadData, payloadSize, handler.mUserData))
            {
                return true;
            }
        }
    }
    return false;
}

bool EditorUIHookManager::HandleAssetImport(const char* filePath, const char* extension)
{
    for (const RegisteredAssetImporter& imp : mAssetImporters)
    {
        if (imp.mExtension == extension && imp.mImportFunc)
        {
            if (imp.mImportFunc(filePath, extension, imp.mUserData))
            {
                return true;
            }
        }
    }
    return false;
}

bool EditorUIHookManager::FireOnPreAssetImport(const char* filePath)
{
    for (const auto& entry : mOnPreAssetImport)
    {
        if (entry.mCallback && !entry.mCallback(filePath, entry.mUserData))
        {
            return false; // Cancelled
        }
    }
    return true;
}

void EditorUIHookManager::FireOnPostAssetImport(const char* assetPath)
{
    for (const auto& entry : mOnPostAssetImport)
    {
        if (entry.mCallback) entry.mCallback(assetPath, entry.mUserData);
    }
}

// ===== Batch 10: Build Pipeline & Editor State =====

bool EditorUIHookManager::FireOnPreBuild(int32_t platform)
{
    for (const auto& entry : mOnPreBuild)
    {
        if (entry.mCallback && !entry.mCallback(platform, entry.mUserData))
        {
            return false; // Cancelled
        }
    }
    return true;
}

void EditorUIHookManager::FireOnPostBuild(int32_t platform, bool success)
{
    for (const auto& entry : mOnPostBuild)
    {
        if (entry.mCallback) entry.mCallback(platform, success, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnEditorModeChanged(int32_t newMode)
{
    for (const auto& entry : mOnEditorModeChanged)
    {
        if (entry.mCallback) entry.mCallback(newMode, entry.mUserData);
    }
}

void EditorUIHookManager::DrawGizmoTools(void* selectedNode)
{
    for (RegisteredGizmoTool& tool : mGizmoTools)
    {
        ImGui::SameLine();
        if (tool.mIsActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        if (ImGui::Button(tool.mIconText.c_str()))
        {
            tool.mIsActive = !tool.mIsActive;
        }
        if (tool.mIsActive) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered() && !tool.mTooltip.empty())
        {
            ImGui::SetTooltip("%s", tool.mTooltip.c_str());
        }

        if (tool.mIsActive && tool.mDrawFunc && selectedNode)
        {
            tool.mDrawFunc(selectedNode, tool.mUserData);
        }
    }
}

// ===== Event Dispatchers =====

void EditorUIHookManager::FireOnProjectOpen(const char* projectPath)
{
    for (const auto& entry : mOnProjectOpen)
    {
        if (entry.mCallback) entry.mCallback(projectPath, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnProjectClose(const char* projectPath)
{
    for (const auto& entry : mOnProjectClose)
    {
        if (entry.mCallback) entry.mCallback(projectPath, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnProjectSave(const char* filePath)
{
    for (const auto& entry : mOnProjectSave)
    {
        if (entry.mCallback) entry.mCallback(filePath, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnSceneOpen(const char* scenePath)
{
    for (const auto& entry : mOnSceneOpen)
    {
        if (entry.mCallback) entry.mCallback(scenePath, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnSceneClose(const char* scenePath)
{
    for (const auto& entry : mOnSceneClose)
    {
        if (entry.mCallback) entry.mCallback(scenePath, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnPackageStarted(int32_t platform)
{
    for (const auto& entry : mOnPackageStarted)
    {
        if (entry.mCallback) entry.mCallback(platform, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnPackageFinished(int32_t platform, bool success)
{
    for (const auto& entry : mOnPackageFinished)
    {
        if (entry.mCallback) entry.mCallback(platform, success, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnSelectionChanged()
{
    for (const auto& entry : mOnSelectionChanged)
    {
        if (entry.mCallback) entry.mCallback(entry.mUserData);
    }
}

void EditorUIHookManager::FireOnPlayModeChanged(int32_t state)
{
    for (const auto& entry : mOnPlayModeChanged)
    {
        if (entry.mCallback) entry.mCallback(state, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnEditorShutdown()
{
    for (const auto& entry : mOnEditorShutdown)
    {
        if (entry.mCallback) entry.mCallback(entry.mUserData);
    }
}

void EditorUIHookManager::FireOnAssetImported(const char* assetPath)
{
    for (const auto& entry : mOnAssetImported)
    {
        if (entry.mCallback) entry.mCallback(assetPath, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnAssetDeleted(const char* assetPath)
{
    for (const auto& entry : mOnAssetDeleted)
    {
        if (entry.mCallback) entry.mCallback(assetPath, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnAssetSaved(const char* assetPath)
{
    for (const auto& entry : mOnAssetSaved)
    {
        if (entry.mCallback) entry.mCallback(assetPath, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnAssetOpen(const char* assetName)
{
    for (const auto& entry : mOnAssetOpen)
    {
        if (entry.mCallback) entry.mCallback(assetName, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnAssetOpened(const char* assetName)
{
    for (const auto& entry : mOnAssetOpened)
    {
        if (entry.mCallback) entry.mCallback(assetName, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnUndoRedo()
{
    for (const auto& entry : mOnUndoRedo)
    {
        if (entry.mCallback) entry.mCallback(entry.mUserData);
    }
}

void EditorUIHookManager::FireOnAssetDropHierarchy(const char* assetName)
{
    for (const auto& entry : mOnAssetDropHierarchy)
    {
        if (entry.mCallback) entry.mCallback(assetName, entry.mUserData);
    }
}

void EditorUIHookManager::FireOnAssetDropViewport(const char* assetName)
{
    for (const auto& entry : mOnAssetDropViewport)
    {
        if (entry.mCallback) entry.mCallback(assetName, entry.mUserData);
    }
}

HookId GenerateHookId(const char* identifier)
{
    if (identifier == nullptr)
    {
        return 0;
    }

    // Simple hash function
    uint64_t hash = 0;
    while (*identifier)
    {
        hash = hash * 31 + static_cast<uint64_t>(*identifier);
        ++identifier;
    }
    return hash;
}

void EditorUIHookManager::FireOnControllerServerStateChanged(int32_t state)
{
    for (const auto& entry : mOnControllerServerStateChanged)
    {
        if (entry.mCallback) entry.mCallback(state, entry.mUserData);
    }
}

#endif // EDITOR
