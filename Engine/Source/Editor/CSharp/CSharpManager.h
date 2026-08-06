#pragma once

#if EDITOR

#include <string>

// Drives the PolyphaseSharp C# -> Lua transpiler (Tools/PolyphaseSharp).
// The .NET SDK is an authoring-time dependency only: pure-Lua projects never
// touch any of this, and shipped games run the generated Lua with no .NET.
class CSharpManager
{
public:
    static CSharpManager* Get();

    // True when the loaded project has CSharpScripting=1 in Config.ini.
    bool IsEnabledForProject() const;

    // <Project>/Scripts/CSharp (empty string when no project is loaded).
    std::string GetProjectCSharpDir() const;

    // Any .cs source under Scripts/CSharp (cheap disk probe).
    bool ProjectHasCSharpSources() const;

    // Probe `dotnet --version`. Result is cached; pass forceReprobe after an
    // install. Requires major version >= 8.
    bool CheckDotnet(std::string* outVersion = nullptr, bool forceReprobe = false);

    // Build the transpiler tool if its sources are newer than the built dll
    // (or it was never built). Blocking; returns the dll path or "" on failure.
    std::string EnsureToolBuilt(std::string* outLog = nullptr);

    // Transpile <Project>/Scripts/CSharp. Blocking. Diagnostics are pushed to
    // the log (file(line,col): error CS/PS... format) and appended to outLog.
    // Returns false on compile/validation errors or missing prerequisites.
    bool Transpile(bool checkOnly = false, std::string* outLog = nullptr);

    // Open the project's C# workspace in the IDE chosen in
    // Preferences > External > Editors (default Visual Studio). Creates any
    // missing workspace files first, so projects enabled by older builds
    // pick up Game.sln retroactively.
    void OpenIde();

    // Create Game.csproj / Game.sln under Scripts/CSharp if absent.
    // Idempotent; called from EnableForProject and OpenIde.
    void EnsureWorkspaceFiles();

    // Turn C# scripting on for the loaded project: sets CSharpScripting=1 in
    // Config.ini, scaffolds Scripts/CSharp/ (Game.csproj + sample Rotator.cs),
    // appends Intermediate/CSharp/ to the project .gitignore, and probes dotnet
    // (logging install guidance when missing). Safe to call repeatedly.
    bool EnableForProject();

    // The winget/apt command (or download URL) offered when dotnet is missing.
    static const char* GetDotnetInstallCommand();
    static const char* GetDotnetInstallUrl();

private:
    bool mDotnetProbed = false;
    bool mDotnetOk = false;
    std::string mDotnetVersion;
    std::string mToolDllPath;
};

#endif
