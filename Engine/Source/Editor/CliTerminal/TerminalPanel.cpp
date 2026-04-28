#if EDITOR

#include "TerminalPanel.h"

#include "EditorImgui.h"
#include "Preferences/External/CliModule.h"
#include "Preferences/PreferencesManager.h"

#include "Log.h"
#include "System/System.h"

#include "imgui.h"

#include <cstring>
#include <string>
#include <EditorIcons.h>

static TerminalPanel sTerminalPanel;

TerminalPanel* GetTerminalPanel()
{
    return &sTerminalPanel;
}

namespace
{
    /**
     * Push text to both the engine's system clipboard (XCB on Linux,
     * Win32 OpenClipboard on Windows) AND ImGui's in-process clipboard so
     * the value is reachable from any other widget in the same process
     * (e.g. ImGui::InputText paste). The editor never installs an
     * io.SetClipboardTextFn callback that bridges ImGui to SYS_*Clipboard*,
     * so calling ImGui::SetClipboardText alone leaves the system clipboard
     * empty on Linux. We fan out to both here.
     */
    void CopyToClipboardBoth(const std::string& text)
    {
        SYS_SetClipboardText(text);
        ImGui::SetClipboardText(text.c_str());
    }

    CliModule* FindCliModule()
    {
        PreferencesManager* mgr = PreferencesManager::Get();
        if (mgr == nullptr)
        {
            return nullptr;
        }
        return static_cast<CliModule*>(mgr->FindModule("External/CLI"));
    }

    ImVec4 ColorForKind(TerminalEntryKind kind)
    {
        switch (kind)
        {
            case TerminalEntryKind::Stdout:   return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
            case TerminalEntryKind::Stderr:   return ImVec4(1.00f, 0.55f, 0.55f, 1.0f);
            case TerminalEntryKind::System:   return ImVec4(0.50f, 0.80f, 1.00f, 1.0f);
            case TerminalEntryKind::UserEcho: return ImVec4(0.70f, 1.00f, 0.70f, 1.0f);
            default:                          return ImVec4(1, 1, 1, 1);
        }
    }

    ImVec4 ColorForState(TerminalSessionState state)
    {
        switch (state)
        {
            case TerminalSessionState::Stopped:  return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
            case TerminalSessionState::Starting: return ImVec4(1.00f, 0.85f, 0.40f, 1.0f);
            case TerminalSessionState::Running:  return ImVec4(0.40f, 0.90f, 0.40f, 1.0f);
            case TerminalSessionState::Stopping: return ImVec4(1.00f, 0.65f, 0.30f, 1.0f);
            case TerminalSessionState::Error:    return ImVec4(1.00f, 0.40f, 0.40f, 1.0f);
            default:                             return ImVec4(1, 1, 1, 1);
        }
    }

    /** Encode a single Unicode codepoint as UTF-8. Returns byte length. */
    int EncodeUtf8(unsigned int c, char buf[4])
    {
        if (c < 0x80)
        {
            buf[0] = static_cast<char>(c);
            return 1;
        }
        if (c < 0x800)
        {
            buf[0] = static_cast<char>(0xC0 | (c >> 6));
            buf[1] = static_cast<char>(0x80 | (c & 0x3F));
            return 2;
        }
        if (c < 0x10000)
        {
            buf[0] = static_cast<char>(0xE0 | (c >> 12));
            buf[1] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            buf[2] = static_cast<char>(0x80 | (c & 0x3F));
            return 3;
        }
        buf[0] = static_cast<char>(0xF0 | (c >> 18));
        buf[1] = static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        buf[2] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        buf[3] = static_cast<char>(0x80 | (c & 0x3F));
        return 4;
    }
}

void TerminalPanel::Tick()
{
    // Detect open/close transitions to drive auto-launch and auto-close.
    bool opened = (mVisible && !mPrevVisible);
    bool closed = (!mVisible && mPrevVisible);
    mPrevVisible = mVisible;

    if (opened)
    {
        mLaunchedThisOpen = false;

        CliModule* cli = FindCliModule();
        if (cli != nullptr && cli->mEnabled && cli->mLaunchOnPanelOpen)
        {
            TerminalSessionState s = mSession.GetState();
            if (s == TerminalSessionState::Stopped)
            {
                mSession.Start();
                mLaunchedThisOpen = true;
            }
        }

        if (mSession.IsTty())
        {
            mFocusOutputNextFrame = true;
        }
        else
        {
            mFocusInputNextFrame = true;
        }
    }

    if (closed)
    {
        CliModule* cli = FindCliModule();
        if (cli != nullptr && cli->mCloseProcessOnPanelClose)
        {
            if (mSession.GetState() == TerminalSessionState::Running)
            {
                mSession.Stop();
            }
        }
    }

    mSession.Tick();
}

void TerminalPanel::Shutdown()
{
    // Persist the last command if one was set.
    CliModule* cli = FindCliModule();
    if (cli != nullptr)
    {
        cli->mLastCommand = mInputBuffer;
        cli->SetDirty(true);
    }

    mSession.ShutdownSync();
}

void TerminalPanel::DrawContent()
{
    // Top padding so the toolbar buttons aren't flush against the dock
    // tab bar / title strip.
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    DrawToolbar();
    ImGui::Spacing();
    DrawTuiKeys();
    ImGui::Spacing();
    ImGui::Separator();
    DrawOutput();
    ImGui::Separator();
    DrawInput();
}

void TerminalPanel::DrawTuiKeys()
{
    // Compact row of "send raw key" buttons for navigating interactive TUI
    // prompts (arrow keys for menus, Enter to confirm, Esc/Ctrl+C to abort).
    // Only useful when the session is running and connected to a real TTY,
    // but we render them whenever the session is running so the user can
    // also use them with pipe-mode shells if desired.

    // Labeled header so users know what these icon buttons do. Tooltip on
    // hover gives a longer explanation.
    ImGui::TextDisabled("Send Key:");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Sends a single keystroke directly to the running program.\n"
            "Useful for navigating interactive TUI prompts (claude, vim,\n"
            "lazygit, etc.) when the input box below isn't appropriate.\n\n"
            "You can also click into the output area above and type\n"
            "directly — the keyboard is forwarded to the child process\n"
            "in real time when the session is in TTY mode.");
    }
    ImGui::SameLine();

    bool running = (mSession.GetState() == TerminalSessionState::Running);
    if (!running) ImGui::BeginDisabled();

    auto sendKey = [this](const char* seq, size_t len) {
        mSession.SendRaw(seq, len);
        mScrollToBottom = true;
    };

    auto tip = [](const char* text) {
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", text);
        }
    };

    if (ImGui::SmallButton(ICON_DASHICONS_ARROW_UP))    sendKey("\x1b[A", 3);
    tip("Up arrow  (\\x1b[A)");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_DASHICONS_ARROW_DOWN))  sendKey("\x1b[B", 3);
    tip("Down arrow  (\\x1b[B)");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MATERIAL_SYMBOLS_ARROW_LEFT))  sendKey("\x1b[D", 3);
    tip("Left arrow  (\\x1b[D)");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MATERIAL_SYMBOLS_ARROW_RIGHT)) sendKey("\x1b[C", 3);
    tip("Right arrow  (\\x1b[C)");
    ImGui::SameLine();
    if (ImGui::SmallButton("Enter")) sendKey("\r", 1);
    tip("Enter / Return  (\\r)");
    ImGui::SameLine();
    if (ImGui::SmallButton("Tab"))   sendKey("\t", 1);
    tip("Tab  (\\t)");
    ImGui::SameLine();
    if (ImGui::SmallButton("Esc"))   sendKey("\x1b", 1);
    tip("Escape  (\\x1b)");
    ImGui::SameLine();
    if (ImGui::SmallButton("Space")) sendKey(" ", 1);
    tip("Space");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MATERIAL_SYMBOLS_POWER_SETTINGS_CIRCLE))    sendKey("\x03", 1);
    tip("Ctrl+C  (\\x03) — interrupt the running program");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MDI_THUMBS_UP "  : Y"))     sendKey("y", 1);
    tip("Send 'y' — answer yes to a single-key prompt");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MDI_THUMBS_DOWN "  : N"))     sendKey("n", 1);
    tip("Send 'n' — answer no to a single-key prompt");

    if (!running) ImGui::EndDisabled();
}

void TerminalPanel::DrawToolbar()
{
    TerminalSessionState state = mSession.GetState();
    bool canStart = (state == TerminalSessionState::Stopped || state == TerminalSessionState::Error);
    bool canStop  = (state == TerminalSessionState::Running);

    if (!canStart) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_MATERIAL_SYMBOLS_ROCKET_LAUNCH))
    {
        mSession.Start();
        // In TTY mode the output area is the keyboard target so interactive
        // apps can be typed into directly. In pipe mode the input box is
        // where users type commands.
        if (mSession.IsTty())
        {
            mFocusOutputNextFrame = true;
        }
        else
        {
            mFocusInputNextFrame = true;
        }
    }
    if (!canStart) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!canStop) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_IC_BASELINE_STOP))
    {
        mSession.Stop();
    }
    if (!canStop) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button(ICON_UNDO))
    {
        mSession.Restart();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_ZONDICONS_TRASH))
    {
        mSession.Clear();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MATERIAL_SYMBOLS_FILE_COPY_SHARP))
    {
        CopyToClipboardBoth(BuildAllLinesText());
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Copy entire log to clipboard");
    }

    ImGui::SameLine();
    {
        bool hasSelection = !mSelectedLineIndices.empty();
        if (!hasSelection) ImGui::BeginDisabled();
        if (ImGui::Button("Copy Selected"))
        {
            CopyToClipboardBoth(BuildSelectedLinesText());
        }
        if (ImGui::IsItemHovered() && hasSelection)
        {
            ImGui::SetTooltip(
                "Copy the %zu currently-selected row(s).\n"
                "Click a row to select, Ctrl+Click to toggle,\n"
                "Shift+Click to extend the range.",
                mSelectedLineIndices.size());
        }
        if (!hasSelection) ImGui::EndDisabled();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &mAutoScroll);

    ImGui::SameLine();
    if (ImGui::Button("Preferences..."))
    {
        // Best-effort: just log a hint. The full Preferences window opens via Edit menu.
        LogDebug("[CLI] Open Edit > Preferences > External > CLI to configure the terminal.");
    }

    // State pill on the right.
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    ImGui::TextColored(ColorForState(state), "[ %s ]", TerminalSessionStateName(state));

    const std::string& label = mSession.GetExecutableLabel();
    if (!label.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", label.c_str());
    }

    if (state == TerminalSessionState::Error && !mSession.GetLastError().empty())
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "(%s)", mSession.GetLastError().c_str());
    }
}

void TerminalPanel::DrawOutput()
{
    // Reserve space for the input box (~one line height + spacing).
    float footerHeight = ImGui::GetFrameHeightWithSpacing();
    // ImGuiWindowFlags_NoNavInputs prevents ImGui's keyboard nav from
    // consuming arrow keys when the user wants them forwarded to the child.
    ImGui::BeginChild("##CliTerminalOutput", ImVec2(0, -footerHeight), false,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoNavInputs);

    // Push the monospace terminal font (Roboto Mono with extended glyph
    // ranges) so box-drawing characters, block elements, arrows, and
    // dingbat spinners render properly instead of falling back to '?'.
    // Falls back to the editor default font if the file wasn't loaded.
    ImFont* termFont = GetEditorTerminalFont();
    if (termFont != nullptr)
    {
        ImGui::PushFont(termFont);
    }

    // Auto-focus the output area on TTY start so the user can immediately
    // type into the running program. Cleared after one frame.
    if (mFocusOutputNextFrame)
    {
        ImGui::SetWindowFocus();
        mFocusOutputNextFrame = false;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

    const auto& entries = mSession.GetBuffer().GetEntries();

    // One-shot diagnostic: log when entries first becomes non-empty so we
    // can confirm the panel sees the buffer.
    {
        static bool sLoggedNonEmpty = false;
        if (!sLoggedNonEmpty && !entries.empty())
        {
            sLoggedNonEmpty = true;
            LogDebug("[CLI] DrawOutput: first frame with entries, count=%zu buffer=%p",
                     entries.size(), (const void*)&mSession.GetBuffer());
        }
    }

    // Render each entry as one or more Selectable lines so the user can
    // click to highlight a line and right-click to copy. We split each
    // chunk on '\n' so multi-line chunks become multiple selectable rows.
    //
    // Selection model (multi-row):
    //   - Plain click            -> replace selection with this row, set anchor
    //   - Ctrl+Click             -> toggle this row, set anchor
    //   - Shift+Click            -> select inclusive range from anchor..this
    //   - Double-click           -> copy that single row to clipboard
    int globalLineIndex = 0;
    ImGuiIO& io = ImGui::GetIO();
    for (const TerminalOutputEntry& entry : entries)
    {
        ImVec4 color = ColorForKind(entry.mKind);
        ImGui::PushStyleColor(ImGuiCol_Text, color);

        const std::string& s = entry.mText;
        size_t start = 0;
        while (start <= s.size())
        {
            size_t nl = s.find('\n', start);
            size_t lineEnd = (nl == std::string::npos) ? s.size() : nl;
            size_t actualEnd = lineEnd;
            if (actualEnd > start && s[actualEnd - 1] == '\r')
            {
                --actualEnd;
            }

            // Build a stable temporary std::string for the line so we can
            // pass it to Selectable (which needs a null-terminated label)
            // and to SetClipboardText.
            std::string line;
            if (actualEnd > start)
            {
                line.assign(s.data() + start, actualEnd - start);
            }

            int rowIndex = globalLineIndex++;
            ImGui::PushID(rowIndex);

            const char* label = line.empty() ? " " : line.c_str();
            bool selected = (mSelectedLineIndices.count(rowIndex) != 0);
            if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (io.KeyShift && mSelectionAnchor >= 0)
                {
                    int lo = mSelectionAnchor < rowIndex ? mSelectionAnchor : rowIndex;
                    int hi = mSelectionAnchor > rowIndex ? mSelectionAnchor : rowIndex;
                    if (!io.KeyCtrl)
                    {
                        mSelectedLineIndices.clear();
                    }
                    for (int i = lo; i <= hi; ++i)
                    {
                        mSelectedLineIndices.insert(i);
                    }
                }
                else if (io.KeyCtrl)
                {
                    if (mSelectedLineIndices.count(rowIndex) != 0)
                    {
                        mSelectedLineIndices.erase(rowIndex);
                    }
                    else
                    {
                        mSelectedLineIndices.insert(rowIndex);
                    }
                    mSelectionAnchor = rowIndex;
                }
                else
                {
                    mSelectedLineIndices.clear();
                    mSelectedLineIndices.insert(rowIndex);
                    mSelectionAnchor = rowIndex;
                }

                if (ImGui::IsMouseDoubleClicked(0))
                {
                    CopyToClipboardBoth(line);
                }
            }

            if (ImGui::BeginPopupContextItem("##LineCtx"))
            {
                // Right-clicking a row that isn't part of the current
                // selection should make it the (sole) selection so the
                // "Copy selected" action below operates on what the user
                // actually right-clicked.
                if (mSelectedLineIndices.count(rowIndex) == 0)
                {
                    mSelectedLineIndices.clear();
                    mSelectedLineIndices.insert(rowIndex);
                    mSelectionAnchor = rowIndex;
                }

                if (ImGui::MenuItem("Copy line"))
                {
                    CopyToClipboardBoth(line);
                }
                char selLabel[64];
                snprintf(selLabel, sizeof(selLabel),
                         "Copy selected (%zu)", mSelectedLineIndices.size());
                if (ImGui::MenuItem(selLabel, nullptr, false,
                                    !mSelectedLineIndices.empty()))
                {
                    CopyToClipboardBoth(BuildSelectedLinesText());
                }
                if (ImGui::MenuItem("Copy all"))
                {
                    CopyToClipboardBoth(BuildAllLinesText());
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Select all"))
                {
                    mSelectedLineIndices.clear();
                    for (int i = 0; i < mSelectionLineCount; ++i)
                    {
                        mSelectedLineIndices.insert(i);
                    }
                }
                if (ImGui::MenuItem("Clear selection", nullptr, false,
                                    !mSelectedLineIndices.empty()))
                {
                    mSelectedLineIndices.clear();
                    mSelectionAnchor = -1;
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();

            if (nl == std::string::npos)
            {
                break;
            }
            start = nl + 1;
        }

        ImGui::PopStyleColor();
    }

    // Remember the row count for "Select all" and to allow Ctrl+A handling.
    mSelectionLineCount = globalLineIndex;

    // Ctrl+A while the output area is focused selects every visible row.
    if (ImGui::IsWindowFocused() && io.KeyCtrl &&
        ImGui::IsKeyPressed(ImGuiKey_A, false))
    {
        mSelectedLineIndices.clear();
        for (int i = 0; i < mSelectionLineCount; ++i)
        {
            mSelectedLineIndices.insert(i);
        }
    }

    // Forward raw keystrokes to the child process when in TTY mode and the
    // output child window has focus. This makes interactive TUI apps (claude,
    // lazygit, vim, etc.) usable: arrow keys navigate menus, typed characters
    // appear immediately, Ctrl+C interrupts, etc.
    bool tty = (mSession.GetState() == TerminalSessionState::Running) && mSession.IsTty();
    bool childFocused = ImGui::IsWindowFocused();
    if (tty && childFocused)
    {
        ForwardKeysToProcess();
    }
    else if (ImGui::IsWindowHovered() && ImGui::IsKeyPressed(ImGuiKey_C) &&
             (ImGui::GetIO().KeyCtrl))
    {
        // Pipe-mode fallback: Ctrl+C copies the current selection if any,
        // otherwise the entire log.
        if (!mSelectedLineIndices.empty())
        {
            CopyToClipboardBoth(BuildSelectedLinesText());
        }
        else
        {
            CopyToClipboardBoth(BuildAllLinesText());
        }
    }

    ImGui::PopStyleVar();

    if (mScrollToBottom || (mAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f))
    {
        ImGui::SetScrollHereY(1.0f);
    }
    mScrollToBottom = false;

    if (termFont != nullptr)
    {
        ImGui::PopFont();
    }

    ImGui::EndChild();
}

void TerminalPanel::DrawInput()
{
    ImGuiInputTextFlags flags =
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CallbackHistory  |
        ImGuiInputTextFlags_CallbackCompletion;

    if (mFocusInputNextFrame)
    {
        ImGui::SetKeyboardFocusHere();
        mFocusInputNextFrame = false;
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##CliInput", mInputBuffer, sizeof(mInputBuffer), flags, &TerminalPanel::InputCallback, this))
    {
        OnSubmit();
        mFocusInputNextFrame = true;
    }
}

void TerminalPanel::OnSubmit()
{
    std::string line = mInputBuffer;
    mInputBuffer[0] = '\0';

    if (line.empty())
    {
        return;
    }

    if (mSession.GetState() != TerminalSessionState::Running)
    {
        // Not running — try to start, then send.
        if (!mSession.Start())
        {
            return;
        }
    }

    if (mSession.SendCommand(line))
    {
        mHistory.Add(line);
        mScrollToBottom = true;
    }
}

void TerminalPanel::ForwardKeysToProcess()
{
    ImGuiIO& io = ImGui::GetIO();

    // ---- Typed printable characters (handles Shift, IME, layout, etc.) ----
    // Read and clear ImGui's character input queue. Each entry is a Unicode
    // codepoint that should be sent to the child as UTF-8.
    if (io.InputQueueCharacters.Size > 0)
    {
        for (int i = 0; i < io.InputQueueCharacters.Size; ++i)
        {
            unsigned int cp = static_cast<unsigned int>(io.InputQueueCharacters[i]);
            char buf[4];
            int len = EncodeUtf8(cp, buf);
            if (len > 0)
            {
                mSession.SendRaw(buf, static_cast<size_t>(len));
            }
        }
        io.InputQueueCharacters.resize(0);
        mScrollToBottom = true;
    }

    // ---- Special keys (not present in the character queue) ----
    struct KeyMap
    {
        ImGuiKey key;
        const char* seq;
        size_t len;
    };
    static const KeyMap sKeyMap[] = {
        { ImGuiKey_Enter,        "\r",         1 },
        { ImGuiKey_KeypadEnter,  "\r",         1 },
        { ImGuiKey_Tab,          "\t",         1 },
        { ImGuiKey_Escape,       "\x1b",       1 },
        { ImGuiKey_Backspace,    "\x7f",       1 },  // DEL is the conventional TTY backspace
        { ImGuiKey_UpArrow,      "\x1b[A",     3 },
        { ImGuiKey_DownArrow,    "\x1b[B",     3 },
        { ImGuiKey_RightArrow,   "\x1b[C",     3 },
        { ImGuiKey_LeftArrow,    "\x1b[D",     3 },
        { ImGuiKey_Home,         "\x1b[H",     3 },
        { ImGuiKey_End,          "\x1b[F",     3 },
        { ImGuiKey_Delete,       "\x1b[3~",    4 },
        { ImGuiKey_PageUp,       "\x1b[5~",    4 },
        { ImGuiKey_PageDown,     "\x1b[6~",    4 },
        { ImGuiKey_F1,           "\x1bOP",     3 },
        { ImGuiKey_F2,           "\x1bOQ",     3 },
        { ImGuiKey_F3,           "\x1bOR",     3 },
        { ImGuiKey_F4,           "\x1bOS",     3 },
        { ImGuiKey_F5,           "\x1b[15~",   5 },
        { ImGuiKey_F6,           "\x1b[17~",   5 },
        { ImGuiKey_F7,           "\x1b[18~",   5 },
        { ImGuiKey_F8,           "\x1b[19~",   5 },
        { ImGuiKey_F9,           "\x1b[20~",   5 },
        { ImGuiKey_F10,          "\x1b[21~",   5 },
        { ImGuiKey_F11,          "\x1b[23~",   5 },
        { ImGuiKey_F12,          "\x1b[24~",   5 },
    };
    for (const KeyMap& km : sKeyMap)
    {
        if (ImGui::IsKeyPressed(km.key, true))
        {
            mSession.SendRaw(km.seq, km.len);
            mScrollToBottom = true;
        }
    }

    // ---- Ctrl+letter -> control characters ----
    // We intercept these in TTY mode so they reach the child instead of
    // triggering ImGui shortcuts (Ctrl+C copy, Ctrl+V paste, etc.).
    if (io.KeyCtrl)
    {
        struct CtrlMap
        {
            ImGuiKey key;
            char ctrl;
        };
        static const CtrlMap sCtrl[] = {
            { ImGuiKey_A, 0x01 }, { ImGuiKey_B, 0x02 }, { ImGuiKey_C, 0x03 },
            { ImGuiKey_D, 0x04 }, { ImGuiKey_E, 0x05 }, { ImGuiKey_F, 0x06 },
            { ImGuiKey_G, 0x07 }, { ImGuiKey_H, 0x08 }, { ImGuiKey_I, 0x09 },
            { ImGuiKey_J, 0x0A }, { ImGuiKey_K, 0x0B }, { ImGuiKey_L, 0x0C },
            { ImGuiKey_M, 0x0D }, { ImGuiKey_N, 0x0E }, { ImGuiKey_O, 0x0F },
            { ImGuiKey_P, 0x10 }, { ImGuiKey_Q, 0x11 }, { ImGuiKey_R, 0x12 },
            { ImGuiKey_S, 0x13 }, { ImGuiKey_T, 0x14 }, { ImGuiKey_U, 0x15 },
            { ImGuiKey_V, 0x16 }, { ImGuiKey_W, 0x17 }, { ImGuiKey_X, 0x18 },
            { ImGuiKey_Y, 0x19 }, { ImGuiKey_Z, 0x1A },
        };
        for (const CtrlMap& cm : sCtrl)
        {
            if (ImGui::IsKeyPressed(cm.key, false))
            {
                mSession.SendRaw(&cm.ctrl, 1);
                mScrollToBottom = true;
            }
        }
    }
}

std::string TerminalPanel::BuildAllLinesText()
{
    const auto& entries = mSession.GetBuffer().GetEntries();
    std::string all;
    all.reserve(entries.size() * 32);
    for (const TerminalOutputEntry& e : entries)
    {
        all.append(e.mText);
        if (!e.mText.empty() && e.mText.back() != '\n')
        {
            all.push_back('\n');
        }
    }
    return all;
}

std::string TerminalPanel::BuildSelectedLinesText()
{
    if (mSelectedLineIndices.empty())
    {
        return {};
    }

    // Walk the buffer the same way DrawOutput does, in row order, picking
    // the rows whose row index is in mSelectedLineIndices. This guarantees
    // the copied text matches the on-screen order even when the user
    // selected rows out-of-order.
    const auto& entries = mSession.GetBuffer().GetEntries();
    std::string out;
    int rowIndex = 0;
    for (const TerminalOutputEntry& entry : entries)
    {
        const std::string& s = entry.mText;
        size_t start = 0;
        while (start <= s.size())
        {
            size_t nl = s.find('\n', start);
            size_t lineEnd = (nl == std::string::npos) ? s.size() : nl;
            size_t actualEnd = lineEnd;
            if (actualEnd > start && s[actualEnd - 1] == '\r')
            {
                --actualEnd;
            }

            if (mSelectedLineIndices.count(rowIndex) != 0)
            {
                if (actualEnd > start)
                {
                    out.append(s.data() + start, actualEnd - start);
                }
                out.push_back('\n');
            }

            ++rowIndex;
            if (nl == std::string::npos)
            {
                break;
            }
            start = nl + 1;
        }
    }
    return out;
}

int TerminalPanel::InputCallback(ImGuiInputTextCallbackData* data)
{
    TerminalPanel* panel = static_cast<TerminalPanel*>(data->UserData);
    if (panel == nullptr)
    {
        return 0;
    }

    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
    {
        const std::string* item = nullptr;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            item = panel->mHistory.Prev();
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            item = panel->mHistory.Next();
        }

        data->DeleteChars(0, data->BufTextLen);
        if (item != nullptr)
        {
            data->InsertChars(0, item->c_str());
        }
    }

    return 0;
}

#endif
