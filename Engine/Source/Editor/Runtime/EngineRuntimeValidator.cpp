#if EDITOR

#include "Editor/Runtime/EngineRuntimeValidator.h"

#include "Engine/Constants.h"
#include "Engine/EngineRuntimeManifest.h"
#include "Engine/Utils/Sha256.h"
#include "System/ModuleLoader.h"
#include "System/System.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    bool PathExists(const std::string& path)
    {
        return SYS_DoesFileExist(path.c_str(), false);
    }

    std::string JoinPath(const std::string& a, const std::string& b)
    {
        if (a.empty()) return b;
        if (b.empty()) return a;
        char last = a.back();
        if (last == '/' || last == '\\')
            return a + b;
        return a + "/" + b;
    }

    bool ReadFileToString(const std::string& path, std::string& out)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
            return false;
        std::ostringstream ss;
        ss << in.rdbuf();
        out = ss.str();
        return in.good() || in.eof();
    }

    // Linux: probe DT_RPATH/DT_RUNPATH for $ORIGIN via `readelf -d`. Returns
    // 0 = $ORIGIN seen, 1 = readelf ran but $ORIGIN absent, -1 = readelf
    // unavailable or other failure.
    int ProbeLinuxRpath(const std::string& soPath)
    {
        std::string cmd = "readelf -d ";
        cmd += '"';
        cmd += soPath;
        cmd += '"';
        cmd += " 2>/dev/null";

        std::string output;
        int exitCode = 0;
        if (!SYS_ExecFull(cmd.c_str(), &output, nullptr, &exitCode))
            return -1;
        if (exitCode != 0)
            return -1;
        if (output.find("$ORIGIN") != std::string::npos)
            return 0;
        return 1;
    }
}

std::string GetProjectRuntimeFolder(const std::string& projectDir,
                                    const std::string& platform,
                                    const std::string& arch,
                                    const std::string& config)
{
    if (projectDir.empty() || platform.empty() || arch.empty() || config.empty())
        return std::string();

    std::string folderName = platform + "-" + arch + "-" + config;
    std::string base = JoinPath(projectDir, "Vendor");
    base = JoinPath(base, "PolyphaseRuntime");
    base = JoinPath(base, folderName);
    return base;
}

static void BuildContextHeader(const RuntimeValidationOptions& opts,
                               const std::string& runtimeFolder,
                               Validation::Report& report)
{
    std::ostringstream ss;
    ss << "Project:  " << opts.projectDir << "\n";
    ss << "Target:   " << opts.platform << " " << opts.arch << " " << opts.config << "\n";
    ss << "ABI need: " << opts.requiredAbi << "\n";
    ss << "Folder:   " << runtimeFolder << "\n";
    report.contextHeader = ss.str();
}

RuntimeValidationResult ValidateProjectRuntime(const RuntimeValidationOptions& opts)
{
    using Validation::AddError;
    using Validation::AddWarning;
    using Validation::AddInfo;

    RuntimeValidationResult result;
    result.runtimeFolder = GetProjectRuntimeFolder(
        opts.projectDir, opts.platform, opts.arch, opts.config);

    BuildContextHeader(opts, result.runtimeFolder, result.report);

    if (result.runtimeFolder.empty())
    {
        AddError(result.report, "RT_FOLDER_MISSING",
                 "Required option (projectDir/platform/arch/config) is empty.");
        result.report.summary = "Runtime invalid: configuration incomplete";
        return result;
    }

    if (!PathExists(result.runtimeFolder))
    {
        AddError(result.report, "RT_FOLDER_MISSING",
                 "Vendored runtime folder does not exist.",
                 result.runtimeFolder);
        result.report.summary = "Runtime invalid: folder missing";
        return result;
    }

    std::string manifestPath = JoinPath(result.runtimeFolder, "engine-runtime.json");
    if (!PathExists(manifestPath))
    {
        AddError(result.report, "RT_MANIFEST_MISSING",
                 "engine-runtime.json is missing from the runtime folder.",
                 manifestPath);
        result.report.summary = "Runtime invalid: manifest missing";
        return result;
    }

    std::string manifestJson;
    if (!ReadFileToString(manifestPath, manifestJson))
    {
        AddError(result.report, "RT_MANIFEST_PARSE",
                 "Failed to read engine-runtime.json from disk.",
                 manifestPath);
        result.report.summary = "Runtime invalid: manifest unreadable";
        return result;
    }

    std::string parseError;
    if (!ParseEngineRuntimeManifestJson(manifestJson, result.manifest, parseError))
    {
        AddError(result.report, "RT_MANIFEST_PARSE",
                 "engine-runtime.json failed to parse.",
                 parseError);
        result.report.summary = "Runtime invalid: manifest parse error";
        return result;
    }

    if (result.manifest.schemaVersion > kRuntimeManifestSchemaVersionSupported)
    {
        std::ostringstream d;
        d << "manifest schemaVersion=" << result.manifest.schemaVersion
          << ", validator supports up to "
          << kRuntimeManifestSchemaVersionSupported;
        AddError(result.report, "RT_SCHEMA_VERSION",
                 "Manifest schema is newer than this engine supports.",
                 d.str());
        result.report.summary = "Runtime invalid: unsupported manifest schema";
        return result;
    }

    if (result.manifest.targetPlatform != opts.platform ||
        result.manifest.targetArch     != opts.arch ||
        result.manifest.targetConfig   != opts.config)
    {
        std::ostringstream d;
        d << "requested: " << opts.platform << " " << opts.arch << " " << opts.config << "\n"
          << "manifest:  " << result.manifest.targetPlatform << " "
                          << result.manifest.targetArch << " "
                          << result.manifest.targetConfig;
        AddError(result.report, "RT_TARGET_MISMATCH",
                 "Vendored runtime is for a different platform/arch/config.",
                 d.str());
    }

    if (result.manifest.engineAbi != opts.requiredAbi)
    {
        std::ostringstream d;
        d << "required ABI: " << opts.requiredAbi
          << ", manifest ABI: " << result.manifest.engineAbi;
        AddError(result.report, "RT_ABI_MISMATCH",
                 "Vendored runtime ABI does not match this engine.",
                 d.str());
    }
    else if (result.manifest.engineVersion != POLYPHASE_VERSION_STRING)
    {
        std::ostringstream d;
        d << "engine source: " << POLYPHASE_VERSION_STRING
          << ", manifest:     " << result.manifest.engineVersion;
        AddWarning(result.report, "RT_VERSION_OLDER",
                   "ABI matches but engine version differs.",
                   d.str());
    }

    if (result.manifest.binaryModule.empty())
    {
        AddError(result.report, "RT_BINARY_MISSING",
                 "Manifest does not name a binary module.");
    }
    else
    {
        std::string binaryPath = JoinPath(result.runtimeFolder, result.manifest.binaryModule);
        if (!PathExists(binaryPath))
        {
            AddError(result.report, "RT_BINARY_MISSING",
                     "Manifest names a binary module that is not on disk.",
                     binaryPath);
        }
        else
        {
            std::string hex;
            std::string hashError;
            if (!Sha256::HashFileHex(binaryPath, hex, hashError))
            {
                AddError(result.report, "RT_BINARY_HASH",
                         "Failed to hash the runtime binary.",
                         hashError);
            }
            else if (hex != result.manifest.binaryModuleSha256)
            {
                std::ostringstream d;
                d << "expected: " << result.manifest.binaryModuleSha256 << "\n"
                  << "actual:   " << hex;
                AddError(result.report, "RT_BINARY_HASH",
                         "Runtime binary SHA256 does not match manifest.",
                         d.str());
            }

            if (opts.platform == "Windows" && !result.manifest.binaryImportLib.empty())
            {
                std::string libPath = JoinPath(result.runtimeFolder,
                                               result.manifest.binaryImportLib);
                if (!PathExists(libPath))
                {
                    Validation::Severity sev = opts.attemptSymbolProbe
                        ? Validation::Severity::Error
                        : Validation::Severity::Warning;
                    if (sev == Validation::Severity::Error)
                        AddError(result.report, "RT_IMPORTLIB_MISSING",
                                 "Manifest names an import lib that is not on disk.",
                                 libPath);
                    else
                        AddWarning(result.report, "RT_IMPORTLIB_MISSING",
                                   "Manifest names an import lib that is not on disk.",
                                   libPath);
                }
            }

            if (opts.platform == "Linux")
            {
                int rpath = ProbeLinuxRpath(binaryPath);
                if (rpath == 1)
                {
                    AddWarning(result.report, "RT_RPATH_NONLOCAL",
                               "Linux runtime .so has no $ORIGIN rpath; loading may fail at runtime.",
                               binaryPath);
                }
                // rpath == -1: readelf unavailable — silent skip
            }

            if (opts.attemptSymbolProbe &&
                !result.manifest.requiredExports.empty() &&
                !Validation::HasError(result.report))
            {
                void* handle = MOD_Load(binaryPath.c_str());
                if (handle == nullptr)
                {
                    AddError(result.report, "RT_SYMBOL_PROBE_FAILED",
                             "Failed to load the runtime module for symbol probing.",
                             MOD_GetError());
                }
                else
                {
                    std::ostringstream missing;
                    bool any = false;
                    for (const std::string& sym : result.manifest.requiredExports)
                    {
                        if (MOD_Symbol(handle, sym.c_str()) == nullptr)
                        {
                            if (any) missing << ", ";
                            missing << sym;
                            any = true;
                        }
                    }
                    MOD_Unload(handle);
                    if (any)
                    {
                        AddError(result.report, "RT_SYMBOL_PROBE_FAILED",
                                 "Runtime module is missing required exports.",
                                 missing.str());
                    }
                }
            }
        }
    }

    result.report.valid = !Validation::HasError(result.report);
    if (result.report.valid)
    {
        if (Validation::HasWarning(result.report))
            result.report.summary = "Runtime valid (with warnings)";
        else
            result.report.summary = "Runtime valid";

        std::ostringstream info;
        info << "ABI " << result.manifest.engineAbi
             << ", build " << result.manifest.buildHash
             << ", " << result.manifest.binaryModule;
        AddInfo(result.report, "RT_OK", "Runtime accepted.", info.str());
    }
    else
    {
        if (result.report.summary.empty())
            result.report.summary = "Runtime invalid";
    }
    return result;
}

RuntimeValidationResult UpdateProjectRuntime(const RuntimeUpdateOptions& updateOpts)
{
    using Validation::AddError;

    RuntimeValidationResult result;
    result.runtimeFolder = GetProjectRuntimeFolder(
        updateOpts.projectDir, updateOpts.platform, updateOpts.arch, updateOpts.config);

    RuntimeValidationOptions revalidateOpts;
    revalidateOpts.projectDir = updateOpts.projectDir;
    revalidateOpts.platform = updateOpts.platform;
    revalidateOpts.arch = updateOpts.arch;
    revalidateOpts.config = updateOpts.config;

    if (result.runtimeFolder.empty())
    {
        BuildContextHeader(revalidateOpts, result.runtimeFolder, result.report);
        AddError(result.report, "RT_FOLDER_MISSING",
                 "Required option is empty (projectDir/platform/arch/config).");
        result.report.summary = "Runtime update failed: configuration incomplete";
        return result;
    }

    if (updateOpts.sourceRuntimePath.empty() ||
        !PathExists(updateOpts.sourceRuntimePath))
    {
        BuildContextHeader(revalidateOpts, result.runtimeFolder, result.report);
        AddError(result.report, "RT_FOLDER_MISSING",
                 "Source runtime path does not exist.",
                 updateOpts.sourceRuntimePath);
        result.report.summary = "Runtime update failed: source missing";
        return result;
    }

    // SYS_CopyDirectoryRecursive overwrites by default.
    if (!SYS_CopyDirectoryRecursive(updateOpts.sourceRuntimePath, result.runtimeFolder))
    {
        BuildContextHeader(revalidateOpts, result.runtimeFolder, result.report);
        AddError(result.report, "RT_FOLDER_MISSING",
                 "Failed to copy source runtime into project Vendor folder.",
                 updateOpts.sourceRuntimePath + " -> " + result.runtimeFolder);
        result.report.summary = "Runtime update failed: copy error";
        return result;
    }

    return ValidateProjectRuntime(revalidateOpts);
}

#endif // EDITOR
