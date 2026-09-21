#pragma once

#if EDITOR

#include <string>
#include <vector>

class ScriptEditorWindow
{
public:
    void Init();
    void Shutdown();
    void DrawContent();

    void OpenFile(const std::string& filePath);

    // The editor font atlas was rebuilt: Zep's cached ImFont pointers are stale.
    // sizeRatio = new text scale / old text scale.
    void OnEditorFontsRebuilt(float sizeRatio);
    bool HasUnsavedChanges() const;

    // Toolbar actions
    void DoNew();
    void DoOpen();
    void DoSave();
    void DoSaveAs();
    void DoCloseCurrentBuffer();

private:
    void* mEditor = nullptr; // Zep::ZepEditor_ImGui* (opaque, created on first OpenFile)
    bool mInitialized = false;

    void EnsureEditor();

    // Recent files
    std::vector<std::string> mRecentFiles;
    static const size_t kMaxRecentFiles = 10;
    void AddRecentFile(const std::string& filePath);
    void LoadRecentFiles();
    void SaveRecentFiles();
    std::string GetRecentFilesPath() const;
};

ScriptEditorWindow* GetScriptEditorWindow();

#endif
