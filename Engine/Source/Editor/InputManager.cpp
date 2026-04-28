#if EDITOR

#include "InputManager.h"
#include "ActionManager.h"
#include "InputDevices.h"
#include "EditorUtils.h"
#include "EditorState.h"
#include "Renderer.h"
#include "Viewport3d.h"
#include "Viewport2d.h"
#include "Assets/Scene.h"
#include "Assets/Timeline.h"
#include "AssetManager.h"

#include "Input/Input.h"
#include "Hotkeys/EditorHotkeyMap.h"

#include "imgui.h"

InputManager* InputManager::sInstance = nullptr;

void InputManager::Create()
{
    Destroy();
    sInstance = new InputManager();
}

void InputManager::Destroy()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

InputManager* InputManager::Get()
{
    return sInstance;
}

InputManager::InputManager()
{

}

InputManager::~InputManager()
{

}

void InputManager::Update()
{
    UpdateHotkeys();
}

void InputManager::UpdateHotkeys()
{
    // ctrlDown / altDown are still needed by the PIE safety block below
    // (Ctrl+Alt+P releases the cursor, Alt+P pauses). Everything else now
    // routes through EditorHotkeyMap which handles modifier matching itself.
    const bool ctrlDown = IsControlDown();
    const bool altDown = IsAltDown();

    const bool textFieldActive = ImGui::GetIO().WantTextInput;

    bool popupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);

    if (GetEditorState()->mPlayInEditor)
    {
        if (IsKeyJustDown(POLYPHASE_KEY_ESCAPE))
        {
            if (GetEditorState()->mNodePropertySelect)
            {
                GetEditorState()->ClearNodePropertySelect();
            }
            else
            {
                GetEditorState()->EndPlayInEditor();
            }
        }
        else if (IsKeyJustDown(POLYPHASE_KEY_P) && altDown && ctrlDown)
        {
            // Ctrl+Alt+P: release cursor capture without ending PIE
            if (GetEditorState()->mGamePreviewCaptured)
            {
                GetEditorState()->mGamePreviewCaptured = false;
                INP_TrapCursor(false);
            }
        }
        else if (IsKeyJustDown(POLYPHASE_KEY_P) && altDown)
        {
            bool pause = !GetEditorState()->IsPlayInEditorPaused();
            GetEditorState()->SetPlayInEditorPaused(pause);
        }
        else if (IsKeyJustDown(POLYPHASE_KEY_F8))
        {
            if (GetEditorState()->mEjected)
            {
                GetEditorState()->InjectPlayInEditor();
            }
            else
            {
                GetEditorState()->EjectPlayInEditor();
            }
        }
        else if (IsKeyJustDown(POLYPHASE_KEY_F10))
        {
            FrameStep();
        }
    }
    else if (!popupOpen)
    {
        EditorHotkeyMap* hotkeys = EditorHotkeyMap::Get();
        EditorMode editorMode = GetEditorState()->GetEditorMode();
        const bool isScene = (editorMode == EditorMode::Scene) || (editorMode == EditorMode::Scene2D) || (editorMode == EditorMode::Scene3D);

        if (hotkeys->IsActionJustTriggered(EditorAction::File_NewProject))
        {
            ActionManager::Get()->CreateNewProject();
            // Fix issue where Ctrl+Shift+P modifiers feel stuck after the project dialog opens.
            ClearControlDown();
            ClearShiftDown();
            hotkeys->ConsumeBindingKey(EditorAction::File_NewProject);
        }
        else if (hotkeys->IsActionJustTriggered(EditorAction::File_OpenProject))
        {
            ActionManager::Get()->OpenProject();
            ClearControlDown();
            ClearShiftDown();
            hotkeys->ConsumeBindingKey(EditorAction::File_OpenProject);
        }
        else if (hotkeys->IsActionJustTriggered(EditorAction::PIE_Toggle))
        {
            if (GetEditorState()->mPlayTarget == 0) // PlayInEditor (game window)
                GetEditorState()->mPlayInGameWindow = true;
            GetEditorState()->BeginPlayInEditor();
        }
        else if (hotkeys->IsActionJustTriggered(EditorAction::File_SaveAllAssets))
        {
            if (isScene)
            {
                ActionManager::Get()->ResaveAllAssets();
                hotkeys->ConsumeBindingKey(EditorAction::File_SaveAllAssets);
            }
        }
        else if (hotkeys->IsActionJustTriggered(EditorAction::File_SaveScene))
        {
            if (isScene)
            {
                ActionManager::Get()->SaveScene(false);

                // Save the edited timeline if one is open
                Timeline* editedTimeline = GetEditorState()->mEditedTimelineRef.Get<Timeline>();
                if (editedTimeline != nullptr)
                {
                    AssetManager::Get()->SaveAsset(editedTimeline->GetName());
                }

                ClearControlDown();
                ClearShiftDown();
                hotkeys->ConsumeBindingKey(EditorAction::File_SaveScene);
            }
        }
        else if (!textFieldActive && hotkeys->IsActionJustTriggered(EditorAction::File_SaveSelectedAsset))
        {
            ActionManager::Get()->SaveSelectedAsset();
        }
        else if (hotkeys->IsActionJustTriggered(EditorAction::File_OpenScene))
        {
            ActionManager::Get()->OpenScene();
            ClearControlDown();
            hotkeys->ConsumeBindingKey(EditorAction::File_OpenScene);
        }
        else if (hotkeys->IsActionJustTriggered(EditorAction::File_ImportAsset))
        {
            ActionManager::Get()->ImportAsset();
            ClearControlDown();
            hotkeys->ConsumeBindingKey(EditorAction::File_ImportAsset);
        }
        else if (!textFieldActive && hotkeys->IsActionJustTriggered(EditorAction::Edit_Redo))
        {
            ActionManager::Get()->Redo();
        }
        else if (!textFieldActive && hotkeys->IsActionJustTriggered(EditorAction::Edit_Undo))
        {
            ActionManager::Get()->Undo();
        }
        else if (!textFieldActive && hotkeys->IsActionJustTriggered(EditorAction::Edit_ReloadScripts))
        {
            ReloadAllScripts();
        }

        // Mode switches require viewport focus to avoid stomping Ctrl+number in text fields.
        if (GetEditorState()->GetViewport3D()->ShouldHandleInput() ||
            GetEditorState()->GetViewport2D()->ShouldHandleInput())
        {
            if (hotkeys->IsActionJustTriggered(EditorAction::Mode_Scene))
            {
                GetEditorState()->SetEditorMode(EditorMode::Scene);
            }
            else if (hotkeys->IsActionJustTriggered(EditorAction::Mode_Scene2D))
            {
                GetEditorState()->SetEditorMode(EditorMode::Scene2D);
            }
            else if (hotkeys->IsActionJustTriggered(EditorAction::Mode_Scene3D))
            {
                GetEditorState()->SetEditorMode(EditorMode::Scene3D);
                GetEditorState()->SetPaintMode(PaintMode::None);
            }
            else if (hotkeys->IsActionJustTriggered(EditorAction::Mode_PaintColor))
            {
                GetEditorState()->SetEditorMode(EditorMode::Scene3D);
                GetEditorState()->SetPaintMode(PaintMode::Color);
            }
            else if (hotkeys->IsActionJustTriggered(EditorAction::Mode_PaintInstance))
            {
                GetEditorState()->SetEditorMode(EditorMode::Scene3D);
                GetEditorState()->SetPaintMode(PaintMode::Instance);
            }
        }

        // Escape stays hardcoded -- it's a safety key for clearing selection / mode.
        if (IsKeyJustDown(POLYPHASE_KEY_ESCAPE))
        {
            if (GetEditorState()->mNodePropertySelect)
            {
                GetEditorState()->ClearNodePropertySelect();
            }
            else
            {
                GetEditorState()->SetEditorMode(EditorMode::Scene);
            }
        }
    }

    // Clicked but nothing was selected? Just clear node prop select mode.
    if (GetEditorState()->mNodePropertySelect &&
        IsMouseButtonJustDown(MOUSE_LEFT))
    {
        GetEditorState()->ClearNodePropertySelect();
    }

    if (!IsPlaying() || GetEditorState()->mEjected)
    {
        EditorHotkeyMap* hotkeys = EditorHotkeyMap::Get();
        if (!textFieldActive && hotkeys->IsActionJustTriggered(EditorAction::UI_ToggleLeftPane))
        {
            GetEditorState()->mShowLeftPane = !GetEditorState()->mShowLeftPane;
        }
        else if (!textFieldActive && hotkeys->IsActionJustTriggered(EditorAction::UI_ToggleRightPane))
        {
            GetEditorState()->mShowRightPane = !GetEditorState()->mShowRightPane;
        }
        else if (!textFieldActive && hotkeys->IsActionJustTriggered(EditorAction::UI_ToggleAll))
        {
            GetEditorState()->mShowInterface = !GetEditorState()->mShowInterface;
        }
    }
}

#endif
