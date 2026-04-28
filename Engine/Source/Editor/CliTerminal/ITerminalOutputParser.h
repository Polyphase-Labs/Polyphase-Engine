#pragma once

#if EDITOR

#include <string>

/**
 * @brief Pluggable parser for the raw byte stream emitted by a child process.
 *
 * The CLI Terminal panel renders a log view, not a full terminal emulator. A
 * parser's job is to convert the raw stream — which may contain CSI escape
 * sequences, OSC strings, control bytes, frame-redraws, etc. — into the plain
 * text that the panel actually displays.
 *
 * Parsers are pluggable so per-application policies can be slotted in without
 * touching the PTY backends. For example:
 *   - Default parser: strips ANSI escapes, normalises CRLF, keeps colour off.
 *   - ClaudeCodeParser: also dedupes Ink frame redraws and suppresses
 *     bracketed-paste / mouse-tracking DECSET noise.
 *   - User-supplied parsers: anything they want, registered at runtime via
 *     TerminalParserRegistry::RegisterParser.
 *
 * Parsers are stateful (escape sequences can span chunk boundaries), and a
 * single parser instance is used for the lifetime of one process launch. The
 * PTY reader thread is the only caller into Process() so implementations
 * don't need to be thread-safe.
 */
class ITerminalOutputParser
{
public:
    virtual ~ITerminalOutputParser() = default;

    /**
     * Consume a chunk of raw bytes from the child process and return the
     * cleaned text that should be appended to the log buffer. May return an
     * empty string if the chunk was entirely escape-sequence overhead.
     */
    virtual std::string Process(const char* data, size_t len) = 0;

    /** Reset all internal state. Called between launches if reused. */
    virtual void Reset() = 0;

    /** Stable identifier used for logging and the registry lookup table. */
    virtual const char* GetName() const = 0;
};

#endif
