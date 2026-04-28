#pragma once

#if EDITOR

#include "EditorAction.h"

#include <string>
#include <vector>

class EditorHotkeysWindow
{
public:

    void Open();
    bool IsOpen() const;
    bool IsCapturing() const;

    void Draw();

private:

    void DrawPresetBar();
    void DrawSearchBar();
    void DrawCategorySection(const char* category);
    void DrawActionRow(EditorAction action);
    void DrawCaptureOverlay();

    void RefreshPresetList();
    void StartCapture(EditorAction action);
    bool PollKeyPress(int32_t& outKeyCode) const;

    bool                       mIsOpen        = false;
    std::vector<std::string>   mPresetNames;
    int                        mSelectedPreset = -1;
    char                       mNewPresetName[64] = { 0 };
    char                       mImportExportPath[260] = { 0 };
    char                       mSearchBuffer[64] = { 0 };

    bool                       mCapturing     = false;
    EditorAction               mCaptureAction = EditorAction::Count;
    float                      mCaptureTimer  = 0.0f;
};

EditorHotkeysWindow* GetEditorHotkeysWindow();

#endif
