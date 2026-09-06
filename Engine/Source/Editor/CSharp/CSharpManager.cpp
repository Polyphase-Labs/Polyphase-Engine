#if EDITOR

#include "CSharp/CSharpManager.h"

#include "Engine.h"
#include "EngineTypes.h"
#include "Log.h"
#include "System/System.h"
#include "Preferences/PreferencesManager.h"
#include "Preferences/External/EditorsModule.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

CSharpManager* CSharpManager::Get()
{
    static CSharpManager sInstance;
    return &sInstance;
}

bool CSharpManager::IsEnabledForProject() const
{
    return GetEngineConfig()->mCSharpScripting &&
           GetEngineState()->mProjectDirectory != "";
}

std::string CSharpManager::GetProjectCSharpDir() const
{
    const std::string& projDir = GetEngineState()->mProjectDirectory;
    if (projDir == "")
        return "";
    return projDir + "Scripts/CSharp";
}

bool CSharpManager::ProjectHasCSharpSources() const
{
    std::string dir = GetProjectCSharpDir();
    if (dir == "")
        return false;

    std::error_code ec;
    if (!fs::exists(dir, ec))
        return false;

    for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec)
            break;
        if (!it->is_regular_file(ec))
            continue;
        std::string name = it->path().filename().string();
        // obj/bin (dotnet build junk) shouldn't count as sources.
        std::string pathStr = it->path().generic_string();
        if (pathStr.find("/obj/") != std::string::npos || pathStr.find("/bin/") != std::string::npos)
            continue;
        if (name.size() > 3 && name.compare(name.size() - 3, 3, ".cs") == 0)
            return true;
    }
    return false;
}

bool CSharpManager::CheckDotnet(std::string* outVersion, bool forceReprobe)
{
    if (!mDotnetProbed || forceReprobe)
    {
        mDotnetProbed = true;
        mDotnetOk = false;
        mDotnetVersion = "";

        std::string output;
        SYS_Exec("dotnet --version", &output);

        // Expect something like "9.0.316". Trim whitespace/newline.
        while (!output.empty() && (output.back() == '\n' || output.back() == '\r' || output.back() == ' '))
            output.pop_back();

        if (!output.empty() && isdigit((unsigned char)output[0]))
        {
            mDotnetVersion = output;
            int major = atoi(output.c_str());
            mDotnetOk = (major >= 8);
            if (!mDotnetOk)
            {
                LogWarning("[CSharp] dotnet %s found, but C# scripting needs .NET SDK 8 or newer.",
                    output.c_str());
            }
        }
    }

    if (outVersion != nullptr)
        *outVersion = mDotnetVersion;
    return mDotnetOk;
}

void CSharpManager::OpenIde()
{
    std::string csharpDir = GetProjectCSharpDir();
    if (csharpDir == "" || !fs::exists(csharpDir))
    {
        LogWarning("[CSharp] No Scripts/CSharp folder. Use Tools > CSharp > Enable C# for This Project first.");
        return;
    }

    // Projects enabled by older engine builds may predate Game.sln (or even
    // Game.csproj) — create whatever is missing before launching anything.
    EnsureWorkspaceFiles();

    // The IDE choice lives in Preferences > External > Editors (defaults to
    // Visual Studio). The module owns the per-IDE launch quirks.
    EditorsModule* editors = static_cast<EditorsModule*>(
        PreferencesManager::Get()->FindModule("External/Editors"));
    if (editors != nullptr)
    {
        editors->OpenCSharpWorkspace(csharpDir);
    }
}

void CSharpManager::EnsureWorkspaceFiles()
{
    const std::string& projDir = GetEngineState()->mProjectDirectory;
    if (projDir == "")
        return;

    std::string csharpDir = projDir + "Scripts/CSharp";
    std::error_code ec;
    std::filesystem::create_directories(csharpDir, ec);

    std::string engineDir = SYS_GetPolyphasePath();
    for (char& c : engineDir) { if (c == '\\') c = '/'; }
    if (!engineDir.empty() && engineDir.back() != '/')
        engineDir += "/";

    // Game.csproj: IntelliSense project. The engine API is source-included so no
    // prebuilt DLL is required; obj/bin land in Intermediate/CSharp (gitignored).
    // The EngineApi path is machine-local — the editor refreshes it on project
    // open, so the file stays correct when the project moves between machines.
    std::string csprojPath = csharpDir + "/Game.csproj";
    if (!SYS_DoesFileExist(csprojPath.c_str(), false))
    {
        FILE* csproj = fopen(csprojPath.c_str(), "w");
        if (csproj != nullptr)
        {
            fprintf(csproj,
                "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
                "\n"
                "  <PropertyGroup>\n"
                "    <TargetFramework>netstandard2.1</TargetFramework>\n"
                "    <LangVersion>latest</LangVersion>\n"
                "    <Nullable>disable</Nullable>\n"
                "    <BaseIntermediateOutputPath>../../Intermediate/CSharp/obj/</BaseIntermediateOutputPath>\n"
                "    <BaseOutputPath>../../Intermediate/CSharp/bin/</BaseOutputPath>\n"
                "    <!-- extern engine-API members carry no bodies; scripts never run on the .NET runtime -->\n"
                "    <NoWarn>CS0626;CS0824;CS0169;CS0649</NoWarn>\n"
                "  </PropertyGroup>\n"
                "\n"
                "  <ItemGroup>\n"
                "    <!-- POLYPHASE_ENGINE_API (auto-updated by the editor on project open) -->\n"
                "    <Compile Include=\"%sTools/PolyphaseSharp/Polyphase.Engine/Polyphase/**/*.cs\" Link=\"EngineApi/%%(Filename)%%(Extension)\" />\n"
                "  </ItemGroup>\n"
                "\n"
                "</Project>\n",
                engineDir.c_str());
            fclose(csproj);
        }
    }

    // Game.sln: what Visual Studio double-clicks and what VS Code's C# Dev Kit
    // uses for its solution view. Plain fixed-GUID single-project solution.
    std::string slnPath = csharpDir + "/Game.sln";
    if (!SYS_DoesFileExist(slnPath.c_str(), false))
    {
        FILE* sln = fopen(slnPath.c_str(), "w");
        if (sln != nullptr)
        {
            fprintf(sln,
                "Microsoft Visual Studio Solution File, Format Version 12.00\n"
                "# Visual Studio Version 17\n"
                "Project(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\") = \"Game\", \"Game.csproj\", \"{9A1B2C3D-4E5F-4A6B-8C7D-0E1F2A3B4C5D}\"\n"
                "EndProject\n"
                "Global\n"
                "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n"
                "\t\tDebug|Any CPU = Debug|Any CPU\n"
                "\t\tRelease|Any CPU = Release|Any CPU\n"
                "\tEndGlobalSection\n"
                "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n"
                "\t\t{9A1B2C3D-4E5F-4A6B-8C7D-0E1F2A3B4C5D}.Debug|Any CPU.ActiveCfg = Debug|Any CPU\n"
                "\t\t{9A1B2C3D-4E5F-4A6B-8C7D-0E1F2A3B4C5D}.Debug|Any CPU.Build.0 = Debug|Any CPU\n"
                "\t\t{9A1B2C3D-4E5F-4A6B-8C7D-0E1F2A3B4C5D}.Release|Any CPU.ActiveCfg = Release|Any CPU\n"
                "\t\t{9A1B2C3D-4E5F-4A6B-8C7D-0E1F2A3B4C5D}.Release|Any CPU.Build.0 = Release|Any CPU\n"
                "\tEndGlobalSection\n"
                "EndGlobal\n");
            fclose(sln);
        }
    }
}

bool CSharpManager::EnableForProject()
{
    const std::string& projDir = GetEngineState()->mProjectDirectory;
    if (projDir == "")
    {
        LogWarning("[CSharp] no project loaded");
        return false;
    }

    GetMutableEngineConfig()->mCSharpScripting = true;
    WriteEngineConfig();

    EnsureWorkspaceFiles();

    std::string csharpDir = projDir + "Scripts/CSharp";

    // Drop a sample script only into an empty C# folder. NOT named "Rotator" —
    // script class names are global and Engine/Scripts/Rotator.lua already owns
    // that one.
    if (!ProjectHasCSharpSources())
    {
        std::string samplePath = csharpDir + "/SpinnerCS.cs";
        FILE* sample = fopen(samplePath.c_str(), "w");
        if (sample != nullptr)
        {
            fprintf(sample,
                "using Polyphase;\n"
                "\n"
                "public class SpinnerCS : Script3D\n"
                "{\n"
                "    [Property] public Vector3 AngularVelocity = new Vector3(0, 90, 0);\n"
                "\n"
                "    public override void Tick(float deltaTime)\n"
                "    {\n"
                "        AddRotation(AngularVelocity * deltaTime);\n"
                "    }\n"
                "}\n");
            fclose(sample);
        }
    }

    std::string version;
    if (CheckDotnet(&version, true))
    {
        LogDebug("[CSharp] enabled for project (dotnet %s). Build with Ctrl+R or Tools > CSharp.",
            version.c_str());
    }
    else
    {
        LogWarning("[CSharp] enabled, but the .NET SDK (8+) was not found. Install it with:  %s"
                   "  or download from %s — then use Tools > CSharp > Check Dependencies.",
            GetDotnetInstallCommand(), GetDotnetInstallUrl());
    }
    return true;
}

const char* CSharpManager::GetDotnetInstallCommand()
{
#if PLATFORM_WINDOWS
    return "winget install Microsoft.DotNet.SDK.8";
#elif PLATFORM_MAC
    return "brew install --cask dotnet-sdk";
#else
    return "sudo apt install dotnet-sdk-8.0";
#endif
}

const char* CSharpManager::GetDotnetInstallUrl()
{
    return "https://dotnet.microsoft.com/download/dotnet/8.0";
}

// Newest write time among the tool's source files (csproj + .cs + vendored
// compiler + API + CoreSystem lua), used as the rebuild fingerprint.
static fs::file_time_type GetNewestToolSourceTime(const std::string& toolRoot)
{
    fs::file_time_type newest = fs::file_time_type::min();
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(toolRoot, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec)
            break;
        if (!it->is_regular_file(ec))
            continue;
        std::string pathStr = it->path().generic_string();
        if (pathStr.find("/bin/") != std::string::npos ||
            pathStr.find("/obj/") != std::string::npos ||
            pathStr.find("/Tests/") != std::string::npos)
            continue;
        std::string ext = it->path().extension().string();
        if (ext != ".cs" && ext != ".csproj" && ext != ".lua" && ext != ".xml")
            continue;
        fs::file_time_type t = it->last_write_time(ec);
        if (!ec && t > newest)
            newest = t;
    }
    return newest;
}

std::string CSharpManager::EnsureToolBuilt(std::string* outLog)
{
    if (!CheckDotnet())
    {
        if (outLog != nullptr)
            *outLog += "dotnet SDK 8+ not found. Install it with: " +
                       std::string(GetDotnetInstallCommand()) + "\n";
        LogError("[CSharp] dotnet SDK 8+ not found. Install: %s (or %s)",
            GetDotnetInstallCommand(), GetDotnetInstallUrl());
        return "";
    }

    std::string engineDir = SYS_GetPolyphasePath();
    for (char& c : engineDir) { if (c == '\\') c = '/'; }
    if (!engineDir.empty() && engineDir.back() != '/')
        engineDir += "/";

    std::string toolRoot = engineDir + "Tools/PolyphaseSharp";
    std::string csproj = toolRoot + "/PolyphaseSharp/PolyphaseSharp.csproj";
    std::string dll = toolRoot + "/PolyphaseSharp/bin/Release/net8.0/polyphasesharp.dll";

    if (!SYS_DoesFileExist(csproj.c_str(), false))
    {
        LogError("[CSharp] transpiler tool sources missing at %s", toolRoot.c_str());
        return "";
    }

    // Skip the (multi-second) dotnet build when the built dll is fresh.
    std::error_code ec;
    if (fs::exists(dll, ec))
    {
        fs::file_time_type dllTime = fs::last_write_time(dll, ec);
        if (!ec && GetNewestToolSourceTime(toolRoot) <= dllTime)
        {
            mToolDllPath = dll;
            return mToolDllPath;
        }
    }

    LogDebug("[CSharp] building PolyphaseSharp tool (first run or sources changed)...");
    std::string cmd = "dotnet build \"" + csproj + "\" -c Release --nologo -v q";
    std::string execOut, execErr;
    int exitCode = -1;
    SYS_ExecFull(cmd.c_str(), &execOut, &execErr, &exitCode);

    if (outLog != nullptr)
        *outLog += execOut;

    if (exitCode != 0 || !fs::exists(dll, ec))
    {
        LogError("[CSharp] PolyphaseSharp tool build failed (exit %d):\n%s", exitCode, execOut.c_str());
        return "";
    }

    mToolDllPath = dll;
    return mToolDllPath;
}

bool CSharpManager::Transpile(bool checkOnly, std::string* outLog)
{
    std::string csharpDir = GetProjectCSharpDir();
    if (csharpDir == "")
    {
        LogWarning("[CSharp] no project loaded");
        return false;
    }

    if (!ProjectHasCSharpSources())
    {
        // Nothing to do — not an error (the tool would only clean orphans).
        return true;
    }

    std::string toolDll = EnsureToolBuilt(outLog);
    if (toolDll == "")
        return false;

    // Engine/Scripts (and addon script roots) share the flat script-class
    // namespace — hand them to the validator so a C# class shadowing e.g. the
    // engine's Rotator.lua fails the transpile instead of silently losing the
    // duplicate at packaging time.
    std::string engineDir = SYS_GetPolyphasePath();
    for (char& c : engineDir) { if (c == '\\') c = '/'; }
    if (!engineDir.empty() && engineDir.back() != '/')
        engineDir += "/";
    std::string luaRoots = engineDir + "Engine/Scripts";
    std::string packagesDir = GetEngineState()->mProjectDirectory + "Packages";
    std::error_code rootsEc;
    if (fs::exists(packagesDir, rootsEc))
        luaRoots += ";" + packagesDir;

    std::string cmd = "dotnet \"" + toolDll + "\" --scripts \"" + csharpDir + "\"" +
                      " --lua-roots \"" + luaRoots + "\"";
    if (checkOnly)
        cmd += " --check";

    std::string execOut, execErr;
    int exitCode = -1;
    SYS_ExecFull(cmd.c_str(), &execOut, &execErr, &exitCode);

    if (outLog != nullptr)
        *outLog += execOut;

    // Surface diagnostics line by line so file(line,col) references land in the
    // Log window with the right severity.
    size_t start = 0;
    while (start < execOut.size())
    {
        size_t end = execOut.find('\n', start);
        if (end == std::string::npos)
            end = execOut.size();
        std::string line = execOut.substr(start, end - start);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (!line.empty())
        {
            if (line.find("): error ") != std::string::npos || line.find("error PS") != std::string::npos)
                LogError("[CSharp] %s", line.c_str());
            else if (line.find("): warning ") != std::string::npos)
                LogWarning("[CSharp] %s", line.c_str());
            else
                LogDebug("[CSharp] %s", line.c_str());
        }
        start = end + 1;
    }

    if (exitCode != 0)
    {
        LogError("[CSharp] transpile failed (exit %d)", exitCode);
        return false;
    }
    return true;
}

#endif // EDITOR
