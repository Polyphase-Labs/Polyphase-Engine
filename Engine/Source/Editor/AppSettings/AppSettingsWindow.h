#pragma once

#if EDITOR

#include <string>

class AppSettingsWindow
{
public:
    AppSettingsWindow();
    ~AppSettingsWindow();

    void Open();
    void Close();
    void Draw();
    bool IsOpen() const { return mIsOpen; }

private:
    void DrawGeneralSection();
    void DrawWindowSection();
    void DrawGraphicsSection();
    void DrawRuntimeSection();
    void DrawPackagingSection();
    void DrawIconSection();

    bool mIsOpen = false;

    // Text input buffers
    char mProjectNameBuffer[256] = {};
    char mDefaultSceneBuffer[256] = {};
    char mDefaultEditorSceneBuffer[256] = {};
    char mIconPathBuffer[512] = {};
};

AppSettingsWindow* GetAppSettingsWindow();

#endif
