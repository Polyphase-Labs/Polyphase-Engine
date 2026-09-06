#if PLATFORM_MAC

// Cocoa half of the macOS system layer: NSApplication bootstrap, the window
// with its CAMetalLayer, the NSEvent pump that feeds INP_* and ImGui, window
// state (title / fullscreen / rect), clipboard, native file dialogs, Finder
// integration and the executable path. Everything POSIX lives in
// System_Mac.cpp.
//
// Coordinate model: the engine sees backing pixels everywhere
// (mWindowWidth/Height = CAMetalLayer.drawableSize, mouse = points *
// backingScaleFactor). Cocoa itself works in points; convert at the boundary.

#include "System/System.h"
#include "System/SystemUtils.h"

#include "Engine.h"
#include "Log.h"
#include "Input/Input.h"
#include "Input/InputTypes.h"

#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>
#include <string>
#include <vector>

#if EDITOR
#include "imgui.h"
#include "imgui_impl_mac.h"
#endif

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <CoreGraphics/CoreGraphics.h>

extern bool gWarpCursor;
extern int32_t gWarpCursorX;
extern int32_t gWarpCursorY;

static NSWindow* sWindow = nil;
static NSView* sView = nil;
static CAMetalLayer* sMetalLayer = nil;
static id sDelegate = nil;

// Set once the renderer exists; before that, resize notifications only update
// the cached size (ResizeWindow() talks to the graphics backend).
static bool sGraphicsReady = false;

// Mouse deltas accumulated from NSEvent while the cursor is trapped (the
// cursor position itself is frozen by CGAssociateMouseAndMouseCursorPosition).
static int32_t sTrapDeltaX = 0;
static int32_t sTrapDeltaY = 0;

static std::vector<std::string> sDroppedFiles;

static float BackingScale()
{
    if (sWindow != nil)
        return (float)sWindow.backingScaleFactor;
    NSScreen* screen = [NSScreen mainScreen];
    return screen != nil ? (float)screen.backingScaleFactor : 1.0f;
}

static void UpdateDrawableSize()
{
    if (sView == nil || sMetalLayer == nil)
        return;

    float scale = BackingScale();
    NSSize bounds = sView.bounds.size;
    CGSize drawable = CGSizeMake(bounds.width * scale, bounds.height * scale);

    sMetalLayer.contentsScale = scale;
    sMetalLayer.drawableSize = drawable;

    EngineState* engine = GetEngineState();
    engine->mSystem.mBackingScale = scale;

    uint32_t width = (uint32_t)drawable.width;
    uint32_t height = (uint32_t)drawable.height;

    if (sWindow.miniaturized)
    {
        width = 0;
        height = 0;
    }

    if (sGraphicsReady)
    {
        if (width != engine->mWindowWidth || height != engine->mWindowHeight || width == 0)
        {
            ResizeWindow(width, height);
        }
    }
    else if (width != 0 && height != 0)
    {
        engine->mWindowWidth = width;
        engine->mWindowHeight = height;
    }
}

// ---------------------------------------------------------------------------
// The content view. Backed by a CAMetalLayer that MoltenVK presents into.
// ---------------------------------------------------------------------------
@interface PolyphaseMetalView : NSView <NSDraggingDestination>
@end

@implementation PolyphaseMetalView

- (instancetype)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        self.wantsLayer = YES;
        self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
        [self registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
    }
    return self;
}

- (CALayer*)makeBackingLayer
{
    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.opaque = YES;
    return layer;
}

- (BOOL)isFlipped
{
    // Top-left origin, like every other Polyphase window backend.
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)wantsUpdateLayer
{
    return YES;
}

- (void)updateLayer
{
    // Drawing happens from the engine loop; nothing to do here.
}

- (void)keyDown:(NSEvent*)event
{
    // Consumed by the engine pump; swallow so Cocoa doesn't beep.
    (void)event;
}

- (void)keyUp:(NSEvent*)event
{
    (void)event;
}

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    UpdateDrawableSize();
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    UpdateDrawableSize();
}

// Drag and drop of files from Finder.
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
    return NSDragOperationCopy;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    NSPasteboard* pasteboard = [sender draggingPasteboard];
    NSArray<NSURL*>* urls = [pasteboard readObjectsForClasses:@[[NSURL class]]
                                                      options:@{ NSPasteboardURLReadingFileURLsOnlyKey : @YES }];
    for (NSURL* url in urls)
    {
        if (url.path != nil)
        {
            sDroppedFiles.push_back(std::string(url.path.UTF8String));
        }
    }
    return urls.count > 0;
}

@end

// ---------------------------------------------------------------------------
// Window + application delegate.
// ---------------------------------------------------------------------------
@interface PolyphaseAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

@implementation PolyphaseAppDelegate

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
    // Cmd-Q / Dock quit: route through the engine's own shutdown.
    GetEngineState()->mQuit = true;
    return NSTerminateCancel;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
    return NO;
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    GetEngineState()->mQuit = true;
    return NO;
}

- (void)windowDidResize:(NSNotification*)notification
{
    UpdateDrawableSize();
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification
{
    UpdateDrawableSize();
}

- (void)windowDidMiniaturize:(NSNotification*)notification
{
    if (sGraphicsReady)
        ResizeWindow(0, 0);
}

- (void)windowDidDeminiaturize:(NSNotification*)notification
{
    UpdateDrawableSize();
}

- (void)windowDidBecomeKey:(NSNotification*)notification
{
    GetEngineState()->mSystem.mWindowHasFocus = true;
    INP_TrapCursor(INP_IsCursorTrapped());
}

- (void)windowDidResignKey:(NSNotification*)notification
{
    GetEngineState()->mSystem.mWindowHasFocus = false;
    INP_ClearAllKeys();
    INP_ClearAllMouseButtons();
    // Release the trap while unfocused, but remember the request (mirrors the
    // xcb ungrab on FOCUS_OUT).
    CGAssociateMouseAndMouseCursorPosition(true);
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification
{
    GetEngineState()->mSystem.mFullscreen = true;
}

- (void)windowDidExitFullScreen:(NSNotification*)notification
{
    GetEngineState()->mSystem.mFullscreen = false;
}

@end

// ---------------------------------------------------------------------------
// Event pump
// ---------------------------------------------------------------------------
static void SetModifierKey(int32_t keyCode, bool down)
{
    if (down)
        INP_SetKey(keyCode);
    else
        INP_ClearKey(keyCode);
}

static void HandleFlagsChanged(NSEvent* event)
{
    NSEventModifierFlags flags = [event modifierFlags];

    // Device-dependent bits distinguish left/right modifiers.
    bool lShift = (flags & 0x0002) != 0;
    bool rShift = (flags & 0x0004) != 0;
    bool lCtrl  = (flags & 0x0001) != 0;
    bool rCtrl  = (flags & 0x2000) != 0;
    bool lAlt   = (flags & 0x0020) != 0;
    bool rAlt   = (flags & 0x0040) != 0;
    bool lCmd   = (flags & 0x0008) != 0;
    bool rCmd   = (flags & 0x0010) != 0;

    // If the device bits are absent (some virtual keyboards), fall back to the
    // generic masks on the left-hand keys.
    if (!lShift && !rShift && (flags & NSEventModifierFlagShift)) lShift = true;
    if (!lCtrl && !rCtrl && (flags & NSEventModifierFlagControl)) lCtrl = true;
    if (!lAlt && !rAlt && (flags & NSEventModifierFlagOption)) lAlt = true;
    if (!lCmd && !rCmd && (flags & NSEventModifierFlagCommand)) lCmd = true;

    SetModifierKey(POLYPHASE_KEY_SHIFT_L, lShift);
    SetModifierKey(POLYPHASE_KEY_SHIFT_R, rShift);
    SetModifierKey(POLYPHASE_KEY_ALT_L, lAlt);
    SetModifierKey(POLYPHASE_KEY_ALT_R, rAlt);

    // Command aliases to Control so the editor's Ctrl-based hotkeys behave as
    // Cmd shortcuts on a Mac keyboard.
    SetModifierKey(POLYPHASE_KEY_CONTROL_L, lCtrl || lCmd);
    SetModifierKey(POLYPHASE_KEY_CONTROL_R, rCtrl || rCmd);
}

static void HandleMouseMove(NSEvent* event)
{
    if (sView == nil)
        return;

    if (INP_IsCursorTrapped())
    {
        sTrapDeltaX += (int32_t)([event deltaX] * BackingScale());
        sTrapDeltaY += (int32_t)([event deltaY] * BackingScale());
        return;
    }

    NSPoint p = [event locationInWindow];
    if (event.window == nil)
        p = [sWindow convertPointFromScreen:p];
    p = [sView convertPoint:p fromView:nil];   // view is flipped: y down

    float scale = BackingScale();
    INP_SetMousePosition((int32_t)(p.x * scale), (int32_t)(p.y * scale));
}

static void HandleNSEvent(NSEvent* event)
{
    switch (event.type)
    {
    case NSEventTypeMouseMoved:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
        HandleMouseMove(event);
        break;

    case NSEventTypeLeftMouseDown:
        INP_SetMouseButton(MouseCode::MOUSE_LEFT);
        break;
    case NSEventTypeLeftMouseUp:
        INP_ClearMouseButton(MouseCode::MOUSE_LEFT);
        break;
    case NSEventTypeRightMouseDown:
        INP_SetMouseButton(MouseCode::MOUSE_RIGHT);
        break;
    case NSEventTypeRightMouseUp:
        INP_ClearMouseButton(MouseCode::MOUSE_RIGHT);
        break;
    case NSEventTypeOtherMouseDown:
    {
        NSInteger button = [event buttonNumber];
        if (button == 2) INP_SetMouseButton(MouseCode::MOUSE_MIDDLE);
        else if (button == 3) INP_SetMouseButton(MouseCode::MOUSE_X1);
        else if (button == 4) INP_SetMouseButton(MouseCode::MOUSE_X2);
        break;
    }
    case NSEventTypeOtherMouseUp:
    {
        NSInteger button = [event buttonNumber];
        if (button == 2) INP_ClearMouseButton(MouseCode::MOUSE_MIDDLE);
        else if (button == 3) INP_ClearMouseButton(MouseCode::MOUSE_X1);
        else if (button == 4) INP_ClearMouseButton(MouseCode::MOUSE_X2);
        break;
    }

    case NSEventTypeScrollWheel:
    {
        if (event.phase == NSEventPhaseCancelled)
            break;

        // Accumulate precise (trackpad) deltas into whole wheel ticks.
        static double sScrollAccum = 0.0;
        double dy = [event scrollingDeltaY];
        if ([event hasPreciseScrollingDeltas])
            dy *= 0.02;
        sScrollAccum += dy;

        int32_t ticks = (int32_t)sScrollAccum;
        if (ticks != 0)
        {
            INP_SetScrollWheelDelta(INP_GetScrollWheelDelta() + ticks);
            sScrollAccum -= ticks;
        }
        break;
    }

    case NSEventTypeKeyDown:
        if (![event isARepeat])
        {
            INP_SetKey((int32_t)[event keyCode]);
        }
        break;
    case NSEventTypeKeyUp:
        INP_ClearKey((int32_t)[event keyCode]);
        break;

    case NSEventTypeFlagsChanged:
        HandleFlagsChanged(event);
        break;

    default:
        break;
    }

#if EDITOR
    ImGui_ImplMac_EventHandler((__bridge void*)event);
#endif
}

static void BuildMainMenu()
{
    NSMenu* menuBar = [[NSMenu alloc] init];

    NSMenuItem* appItem = [[NSMenuItem alloc] init];
    [menuBar addItem:appItem];
    NSMenu* appMenu = [[NSMenu alloc] init];
    NSString* appName = [[NSProcessInfo processInfo] processName];
    [appMenu addItemWithTitle:[NSString stringWithFormat:@"Hide %@", appName] action:@selector(hide:) keyEquivalent:@"h"];
    NSMenuItem* hideOthers = [appMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    hideOthers.keyEquivalentModifierMask = NSEventModifierFlagOption | NSEventModifierFlagCommand;
    [appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:[NSString stringWithFormat:@"Quit %@", appName] action:@selector(terminate:) keyEquivalent:@"q"];
    appItem.submenu = appMenu;

    NSMenuItem* windowItem = [[NSMenuItem alloc] init];
    [menuBar addItem:windowItem];
    NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    NSMenuItem* fullscreen = [windowMenu addItemWithTitle:@"Toggle Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
    fullscreen.keyEquivalentModifierMask = NSEventModifierFlagControl | NSEventModifierFlagCommand;
    windowItem.submenu = windowMenu;
    [NSApp setWindowsMenu:windowMenu];

    [NSApp setMainMenu:menuBar];
}

void SYS_Initialize()
{
    EngineState& engine = *GetEngineState();
    SystemState& system = engine.mSystem;

    // Skip window creation in headless mode
    if (IsHeadless())
    {
        LogDebug("SYS_Initialize: Headless mode, skipping window creation");
        return;
    }

    @autoreleasepool
    {
        [NSApplication sharedApplication];
        // A bare Mach-O (no bundle) needs this to get a Dock icon and key focus.
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        PolyphaseAppDelegate* delegate = [PolyphaseAppDelegate new];
        sDelegate = delegate;
        [NSApp setDelegate:delegate];

        BuildMainMenu();
        [NSApp finishLaunching];

        NSRect contentRect = NSMakeRect(0, 0, GetEngineConfig()->mWindowWidth, GetEngineConfig()->mWindowHeight);
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

        sWindow = [[NSWindow alloc] initWithContentRect:contentRect
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
        sWindow.releasedWhenClosed = NO;
        sWindow.title = [NSString stringWithUTF8String:engine.mProjectName.c_str()];
        sWindow.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
        sWindow.delegate = delegate;
        [sWindow center];

        PolyphaseMetalView* view = [[PolyphaseMetalView alloc] initWithFrame:contentRect];
        sView = view;
        sMetalLayer = (CAMetalLayer*)view.layer;
        sWindow.contentView = view;
        [sWindow makeFirstResponder:view];

        system.mNsWindow = (__bridge void*)sWindow;
        system.mNsView = (__bridge void*)sView;
        system.mMetalLayer = (__bridge void*)sMetalLayer;

        [sWindow makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        // Populate mWindowWidth/Height (pixels) before the graphics backend
        // creates its swapchain.
        UpdateDrawableSize();

        LogDebug("Cocoa window created (%ux%u px, scale %.1f)", engine.mWindowWidth, engine.mWindowHeight, system.mBackingScale);

        if (GetEngineConfig()->mFullscreen)
        {
            SYS_SetFullscreen(true);
        }

#if EDITOR
        ImGui_ImplMac_Init(system.mNsView);
#endif
    }
}

void SYS_Shutdown()
{
    if (IsHeadless())
    {
        LogDebug("SYS_Shutdown: Headless mode, skipping window cleanup");
        return;
    }

    @autoreleasepool
    {
#if EDITOR
        ImGui_ImplMac_Shutdown();
#endif

        sGraphicsReady = false;

        if (sWindow != nil)
        {
            sWindow.delegate = nil;
            [sWindow close];
        }

        sMetalLayer = nil;
        sView = nil;
        sWindow = nil;
        [NSApp setDelegate:nil];
        sDelegate = nil;

        SystemState& system = GetEngineState()->mSystem;
        system.mNsWindow = nullptr;
        system.mNsView = nullptr;
        system.mMetalLayer = nullptr;
    }
}

void SYS_Update()
{
    if (sWindow == nil)
        return;

    // By the time the main loop runs SYS_Update the renderer exists, so
    // resize notifications can go through ResizeWindow() from here on.
    sGraphicsReady = true;

    int32_t prevMouseX = 0;
    int32_t prevMouseY = 0;
    INP_GetMousePosition(prevMouseX, prevMouseY);

    bool trappedAtStart = INP_IsCursorTrapped();
    sTrapDeltaX = 0;
    sTrapDeltaY = 0;

    @autoreleasepool
    {
        NSEvent* event = nil;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate distantPast]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES]) != nil)
        {
            HandleNSEvent(event);
            [NSApp sendEvent:event];
        }
    }

    static bool sPrevWarped = false;
    bool warped = false;

    if (gWarpCursor)
    {
        warped = true;

        float scale = BackingScale();
        NSPoint viewPoint = NSMakePoint(gWarpCursorX / scale, gWarpCursorY / scale);
        NSPoint windowPoint = [sView convertPoint:viewPoint toView:nil];
        NSRect screenRect = [sWindow convertRectToScreen:NSMakeRect(windowPoint.x, windowPoint.y, 0, 0)];

        // Cocoa screen space is bottom-left origin; CG is top-left of the primary display.
        CGFloat primaryHeight = [[NSScreen screens] firstObject].frame.size.height;
        CGPoint cgPoint = CGPointMake(screenRect.origin.x, primaryHeight - screenRect.origin.y);
        CGWarpMouseCursorPosition(cgPoint);
        if (!INP_IsCursorTrapped())
        {
            // Re-associate immediately so the warp doesn't suppress the next
            // few motion events.
            CGAssociateMouseAndMouseCursorPosition(true);
        }

        gWarpCursor = false;
    }

    if (warped != sPrevWarped)
    {
        GetEngineState()->mInput.mMouseDeltaX = 0;
        GetEngineState()->mInput.mMouseDeltaY = 0;
    }
    else if (trappedAtStart && INP_IsCursorTrapped())
    {
        GetEngineState()->mInput.mMouseDeltaX = sTrapDeltaX;
        GetEngineState()->mInput.mMouseDeltaY = sTrapDeltaY;
    }
    else
    {
        int32_t newMouseX = 0;
        int32_t newMouseY = 0;
        INP_GetMousePosition(newMouseX, newMouseY);

        GetEngineState()->mInput.mMouseDeltaX = (newMouseX - prevMouseX);
        GetEngineState()->mInput.mMouseDeltaY = (newMouseY - prevMouseY);
    }

    sPrevWarped = warped;

    if (INP_IsKeyDown(POLYPHASE_KEY_ALT_L) || INP_IsKeyDown(POLYPHASE_KEY_ALT_R))
    {
        if (INP_IsKeyJustDown(POLYPHASE_KEY_ENTER))
        {
            SYS_SetFullscreen(!GetEngineState()->mSystem.mFullscreen);
        }
    }

#if EDITOR
    ImGui_ImplMac_NewFrame();
#endif
}

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------
std::string SYS_GetExecutablePath()
{
    char buf[MAX_PATH_SIZE] = {};
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0)
    {
        return "";
    }

    char resolved[PATH_MAX] = {};
    if (realpath(buf, resolved) != nullptr)
    {
        return std::string(resolved);
    }
    return std::string(buf);
}

float SYS_GetDisplayScale()
{
    return BackingScale();
}

void SYS_DrainDroppedFiles(std::vector<std::string>& outPaths)
{
    if (!sDroppedFiles.empty())
    {
        outPaths.insert(outPaths.end(), sDroppedFiles.begin(), sDroppedFiles.end());
        sDroppedFiles.clear();
    }
}

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------
static NSURL* ProjectDirectoryURL()
{
    const std::string& projDir = GetEngineState()->mProjectDirectory;
    std::string path = projDir;
    if (path.empty() || path[0] != '/')
    {
        path = SYS_GetCurrentDirectoryPath() + projDir;
    }
    return [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()] isDirectory:YES];
}

std::vector<std::string> SYS_OpenFileDialog()
{
    std::vector<std::string> retFilenames;

    @autoreleasepool
    {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = YES;
        panel.directoryURL = ProjectDirectoryURL();

        if ([panel runModal] == NSModalResponseOK)
        {
            for (NSURL* url in panel.URLs)
            {
                retFilenames.push_back(std::string(url.path.UTF8String));
            }
        }
    }

    // Callers index [0] unconditionally (the zenity path always pushed one entry).
    if (retFilenames.empty())
    {
        retFilenames.push_back("");
    }
    return retFilenames;
}

std::string SYS_SaveFileDialog()
{
    std::string result;

    @autoreleasepool
    {
        NSSavePanel* panel = [NSSavePanel savePanel];
        panel.directoryURL = ProjectDirectoryURL();
        panel.canCreateDirectories = YES;

        if ([panel runModal] == NSModalResponseOK && panel.URL != nil)
        {
            result = panel.URL.path.UTF8String;
        }
    }

    return result;
}

std::string SYS_SelectFolderDialog()
{
    std::string result;

    @autoreleasepool
    {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = NO;
        panel.canChooseDirectories = YES;
        panel.canCreateDirectories = YES;
        panel.allowsMultipleSelection = NO;
        panel.directoryURL = ProjectDirectoryURL();

        if ([panel runModal] == NSModalResponseOK && panel.URL != nil)
        {
            result = panel.URL.path.UTF8String;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Clipboard
// ---------------------------------------------------------------------------
void SYS_SetClipboardText(const std::string& str)
{
    @autoreleasepool
    {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        [pasteboard setString:[NSString stringWithUTF8String:str.c_str()] forType:NSPasteboardTypeString];
    }
}

std::string SYS_GetClipboardText()
{
    std::string result;
    @autoreleasepool
    {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        NSString* string = [pasteboard stringForType:NSPasteboardTypeString];
        if (string != nil)
        {
            result = string.UTF8String;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Finder / default apps
// ---------------------------------------------------------------------------
void SYS_ExplorerOpenDirectory(const std::string& dirPath)
{
    std::string cmd = "open \"" + dirPath + "\"";
    SYS_Exec(cmd.c_str());
}

void SYS_OpenFileWithDefaultApp(const std::string& filePath)
{
    std::string cmd = "open \"" + filePath + "\"";
    SYS_Exec(cmd.c_str());
}

// ---------------------------------------------------------------------------
// Window state
// ---------------------------------------------------------------------------
void SYS_SetWindowTitle(const char* title)
{
    if (sWindow != nil)
    {
        sWindow.title = [NSString stringWithUTF8String:title];
    }
}

void SYS_SetFullscreen(bool fullscreen)
{
    if (sWindow == nil)
        return;

    SystemState& system = GetEngineState()->mSystem;
    bool isFullscreen = (sWindow.styleMask & NSWindowStyleMaskFullScreen) != 0;

    if (isFullscreen != fullscreen)
    {
        [sWindow toggleFullScreen:nil];
    }
    system.mFullscreen = fullscreen;
}

// Window rects are in points with a top-left screen origin, matching the
// Windows / xcb backends.
void SYS_SetWindowRect(int32_t x, int32_t y, int32_t width, int32_t height)
{
    if (sWindow == nil)
        return;

    CGFloat screenHeight = [[NSScreen screens] firstObject].frame.size.height;
    NSRect contentRect = NSMakeRect(x, screenHeight - y - height, width, height);
    NSRect frame = [sWindow frameRectForContentRect:contentRect];
    [sWindow setFrame:frame display:YES];
}

void SYS_GetWindowRect(int32_t& outX, int32_t& outY, int32_t& outWidth, int32_t& outHeight)
{
    if (sWindow == nil)
    {
        outX = outY = outWidth = outHeight = 0;
        return;
    }

    CGFloat screenHeight = [[NSScreen screens] firstObject].frame.size.height;
    NSRect contentRect = [sWindow contentRectForFrameRect:sWindow.frame];
    outX = (int32_t)contentRect.origin.x;
    outY = (int32_t)(screenHeight - contentRect.origin.y - contentRect.size.height);
    outWidth = (int32_t)contentRect.size.width;
    outHeight = (int32_t)contentRect.size.height;
}

bool SYS_IsWindowMaximized()
{
    return sWindow != nil && sWindow.zoomed;
}

void SYS_MaximizeWindow()
{
    if (sWindow != nil && !sWindow.zoomed)
    {
        [sWindow zoom:nil];
    }
}

void SYS_RestoreWindow()
{
    if (sWindow != nil && sWindow.zoomed)
    {
        [sWindow zoom:nil];
    }
}

#endif
