#if EDITOR

#include "EditorHotkeyMap.h"

#include "EditorState.h"
#include "Input/Input.h"
#include "Input/InputTypes.h"
#include "Stream.h"
#include "Utilities.h"
#include "System/System.h"
#include "Log.h"

#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"

#include <cstdlib>
#include <cstring>

EditorHotkeyMap* EditorHotkeyMap::sInstance = nullptr;

void EditorHotkeyMap::Create()
{
    Destroy();
    sInstance = new EditorHotkeyMap();

    // The constructor already called ResetAllToDefaults(), so the in-memory
    // bindings ARE the canonical defaults at this point. PreferencesManager
    // hasn't run yet, so user customizations haven't been loaded over them.
    // Save them out as the "Default" preset if it isn't already on disk.
    sInstance->EnsureDefaultPresetExists();
}

void EditorHotkeyMap::Destroy()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

EditorHotkeyMap* EditorHotkeyMap::Get()
{
    return sInstance;
}

EditorHotkeyMap::EditorHotkeyMap()
{
    ResetAllToDefaults();
}

// ===== PIE hand-off rule =====
//
// Editor hotkeys fire when:
//   - Not in PIE (normal editor)                              -> active
//   - In PIE but ejected (F8)                                 -> active
//   - In PIE but the cursor is not captured by the game window -> active
//   - In PIE with cursor captured (game owns input)           -> SILENT
bool EditorHotkeyMap::AreEditorHotkeysActive()
{
    EditorState* es = GetEditorState();
    if (es == nullptr) return true;
    if (!es->mPlayInEditor) return true;
    if (es->mEjected) return true;
    if (!es->mGamePreviewCaptured) return true;
    return false;
}

// ----- Binding match helper -----
//
// Modifier matching is exact: Ctrl+S only fires when Ctrl is held AND Shift
// is NOT held AND Alt is NOT held. This preserves the existing semantics from
// InputManager.cpp:58-61. Space-required bindings additionally check Space.
bool EditorHotkeyMap::MatchesBinding(const KeyBinding& binding) const
{
    if (!binding.IsValid())
        return false;

    const bool ctrl  = INP_IsKeyDown(POLYPHASE_KEY_CONTROL_L) || INP_IsKeyDown(POLYPHASE_KEY_CONTROL_R);
    const bool shift = INP_IsKeyDown(POLYPHASE_KEY_SHIFT_L)   || INP_IsKeyDown(POLYPHASE_KEY_SHIFT_R);
    const bool alt   = INP_IsKeyDown(POLYPHASE_KEY_ALT_L)     || INP_IsKeyDown(POLYPHASE_KEY_ALT_R);

    if (ctrl  != binding.mCtrl)  return false;
    if (shift != binding.mShift) return false;
    if (alt   != binding.mAlt)   return false;

    if (binding.mRequireSpace && !INP_IsKeyDown(POLYPHASE_KEY_SPACE))
        return false;

    return true;
}

// ----- Query API -----

bool EditorHotkeyMap::IsActionJustTriggered(EditorAction action) const
{
    if (!AreEditorHotkeysActive())
        return false;

    const KeyBinding& b = mBindings[(int32_t)action];
    if (!b.IsValid())
        return false;

    if (!INP_IsKeyJustDown(b.mKeyCode))
        return false;

    return MatchesBinding(b);
}

bool EditorHotkeyMap::IsActionDown(EditorAction action) const
{
    if (!AreEditorHotkeysActive())
        return false;

    const KeyBinding& b = mBindings[(int32_t)action];
    if (!b.IsValid())
        return false;

    if (!INP_IsKeyDown(b.mKeyCode))
        return false;

    return MatchesBinding(b);
}

bool EditorHotkeyMap::IsActionJustReleased(EditorAction action) const
{
    if (!AreEditorHotkeysActive())
        return false;

    const KeyBinding& b = mBindings[(int32_t)action];
    if (!b.IsValid())
        return false;

    return INP_IsKeyJustUp(b.mKeyCode);
}

void EditorHotkeyMap::ConsumeBindingKey(EditorAction action) const
{
    const KeyBinding& b = mBindings[(int32_t)action];
    if (b.IsValid())
    {
        INP_ClearKey(b.mKeyCode);
    }
}

// ----- Binding management -----

KeyBinding EditorHotkeyMap::GetBinding(EditorAction action) const
{
    return mBindings[(int32_t)action];
}

void EditorHotkeyMap::SetBinding(EditorAction action, const KeyBinding& binding)
{
    mBindings[(int32_t)action] = binding;
}

void EditorHotkeyMap::ClearBinding(EditorAction action)
{
    mBindings[(int32_t)action] = KeyBinding();
}

void EditorHotkeyMap::ResetActionToDefault(EditorAction action)
{
    mBindings[(int32_t)action] = GetEditorActionInfo(action).mDefault;
}

void EditorHotkeyMap::ResetAllToDefaults()
{
    for (int32_t i = 0; i < (int32_t)EditorAction::Count; ++i)
    {
        mBindings[i] = GetEditorActionInfo((EditorAction)i).mDefault;
    }
}

std::vector<EditorAction> EditorHotkeyMap::FindConflicts(const KeyBinding& binding,
                                                          EditorAction excluding) const
{
    std::vector<EditorAction> out;
    if (!binding.IsValid())
        return out;

    for (int32_t i = 0; i < (int32_t)EditorAction::Count; ++i)
    {
        if ((EditorAction)i == excluding)
            continue;
        if (mBindings[i] == binding)
            out.push_back((EditorAction)i);
    }
    return out;
}

// ----- Display string -----

std::string EditorHotkeyMap::BindingToDisplayString(const KeyBinding& binding)
{
    if (!binding.IsValid())
        return std::string("\xe2\x80\x94"); // em-dash

    std::string out;
    if (binding.mCtrl)         out += "Ctrl+";
    if (binding.mShift)        out += "Shift+";
    if (binding.mAlt)          out += "Alt+";
    if (binding.mRequireSpace) out += "Space+";

    const char* sym = EditorActionKeyCodeToSymbol(binding.mKeyCode);
    if (sym[0] == '\0')
        out += "?";
    else
        out += sym;
    return out;
}

// ===== JSON serialization =====
//
// Bindings serialize by symbolic key name ("Z", "Numpad 5") so a preset
// authored on Windows still loads on Linux/Android. Modifier flags ride
// alongside as bools. The "version" key is per-document.

void EditorHotkeyMap::WriteBindingToJson(const KeyBinding& binding,
                                          rapidjson::Value& outObj,
                                          rapidjson::Document::AllocatorType& alloc) const
{
    outObj.SetObject();

    if (binding.IsValid())
    {
        const char* sym = EditorActionKeyCodeToSymbol(binding.mKeyCode);
        rapidjson::Value keyVal(sym, alloc);
        outObj.AddMember("key", keyVal, alloc);
    }
    else
    {
        rapidjson::Value keyVal("", alloc);
        outObj.AddMember("key", keyVal, alloc);
    }
    outObj.AddMember("ctrl",  binding.mCtrl,         alloc);
    outObj.AddMember("shift", binding.mShift,        alloc);
    outObj.AddMember("alt",   binding.mAlt,          alloc);
    outObj.AddMember("space", binding.mRequireSpace, alloc);
}

bool EditorHotkeyMap::ReadBindingFromJson(const rapidjson::Value& obj, KeyBinding& outBinding) const
{
    outBinding = KeyBinding();
    if (!obj.IsObject())
        return false;

    if (obj.HasMember("key") && obj["key"].IsString())
    {
        const char* sym = obj["key"].GetString();
        outBinding.mKeyCode = EditorActionSymbolToKeyCode(sym);
    }
    if (obj.HasMember("ctrl")  && obj["ctrl"].IsBool())  outBinding.mCtrl         = obj["ctrl"].GetBool();
    if (obj.HasMember("shift") && obj["shift"].IsBool()) outBinding.mShift        = obj["shift"].GetBool();
    if (obj.HasMember("alt")   && obj["alt"].IsBool())   outBinding.mAlt          = obj["alt"].GetBool();
    if (obj.HasMember("space") && obj["space"].IsBool()) outBinding.mRequireSpace = obj["space"].GetBool();
    return true;
}

void EditorHotkeyMap::SerializeToJson(rapidjson::Document& doc) const
{
    if (!doc.IsObject())
        doc.SetObject();

    auto& alloc = doc.GetAllocator();

    if (doc.HasMember("version"))
        doc["version"].SetInt(1);
    else
        doc.AddMember("version", 1, alloc);

    rapidjson::Value bindings(rapidjson::kObjectType);
    for (int32_t i = 0; i < (int32_t)EditorAction::Count; ++i)
    {
        const EditorActionInfo& info = GetEditorActionInfo((EditorAction)i);
        rapidjson::Value bindingObj(rapidjson::kObjectType);
        WriteBindingToJson(mBindings[i], bindingObj, alloc);

        rapidjson::Value keyVal(info.mSerializeKey, alloc);
        bindings.AddMember(keyVal, bindingObj, alloc);
    }

    if (doc.HasMember("bindings"))
        doc["bindings"] = bindings;
    else
        doc.AddMember("bindings", bindings, alloc);
}

void EditorHotkeyMap::DeserializeFromJson(const rapidjson::Document& doc)
{
    // Start from defaults so that any bindings absent from the file get a
    // sensible value (lets users share partial presets).
    ResetAllToDefaults();

    if (!doc.IsObject() || !doc.HasMember("bindings") || !doc["bindings"].IsObject())
        return;

    const rapidjson::Value& bindings = doc["bindings"];
    for (auto it = bindings.MemberBegin(); it != bindings.MemberEnd(); ++it)
    {
        const char* serializeKey = it->name.GetString();
        EditorAction action = FindEditorActionByKey(serializeKey);
        if (action == EditorAction::Count)
            continue; // unknown action -- forwards-compat with future presets

        KeyBinding b;
        if (ReadBindingFromJson(it->value, b))
        {
            mBindings[(int32_t)action] = b;
        }
    }
}

// ===== Preset file I/O =====

std::string EditorHotkeyMap::GetPresetsDirectory()
{
    std::string dir;
#if PLATFORM_WINDOWS
    const char* appData = getenv("APPDATA");
    if (appData != nullptr)
    {
        dir = std::string(appData) + "/PolyphaseEditor/HotkeyPresets";
    }
#elif PLATFORM_LINUX
    const char* home = getenv("HOME");
    if (home != nullptr)
    {
        dir = std::string(home) + "/.config/PolyphaseEditor/HotkeyPresets";
    }
#endif

    if (dir.empty())
    {
        dir = "Engine/Saves/HotkeyPresets";
    }
    return dir;
}

static void EnsureHotkeyPresetsDirectory()
{
    std::string dir = EditorHotkeyMap::GetPresetsDirectory();

#if PLATFORM_WINDOWS
    const char* appData = getenv("APPDATA");
    if (appData != nullptr)
    {
        std::string parent = std::string(appData) + "/PolyphaseEditor";
        if (!DoesDirExist(parent.c_str()))
            SYS_CreateDirectory(parent.c_str());
    }
#elif PLATFORM_LINUX
    const char* home = getenv("HOME");
    if (home != nullptr)
    {
        std::string configDir = std::string(home) + "/.config";
        if (!DoesDirExist(configDir.c_str()))
            SYS_CreateDirectory(configDir.c_str());
        std::string parent = configDir + "/PolyphaseEditor";
        if (!DoesDirExist(parent.c_str()))
            SYS_CreateDirectory(parent.c_str());
    }
#endif

    if (!DoesDirExist(dir.c_str()))
        SYS_CreateDirectory(dir.c_str());
}

static bool WriteJsonDocToFile(const rapidjson::Document& doc, const std::string& path)
{
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    Stream stream(buffer.GetString(), (uint32_t)buffer.GetSize());
    return stream.WriteFile(path.c_str());
}

static bool ReadJsonDocFromFile(rapidjson::Document& doc, const std::string& path)
{
    if (!SYS_DoesFileExist(path.c_str(), false))
        return false;

    Stream stream;
    if (!stream.ReadFile(path.c_str(), false))
        return false;

    std::string jsonStr(stream.GetData(), stream.GetSize());
    doc.Parse(jsonStr.c_str());
    return !doc.HasParseError();
}

bool EditorHotkeyMap::SavePreset(const std::string& name)
{
    EnsureHotkeyPresetsDirectory();

    rapidjson::Document doc;
    doc.SetObject();
    SerializeToJson(doc);

    std::string filePath = GetPresetsDirectory() + "/" + name + ".json";
    bool success = WriteJsonDocToFile(doc, filePath);

    if (success)
        LogDebug("Editor hotkey preset saved: %s", name.c_str());
    else
        LogError("Failed to save editor hotkey preset: %s", name.c_str());

    return success;
}

bool EditorHotkeyMap::LoadPreset(const std::string& name)
{
    std::string filePath = GetPresetsDirectory() + "/" + name + ".json";

    rapidjson::Document doc;
    if (!ReadJsonDocFromFile(doc, filePath))
    {
        LogWarning("Failed to load editor hotkey preset: %s", name.c_str());
        return false;
    }

    DeserializeFromJson(doc);
    LogDebug("Editor hotkey preset loaded: %s", name.c_str());
    return true;
}

bool EditorHotkeyMap::DeletePreset(const std::string& name)
{
    std::string filePath = GetPresetsDirectory() + "/" + name + ".json";
    return (remove(filePath.c_str()) == 0);
}

std::vector<std::string> EditorHotkeyMap::GetPresetNames() const
{
    std::vector<std::string> names;
    std::string dir = GetPresetsDirectory();

    if (!DoesDirExist(dir.c_str()))
        return names;

    DirEntry entry;
    SYS_OpenDirectory(dir + "/", entry);

    while (entry.mValid)
    {
        if (!entry.mDirectory)
        {
            std::string filename = entry.mFilename;
            size_t extPos = filename.rfind(".json");
            if (extPos != std::string::npos && extPos == filename.size() - 5)
            {
                names.push_back(filename.substr(0, extPos));
            }
        }
        SYS_IterateDirectory(entry);
    }

    SYS_CloseDirectory(entry);
    return names;
}

bool EditorHotkeyMap::ExportToFile(const std::string& absPath)
{
    rapidjson::Document doc;
    doc.SetObject();
    SerializeToJson(doc);
    return WriteJsonDocToFile(doc, absPath);
}

bool EditorHotkeyMap::ImportFromFile(const std::string& absPath)
{
    rapidjson::Document doc;
    if (!ReadJsonDocFromFile(doc, absPath))
    {
        LogWarning("Failed to import hotkey preset from: %s", absPath.c_str());
        return false;
    }
    DeserializeFromJson(doc);
    LogDebug("Imported hotkey preset from: %s", absPath.c_str());
    return true;
}

void EditorHotkeyMap::EnsureDefaultPresetExists()
{
    std::string filePath = GetPresetsDirectory() + "/" + GetDefaultPresetName() + ".json";
    if (SYS_DoesFileExist(filePath.c_str(), false))
        return;

    // Bindings should already be at defaults when this is called from
    // Create(), but reset just in case so we never accidentally write a
    // customized state into Default.json.
    KeyBinding saved[(int32_t)EditorAction::Count];
    for (int32_t i = 0; i < (int32_t)EditorAction::Count; ++i)
        saved[i] = mBindings[i];

    ResetAllToDefaults();
    SavePreset(GetDefaultPresetName());

    // Restore whatever was in memory before, in case the caller wasn't
    // Create() and the user had customizations loaded.
    for (int32_t i = 0; i < (int32_t)EditorAction::Count; ++i)
        mBindings[i] = saved[i];
}

#endif
