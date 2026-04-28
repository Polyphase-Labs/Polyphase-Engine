#pragma once

#if EDITOR

#include "CommandHistory.h"
#include "TerminalSession.h"

#include <set>
#include <string>

struct ImGuiInputTextCallbackData;

/**
 * @brief Singleton dockable CLI Terminal panel.
 *
 * Owns one TerminalSession and one CommandHistory. Drawn from the editor
 * dockspace via DrawContent(). Tick() is called every frame from EditorMain
 * to drain output and advance the session state machine. Shutdown() is
 * called from EditorImguiPreShutdown to ensure all child processes and
 * worker threads are torn down before the rest of the editor.
 */
class TerminalPanel
{
public:
    void DrawContent();
    void Tick();
    void Shutdown();

    bool mVisible = false;

private:
    void DrawToolbar();
    void DrawTuiKeys();
    void DrawOutput();
    void DrawInput();
    void OnSubmit();
    void ForwardKeysToProcess();

    static int InputCallback(ImGuiInputTextCallbackData* data);

    TerminalSession mSession;
    CommandHistory  mHistory;

    char mInputBuffer[2048] = {};
    bool mAutoScroll = true;
    bool mFocusInputNextFrame = false;
    bool mFocusOutputNextFrame = false;
    bool mPrevVisible = false;
    bool mLaunchedThisOpen = false;

    bool mScrollToBottom = false;

    // Multi-row selection in the output area. mSelectedLineIndices holds
    // the set of currently-selected (zero-based) row indices. mSelectionAnchor
    // is the row index of the most recent plain click and acts as the anchor
    // for shift-click range extension. mSelectionLineCount is the row count
    // observed on the previous frame and is used to drop indices that fell
    // off the top of the bounded ring buffer.
    std::set<int> mSelectedLineIndices;
    int mSelectionAnchor = -1;
    int mSelectionLineCount = 0;

    // Build a newline-joined string of the rows in mSelectedLineIndices,
    // walking the buffer in row order so the result matches on-screen order.
    // Not const because TerminalSession::GetBuffer() is non-const.
    std::string BuildSelectedLinesText();

    // Build a newline-joined string of every entry in the buffer.
    std::string BuildAllLinesText();
};

TerminalPanel* GetTerminalPanel();

#endif
