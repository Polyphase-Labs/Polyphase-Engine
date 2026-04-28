#pragma once

#if EDITOR

#include "Input/PlayerInputSystem.h"

#include <stdint.h>
#include <string>
#include <vector>

class PlayerInputEditor
{
public:
    void Open();
    void Draw();
    bool IsOpen() const;

private:

    bool mIsOpen = false;
    int mSelectedActionIndex = -1;
    char mNewCategory[64] = {};
    char mNewActionName[64] = {};

    // Capture state for binding
    bool mCapturing = false;
    int mCaptureBindingSlot = -1;
    float mCaptureTimer = 0.0f;

    // Rename/Move/DuplicateTo popup state
    enum class PopupMode { None, Rename, DuplicateTo, MoveTo, DuplicateCategory };
    PopupMode mPopupMode = PopupMode::None;
    int mPopupActionIndex = -1;
    char mPopupCategory[64] = {};
    char mPopupName[64] = {};
    std::string mPopupSourceCategory;

    // Clipboard for copy/paste
    bool mHasClipboard = false;
    InputAction mClipboardAction;

    void DrawActionList();
    void DrawActionDetails();
    void DrawCaptureOverlay();
    void DrawActionContextMenu(int actionIndex);
    void DrawCategoryContextMenu(const std::string& category);
    void DrawPopupModal();
    void DuplicateAction(int actionIndex, std::string toCategory, std::string newName);
    void PasteAction(const std::string& toCategory, bool overrideExisting);
};

PlayerInputEditor* GetPlayerInputEditor();

#endif
