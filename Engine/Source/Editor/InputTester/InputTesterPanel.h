#pragma once

#if EDITOR

class InputTesterPanel
{
public:
    void Draw();
    void DrawContent();

    bool mShowKeyboard = false;
    bool mShowMouse = false;
    bool mShowRawDInput = true;

private:
    void DrawGamepadSlot(int slot);
    void DrawKeyboardSection();
    void DrawMouseSection();
};

InputTesterPanel* GetInputTesterPanel();

#endif
