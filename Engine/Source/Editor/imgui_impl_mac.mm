#if PLATFORM_MAC

// See imgui_impl_mac.h. Based on the official OSX backend (imgui_impl_osx.mm)
// with the global event monitor and gamepad polling removed: the engine owns
// both (System_MacCocoa.mm pump, Input_Mac.mm gamepads).

#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_mac.h"

#include "Input/Input.h"
#include "InputDevices.h"
#include "Engine.h"
#include "Utilities.h"

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

@class ImGuiMacObserver;
@class ImGuiMacKeyEventResponder;

bool INP_IsCursorHiddenByEngine();

struct ImGui_ImplMac_Data
{
    NSView*                     View;
    NSCursor*                   MouseCursors[ImGuiMouseCursor_COUNT];
    bool                        MouseCursorHidden;
    bool                        SuperDown;
    ImGuiMacObserver*           Observer;
    ImGuiMacKeyEventResponder*  KeyEventResponder;
    NSTextInputContext*         InputContext;

    ImGui_ImplMac_Data()        { memset((void*)this, 0, sizeof(*this)); }
};

static ImGui_ImplMac_Data* ImGui_ImplMac_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplMac_Data*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}

// Undocumented methods for creating cursors.
@interface NSCursor()
+ (id)_windowResizeNorthWestSouthEastCursor;
+ (id)_windowResizeNorthEastSouthWestCursor;
+ (id)_windowResizeNorthSouthCursor;
+ (id)_windowResizeEastWestCursor;
@end

// NSTextInputClient so the macOS text input manager (dead keys, IME) can hand
// us composed text via insertText:. Raw key state reaches ImGui through the
// engine pump; this responder only translates key presses into characters.
@interface ImGuiMacKeyEventResponder: NSView<NSTextInputClient>
@end

@implementation ImGuiMacKeyEventResponder
{
    float _posX;
    float _posY;
    NSRect _imeRect;
}

- (void)setImePosX:(float)posX imePosY:(float)posY
{
    _posX = posX;
    _posY = posY;
}

- (void)updateImePosWithView:(NSView *)view
{
    NSWindow *window = view.window;
    if (!window)
        return;
    NSRect contentRect = [window contentRectForFrameRect:window.frame];
    NSRect rect = NSMakeRect(_posX, contentRect.size.height - _posY, 0, 0);
    _imeRect = [window convertRectToScreen:rect];
}

- (void)viewDidMoveToWindow
{
    // Ensure self is a first responder to receive the input events.
    [self.window makeFirstResponder:self];
}

- (void)keyDown:(NSEvent*)event
{
    // Key state was already fed to ImGui by the engine pump. Only run the
    // input manager here so insertText: fires for printable / composed text.
    [self interpretKeyEvents:@[event]];
}

- (void)keyUp:(NSEvent*)event
{
    (void)event;
}

- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange
{
    ImGuiIO& io = ImGui::GetIO();

    NSString* characters;
    if ([aString isKindOfClass:[NSAttributedString class]])
        characters = [aString string];
    else
        characters = (NSString*)aString;

    io.AddInputCharactersUTF8(characters.UTF8String);
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)doCommandBySelector:(SEL)myselector
{
}

- (nullable NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange
{
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange
{
    return _imeRect;
}

- (BOOL)hasMarkedText
{
    return NO;
}

- (NSRange)markedRange
{
    return NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange
{
    return NSMakeRange(NSNotFound, 0);
}

- (void)setMarkedText:(nonnull id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{
}

- (void)unmarkText
{
}

- (nonnull NSArray<NSAttributedStringKey>*)validAttributesForMarkedText
{
    return @[];
}

@end

@interface ImGuiMacObserver : NSObject
- (void)onApplicationBecomeActive:(NSNotification*)aNotification;
- (void)onApplicationBecomeInactive:(NSNotification*)aNotification;
@end

@implementation ImGuiMacObserver

- (void)onApplicationBecomeActive:(NSNotification*)aNotification
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddFocusEvent(true);
}

- (void)onApplicationBecomeInactive:(NSNotification*)aNotification
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddFocusEvent(false);
}

@end

static ImGuiKey ImGui_ImplMac_KeyCodeToImGuiKey(int key_code)
{
    switch (key_code)
    {
        case kVK_ANSI_A: return ImGuiKey_A;
        case kVK_ANSI_S: return ImGuiKey_S;
        case kVK_ANSI_D: return ImGuiKey_D;
        case kVK_ANSI_F: return ImGuiKey_F;
        case kVK_ANSI_H: return ImGuiKey_H;
        case kVK_ANSI_G: return ImGuiKey_G;
        case kVK_ANSI_Z: return ImGuiKey_Z;
        case kVK_ANSI_X: return ImGuiKey_X;
        case kVK_ANSI_C: return ImGuiKey_C;
        case kVK_ANSI_V: return ImGuiKey_V;
        case kVK_ANSI_B: return ImGuiKey_B;
        case kVK_ANSI_Q: return ImGuiKey_Q;
        case kVK_ANSI_W: return ImGuiKey_W;
        case kVK_ANSI_E: return ImGuiKey_E;
        case kVK_ANSI_R: return ImGuiKey_R;
        case kVK_ANSI_Y: return ImGuiKey_Y;
        case kVK_ANSI_T: return ImGuiKey_T;
        case kVK_ANSI_1: return ImGuiKey_1;
        case kVK_ANSI_2: return ImGuiKey_2;
        case kVK_ANSI_3: return ImGuiKey_3;
        case kVK_ANSI_4: return ImGuiKey_4;
        case kVK_ANSI_6: return ImGuiKey_6;
        case kVK_ANSI_5: return ImGuiKey_5;
        case kVK_ANSI_Equal: return ImGuiKey_Equal;
        case kVK_ANSI_9: return ImGuiKey_9;
        case kVK_ANSI_7: return ImGuiKey_7;
        case kVK_ANSI_Minus: return ImGuiKey_Minus;
        case kVK_ANSI_8: return ImGuiKey_8;
        case kVK_ANSI_0: return ImGuiKey_0;
        case kVK_ANSI_RightBracket: return ImGuiKey_RightBracket;
        case kVK_ANSI_O: return ImGuiKey_O;
        case kVK_ANSI_U: return ImGuiKey_U;
        case kVK_ANSI_LeftBracket: return ImGuiKey_LeftBracket;
        case kVK_ANSI_I: return ImGuiKey_I;
        case kVK_ANSI_P: return ImGuiKey_P;
        case kVK_ANSI_L: return ImGuiKey_L;
        case kVK_ANSI_J: return ImGuiKey_J;
        case kVK_ANSI_Quote: return ImGuiKey_Apostrophe;
        case kVK_ANSI_K: return ImGuiKey_K;
        case kVK_ANSI_Semicolon: return ImGuiKey_Semicolon;
        case kVK_ANSI_Backslash: return ImGuiKey_Backslash;
        case kVK_ANSI_Comma: return ImGuiKey_Comma;
        case kVK_ANSI_Slash: return ImGuiKey_Slash;
        case kVK_ANSI_N: return ImGuiKey_N;
        case kVK_ANSI_M: return ImGuiKey_M;
        case kVK_ANSI_Period: return ImGuiKey_Period;
        case kVK_ANSI_Grave: return ImGuiKey_GraveAccent;
        case kVK_ANSI_KeypadDecimal: return ImGuiKey_KeypadDecimal;
        case kVK_ANSI_KeypadMultiply: return ImGuiKey_KeypadMultiply;
        case kVK_ANSI_KeypadPlus: return ImGuiKey_KeypadAdd;
        case kVK_ANSI_KeypadClear: return ImGuiKey_NumLock;
        case kVK_ANSI_KeypadDivide: return ImGuiKey_KeypadDivide;
        case kVK_ANSI_KeypadEnter: return ImGuiKey_KeypadEnter;
        case kVK_ANSI_KeypadMinus: return ImGuiKey_KeypadSubtract;
        case kVK_ANSI_KeypadEquals: return ImGuiKey_KeypadEqual;
        case kVK_ANSI_Keypad0: return ImGuiKey_Keypad0;
        case kVK_ANSI_Keypad1: return ImGuiKey_Keypad1;
        case kVK_ANSI_Keypad2: return ImGuiKey_Keypad2;
        case kVK_ANSI_Keypad3: return ImGuiKey_Keypad3;
        case kVK_ANSI_Keypad4: return ImGuiKey_Keypad4;
        case kVK_ANSI_Keypad5: return ImGuiKey_Keypad5;
        case kVK_ANSI_Keypad6: return ImGuiKey_Keypad6;
        case kVK_ANSI_Keypad7: return ImGuiKey_Keypad7;
        case kVK_ANSI_Keypad8: return ImGuiKey_Keypad8;
        case kVK_ANSI_Keypad9: return ImGuiKey_Keypad9;
        case kVK_Return: return ImGuiKey_Enter;
        case kVK_Tab: return ImGuiKey_Tab;
        case kVK_Space: return ImGuiKey_Space;
        case kVK_Delete: return ImGuiKey_Backspace;
        case kVK_Escape: return ImGuiKey_Escape;
        case kVK_CapsLock: return ImGuiKey_CapsLock;
        case kVK_Control: return ImGuiKey_LeftCtrl;
        case kVK_Shift: return ImGuiKey_LeftShift;
        case kVK_Option: return ImGuiKey_LeftAlt;
        case kVK_Command: return ImGuiKey_LeftSuper;
        case kVK_RightControl: return ImGuiKey_RightCtrl;
        case kVK_RightShift: return ImGuiKey_RightShift;
        case kVK_RightOption: return ImGuiKey_RightAlt;
        case kVK_RightCommand: return ImGuiKey_RightSuper;
        case kVK_F5: return ImGuiKey_F5;
        case kVK_F6: return ImGuiKey_F6;
        case kVK_F7: return ImGuiKey_F7;
        case kVK_F3: return ImGuiKey_F3;
        case kVK_F8: return ImGuiKey_F8;
        case kVK_F9: return ImGuiKey_F9;
        case kVK_F11: return ImGuiKey_F11;
        case kVK_F13: return ImGuiKey_PrintScreen;
        case kVK_F10: return ImGuiKey_F10;
        case 0x6E: return ImGuiKey_Menu;
        case kVK_F12: return ImGuiKey_F12;
        case kVK_Help: return ImGuiKey_Insert;
        case kVK_Home: return ImGuiKey_Home;
        case kVK_PageUp: return ImGuiKey_PageUp;
        case kVK_ForwardDelete: return ImGuiKey_Delete;
        case kVK_F4: return ImGuiKey_F4;
        case kVK_End: return ImGuiKey_End;
        case kVK_F2: return ImGuiKey_F2;
        case kVK_PageDown: return ImGuiKey_PageDown;
        case kVK_F1: return ImGuiKey_F1;
        case kVK_LeftArrow: return ImGuiKey_LeftArrow;
        case kVK_RightArrow: return ImGuiKey_RightArrow;
        case kVK_DownArrow: return ImGuiKey_DownArrow;
        case kVK_UpArrow: return ImGuiKey_UpArrow;
        default: return ImGuiKey_None;
    }
}

bool ImGui_ImplMac_Init(void* nsView)
{
    NSView* view = (__bridge NSView*)nsView;

    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

    ImGui_ImplMac_Data* bd = IM_NEW(ImGui_ImplMac_Data)();
    io.BackendPlatformUserData = (void*)bd;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendPlatformName = "imgui_impl_mac";

    bd->View = view;
    bd->Observer = [ImGuiMacObserver new];

    ImGui::GetMainViewport()->PlatformHandleRaw = nsView;

    // Load cursors. Some of them are undocumented.
    bd->MouseCursorHidden = false;
    bd->MouseCursors[ImGuiMouseCursor_Arrow] = [NSCursor arrowCursor];
    bd->MouseCursors[ImGuiMouseCursor_TextInput] = [NSCursor IBeamCursor];
    bd->MouseCursors[ImGuiMouseCursor_ResizeAll] = [NSCursor closedHandCursor];
    bd->MouseCursors[ImGuiMouseCursor_Hand] = [NSCursor pointingHandCursor];
    bd->MouseCursors[ImGuiMouseCursor_NotAllowed] = [NSCursor operationNotAllowedCursor];
    bd->MouseCursors[ImGuiMouseCursor_ResizeNS] = [NSCursor respondsToSelector:@selector(_windowResizeNorthSouthCursor)] ? [NSCursor _windowResizeNorthSouthCursor] : [NSCursor resizeUpDownCursor];
    bd->MouseCursors[ImGuiMouseCursor_ResizeEW] = [NSCursor respondsToSelector:@selector(_windowResizeEastWestCursor)] ? [NSCursor _windowResizeEastWestCursor] : [NSCursor resizeLeftRightCursor];
    bd->MouseCursors[ImGuiMouseCursor_ResizeNESW] = [NSCursor respondsToSelector:@selector(_windowResizeNorthEastSouthWestCursor)] ? [NSCursor _windowResizeNorthEastSouthWestCursor] : [NSCursor closedHandCursor];
    bd->MouseCursors[ImGuiMouseCursor_ResizeNWSE] = [NSCursor respondsToSelector:@selector(_windowResizeNorthWestSouthEastCursor)] ? [NSCursor _windowResizeNorthWestSouthEastCursor] : [NSCursor closedHandCursor];

    io.SetClipboardTextFn = [](void*, const char* str) -> void
    {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString] owner:nil];
        [pasteboard setString:[NSString stringWithUTF8String:str] forType:NSPasteboardTypeString];
    };

    io.GetClipboardTextFn = [](void*) -> const char*
    {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        NSString* available = [pasteboard availableTypeFromArray: [NSArray arrayWithObject:NSPasteboardTypeString]];
        if (![available isEqualToString:NSPasteboardTypeString])
            return nullptr;

        NSString* string = [pasteboard stringForType:NSPasteboardTypeString];
        if (string == nil)
            return nullptr;

        const char* string_c = (const char*)[string UTF8String];
        size_t string_len = strlen(string_c);
        static ImVector<char> s_clipboard;
        s_clipboard.resize((int)string_len + 1);
        strcpy(s_clipboard.Data, string_c);
        return s_clipboard.Data;
    };

    [[NSNotificationCenter defaultCenter] addObserver:bd->Observer
                                             selector:@selector(onApplicationBecomeActive:)
                                                 name:NSApplicationDidBecomeActiveNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:bd->Observer
                                             selector:@selector(onApplicationBecomeInactive:)
                                                 name:NSApplicationDidResignActiveNotification
                                               object:nil];

    // Add the NSTextInputClient to the view hierarchy,
    // to receive keyboard events and translate them to input text.
    bd->KeyEventResponder = [[ImGuiMacKeyEventResponder alloc] initWithFrame:NSZeroRect];
    bd->InputContext = [[NSTextInputContext alloc] initWithClient:bd->KeyEventResponder];
    [view addSubview:bd->KeyEventResponder];

    io.SetPlatformImeDataFn = [](ImGuiViewport* viewport, ImGuiPlatformImeData* data) -> void
    {
        ImGui_ImplMac_Data* bd = ImGui_ImplMac_GetBackendData();
        if (bd == nullptr)
            return;
        if (data->WantVisible)
        {
            [bd->InputContext activate];
        }
        else
        {
            [bd->InputContext discardMarkedText];
            [bd->InputContext invalidateCharacterCoordinates];
            [bd->InputContext deactivate];
        }
        // ImGui coordinates are in pixels; the responder wants points.
        float scale = GetEngineState()->mSystem.mBackingScale;
        if (scale <= 0.0f) scale = 1.0f;
        [bd->KeyEventResponder setImePosX:data->InputPos.x / scale imePosY:(data->InputPos.y + data->InputLineHeight) / scale];
    };

    return true;
}

void ImGui_ImplMac_Shutdown()
{
    ImGui_ImplMac_Data* bd = ImGui_ImplMac_GetBackendData();
    IM_ASSERT(bd != nullptr && "No platform backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    [[NSNotificationCenter defaultCenter] removeObserver:bd->Observer];
    [bd->KeyEventResponder removeFromSuperview];
    bd->Observer = nil;
    bd->KeyEventResponder = nil;
    bd->InputContext = nil;
    bd->View = nil;
    for (int i = 0; i < ImGuiMouseCursor_COUNT; ++i)
        bd->MouseCursors[i] = nil;

    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_HasGamepad);
    io.SetClipboardTextFn = nullptr;
    io.GetClipboardTextFn = nullptr;
    io.SetPlatformImeDataFn = nullptr;
    IM_DELETE(bd);
}

static void ImGui_ImplMac_UpdateMouseCursor()
{
    ImGui_ImplMac_Data* bd = ImGui_ImplMac_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return;

    // The engine owns cursor visibility while it has hidden the cursor
    // (e.g. mouse-look during play-in-editor).
    if (INP_IsCursorHiddenByEngine())
        return;

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (io.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None)
    {
        if (!bd->MouseCursorHidden)
        {
            bd->MouseCursorHidden = true;
            [NSCursor hide];
        }
    }
    else
    {
        NSCursor* desired = bd->MouseCursors[imgui_cursor] ?: bd->MouseCursors[ImGuiMouseCursor_Arrow];
        if (desired != NSCursor.currentCursor)
        {
            [desired set];
        }
        if (bd->MouseCursorHidden)
        {
            bd->MouseCursorHidden = false;
            [NSCursor unhide];
        }
    }
}

static void ImGui_ImplMac_UpdateKeyModifiers()
{
    ImGui_ImplMac_Data* bd = ImGui_ImplMac_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl,  IsControlDown());
    io.AddKeyEvent(ImGuiMod_Shift, IsShiftDown());
    io.AddKeyEvent(ImGuiMod_Alt,   IsAltDown());
    io.AddKeyEvent(ImGuiMod_Super, bd->SuperDown);
}

void ImGui_ImplMac_NewFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplMac_Data* bd = ImGui_ImplMac_GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplMac_Init()?");

    // Setup display size (every frame to accommodate for window resizing).
    // Pixels, like every other Polyphase backend; the editor applies its own
    // interface scale on top.
    io.DisplaySize = ImVec2((float)GetEngineState()->mWindowWidth, (float)GetEngineState()->mWindowHeight);

    // Setup time step
    io.DeltaTime = glm::max(GetEngineState()->mRealDeltaTime, 0.0001f);

    ImGui_ImplMac_UpdateKeyModifiers();
    ImGui_ImplMac_UpdateMouseCursor();

    if (io.WantTextInput)
        [bd->KeyEventResponder updateImePosWithView:bd->View];
}

// Must only be called for a mouse event, otherwise an exception occurs.
static ImGuiMouseSource GetMouseSource(NSEvent* event)
{
    switch (event.subtype)
    {
        case NSEventSubtypeTabletPoint:
            return ImGuiMouseSource_Pen;
        case NSEventSubtypeMouseEvent:
        default:
            return ImGuiMouseSource_Mouse;
    }
}

int32_t ImGui_ImplMac_EventHandler(void* nsEvent)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return 0;

    ImGui_ImplMac_Data* bd = ImGui_ImplMac_GetBackendData();
    if (bd == nullptr)
        return 0;

    NSEvent* event = (__bridge NSEvent*)nsEvent;
    NSView* view = bd->View;
    ImGuiIO& io = ImGui::GetIO();

    if (event.type == NSEventTypeLeftMouseDown || event.type == NSEventTypeRightMouseDown || event.type == NSEventTypeOtherMouseDown)
    {
        int button = (int)[event buttonNumber];
        if (button >= 0 && button < ImGuiMouseButton_COUNT)
        {
            io.AddMouseSourceEvent(GetMouseSource(event));
            io.AddMouseButtonEvent(button, true);
        }
        return 0;
    }

    if (event.type == NSEventTypeLeftMouseUp || event.type == NSEventTypeRightMouseUp || event.type == NSEventTypeOtherMouseUp)
    {
        int button = (int)[event buttonNumber];
        if (button >= 0 && button < ImGuiMouseButton_COUNT)
        {
            io.AddMouseSourceEvent(GetMouseSource(event));
            io.AddMouseButtonEvent(button, false);
        }
        return 0;
    }

    if (event.type == NSEventTypeMouseMoved || event.type == NSEventTypeLeftMouseDragged || event.type == NSEventTypeRightMouseDragged || event.type == NSEventTypeOtherMouseDragged)
    {
        NSPoint mousePoint = event.locationInWindow;
        if (event.window == nil)
            mousePoint = [[view window] convertPointFromScreen:mousePoint];
        mousePoint = [view convertPoint:mousePoint fromView:nil];
        if (![view isFlipped])
            mousePoint = NSMakePoint(mousePoint.x, view.bounds.size.height - mousePoint.y);

        float scale = GetEngineState()->mSystem.mBackingScale;
        if (scale <= 0.0f) scale = 1.0f;
        io.AddMouseSourceEvent(GetMouseSource(event));
        io.AddMousePosEvent((float)mousePoint.x * scale, (float)mousePoint.y * scale);
        return 0;
    }

    if (event.type == NSEventTypeScrollWheel)
    {
        // Ignore canceled events (two-finger tap to stop momentum scrolling can
        // otherwise deliver a large bogus delta).
        if (event.phase == NSEventPhaseCancelled)
            return 0;

        double wheel_dx = [event scrollingDeltaX];
        double wheel_dy = [event scrollingDeltaY];
        if ([event hasPreciseScrollingDeltas])
        {
            wheel_dx *= 0.01;
            wheel_dy *= 0.01;
        }
        if (wheel_dx != 0.0 || wheel_dy != 0.0)
            io.AddMouseWheelEvent((float)wheel_dx, (float)wheel_dy);

        return 0;
    }

    if (event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp)
    {
        if ([event isARepeat])
            return 0;

        int key_code = (int)[event keyCode];
        ImGuiKey key = ImGui_ImplMac_KeyCodeToImGuiKey(key_code);
        if (key != ImGuiKey_None)
        {
            io.AddKeyEvent(key, event.type == NSEventTypeKeyDown);
            io.SetKeyEventNativeData(key, key_code, -1);
        }
        return 0;
    }

    if (event.type == NSEventTypeFlagsChanged)
    {
        unsigned short key_code = [event keyCode];
        NSEventModifierFlags modifier_flags = [event modifierFlags];

        bd->SuperDown = (modifier_flags & NSEventModifierFlagCommand) != 0;

        ImGuiKey key = ImGui_ImplMac_KeyCodeToImGuiKey(key_code);
        if (key != ImGuiKey_None)
        {
            // macOS does not generate down/up event for modifiers. Use the
            // hardware dependent masks to extract that information.
            NSEventModifierFlags mask = 0;
            switch (key)
            {
                case ImGuiKey_LeftCtrl:   mask = 0x0001; break;
                case ImGuiKey_RightCtrl:  mask = 0x2000; break;
                case ImGuiKey_LeftShift:  mask = 0x0002; break;
                case ImGuiKey_RightShift: mask = 0x0004; break;
                case ImGuiKey_LeftSuper:  mask = 0x0008; break;
                case ImGuiKey_RightSuper: mask = 0x0010; break;
                case ImGuiKey_LeftAlt:    mask = 0x0020; break;
                case ImGuiKey_RightAlt:   mask = 0x0040; break;
                default:
                    return 0;
            }

            io.AddKeyEvent(key, (modifier_flags & mask) != 0);
            io.SetKeyEventNativeData(key, key_code, -1);
        }
        return 0;
    }

    return 0;
}

#endif // #ifndef IMGUI_DISABLE

#endif
