#pragma once

#include "ScriptMacros.h"
#include <functional>
#include <unordered_map>
#include <unordered_set>

class Script;

class ScriptUtils
{
public:

    static bool IsScriptLoaded(const std::string& className);
    static bool ReloadScriptFile(const std::string& fileName);
    static bool CallLuaFunc(int numArgs, int numResults = 0);
    static bool LoadScriptFile(const std::string& fileName, const std::string& className);
    // Optional progress callback. Called once per file with (fileName, done,
    // total). Return false to abort the loop after the current file. Default
    // nullptr keeps non-editor callers unchanged.
    using ReloadProgressFn = std::function<bool(const std::string&, int, int)>;
    static void ReloadAllScriptFiles(const ReloadProgressFn& onProgress = nullptr);
    static void LoadAllScripts();
    static void LoadScriptDirectory(const std::string& dirName, bool recurse = true);
    // Wipe the loaded-script registry. The Lua globals themselves are left in
    // place (re-running a class chunk overwrites its table), but subsequent
    // Script attaches will re-read the .lua from disk instead of seeing the
    // IsScriptLoaded fast-path and reusing the stale prototype. Called on
    // project switch so addon Lua edits made between sessions take effect.
    static void ClearLoadedScripts();

    static std::string GetClassNameFromFileName(const std::string& fileName);
    static void SetEmbeddedScripts(EmbeddedFile* embeddedScripts, uint32_t numEmbeddedScripts);
    static EmbeddedFile* FindEmbeddedScript(const std::string& className);
    static bool RunScript(const char* fileName, Datum* ret = nullptr);

    static uint32_t GetNextScriptInstanceNumber();

    static void CallMethod(Node* node, const char* funcName, uint32_t numParams, const Datum** params, Datum* ret);
    static void SetBreakOnScriptError(bool enableBreak);

    static void GarbageCollect();

    static Datum GetField(Node* node, const char* key);
    static void SetField(Node* node, const char* key, const Datum& value);
    static Datum GetField(Node* node, int32_t key);
    static void SetField(Node* node, int32_t key, const Datum& value);

    static Datum GetField(const char* table, const char* key);
    static void SetField(const char* table, const char* key, const Datum& value);
    static Datum GetField(const char* table, int32_t key);
    static void SetField(const char* table, int32_t key, const Datum& value);

    static void DumpStack();

private:

    // className -> last fileName used to load it. The fileName is preserved so
    // ReloadAllScriptFiles can re-route addon paths (e.g. "Packages/Foo/Bar")
    // through RunScript's Packages/ branch on the next pass; a className-only
    // entry like "Bar" would fall through to the legacy Scripts/ probe and
    // silently fail to reload.
    static std::unordered_map<std::string, std::string> sLoadedLuaFiles;
    static std::unordered_set<std::string> sLoadingLuaFiles;
    static EmbeddedFile* sEmbeddedScripts;
    static uint32_t sNumEmbeddedScripts;
    static uint32_t sNumScriptInstances;

    static bool sBreakOnScriptError;
};
