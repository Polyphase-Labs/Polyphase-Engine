#pragma once

#if EDITOR

#include <string>

class LuaDebuggerPanel
{
public:
    void Init();
    void Shutdown();
    void DrawContent();

    bool mVisible = true;

private:
    int mSelectedFrame = 0;
};

LuaDebuggerPanel* GetLuaDebuggerPanel();

#endif
