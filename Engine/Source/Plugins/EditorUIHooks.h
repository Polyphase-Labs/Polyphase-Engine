#pragma once

/**
 * @file EditorUIHooks.h
 * @brief Editor UI extension system for native addons.
 *
 * Provides hooks for plugins and Lua scripts to extend the editor UI
 * including menus, custom windows, inspectors, and context menus.
 *
 * This entire file is wrapped in #if EDITOR to ensure editor code
 * does not leak into game builds.
 */

#if EDITOR

#include <stdint.h>

// ===== Callback Types =====

/**
 * @brief Callback for menu item clicks.
 * @param userData User data passed during registration
 */
typedef void (*MenuCallback)(void* userData);

/**
 * @brief Callback for drawing custom window content.
 * @param userData User data passed during registration
 */
typedef void (*WindowDrawCallback)(void* userData);

/**
 * @brief Callback for drawing custom inspector content.
 * @param node Pointer to the node being inspected
 * @param userData User data passed during registration
 */
typedef void (*InspectorDrawCallback)(void* node, void* userData);

/**
 * @brief Generic event callback with no additional data.
 * @param userData User data passed during registration
 */
typedef void (*EventCallback)(void* userData);

/**
 * @brief Event callback that receives a string parameter (path, name, etc.).
 * @param str Context string (file path, project path, scene path, asset path)
 * @param userData User data passed during registration
 */
typedef void (*StringEventCallback)(const char* str, void* userData);

/**
 * @brief Callback for platform-specific packaging events.
 * @param platform Platform enum value (see Platform in SystemTypes.h)
 * @param userData User data passed during registration
 */
typedef void (*PlatformEventCallback)(int32_t platform, void* userData);

/**
 * @brief Callback for packaging completion with success/failure status.
 * @param platform Platform enum value that was packaged
 * @param success true if packaging succeeded, false if it failed
 * @param userData User data passed during registration
 */
typedef void (*PackageFinishedCallback)(int32_t platform, bool success, void* userData);

/**
 * @brief Callback for play mode state changes.
 * @param state Play mode state: 0=Enter, 1=Exit, 2=Eject
 * @param userData User data passed during registration
 */
typedef void (*PlayModeCallback)(int32_t state, void* userData);

/**
 * @brief Draw callback for custom top-level menus.
 *
 * Called inside ImGui::BeginMenu/EndMenu. Use ImGui::MenuItem() etc. inside.
 * @param userData User data passed during registration
 */
typedef void (*TopLevelMenuDrawCallback)(void* userData);

/**
 * @brief Draw callback for custom toolbar items.
 *
 * Called inside the viewport toolbar area. Use ImGui widgets to draw controls.
 * @param userData User data passed during registration
 */
typedef void (*ToolbarDrawCallback)(void* userData);

/**
 * @brief Callback for menu item validation (enable/disable).
 * @param userData User data passed during registration
 * @return true if menu item should be enabled, false to grey it out
 */
typedef bool (*MenuValidationCallback)(void* userData);

/**
 * @brief Draw callback for menu sections (called inside BeginMenu/EndMenu).
 * @param parentNode Pointer to the parent node context (can be nullptr)
 * @param userData User data passed during registration
 */
typedef void (*MenuSectionDrawCallback)(void* parentNode, void* userData);

/**
 * @brief Callback for viewport overlay drawing.
 * @param viewportX X position of viewport
 * @param viewportY Y position of viewport
 * @param viewportW Width of viewport
 * @param viewportH Height of viewport
 * @param userData User data passed during registration
 */
typedef void (*ViewportOverlayCallback)(float viewportX, float viewportY, float viewportW, float viewportH, void* userData);

/**
 * @brief Callback for preferences panel drawing.
 * @param userData User data passed during registration
 */
typedef void (*PreferencesPanelDrawCallback)(void* userData);

/**
 * @brief Callback for preferences load/save.
 * @param userData User data passed during registration
 */
typedef void (*PreferencesLoadCallback)(void* userData);
typedef void (*PreferencesSaveCallback)(void* userData);

/**
 * @brief Callback for keyboard shortcuts.
 * @param userData User data passed during registration
 */
typedef void (*ShortcutCallback)(void* userData);

/**
 * @brief Property drawer callback - return true if handled.
 * @param propertyName Name of the property being drawn
 * @param propertyOwner Pointer to the object that owns the property
 * @param propertyType Type identifier for the property
 * @param userData User data passed during registration
 * @return true if the property was drawn by this callback
 */
typedef bool (*PropertyDrawCallback)(const char* propertyName, void* propertyOwner, int32_t propertyType, void* userData);

/**
 * @brief Hierarchy item GUI overlay callback.
 * @param node Pointer to the node being drawn
 * @param rowX X position of the row
 * @param rowY Y position of the row
 * @param rowW Width of the row
 * @param rowH Height of the row
 * @param userData User data passed during registration
 */
typedef void (*HierarchyItemGUICallback)(void* node, float rowX, float rowY, float rowW, float rowH, void* userData);

/**
 * @brief Asset browser item GUI overlay callback.
 * @param assetName Name of the asset
 * @param assetType Type name of the asset
 * @param rowX X position of the row
 * @param rowY Y position of the row
 * @param rowW Width of the row
 * @param rowH Height of the row
 * @param userData User data passed during registration
 */
typedef void (*AssetItemGUICallback)(const char* assetName, const char* assetType, float rowX, float rowY, float rowW, float rowH, void* userData);

/**
 * @brief Hierarchy changed event callback.
 * @param changeType 0=NodeCreated, 1=NodeDestroyed, 2=NodeReparented, 3=NodeRenamed
 * @param node Pointer to the affected node
 * @param userData User data passed during registration
 */
typedef void (*HierarchyChangedCallback)(int32_t changeType, void* node, void* userData);

/**
 * @brief Custom drag-drop handler callback.
 * @param payloadType Type string of the drag payload
 * @param payloadData Pointer to the payload data
 * @param payloadSize Size of the payload data in bytes
 * @param userData User data passed during registration
 * @return true to consume the drop, false to pass through
 */
typedef bool (*DragDropHandlerCallback)(const char* payloadType, const void* payloadData, int32_t payloadSize, void* userData);

/**
 * @brief Custom asset import callback.
 * @param filePath Path of the file to import
 * @param extension File extension (e.g., ".fbx")
 * @param userData User data passed during registration
 * @return true if the import was handled
 */
typedef bool (*AssetImportCallback)(const char* filePath, const char* extension, void* userData);

/**
 * @brief Pre-import callback. Return false to cancel the import.
 * @param filePath Path of the file about to be imported
 * @param userData User data passed during registration
 * @return false to cancel the import
 */
typedef bool (*PreImportCallback)(const char* filePath, void* userData);

/**
 * @brief Pre-build callback. Return false to cancel the build.
 * @param platform Platform enum value
 * @param userData User data passed during registration
 * @return false to cancel the build
 */
typedef bool (*PreBuildCallback)(int32_t platform, void* userData);

/**
 * @brief Editor mode changed callback.
 * @param newMode New editor mode value
 * @param userData User data passed during registration
 */
typedef void (*EditorModeCallback)(int32_t newMode, void* userData);

/**
 * @brief Custom gizmo tool draw callback.
 * @param selectedNode Pointer to the currently selected node
 * @param userData User data passed during registration
 */
typedef void (*GizmoToolDrawCallback)(void* selectedNode, void* userData);

/**
 * @brief Play target callback.
 * @param userData User data passed during registration
 */
typedef void (*PlayTargetCallback)(void* userData);

/**
 * @brief Callback for custom scene type creation.
 *
 * Called when a plugin-registered scene type is selected in the "New Scene" dialog.
 * The plugin should create child nodes under the provided root node.
 *
 * @param sceneName The asset name entered by user
 * @param rootNode Pointer to root Node* to populate (plugin creates children under it)
 * @param userData User data from registration
 */
typedef void (*SceneCreationCallback)(const char* sceneName, void* rootNode, void* userData);

// ===== Controller Server Hooks =====

/**
 * @brief Callback for custom controller REST route handlers.
 * @param method HTTP method (GET, POST, PUT, DELETE)
 * @param path Request path (e.g., "/api/addons/mycustom")
 * @param body Request body (JSON string, empty for GET)
 * @param responseBuffer Buffer to write JSON response into
 * @param bufferSize Size of the response buffer
 * @param userData User data passed during registration
 */
typedef void (*ControllerRouteCallback)(const char* method, const char* path, const char* body, char* responseBuffer, int32_t bufferSize, void* userData);

/**
 * @brief Callback for controller server state changes.
 * @param state 0=Started, 1=Stopped
 * @param userData User data passed during registration
 */
typedef void (*ControllerServerEventCallback)(int32_t state, void* userData);

/**
 * @brief Unique identifier for tracking hooks.
 *
 * Use GenerateHookId() to create from addon ID or Lua script UUID.
 * This allows proper cleanup when plugins are unloaded.
 */
typedef uint64_t HookId;

/**
 * @brief Editor UI extension hooks.
 *
 * Provides functions for plugins to extend the editor UI.
 * All registration functions take a HookId for tracking and cleanup.
 */
struct EditorUIHooks
{
    // ===== Menu Extensions =====

    /**
     * @brief Add a menu item to an existing menu.
     *
     * @param hookId Unique identifier for this hook (for cleanup)
     * @param menuPath Top-level menu: "File", "Edit", "View", "Developer", "Help"
     * @param itemPath Item path within menu, e.g., "My Tool" or "Submenu/My Tool"
     * @param callback Function called when item is clicked
     * @param userData User data passed to callback
     * @param shortcut Optional keyboard shortcut, e.g., "Ctrl+Shift+M" (can be nullptr)
     */
    void (*AddMenuItem)(
        HookId hookId,
        const char* menuPath,
        const char* itemPath,
        MenuCallback callback,
        void* userData,
        const char* shortcut
    );

    /**
     * @brief Add a separator in a menu.
     *
     * @param hookId Unique identifier for this hook
     * @param menuPath Menu to add separator to
     */
    void (*AddMenuSeparator)(HookId hookId, const char* menuPath);

    /**
     * @brief Remove a previously added menu item.
     *
     * @param hookId Hook identifier used during registration
     * @param menuPath Menu containing the item
     * @param itemPath Path of item to remove
     */
    void (*RemoveMenuItem)(HookId hookId, const char* menuPath, const char* itemPath);

    // ===== Custom Windows =====

    /**
     * @brief Register a custom dockable window.
     *
     * @param hookId Unique identifier for this hook
     * @param windowName Display name shown in title bar
     * @param windowId Unique ID for docking persistence
     * @param drawFunc Function called to draw window content
     * @param userData User data passed to drawFunc
     */
    void (*RegisterWindow)(
        HookId hookId,
        const char* windowName,
        const char* windowId,
        WindowDrawCallback drawFunc,
        void* userData
    );

    /**
     * @brief Unregister a custom window.
     *
     * @param hookId Hook identifier used during registration
     * @param windowId Window ID to unregister
     */
    void (*UnregisterWindow)(HookId hookId, const char* windowId);

    /**
     * @brief Open a custom window by ID.
     * @param windowId Window ID to open
     */
    void (*OpenWindow)(const char* windowId);

    /**
     * @brief Close a custom window by ID.
     * @param windowId Window ID to close
     */
    void (*CloseWindow)(const char* windowId);

    /**
     * @brief Check if a custom window is currently open.
     * @param windowId Window ID to check
     * @return true if window is open
     */
    bool (*IsWindowOpen)(const char* windowId);

    // ===== Inspector Extensions =====

    /**
     * @brief Register a custom inspector for a node type.
     *
     * @param hookId Unique identifier for this hook
     * @param nodeTypeName Type name of node, e.g., "MyCustomNode"
     * @param drawFunc Function called to draw inspector content
     * @param userData User data passed to drawFunc
     */
    void (*RegisterInspector)(
        HookId hookId,
        const char* nodeTypeName,
        InspectorDrawCallback drawFunc,
        void* userData
    );

    /**
     * @brief Unregister a custom inspector.
     *
     * @param hookId Hook identifier used during registration
     * @param nodeTypeName Node type name to unregister
     */
    void (*UnregisterInspector)(HookId hookId, const char* nodeTypeName);

    // ===== Context Menu Extensions =====

    /**
     * @brief Add item to node context menu (right-click in hierarchy).
     *
     * @param hookId Unique identifier for this hook
     * @param itemPath Item path in context menu
     * @param callback Function called when item is clicked
     * @param userData User data passed to callback
     */
    void (*AddNodeContextItem)(
        HookId hookId,
        const char* itemPath,
        MenuCallback callback,
        void* userData
    );

    /**
     * @brief Add item to asset context menu (right-click in asset browser).
     *
     * @param hookId Unique identifier for this hook
     * @param itemPath Item path in context menu
     * @param assetTypeFilter Asset type to show for, e.g., "Texture", or "*" for all
     * @param callback Function called when item is clicked
     * @param userData User data passed to callback
     */
    void (*AddAssetContextItem)(
        HookId hookId,
        const char* itemPath,
        const char* assetTypeFilter,
        MenuCallback callback,
        void* userData
    );

    // ===== Top-Level Menus =====

    /**
     * @brief Add a custom top-level menu to the editor viewport bar.
     *
     * The drawFunc is called inside ImGui::BeginMenu/EndMenu.
     * Use ImGui::MenuItem(), ImGui::Separator(), etc. inside.
     *
     * @param hookId Unique identifier for this hook (for cleanup)
     * @param menuName Display name for the top-level menu button
     * @param drawFunc Function called to draw menu contents
     * @param userData User data passed to drawFunc
     */
    void (*AddTopLevelMenuItem)(HookId hookId, const char* menuName,
                                TopLevelMenuDrawCallback drawFunc, void* userData);

    /**
     * @brief Remove a previously added top-level menu.
     * @param hookId Hook identifier used during registration
     * @param menuName Name of the menu to remove
     */
    void (*RemoveTopLevelMenuItem)(HookId hookId, const char* menuName);

    // ===== Toolbar =====

    /**
     * @brief Add a custom item to the editor viewport toolbar.
     * @param hookId Unique identifier for this hook (for cleanup)
     * @param itemName Unique name for the toolbar item
     * @param drawFunc Function called to draw toolbar content (buttons, etc.)
     * @param userData User data passed to drawFunc
     */
    void (*AddToolbarItem)(HookId hookId, const char* itemName,
                           ToolbarDrawCallback drawFunc, void* userData);

    /**
     * @brief Remove a previously added toolbar item.
     * @param hookId Hook identifier used during registration
     * @param itemName Name of the toolbar item to remove
     */
    void (*RemoveToolbarItem)(HookId hookId, const char* itemName);

    // ===== Project Lifecycle Events =====

    /** @brief Register callback for when a project is opened. Receives project path. */
    void (*RegisterOnProjectOpen)(HookId hookId, StringEventCallback cb, void* userData);

    /** @brief Register callback for when a project is about to close. Receives project path. */
    void (*RegisterOnProjectClose)(HookId hookId, StringEventCallback cb, void* userData);

    /** @brief Register callback for when the project/scene is saved. Receives file path. */
    void (*RegisterOnProjectSave)(HookId hookId, StringEventCallback cb, void* userData);

    // ===== Scene Lifecycle Events =====

    /** @brief Register callback for when a scene is opened for editing. Receives scene path. */
    void (*RegisterOnSceneOpen)(HookId hookId, StringEventCallback cb, void* userData);

    /** @brief Register callback for when a scene is closed. Receives scene path. */
    void (*RegisterOnSceneClose)(HookId hookId, StringEventCallback cb, void* userData);

    // ===== Packaging/Build Events =====

    /** @brief Register callback for when packaging starts. Receives platform enum. */
    void (*RegisterOnPackageStarted)(HookId hookId, PlatformEventCallback cb, void* userData);

    /** @brief Register callback for when packaging finishes. Receives platform and success. */
    void (*RegisterOnPackageFinished)(HookId hookId, PackageFinishedCallback cb, void* userData);

    // ===== Editor State Events =====

    /** @brief Register callback for when the selected node(s) change in the editor. */
    void (*RegisterOnSelectionChanged)(HookId hookId, EventCallback cb, void* userData);

    /** @brief Register callback for Play-In-Editor state changes (Enter/Exit/Eject). */
    void (*RegisterOnPlayModeChanged)(HookId hookId, PlayModeCallback cb, void* userData);

    /** @brief Register callback for when the editor is shutting down. Called before cleanup. */
    void (*RegisterOnEditorShutdown)(HookId hookId, EventCallback cb, void* userData);

    // ===== Asset Pipeline Events =====

    /** @brief Register callback for when an asset is imported. Receives asset path. */
    void (*RegisterOnAssetImported)(HookId hookId, StringEventCallback cb, void* userData);

    /** @brief Register callback for when an asset is deleted. Receives asset path. */
    void (*RegisterOnAssetDeleted)(HookId hookId, StringEventCallback cb, void* userData);

    /** @brief Register callback for when an asset is saved. Receives asset path. */
    void (*RegisterOnAssetSaved)(HookId hookId, StringEventCallback cb, void* userData);

    // ===== Asset Open Events =====

    /** @brief Register callback for when an asset is about to be opened (double-clicked). Receives asset name. */
    void (*RegisterOnAssetOpen)(HookId hookId, StringEventCallback cb, void* userData);

    /** @brief Register callback for after an asset has been opened (loaded and displayed). Receives asset name. */
    void (*RegisterOnAssetOpened)(HookId hookId, StringEventCallback cb, void* userData);

    // ===== Undo/Redo =====

    /** @brief Register callback for when an undo or redo operation is performed. */
    void (*RegisterOnUndoRedo)(HookId hookId, EventCallback cb, void* userData);

    // ===== Drag-and-Drop Events =====

    /** @brief Register callback for when an asset is dropped onto the scene hierarchy. Receives asset name. */
    void (*RegisterOnAssetDropHierarchy)(HookId hookId, StringEventCallback cb, void* userData);

    /** @brief Register callback for when an asset is dropped onto the viewport. Receives asset name. */
    void (*RegisterOnAssetDropViewport)(HookId hookId, StringEventCallback cb, void* userData);

    // ===== Batch 1: Menu Positioning & Top-Level Menu Control =====

    /**
     * @brief Add a custom top-level menu with position control.
     * @param hookId Unique identifier for this hook
     * @param menuName Display name for the menu
     * @param drawFunc Function called to draw menu contents
     * @param userData User data passed to drawFunc
     * @param position Position index: -1=append after all, 0=after File, 1=after Edit, 2=after View, 3=after World, 4=after Developer, 5=after Addons, 6=after Extra
     */
    void (*AddTopLevelMenuItemEx)(
        HookId hookId,
        const char* menuName,
        TopLevelMenuDrawCallback drawFunc,
        void* userData,
        int32_t position
    );

    /**
     * @brief Add a menu item with validation callback.
     * @param hookId Unique identifier for this hook
     * @param menuPath Top-level menu name
     * @param itemPath Item path within menu
     * @param callback Function called when item is clicked
     * @param userData User data passed to callback
     * @param shortcut Optional keyboard shortcut (can be nullptr)
     * @param validateFunc Validation callback - return false to grey out item (can be nullptr)
     */
    void (*AddMenuItemEx)(
        HookId hookId,
        const char* menuPath,
        const char* itemPath,
        MenuCallback callback,
        void* userData,
        const char* shortcut,
        MenuValidationCallback validateFunc
    );

    // ===== Batch 2: Create/Spawn Menu Extensions =====

    /**
     * @brief Extend the "Add Node" submenu.
     * @param hookId Unique identifier for this hook
     * @param category Category name: "3D", "Widget", "Other", or custom name for a new submenu
     * @param drawFunc Function called to draw menu items
     * @param userData User data passed to drawFunc
     */
    void (*AddNodeMenuItems)(HookId hookId, const char* category, MenuSectionDrawCallback drawFunc, void* userData);

    /** @brief Remove previously added node menu items. */
    void (*RemoveNodeMenuItems)(HookId hookId, const char* category);

    /**
     * @brief Extend the "Create Asset" submenu in asset browser context menu.
     * @param hookId Unique identifier for this hook
     * @param drawFunc Function called to draw menu items
     * @param userData User data passed to drawFunc
     */
    void (*AddCreateAssetItems)(HookId hookId, MenuSectionDrawCallback drawFunc, void* userData);

    /** @brief Remove previously added create asset items. */
    void (*RemoveCreateAssetItems)(HookId hookId);

    /**
     * @brief Extend the "Spawn Basic 3D" menu.
     * @param hookId Unique identifier for this hook
     * @param drawFunc Function called to draw additional 3D spawn items
     * @param userData User data passed to drawFunc
     */
    void (*AddSpawnBasic3dItems)(HookId hookId, MenuSectionDrawCallback drawFunc, void* userData);

    /**
     * @brief Extend the "Spawn Basic Widget" menu.
     * @param hookId Unique identifier for this hook
     * @param drawFunc Function called to draw additional widget spawn items
     * @param userData User data passed to drawFunc
     */
    void (*AddSpawnBasicWidgetItems)(HookId hookId, MenuSectionDrawCallback drawFunc, void* userData);

    // ===== Scene Type Registration =====

    /**
     * @brief Register a custom scene type for the "New Scene" dialog.
     *
     * Adds a new radio button option in the scene creation dialog.
     * When selected, the createFunc is called to populate the scene root.
     *
     * @param hookId Unique identifier for this hook
     * @param typeName Display name for the scene type (e.g., "Isometric", "Top-Down")
     * @param createFunc Function called to populate the root node
     * @param userData User data passed to createFunc
     */
    void (*RegisterSceneType)(HookId hookId, const char* typeName, SceneCreationCallback createFunc, void* userData);

    /** @brief Unregister a custom scene type. */
    void (*UnregisterSceneType)(HookId hookId, const char* typeName);

    // ===== Batch 3: Viewport Context Menu & Overlay Drawing =====

    /**
     * @brief Add item to viewport right-click context menu.
     * @param hookId Unique identifier for this hook
     * @param itemPath Item path in context menu
     * @param callback Function called when item is clicked
     * @param userData User data passed to callback
     */
    void (*AddViewportContextItem)(HookId hookId, const char* itemPath, MenuCallback callback, void* userData);

    /** @brief Remove a viewport context menu item. */
    void (*RemoveViewportContextItem)(HookId hookId, const char* itemPath);

    /**
     * @brief Register a viewport overlay drawn each frame.
     * @param hookId Unique identifier for this hook
     * @param overlayName Unique name for the overlay
     * @param drawFunc Function called each frame with viewport dimensions
     * @param userData User data passed to drawFunc
     */
    void (*RegisterViewportOverlay)(HookId hookId, const char* overlayName, ViewportOverlayCallback drawFunc, void* userData);

    /** @brief Unregister a viewport overlay. */
    void (*UnregisterViewportOverlay)(HookId hookId, const char* overlayName);

    // ===== Batch 4: Custom Preferences/Settings Pages =====

    /**
     * @brief Register a custom preferences panel.
     * @param hookId Unique identifier for this hook
     * @param panelName Display name in preferences sidebar
     * @param panelCategory Category path, e.g., "Addons/MyAddon"
     * @param drawFunc Function called to draw panel content
     * @param loadFunc Optional load callback (can be nullptr)
     * @param saveFunc Optional save callback (can be nullptr)
     * @param userData User data passed to callbacks
     */
    void (*RegisterPreferencesPanel)(
        HookId hookId,
        const char* panelName,
        const char* panelCategory,
        PreferencesPanelDrawCallback drawFunc,
        PreferencesLoadCallback loadFunc,
        PreferencesSaveCallback saveFunc,
        void* userData
    );

    /** @brief Unregister a custom preferences panel. */
    void (*UnregisterPreferencesPanel)(HookId hookId, const char* panelName);

    // ===== Batch 5: Custom Keyboard Shortcuts =====

    /**
     * @brief Register a keyboard shortcut.
     * @param hookId Unique identifier for this hook
     * @param shortcutId Unique ID, e.g., "myaddon.toggle_panel"
     * @param displayName Human-readable name, e.g., "My Addon: Toggle Panel"
     * @param defaultBinding Default key binding, e.g., "Ctrl+Shift+M"
     * @param callback Function called when shortcut is triggered
     * @param userData User data passed to callback
     */
    void (*RegisterShortcut)(
        HookId hookId,
        const char* shortcutId,
        const char* displayName,
        const char* defaultBinding,
        ShortcutCallback callback,
        void* userData
    );

    /** @brief Unregister a keyboard shortcut. */
    void (*UnregisterShortcut)(HookId hookId, const char* shortcutId);

    // ===== Batch 6: Property Drawer System =====

    /**
     * @brief Register a custom drawer for a property type.
     * @param hookId Unique identifier for this hook
     * @param propertyTypeName Type name to match (e.g., "glm::vec3", "Asset*")
     * @param drawFunc Function called to draw the property
     * @param userData User data passed to drawFunc
     */
    void (*RegisterPropertyDrawer)(HookId hookId, const char* propertyTypeName, PropertyDrawCallback drawFunc, void* userData);

    /** @brief Unregister a custom property drawer. */
    void (*UnregisterPropertyDrawer)(HookId hookId, const char* propertyTypeName);

    // ===== Batch 7: Hierarchy & Asset Browser Extensions =====

    /**
     * @brief Register a hierarchy item GUI overlay callback.
     * Called for each visible node in the hierarchy tree.
     */
    void (*RegisterHierarchyItemGUI)(HookId hookId, HierarchyItemGUICallback drawFunc, void* userData);

    /** @brief Unregister a hierarchy item GUI overlay. */
    void (*UnregisterHierarchyItemGUI)(HookId hookId);

    /**
     * @brief Register an asset browser item GUI overlay callback.
     * Called for each visible asset in the asset browser.
     */
    void (*RegisterAssetItemGUI)(HookId hookId, AssetItemGUICallback drawFunc, void* userData);

    /** @brief Unregister an asset browser item GUI overlay. */
    void (*UnregisterAssetItemGUI)(HookId hookId);

    /**
     * @brief Register a hierarchy changed event callback.
     * changeType: 0=NodeCreated, 1=NodeDestroyed, 2=NodeReparented, 3=NodeRenamed
     */
    void (*RegisterOnHierarchyChanged)(HookId hookId, HierarchyChangedCallback cb, void* userData);

    // ===== Batch 8: Additional Context Menus =====

    /** @brief Add item to scene tab context menu (right-click on scene tabs). */
    void (*AddSceneTabContextItem)(HookId hookId, const char* itemPath, MenuCallback callback, void* userData);

    /** @brief Add item to debug log context menu. */
    void (*AddDebugLogContextItem)(HookId hookId, const char* itemPath, MenuCallback callback, void* userData);

    /** @brief Add item to the import menu. */
    void (*AddImportMenuItem)(HookId hookId, const char* itemPath, MenuCallback callback, void* userData);

    /** @brief Add item to the addons menu. */
    void (*AddAddonsMenuItem)(HookId hookId, const char* itemPath, MenuCallback callback, void* userData);

    /**
     * @brief Add a custom play target to the play dropdown.
     * @param hookId Unique identifier for this hook
     * @param targetName Display name for the target
     * @param iconText Icon text for the target button
     * @param callback Function called when this target is selected and play is pressed
     * @param userData User data passed to callback
     */
    void (*AddPlayTarget)(HookId hookId, const char* targetName, const char* iconText, PlayTargetCallback callback, void* userData);

    /** @brief Remove a custom play target. */
    void (*RemovePlayTarget)(HookId hookId, const char* targetName);

    // ===== Batch 9: Drag-Drop Enhancement & Asset Pipeline =====

    /**
     * @brief Register a custom drag-drop handler.
     * @param hookId Unique identifier for this hook
     * @param targetArea Target area: "Viewport", "Hierarchy", "AssetBrowser", "Inspector"
     * @param handler Function called when a drop occurs
     * @param userData User data passed to handler
     */
    void (*RegisterDragDropHandler)(HookId hookId, const char* targetArea, DragDropHandlerCallback handler, void* userData);

    /**
     * @brief Register a custom asset importer for a file extension.
     * @param hookId Unique identifier for this hook
     * @param extension File extension to handle (e.g., ".fbx")
     * @param importFunc Function called to import the file
     * @param userData User data passed to importFunc
     */
    void (*RegisterAssetImporter)(HookId hookId, const char* extension, AssetImportCallback importFunc, void* userData);

    /** @brief Unregister a custom asset importer. */
    void (*UnregisterAssetImporter)(HookId hookId, const char* extension);

    /** @brief Register a pre-asset-import hook. Return false from callback to cancel. */
    void (*RegisterOnPreAssetImport)(HookId hookId, PreImportCallback cb, void* userData);

    /** @brief Register a post-asset-import hook. */
    void (*RegisterOnPostAssetImport)(HookId hookId, StringEventCallback cb, void* userData);

    // ===== Batch 10: Build Pipeline & Editor State =====

    /** @brief Register a pre-build hook. Return false from callback to cancel. */
    void (*RegisterOnPreBuild)(HookId hookId, PreBuildCallback cb, void* userData);

    /** @brief Register a post-build hook. */
    void (*RegisterOnPostBuild)(HookId hookId, PackageFinishedCallback cb, void* userData);

    /** @brief Register callback for editor mode changes (Scene/2D/3D/Paint). */
    void (*RegisterOnEditorModeChanged)(HookId hookId, EditorModeCallback cb, void* userData);

    /**
     * @brief Register a custom gizmo tool (adds to Translate/Rotate/Scale toolbar).
     * @param hookId Unique identifier for this hook
     * @param toolName Unique name for the tool
     * @param iconText Icon character for toolbar button
     * @param tooltip Tooltip text
     * @param drawFunc Function called when tool is active and a node is selected
     * @param userData User data passed to drawFunc
     */
    void (*RegisterGizmoTool)(
        HookId hookId,
        const char* toolName,
        const char* iconText,
        const char* tooltip,
        GizmoToolDrawCallback drawFunc,
        void* userData
    );

    /** @brief Unregister a custom gizmo tool. */
    void (*UnregisterGizmoTool)(HookId hookId, const char* toolName);

    // ===== Game Preview Resolution Presets =====

    /**
     * @brief Add a custom resolution preset to the Game Preview panel.
     * @param hookId Unique identifier for this hook
     * @param name Display name for the resolution preset (e.g., "Steam Deck 1280x800")
     * @param width Resolution width in pixels
     * @param height Resolution height in pixels
     */
    void (*AddGamePreviewResolution)(HookId hookId, const char* name, uint32_t width, uint32_t height);

    /**
     * @brief Remove a custom resolution preset from the Game Preview panel.
     * @param hookId Hook identifier used during registration
     * @param name Name of the preset to remove
     */
    void (*RemoveGamePreviewResolution)(HookId hookId, const char* name);

    // ===== Controller Server Extension =====

    /**
     * @brief Register a custom REST route on the controller server.
     * @param hookId Unique identifier for this hook
     * @param method HTTP method: "GET", "POST", "PUT", "DELETE"
     * @param path URL path, e.g., "/api/addons/mycustom"
     * @param callback Function called to handle requests
     * @param userData User data passed to callback
     */
    void (*RegisterControllerRoute)(HookId hookId, const char* method, const char* path, ControllerRouteCallback callback, void* userData);

    /**
     * @brief Unregister a custom REST route.
     * @param hookId Hook identifier used during registration
     * @param path URL path to unregister
     */
    void (*UnregisterControllerRoute)(HookId hookId, const char* path);

    /**
     * @brief Register callback for controller server state changes (started/stopped).
     * @param hookId Unique identifier for this hook
     * @param callback Function called on state change
     * @param userData User data passed to callback
     */
    void (*RegisterOnControllerServerStateChanged)(HookId hookId, ControllerServerEventCallback callback, void* userData);

    // ===== Profiling Window Extension =====

    /**
     * @brief Register a custom stat to display in the Profiling window.
     * @param hookId Unique identifier for this hook
     * @param statName Display name for the stat
     * @param category Category name for grouping (optional, can be nullptr)
     * @param maxValue Maximum value for bar visualization (use 16.67f for frame time stats)
     * @param showAsBar If true, display as progress bar; if false, display as text
     */
    void (*RegisterProfilingStat)(HookId hookId, const char* statName, const char* category, float maxValue, bool showAsBar);

    /**
     * @brief Unregister a custom profiling stat.
     * @param hookId Hook identifier used during registration
     * @param statName Name of the stat to unregister
     */
    void (*UnregisterProfilingStat)(HookId hookId, const char* statName);

    /**
     * @brief Set the current value of a custom profiling stat.
     * Call this each frame to update the displayed value.
     * @param statName Name of the stat (must match RegisterProfilingStat name)
     * @param value Current value to display
     */
    void (*SetProfilingStatValue)(const char* statName, float value);

    /**
     * @brief Callback for drawing custom profiling section content.
     * @param userData User data passed during registration
     */
    typedef void (*ProfilingSectionDrawCallback)(void* userData);

    /**
     * @brief Register a custom section in the Profiling window.
     * The section appears as a collapsible header with custom ImGui content.
     * @param hookId Unique identifier for this hook
     * @param sectionName Display name for the section header
     * @param drawFunc Function called to draw section content (use ImGui calls)
     * @param userData User data passed to drawFunc
     */
    void (*RegisterProfilingSection)(HookId hookId, const char* sectionName, void (*drawFunc)(void*), void* userData);

    /**
     * @brief Unregister a custom profiling section.
     * @param hookId Hook identifier used during registration
     * @param sectionName Name of the section to unregister
     */
    void (*UnregisterProfilingSection)(HookId hookId, const char* sectionName);

    // ===== Cleanup =====

    /**
     * @brief Remove ALL hooks registered by this hookId.
     *
     * Call this in OnUnload to ensure proper cleanup.
     *
     * @param hookId Hook identifier to remove all hooks for
     */
    void (*RemoveAllHooks)(HookId hookId);

    // ===== Late additions (appended to preserve ABI) ====================
    //
    // New function pointers must go AT THE END of this struct so prebuilt
    // plugins compiled against older headers keep finding existing hooks
    // at their original offsets. Plugin loaders must check each function
    // pointer for nullptr before calling it — older engine binaries will
    // leave fields they don't know about as nullptr.

    /**
     * @brief Add a single declarative entry to the asset browser's
     *        "Create Asset" submenu — no ImGui code needed from the addon.
     *
     * The dispatcher renders one MenuItem and invokes `callback` on click.
     * `itemPath` supports '/'-separated nesting; sibling items from any
     * addon that share a prefix merge under the same submenu in
     * registration order:
     *
     *     hooks->AddCreateAssetItem(hookId, "WorldStream/World Sector",
     *                               &OnCreateSector, userData);
     *     hooks->AddCreateAssetItem(hookId, "WorldStream/Manifest",
     *                               &OnCreateManifest, userData);
     *
     * renders as `Create Asset > WorldStream > [World Sector | Manifest]`.
     *
     * Removed automatically by RemoveAllHooks(hookId). For full ImGui
     * control inside the menu use AddCreateAssetItems (plural) instead.
     *
     * @param hookId Unique identifier for this hook (for cleanup).
     * @param itemPath '/'-separated menu path; the leaf is the visible label.
     * @param callback Invoked when the leaf MenuItem is clicked.
     * @param userData Passed back to the callback.
     */
    void (*AddCreateAssetItem)(HookId hookId, const char* itemPath,
                               MenuCallback callback, void* userData);

    // ===== Batch 11: Build Targets =====
    //
    // Lets an addon register an entire build/packaging target — compile,
    // cook, finalize, run. See PolyphaseBuildTargetAPI.h for the descriptor
    // shape. The descriptor is deep-copied; the addon may free its source
    // strings immediately after this returns. The registration is scoped to
    // hookId and is automatically cleared by RemoveAllHooks(hookId), so
    // hot-reload is safe.
    //
    // Registering twice with the same targetId replaces the previous entry;
    // this is what makes addon hot-reload swap in fresh function pointers
    // without leaving stale ones in the registry.

    /** @brief Register or replace a build/packaging target. */
    void (*RegisterBuildTarget)(HookId hookId, const struct PolyphaseBuildTargetDesc* desc);

    /** @brief Remove a previously-registered build target by id. */
    void (*UnregisterBuildTarget)(HookId hookId, const char* targetId);

    // ===== Batch 12: Viewport input polling for addons =====
    //
    // Poll-style helpers for tools (e.g. Level Builder) that want to react
    // to mouse hover and clicks inside the 3D viewport. There is no HookId
    // because nothing is allocated — old engines simply leave these fields
    // null and addons must null-check before calling.
    //
    // Both calls are intended to be invoked from inside a
    // RegisterViewportOverlay callback (which fires mid-frame with the
    // viewport rect already laid out).
    //
    // Screen coordinates are unscaled ImGui pixels (i.e. ImGui::GetIO().MousePos);
    // the implementation multiplies by EngineConfig::mEditorInterfaceScale
    // internally to match Renderer pixel space.

    /**
     * @brief Cast a ray from the editor camera through (screenX, screenY)
     *        and return the first hit.
     *
     * Tries Bullet collider ray-test first; on a miss falls back to GPU
     * id-buffer pick + bounding-sphere intersection. On a final miss, if
     * @p fallbackPlaneY is finite (i.e. not NaN), solves the ray against
     * the Y=fallbackPlaneY plane.
     *
     * @param screenX,screenY  Unscaled ImGui pixel coords.
     * @param fallbackPlaneY   World-Y of the fallback ground plane.
     *                         Pass NAN to disable the fallback.
     * @param outHitX/Y/Z      Hit position in world space (zeroed on miss).
     * @param outNormalX/Y/Z   Hit-surface normal (zeroed on miss).
     * @param outHitNode       Node3D* of the hit, or null on miss / plane hit.
     *
     * @return true on hit (collider, sphere fallback, or plane fallback),
     *         false otherwise.
     */
    bool (*Viewport_RaycastUnderMouse)(float screenX, float screenY,
                                       float fallbackPlaneY,
                                       float* outHitX, float* outHitY, float* outHitZ,
                                       float* outNormalX, float* outNormalY, float* outNormalZ,
                                       void** outHitNode);

    /**
     * @brief Read the current viewport rect and pointer state for this frame.
     *
     * `outHovered` follows the same gate as EditorImguiIsViewportHovered():
     * false when an ImGui popup is open or the cursor is outside the
     * viewport rect. `outLeftClicked` is edge-triggered on mouse release
     * with a small drag-threshold so orbit-camera drags don't count as
     * clicks. Any output pointer may be null to skip it.
     */
    void (*Viewport_GetMouseState)(float* outViewportX, float* outViewportY,
                                   float* outViewportW, float* outViewportH,
                                   float* outMouseX,    float* outMouseY,
                                   int* outHovered, int* outLeftClicked,
                                   int* outLeftDown, int* outRightClicked);

    // ===== Batch 13: Selection control (additive — safe for older addons) =====

    /**
     * @brief Suppress the editor's standard "click selects the hit node"
     *        behavior for exactly the next left-mouse-release inside the
     *        viewport. Auto-clears after one consumed click, even if that
     *        click did not actually hit a node.
     *
     * Intended call pattern: a plugin tool (Level Builder placement, paint
     * tools, lasso, etc.) that handles its own click via a viewport callback
     * sets this every frame the tool is "armed" — i.e. the next click is
     * going to be the tool's, not a selection click. The engine reads-and-
     * clears the latch in Viewport3D's click path, so the latch never
     * survives a real click.
     *
     * Safe to call when no latch is needed — it's a single bool write.
     * Null-check before calling: older engine builds won't expose this.
     */
    void (*Viewport_SuppressNextSelectionClick)();

    /**
     * @brief Programmatically clear the editor's current node selection.
     *
     * Equivalent to the user pressing Escape over an empty viewport area.
     * Plugin tools that place objects can call this after a successful
     * place to undo any spurious selection that slipped through, so
     * subsequent hotkeys (R rotate-gizmo, etc.) don't act on a stale pick.
     *
     * Null-check before calling: older engine builds won't expose this.
     */
    void (*Selection_Clear)();

    // ===== Batch 14: File dialogs + OS file-drop dispatch ==================
    //
    // Thin wrappers over the existing SYS_OpenFileDialog / SYS_SaveFileDialog
    // / SYS_SelectFolderDialog calls + the SYS_DrainDroppedFiles polling
    // mechanism, exposed so plugins (Level Builder share UI, asset importers,
    // settings panels) don't have to vendor their own platform code.
    //
    // All dialogs are SYNCHRONOUS — the call blocks until the user picks or
    // cancels. UI threads call them from a button click; no callback needed.
    // The file-drop hook IS callback-based since drops can land any frame.

    /**
     * @brief Show an OS-native file-open dialog.
     *
     * @param title           Window title (may be null for "Open").
     * @param filter          Newline-separated filter pairs, "label\0pattern\0"
     *                        style flattened to "label|pattern;label|pattern".
     *                        Null/empty = all files. Today's impl ignores
     *                        the filter (matches existing SYS_OpenFileDialog
     *                        behavior); accepted for forward compat.
     * @param initialDir      Suggested starting directory; null = system default.
     * @param outPath         Buffer to receive the chosen path (forward slashes).
     * @param outPathSize     Size of outPath buffer in bytes.
     *
     * @return 1 on selection, 0 on cancel.
     */
    int (*ShowOpenFileDialog)(const char* title,
                              const char* filter,
                              const char* initialDir,
                              char* outPath, int outPathSize);

    /**
     * @brief Show an OS-native save-file dialog (with overwrite prompt).
     *
     * @param title           Window title (may be null for "Save As").
     * @param filter          See ShowOpenFileDialog.
     * @param defaultName     Suggested file name (may include extension).
     * @param outPath         Buffer to receive the chosen path (forward slashes).
     * @param outPathSize     Size of outPath buffer.
     *
     * @return 1 on confirm, 0 on cancel.
     */
    int (*ShowSaveFileDialog)(const char* title,
                              const char* filter,
                              const char* defaultName,
                              char* outPath, int outPathSize);

    /**
     * @brief Show an OS-native folder-picker dialog.
     *
     * @param title           Window title (may be null for "Select Folder").
     * @param outPath         Buffer to receive the chosen directory (forward slashes).
     * @param outPathSize     Size of outPath buffer.
     *
     * @return 1 on selection, 0 on cancel.
     */
    int (*ShowSelectFolderDialog)(const char* title,
                                  char* outPath, int outPathSize);

    /**
     * @brief Subscribe to OS file-drop events on the editor window.
     *
     * Each frame the engine drains the OS drop queue (SYS_DrainDroppedFiles)
     * and dispatches the path list to every registered handler. Handlers
     * filter by extension / count and consume what they recognize; the
     * standard FileDropImportModal still sees the drop unless every
     * registered plugin handler returns true.
     *
     * `paths` is a contiguous array of `count` C strings, lifetime ends
     * when the callback returns — copy what you need.
     */
    typedef bool (*FileDropCallback)(int count, const char** paths, void* userData);
    void (*RegisterFileDropHandler)(HookId hookId, FileDropCallback cb, void* userData);
    void (*UnregisterFileDropHandler)(HookId hookId);

    // ===== Batch 15: Viewport Mode dropdown ============================
    //
    // Lets a native addon contribute a new entry to the top-of-editor mode
    // dropdown (Scene / 2D / 3D / Paint Colors / Paint Instances / Voxel /
    // Terrain / Tile Paint). When the user picks an addon mode the editor:
    //   - switches EditorMode to Scene3D (and clears any built-in PaintMode)
    //   - records the active addon mode id on EditorState
    //   - fires OnActivate(userData)
    // Switching to a built-in entry (or another addon entry) fires
    // OnDeactivate(userData) on the previously-active addon mode first.
    //
    // While the addon mode is active the engine calls:
    //   - Tick(deltaTime, userData)  every frame inside the viewport update
    //   - DrawPanel(userData)        every frame in the mode-properties strip
    //
    // Hot-reload safety: re-registering with the same modeId replaces the
    // entry's callbacks (so a freshly reloaded DLL's function pointers
    // overwrite the stale ones). RemoveAllHooks(hookId) deactivates the
    // mode first if it's currently active, then erases the registration.
    //
    // All callbacks are optional — pass nullptr to skip. CanActivate is a
    // gating check: if it returns false the dropdown entry greys out and
    // the editor refuses to switch into the mode. Null CanActivate = always
    // available.

    /**
     * @brief Optional gate — return false to grey out / refuse activation.
     * @param userData User data passed during registration.
     */
    typedef bool (*ViewportModeCanActivateCallback)(void* userData);

    /** @brief Fired when the mode becomes active. */
    typedef void (*ViewportModeActivateCallback)(void* userData);

    /** @brief Fired when the mode is being deactivated (mode-switch or hot-reload). */
    typedef void (*ViewportModeDeactivateCallback)(void* userData);

    /** @brief Fired every viewport-update frame while the mode is active. */
    typedef void (*ViewportModeTickCallback)(float deltaTime, void* userData);

    /** @brief Fired every frame to draw the mode's properties panel/strip. */
    typedef void (*ViewportModeDrawPanelCallback)(void* userData);

    /**
     * @brief Register (or replace) a viewport-mode dropdown entry.
     *
     * @param hookId        Owning hook id (used by RemoveAllHooks for cleanup).
     * @param modeId        Stable id, e.g. "com.myteam.splinepaint". Required.
     *                      Re-registering the same id replaces the entry.
     * @param displayName   Label shown in the dropdown. Required (non-null).
     * @param sortOrder     Ordering hint; lower = earlier. Addon entries
     *                      always render after built-ins regardless.
     * @param canActivate   Optional gate. Null = always available.
     * @param onActivate    Optional. Fired on switch-in.
     * @param onDeactivate  Optional. Fired on switch-out / hot-reload.
     * @param tick          Optional. Fired every viewport update frame.
     * @param drawPanel     Optional. Fired every frame in the panel area.
     * @param userData      Passed back to every callback.
     */
    void (*AddViewportMode)(
        HookId hookId,
        const char* modeId,
        const char* displayName,
        int32_t sortOrder,
        ViewportModeCanActivateCallback canActivate,
        ViewportModeActivateCallback onActivate,
        ViewportModeDeactivateCallback onDeactivate,
        ViewportModeTickCallback tick,
        ViewportModeDrawPanelCallback drawPanel,
        void* userData
    );

    /**
     * @brief Remove a viewport-mode entry by id.
     *
     * If the mode is currently active the editor deactivates it (fires
     * OnDeactivate, clears EditorState::mActiveAddonViewportModeId)
     * before erasing the registration.
     */
    void (*RemoveViewportMode)(HookId hookId, const char* modeId);

    /**
     * @brief Read the id of the currently-active addon viewport mode.
     *
     * @param outBuffer       Destination buffer (caller-owned).
     * @param outBufferSize   Capacity of outBuffer in bytes.
     * @return 1 if an addon mode is active and the id fit into the buffer,
     *         0 otherwise (no mode active, or buffer too small / null).
     */
    int (*GetActiveViewportMode)(char* outBuffer, int outBufferSize);

    // ===== Batch 15 / v8: viewport math for plugin gizmos ==================
    //
    // Mouse → world ray. `mx`/`my` are window-space pixels (same units
    // Viewport_GetMouseState returns). `outOrigin` is the ray's starting
    // point in world space; `outDir` is the normalized direction.
    //
    // Used by plugins that need to roll their own scene intersection —
    // axis-constrained transform gizmos, arbitrary plane intersection,
    // line-to-line distance for rotation rings, etc. The simpler
    // Viewport_RaycastUnderMouse stays for the common "give me the
    // hit point on the scene OR a fallback Y plane" case.
    void (*Viewport_GetMouseWorldRay)(float mx, float my,
                                       float* outOriginX,
                                       float* outOriginY,
                                       float* outOriginZ,
                                       float* outDirX,
                                       float* outDirY,
                                       float* outDirZ);

    // World → editor-viewport screen pixels. Returns 1 if the point
    // projects in front of the camera (sx/sy are usable window-space
    // coords); returns 0 if behind (sx/sy still filled but the caller
    // probably wants to skip drawing). The output coordinate system
    // matches Viewport_GetMouseState — i.e. the same window-space
    // pixels you'd hover-test against.
    //
    // Used by plugins that want to overlay ImGui labels / billboards
    // on top of scene objects (e.g. "Socket: Left" text floating
    // over the socket gizmo).
    int  (*Viewport_WorldToScreen)(float wx, float wy, float wz,
                                    float* outSx, float* outSy);

    // ===== Batch 16 / v9: shared editor image (thumbnail) cache ===========
    //
    // A path-keyed image -> ImGui-texture cache owned by the EDITOR, not by
    // the addon. This exists because the engine's Vulkan types (class Image,
    // class DestroyQueue, DeviceWaitIdle, GetDestroyQueue) carry no
    // POLYPHASE_API annotation and so are absent from Polyphase.lib — an addon
    // that rolls its own loader gets LNK2019 on every one of them. Rather than
    // exporting Vulkan internals, the engine does the decode and the upload
    // and hands back an opaque handle.
    //
    // The handle is `void*` and is bit-identical to ImTextureID; cast it:
    //
    //     ImTextureID tex = (ImTextureID)hooks->EditorImage_Load(absPath);
    //     if (tex != 0) ImGui::Image(tex, ImVec2(96, 96));
    //
    // Backend: Vulkan only. On other editor backends these return nullptr / 0
    // and callers should fall back to a text placeholder — which they already
    // do for the "file missing" case.
    //
    // OWNERSHIP — read this before wiring an addon:
    //   The engine owns every texture for the whole editor session. There is
    //   deliberately NO per-addon and NO per-HookId release entry point, and
    //   RemoveAllHooks(hookId) does NOT drop images this addon loaded.
    //   Reasons: (a) descriptor-set removal is immediate, not deferred through
    //   the engine's DestroyQueue, and addon unload happens mid-frame, so an
    //   unload-time release would free a descriptor the current ImGui draw
    //   list still points at; (b) sibling addons routinely resolve the SAME
    //   absolute path (shared kit icons), so a per-owner release would blank
    //   another addon's UI. Hold nothing across frames — call
    //   EditorImage_Load every frame; the lookup is a hash-map hit.

    /**
     * @brief Get (decoding + uploading on first use) an ImGui texture for the
     *        image file at `absPath`.
     *
     * One decode and one GPU upload per path per editor session. Failures
     * (missing file, undecodable data) are negatively cached, so calling this
     * every frame for a bad path costs one hash lookup, not one open().
     *
     * @param absPath Absolute path to a PNG/JPG/TGA/BMP on disk.
     * @return Opaque handle castable to ImTextureID, or nullptr on failure or
     *         on a non-Vulkan backend. Engine-owned — do NOT free.
     */
    void* (*EditorImage_Load)(const char* absPath);

    /**
     * @brief Pixel dimensions of an image previously returned by
     *        EditorImage_Load.
     *
     * Lets an addon lay out aspect-correct thumbnails without linking its own
     * image decoder. Fills outWidth/outHeight only when the path is present in
     * the cache and decoded successfully.
     *
     * @return 1 on success, 0 if the path is unknown or was a failed decode.
     */
    int (*EditorImage_GetSize)(const char* absPath, int* outWidth, int* outHeight);

    /**
     * @brief Forget `absPath` so the next EditorImage_Load re-reads the file.
     *
     * Call after the addon has rewritten the image on disk (kit re-export,
     * thumbnail regeneration). Safe to call mid-frame: the entry is unlinked
     * immediately but the GPU release is deferred to the start of the next
     * editor frame.
     *
     * Never required for cleanup — the engine releases everything at editor
     * shutdown regardless.
     */
    void (*EditorImage_Invalidate)(const char* absPath);
};

/**
 * @brief Generate a HookId from a string identifier.
 *
 * Use the addon ID or Lua script UUID as the identifier
 * to ensure hooks can be properly tracked and cleaned up.
 *
 * @param identifier Unique string identifier
 * @return HookId for use with hook functions
 */
HookId GenerateHookId(const char* identifier);

#endif // EDITOR
