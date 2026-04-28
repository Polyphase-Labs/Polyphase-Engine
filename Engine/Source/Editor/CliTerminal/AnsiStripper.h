#pragma once

#if EDITOR

#include "ITerminalOutputParser.h"

#include <string>

/**
 * @brief Stateful ANSI escape sequence stripper for ConPTY output.
 *
 * The Windows pseudo-console emits a real terminal byte stream — text
 * interleaved with CSI/OSC escape sequences (cursor positioning, colors,
 * window titles, etc.). The CLI Terminal panel renders log lines, not a
 * full terminal emulator, so we strip the escape sequences and keep only
 * printable text plus newlines/tabs.
 *
 * The state machine persists across calls because escape sequences can be
 * split across read() chunks (a 4 KB read may end mid-escape).
 *
 * Handles:
 *  - CSI sequences:  ESC [ params? final           (final = 0x40..0x7E)
 *  - OSC sequences:  ESC ] string ( BEL | ESC \ )
 *  - Two-byte escapes: ESC <single byte>
 *  - CRLF normalisation: \r\n -> \n, bare \r -> dropped
 *  - Bell, backspace, and other C0 control chars: dropped
 *  - LF and TAB: kept
 *
 * Does NOT yet handle:
 *  - Cursor-positioning re-render (e.g. progress bars that overwrite a line)
 *  - SGR colour parsing (could be added later for coloured output)
 */
class AnsiStripper : public ITerminalOutputParser
{
public:
    /** Process a chunk of raw bytes; returns the cleaned text. */
    std::string Process(const char* data, size_t len) override;

    /** Reset state (call between sessions). */
    void Reset() override { mState = State::Normal; mEndsWithNewline = true; }

    const char* GetName() const override { return "default"; }

    /**
     * Whether ESC[H / ESC[f / ESC[A / ESC[J cursor-position sequences emit
     * a synthetic newline. Default true — useful for non-TTY tools that
     * redraw progress bars in place. ClaudeCodeParser disables this in
     * alt-screen mode because Ink-rendered apps update individual cells
     * via cursor positioning and the synthetic newlines turn each updated
     * character into its own log row.
     */
    void SetSynthesizeFrameNewlines(bool enable) { mSynthesizeFrameNewlines = enable; }
    bool GetSynthesizeFrameNewlines() const      { return mSynthesizeFrameNewlines; }

private:
    enum class State
    {
        Normal,       // regular text
        JustSawCr,    // last byte was CR; deciding CRLF vs bare CR
        Esc,          // last byte was ESC; deciding what kind of sequence follows
        Csi,          // inside ESC [ ... <final>
        Osc,          // inside ESC ] ... ( BEL | ESC \ )
        OscEscaped,   // inside OSC, just saw ESC; waiting for backslash
    };

    State mState = State::Normal;

    // Buffer for accumulating CSI parameter bytes so we can interpret a few
    // common cursor movements (specifically "cursor forward N" -> emit N
    // spaces) which TUI apps use for layout instead of literal spaces.
    std::string mCsiParams;

    // Tracks whether the last byte we emitted (across all Process() calls)
    // was a newline. Used to suppress synthetic frame-redraw newlines that
    // would otherwise stack up on TTY backends (notably Linux PTY) where
    // TUI apps like `claude` emit a clear-screen + cursor-home at the start
    // of every chunk, producing a blank line per redraw cycle.
    bool mEndsWithNewline = true;

    // See SetSynthesizeFrameNewlines.
    bool mSynthesizeFrameNewlines = true;
};

#endif
