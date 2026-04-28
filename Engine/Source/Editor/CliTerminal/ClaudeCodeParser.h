#pragma once

#if EDITOR

#include "AnsiStripper.h"
#include "ITerminalOutputParser.h"

#include <deque>
#include <string>

/**
 * @brief Parser tuned for Anthropic's Claude Code CLI (and other Ink-based
 *        TUIs).
 *
 * Claude Code is built on Ink (https://github.com/vadimdemedes/ink), a React
 * renderer for terminal apps. Ink-rendered apps share a very specific output
 * pattern that the default ANSI stripper handles poorly when displayed as a
 * scrolling log:
 *
 *  - They enter the alternate screen buffer (ESC[?1049h) on launch.
 *  - They re-render the entire visible frame on every state change. Each
 *    frame begins with cursor-home / clear-screen and ends with the same
 *    text as the previous frame plus a one-character delta. In a log view
 *    this looks like the same screen being printed dozens of times per
 *    second — useless noise.
 *  - They emit bracketed-paste / mouse-tracking DECSET sequences which the
 *    default stripper drops cleanly but which can leave stray "h"/"l"
 *    characters if mis-parsed.
 *  - They periodically query the cursor position (ESC[6n). We can't reply
 *    on this side of the PTY, but at least we can drop the query bytes.
 *
 * The strategy here is layered:
 *
 *   1. Run the byte stream through the existing AnsiStripper so all the
 *      generic CSI / OSC handling still applies (CRLF normalisation, two-byte
 *      escapes, frame-marker newlines, etc.).
 *   2. Track alt-screen entry/exit (ESC[?1049h/l) so future passes can
 *      use it as a hint, even though the bytes themselves are dropped by
 *      the inner stripper's generic CSI handler.
 *   3. After the stripper runs, post-process the output to:
 *        - collapse runs of consecutive blank lines down to one,
 *        - rewrite Unicode box-drawing / arrow / bullet characters to
 *          ASCII equivalents so the editor's bundled ImGui font (which
 *          lacks these glyphs) doesn't fall back to "?" on every cell.
 *
 * Earlier versions of this parser also did line-level dedupe of "recent"
 * lines to suppress Ink redraw spam, but in practice claude redraws the
 * entire frame on every keystroke, so legitimate content was being eaten
 * as a "duplicate" and the panel ended up nearly empty. The dedupe has
 * been removed; if redraw noise becomes the dominant problem we should
 * move to whole-frame dedupe (snapshot the screen between two ESC[2J
 * markers and only emit when the snapshot changes), not line-level.
 *
 * The alt-screen detection is a peephole scan, not a full state machine — we
 * specifically look for the literal ESC[?1049h / ESC[?1049l byte sequences
 * (split across chunks if needed) and forward everything else verbatim to
 * the inner stripper.
 *
 * If the heuristics turn out wrong for a particular use case, set
 * CLI_TERMINAL_DEBUG_DUMP=1 in AnsiStripper.cpp to capture the exact byte
 * stream and tune the rules from there.
 */
class ClaudeCodeParser : public ITerminalOutputParser
{
public:
    ClaudeCodeParser();

    std::string Process(const char* data, size_t len) override;
    void        Reset() override;
    const char* GetName() const override { return "claude-code"; }

private:
    /**
     * Look for ESC[?1049h / ESC[?1049l in `data` and update mAltScreen.
     * Bytes are forwarded to the inner stripper unchanged — the stripper's
     * generic CSI handler already drops the ?1049h/l sequence — we only use
     * the scan to track mode for post-processing.
     */
    void ScanAltScreenToggles(const char* data, size_t len);

    /**
     * Apply Claude-specific post-processing to a chunk of stripper output:
     *   - Collapses runs of consecutive blank lines,
     *   - Rewrites Unicode box-drawing / arrow / bullet glyphs to ASCII.
     */
    std::string PostProcess(std::string stripped);

    /**
     * Replace Unicode glyphs that the editor's ImGui font cannot render
     * (box drawing, arrows, bullets, etc.) with ASCII equivalents. Operates
     * in-place on a UTF-8 string.
     */
    static void RewriteUnrenderableGlyphs(std::string& s);

    /** Inner generic stripper. */
    AnsiStripper mStripper;

    /** True between ESC[?1049h and ESC[?1049l. */
    bool mAltScreen = false;

    /**
     * Rolling state for the alt-screen toggle peephole scanner so the
     * literal byte sequence ESC[?1049h can be detected even when split
     * across chunk boundaries. Holds at most 8 bytes of unmatched prefix.
     */
    std::string mPeephole;

    /** Whether the previous PostProcess emit ended on a newline. */
    bool mEndsWithNewline = true;

    /**
     * Move-to-front LRU of recently emitted lines, used in alt-screen mode
     * to suppress Ink frame redraws. When a line being emitted is found in
     * the deque it is dropped and moved to the front (so persistently
     * redrawn frame chrome stays warm and never ages out). When it isn't
     * found it's emitted and pushed to the front, evicting the least
     * recently touched line if the deque is full.
     *
     * Disabled in non-alt-screen mode so plain pipe-mode output is left
     * untouched. Cleared on every alt-screen toggle so a fresh enter
     * starts with a clean slate.
     */
    std::deque<std::string> mRecentEmittedLines;
    static constexpr size_t kRecentEmittedMax = 64;
};

#endif
