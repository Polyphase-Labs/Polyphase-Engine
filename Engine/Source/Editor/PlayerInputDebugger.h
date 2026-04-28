#pragma once

#if EDITOR

class PlayerInputDebugger
{
public:
    void Open();
    void Draw();
    bool IsOpen() const;

private:

    bool mIsOpen = false;
    bool mFilterActiveOnly = false;
    char mFilterText[64] = {};
};

PlayerInputDebugger* GetPlayerInputDebugger();

#endif
