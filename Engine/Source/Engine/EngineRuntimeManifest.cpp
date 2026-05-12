#include "EngineRuntimeManifest.h"

#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace
{
    // Small helper that records the first error encountered and short-circuits
    // subsequent checks via the bool return value.
    bool SetError(std::string& outError, std::string msg)
    {
        outError = std::move(msg);
        return false;
    }

    bool RequireObject(const rapidjson::Value& parent,
                       const char* key,
                       const rapidjson::Value*& outChild,
                       std::string& outError)
    {
        if (!parent.HasMember(key) || !parent[key].IsObject())
        {
            return SetError(outError, std::string("missing required object '") + key + "'");
        }
        outChild = &parent[key];
        return true;
    }

    bool RequireUint(const rapidjson::Value& parent,
                     const char* key,
                     uint32_t& outValue,
                     std::string& outError)
    {
        if (!parent.HasMember(key))
        {
            return SetError(outError, std::string("missing required uint '") + key + "'");
        }
        const rapidjson::Value& v = parent[key];
        if (!v.IsUint() && !v.IsInt())
        {
            return SetError(outError, std::string("field '") + key + "' is not an integer");
        }
        outValue = v.IsUint() ? v.GetUint() : static_cast<uint32_t>(v.GetInt());
        return true;
    }

    bool RequireString(const rapidjson::Value& parent,
                       const char* key,
                       std::string& outValue,
                       std::string& outError)
    {
        if (!parent.HasMember(key) || !parent[key].IsString())
        {
            return SetError(outError, std::string("missing required string '") + key + "'");
        }
        outValue = parent[key].GetString();
        return true;
    }

    void OptionalString(const rapidjson::Value& parent,
                        const char* key,
                        std::string& outValue)
    {
        if (parent.HasMember(key) && parent[key].IsString())
        {
            outValue = parent[key].GetString();
        }
    }

    void OptionalUint(const rapidjson::Value& parent,
                      const char* key,
                      uint32_t& outValue)
    {
        if (parent.HasMember(key))
        {
            const rapidjson::Value& v = parent[key];
            if (v.IsUint())      outValue = v.GetUint();
            else if (v.IsInt())  outValue = static_cast<uint32_t>(v.GetInt());
        }
    }

    void OptionalUint64(const rapidjson::Value& parent,
                        const char* key,
                        uint64_t& outValue)
    {
        if (parent.HasMember(key))
        {
            const rapidjson::Value& v = parent[key];
            if (v.IsUint64())      outValue = v.GetUint64();
            else if (v.IsInt64())  outValue = static_cast<uint64_t>(v.GetInt64());
            else if (v.IsUint())   outValue = v.GetUint();
            else if (v.IsInt())    outValue = static_cast<uint64_t>(v.GetInt());
        }
    }
}

bool ParseEngineRuntimeManifestJson(const std::string& json,
                                    EngineRuntimeManifest& out,
                                    std::string& outError)
{
    outError.clear();

    rapidjson::Document doc;
    doc.Parse(json.c_str(), json.size());
    if (doc.HasParseError())
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "rapidjson parse error %d at offset %zu",
                      static_cast<int>(doc.GetParseError()),
                      static_cast<size_t>(doc.GetErrorOffset()));
        return SetError(outError, buf);
    }
    if (!doc.IsObject())
    {
        return SetError(outError, "root is not a JSON object");
    }

    // schemaVersion (required)
    if (!RequireUint(doc, "schemaVersion", out.schemaVersion, outError))
        return false;

    // engine block
    const rapidjson::Value* engine = nullptr;
    if (!RequireObject(doc, "engine", engine, outError))
        return false;
    OptionalString(*engine, "name",              out.engineName);
    OptionalString(*engine, "version",           out.engineVersion);
    OptionalUint  (*engine, "versionMajor",      out.engineVersionMajor);
    if (!RequireUint(*engine, "abi", out.engineAbi, outError))
        return false;
    OptionalString(*engine, "buildHash",         out.buildHash);
    OptionalString(*engine, "buildTimestampUtc", out.buildTimestampUtc);

    // target block (all three required)
    const rapidjson::Value* target = nullptr;
    if (!RequireObject(doc, "target", target, outError))
        return false;
    if (!RequireString(*target, "platform", out.targetPlatform, outError)) return false;
    if (!RequireString(*target, "arch",     out.targetArch,     outError)) return false;
    if (!RequireString(*target, "config",   out.targetConfig,   outError)) return false;

    // top-level ints (optional with defaults)
    OptionalUint(doc, "addonApiVersion", out.addonApiVersion);
    OptionalUint(doc, "assetVersion",    out.assetVersion);

    // binary block (module + moduleSha256 required)
    const rapidjson::Value* binary = nullptr;
    if (!RequireObject(doc, "binary", binary, outError))
        return false;
    if (!RequireString(*binary, "module",       out.binaryModule,       outError)) return false;
    if (!RequireString(*binary, "moduleSha256", out.binaryModuleSha256, outError)) return false;
    OptionalString(*binary, "importLib",     out.binaryImportLib);
    OptionalString(*binary, "debugSymbols",  out.binaryDebugSymbols);
    OptionalUint64(*binary, "moduleSize",    out.binaryModuleSize);

    // requiredExports (optional array of strings)
    if (doc.HasMember("requiredExports") && doc["requiredExports"].IsArray())
    {
        const rapidjson::Value& arr = doc["requiredExports"];
        out.requiredExports.clear();
        out.requiredExports.reserve(arr.Size());
        for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
        {
            if (arr[i].IsString())
            {
                out.requiredExports.emplace_back(arr[i].GetString());
            }
        }
    }

    // compiler (optional object, optional fields within)
    if (doc.HasMember("compiler") && doc["compiler"].IsObject())
    {
        const rapidjson::Value& comp = doc["compiler"];
        OptionalString(comp, "id",      out.compilerId);
        OptionalString(comp, "version", out.compilerVersion);
        OptionalString(comp, "crt",     out.compilerCrt);
    }

    return true;
}

bool LoadEngineRuntimeManifest(const std::string& jsonPath,
                               EngineRuntimeManifest& out,
                               std::string& outError)
{
    outError.clear();

    std::ifstream in(jsonPath, std::ios::binary);
    if (!in)
    {
        return SetError(outError, "failed to open '" + jsonPath + "'");
    }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string json = ss.str();
    if (json.empty())
    {
        return SetError(outError, "manifest file is empty: '" + jsonPath + "'");
    }
    return ParseEngineRuntimeManifestJson(json, out, outError);
}

std::string SerializeEngineRuntimeManifest(const EngineRuntimeManifest& m)
{
    using namespace rapidjson;
    Document doc;
    doc.SetObject();
    Document::AllocatorType& a = doc.GetAllocator();

    auto addStr = [&](Value& obj, const char* key, const std::string& v) {
        obj.AddMember(StringRef(key), Value(v.c_str(), a), a);
    };

    // Emit keys in the schema's documented order to keep the on-disk file
    // tidy and diff-friendly. rapidjson preserves AddMember insertion order.
    //
    // W1: rapidjson's GenericValue ctor set is {int, unsigned, int64_t,
    // uint64_t, float, double}. On devkitARM (3DS/Wii/GameCube/Switch), the
    // <stdint.h> shipped by newlib types uint32_t as `unsigned long` instead
    // of `unsigned int` — and `unsigned long` matches both `unsigned` (via
    // narrowing) and `uint64_t` (via widening), so the overload resolution
    // is ambiguous and the 3DS build fails with
    // "call of overloaded 'GenericValue(long unsigned int&)' is ambiguous".
    // Cast each integer field to a candidate that's identical-width on every
    // platform we ship: `unsigned` for uint32_t-ish, `uint64_t` for moduleSize.
    doc.AddMember("schemaVersion", static_cast<unsigned>(m.schemaVersion), a);

    Value engine(kObjectType);
    addStr(engine, "name", m.engineName);
    addStr(engine, "version", m.engineVersion);
    engine.AddMember("versionMajor", static_cast<unsigned>(m.engineVersionMajor), a);
    engine.AddMember("abi", static_cast<unsigned>(m.engineAbi), a);
    addStr(engine, "buildHash", m.buildHash);
    addStr(engine, "buildTimestampUtc", m.buildTimestampUtc);
    doc.AddMember("engine", engine, a);

    Value target(kObjectType);
    addStr(target, "platform", m.targetPlatform);
    addStr(target, "arch",     m.targetArch);
    addStr(target, "config",   m.targetConfig);
    doc.AddMember("target", target, a);

    doc.AddMember("addonApiVersion", static_cast<unsigned>(m.addonApiVersion), a);
    doc.AddMember("assetVersion",    static_cast<unsigned>(m.assetVersion),    a);

    Value binary(kObjectType);
    addStr(binary, "module",       m.binaryModule);
    // Empty strings are emitted as "" — see header comment. Don't omit.
    addStr(binary, "importLib",    m.binaryImportLib);
    addStr(binary, "debugSymbols", m.binaryDebugSymbols);
    addStr(binary, "moduleSha256", m.binaryModuleSha256);
    binary.AddMember("moduleSize", static_cast<uint64_t>(m.binaryModuleSize), a);
    doc.AddMember("binary", binary, a);

    Value exports(kArrayType);
    for (const std::string& s : m.requiredExports)
    {
        exports.PushBack(Value(s.c_str(), a), a);
    }
    doc.AddMember("requiredExports", exports, a);

    Value compiler(kObjectType);
    addStr(compiler, "id",      m.compilerId);
    addStr(compiler, "version", m.compilerVersion);
    addStr(compiler, "crt",     m.compilerCrt);
    doc.AddMember("compiler", compiler, a);

    StringBuffer buf;
    PrettyWriter<StringBuffer> writer(buf);
    doc.Accept(writer);
    return std::string(buf.GetString(), buf.GetSize());
}
