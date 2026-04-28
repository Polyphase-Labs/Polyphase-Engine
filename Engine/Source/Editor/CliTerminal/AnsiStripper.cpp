#if EDITOR

#include "AnsiStripper.h"

#include "Log.h"

#include <cstdio>
#include <cstdlib>

// Set to 1 to dump every raw chunk that arrives at the stripper as
// "<hex>  <ascii>" lines via LogDebug. Useful when adapting the parser to
// new TUI apps (e.g. claude on Linux PTY) — leave at 0 in normal builds
// because it is extremely chatty.
#ifndef CLI_TERMINAL_DEBUG_DUMP
#define CLI_TERMINAL_DEBUG_DUMP 0
#endif

namespace
{
#if CLI_TERMINAL_DEBUG_DUMP
    void DebugDumpChunk(const char* data, size_t len)
    {
        // 16 bytes per row, hex on the left and printable-ASCII on the right.
        // Control bytes show as "." in the ASCII column and ESC is rendered
        // explicitly as "\e" in a separate prefix so escape sequences stand
        // out at a glance.
        constexpr size_t kRow = 16;
        char hex[kRow * 3 + 1];
        char asc[kRow + 1];
        for (size_t off = 0; off < len; off += kRow)
        {
            size_t n = (len - off) < kRow ? (len - off) : kRow;
            size_t hp = 0;
            for (size_t i = 0; i < n; ++i)
            {
                unsigned char b = static_cast<unsigned char>(data[off + i]);
                hp += static_cast<size_t>(snprintf(hex + hp, sizeof(hex) - hp,
                                                   "%02X ", b));
                asc[i] = (b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.';
            }
            for (size_t i = n; i < kRow; ++i)
            {
                hp += static_cast<size_t>(snprintf(hex + hp, sizeof(hex) - hp,
                                                   "   "));
                asc[i] = ' ';
            }
            asc[kRow] = '\0';
            LogDebug("[CLI][raw] %04zX  %s |%s|", off, hex, asc);
        }
    }
#endif

    bool IsPrintableOrAllowedControl(unsigned char c)
    {
        // Printable ASCII (and high bytes for UTF-8 passthrough).
        if (c >= 0x20) return true;
        // Allowed control chars.
        if (c == '\n' || c == '\t') return true;
        return false;
    }

    /**
     * Parse the leading integer of a CSI parameter string (e.g. "12;3" -> 12).
     * Returns the default value if the string is empty or doesn't start with a
     * digit. Used to recover N from "ESC [ N C" (cursor forward N columns).
     */
    int ParseFirstParam(const std::string& params, int defaultValue)
    {
        if (params.empty()) return defaultValue;
        char* end = nullptr;
        long v = std::strtol(params.c_str(), &end, 10);
        if (end == params.c_str() || v <= 0) return defaultValue;
        if (v > 1024) v = 1024;  // sanity cap
        return static_cast<int>(v);
    }
}

std::string AnsiStripper::Process(const char* data, size_t len)
{
#if CLI_TERMINAL_DEBUG_DUMP
    if (len > 0)
    {
        LogDebug("[CLI][raw] === chunk len=%zu ===", len);
        DebugDumpChunk(data, len);
    }
#endif

    std::string out;
    out.reserve(len);

    for (size_t i = 0; i < len; ++i)
    {
        unsigned char c = static_cast<unsigned char>(data[i]);

        switch (mState)
        {
            case State::Normal:
                if (c == 0x1B)             // ESC
                {
                    mState = State::Esc;
                    break;
                }
                if (c == '\r')
                {
                    mState = State::JustSawCr;
                    break;
                }
                if (IsPrintableOrAllowedControl(c))
                {
                    out.push_back(static_cast<char>(c));
                }
                // else: bell (0x07), backspace (0x08), other C0 -> drop
                break;

            case State::JustSawCr:
                if (c == '\n')
                {
                    out.push_back('\n');           // CRLF -> LF
                    mState = State::Normal;
                    break;
                }
                // Bare CR: drop the CR and re-process this byte from Normal.
                mState = State::Normal;
                --i;  // for-loop's ++i will land us back on this same byte
                break;

            case State::Esc:
                if (c == '[')
                {
                    mCsiParams.clear();
                    mState = State::Csi;
                }
                else if (c == ']')
                {
                    mState = State::Osc;
                }
                else
                {
                    // Two-byte escape (e.g. ESC 7, ESC =, ESC ( B): consume
                    // the second byte and return to Normal.
                    mState = State::Normal;
                }
                break;

            case State::Csi:
                // Parameter bytes: 0x30..0x3F. Intermediate: 0x20..0x2F.
                // Final byte: 0x40..0x7E ends the sequence.
                if (c >= 0x40 && c <= 0x7E)
                {
                    // Interpret a few sequences that affect visible layout so
                    // log-line rendering of TUI apps preserves at least the
                    // shape of the output.
                    switch (c)
                    {
                        // ESC [ N C / ESC [ N a -> cursor forward N columns.
                        // TUI libraries emit this instead of literal spaces
                        // between fields; materialise as N spaces.
                        case 'C':
                        case 'a':
                        {
                            int n = ParseFirstParam(mCsiParams, 1);
                            out.append(static_cast<size_t>(n), ' ');
                            break;
                        }

                        // ESC [ ? H  / ESC [ N ; M H  -> cursor position. When
                        // a TUI redraws a frame it usually starts with a
                        // cursor-home (or absolute position to row 1). Treat
                        // any cursor-position as a "new frame" marker by
                        // emitting a newline if the current line has content.
                        // ESC [ N A  (cursor up) and ESC [ 2 J (clear screen)
                        // are also frame-redraw signals — same handling.
                        case 'H':
                        case 'f':
                        case 'A':
                        case 'J':
                        {
                            if (!mSynthesizeFrameNewlines)
                            {
                                // Caller (e.g. ClaudeCodeParser in alt-screen
                                // mode) explicitly disabled this — TUI apps
                                // update individual cells via cursor moves
                                // and we don't want each updated character
                                // to become its own log row.
                                break;
                            }
                            // Suppress the synthetic frame-marker newline if
                            // the previous emitted character (this chunk OR
                            // the previous chunk) was already a newline. This
                            // is critical for TTY backends where TUI apps
                            // redraw frequently — without the cross-call
                            // check, every redraw cycle would inject a blank
                            // line into the log.
                            bool lastWasNl = out.empty() ? mEndsWithNewline
                                                         : (out.back() == '\n');
                            if (!lastWasNl)
                            {
                                out.push_back('\n');
                            }
                            break;
                        }

                        default:
                            break;
                    }
                    mCsiParams.clear();
                    mState = State::Normal;
                }
                else
                {
                    // Accumulate parameter / intermediate bytes so the final
                    // handler above can parse N out of them.
                    if (mCsiParams.size() < 32)
                    {
                        mCsiParams.push_back(static_cast<char>(c));
                    }
                }
                break;

            case State::Osc:
                if (c == 0x07)             // BEL terminates OSC
                {
                    mState = State::Normal;
                }
                else if (c == 0x1B)        // ESC begins ST (ESC \)
                {
                    mState = State::OscEscaped;
                }
                break;

            case State::OscEscaped:
                if (c == '\\')
                {
                    mState = State::Normal;  // ST consumed
                }
                else
                {
                    mState = State::Osc;     // false alarm, back to OSC
                }
                break;
        }
    }

    if (!out.empty())
    {
        mEndsWithNewline = (out.back() == '\n');
    }

    return out;
}

#endif
