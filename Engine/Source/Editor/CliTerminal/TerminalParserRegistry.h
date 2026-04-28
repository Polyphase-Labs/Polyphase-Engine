#pragma once

#if EDITOR

#include "ITerminalOutputParser.h"
#include "ITerminalProcess.h"  // for TerminalLaunchConfig

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/**
 * @brief Singleton registry of pluggable terminal output parsers.
 *
 * Built-in parsers (the default ANSI stripper and the Claude Code parser)
 * register themselves on first access. Application code or user plugins can
 * call RegisterParser() at any time to add their own.
 *
 * Lookup happens at process launch:
 *   1. CreateParserFor(cfg) walks registered parsers in order, picking the
 *      first one whose matcher returns true for the given launch config.
 *   2. If no matcher fires, the default parser is returned.
 *
 * Matchers are typically a basename check on cfg.mExecutable, but any
 * custom logic is allowed (env vars, args, etc.).
 *
 * Thread-safety: registration and lookup are protected by an internal mutex,
 * but the parsers themselves are single-threaded once handed out.
 */
class TerminalParserRegistry
{
public:
    using Factory = std::function<std::unique_ptr<ITerminalOutputParser>()>;
    using Matcher = std::function<bool(const TerminalLaunchConfig&)>;

    static TerminalParserRegistry& Get();

    /**
     * Register a parser. `name` must be unique; re-registering the same name
     * replaces the previous entry. `matcher` may be empty, in which case the
     * parser will only be selectable by name (not auto-detected).
     *
     * Registration order matters: the first matching entry wins, so more
     * specific parsers should be registered before more general ones.
     */
    void RegisterParser(const std::string& name, Factory factory, Matcher matcher = {});

    /** Remove a previously registered parser. No-op if name is unknown. */
    void UnregisterParser(const std::string& name);

    /**
     * Pick a parser for `cfg`. If `forcedName` is non-empty, that exact
     * parser is created (or null if unknown). Otherwise the first matcher
     * to fire wins; falls back to the default ANSI parser.
     */
    std::unique_ptr<ITerminalOutputParser> CreateParserFor(
        const TerminalLaunchConfig& cfg,
        const std::string& forcedName = {});

    /** Create a parser by exact name. Returns nullptr if name is unknown. */
    std::unique_ptr<ITerminalOutputParser> CreateParserByName(const std::string& name);

    /** List of registered parser names, in registration order. */
    std::vector<std::string> GetRegisteredNames() const;

    /** Name of the parser that would be selected for `cfg`. */
    std::string GetMatchingParserName(const TerminalLaunchConfig& cfg) const;

private:
    TerminalParserRegistry();
    void RegisterBuiltins();

    struct Entry
    {
        std::string name;
        Factory     factory;
        Matcher     matcher;
    };

    mutable std::mutex      mMutex;
    std::vector<Entry>      mEntries;
    std::string             mDefaultName;  // last-resort parser name
};

#endif
