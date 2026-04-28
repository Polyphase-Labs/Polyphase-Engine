#if EDITOR

#include "TerminalParserRegistry.h"

#include "AnsiStripper.h"
#include "ClaudeCodeParser.h"
#include "Log.h"

#include <algorithm>

namespace
{
    /** Lower-case basename of `path`, with any extension removed. */
    std::string ExecutableBasename(const std::string& path)
    {
        size_t slash = path.find_last_of("/\\");
        std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
        size_t dot = base.find_last_of('.');
        if (dot != std::string::npos)
        {
            base = base.substr(0, dot);
        }
        std::transform(base.begin(), base.end(), base.begin(),
                       [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        return base;
    }
}

TerminalParserRegistry& TerminalParserRegistry::Get()
{
    static TerminalParserRegistry sInstance;
    return sInstance;
}

TerminalParserRegistry::TerminalParserRegistry()
{
    RegisterBuiltins();
}

void TerminalParserRegistry::RegisterBuiltins()
{
    // Default fallback: the bare ANSI stripper. Has no matcher because it is
    // the explicit fallback when nothing else fires.
    RegisterParser(
        "default",
        []() { return std::unique_ptr<ITerminalOutputParser>(new AnsiStripper()); });
    mDefaultName = "default";

    // Claude Code (Anthropic CLI) — built on Ink (React for CLIs). Heavy
    // use of alt-screen, full-frame redraws, SGR colours and bracketed
    // paste. Auto-detected by executable basename.
    RegisterParser(
        "claude-code",
        []() { return std::unique_ptr<ITerminalOutputParser>(new ClaudeCodeParser()); },
        [](const TerminalLaunchConfig& cfg) {
            std::string base = ExecutableBasename(cfg.mExecutable);
            return base == "claude" || base == "claude-code";
        });
}

void TerminalParserRegistry::RegisterParser(const std::string& name, Factory factory, Matcher matcher)
{
    if (name.empty() || !factory)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);

    // Replace if already present (preserves order so users can override
    // built-ins by re-registering with the same name).
    for (Entry& e : mEntries)
    {
        if (e.name == name)
        {
            e.factory = std::move(factory);
            e.matcher = std::move(matcher);
            LogDebug("[CLI] Replaced terminal parser \"%s\"", name.c_str());
            return;
        }
    }

    Entry entry;
    entry.name    = name;
    entry.factory = std::move(factory);
    entry.matcher = std::move(matcher);
    mEntries.push_back(std::move(entry));
    LogDebug("[CLI] Registered terminal parser \"%s\" (%zu total)",
             name.c_str(), mEntries.size());
}

void TerminalParserRegistry::UnregisterParser(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto it = mEntries.begin(); it != mEntries.end(); ++it)
    {
        if (it->name == name)
        {
            mEntries.erase(it);
            return;
        }
    }
}

std::unique_ptr<ITerminalOutputParser> TerminalParserRegistry::CreateParserFor(
    const TerminalLaunchConfig& cfg,
    const std::string& forcedName)
{
    std::lock_guard<std::mutex> lock(mMutex);

    if (!forcedName.empty())
    {
        for (const Entry& e : mEntries)
        {
            if (e.name == forcedName)
            {
                LogDebug("[CLI] Using forced parser \"%s\"", e.name.c_str());
                return e.factory();
            }
        }
        LogDebug("[CLI] Forced parser \"%s\" not found, falling back to default",
                 forcedName.c_str());
    }

    for (const Entry& e : mEntries)
    {
        if (e.matcher && e.matcher(cfg))
        {
            LogDebug("[CLI] Auto-selected parser \"%s\" for executable \"%s\"",
                     e.name.c_str(), cfg.mExecutable.c_str());
            return e.factory();
        }
    }

    // Fallback: explicit default parser.
    for (const Entry& e : mEntries)
    {
        if (e.name == mDefaultName)
        {
            LogDebug("[CLI] Using default parser \"%s\" for executable \"%s\"",
                     e.name.c_str(), cfg.mExecutable.c_str());
            return e.factory();
        }
    }

    // Last-ditch: registry was somehow emptied. Build a fresh AnsiStripper.
    LogDebug("[CLI] No parsers registered; constructing AnsiStripper directly");
    return std::unique_ptr<ITerminalOutputParser>(new AnsiStripper());
}

std::unique_ptr<ITerminalOutputParser> TerminalParserRegistry::CreateParserByName(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (const Entry& e : mEntries)
    {
        if (e.name == name)
        {
            return e.factory();
        }
    }
    return nullptr;
}

std::vector<std::string> TerminalParserRegistry::GetRegisteredNames() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<std::string> names;
    names.reserve(mEntries.size());
    for (const Entry& e : mEntries)
    {
        names.push_back(e.name);
    }
    return names;
}

std::string TerminalParserRegistry::GetMatchingParserName(const TerminalLaunchConfig& cfg) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (const Entry& e : mEntries)
    {
        if (e.matcher && e.matcher(cfg))
        {
            return e.name;
        }
    }
    return mDefaultName;
}

#endif
