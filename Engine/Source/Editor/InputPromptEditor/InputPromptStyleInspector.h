#pragma once

#if EDITOR

class InputPromptStyle;

// Per-asset Inspector extension for InputPromptStyle. Draws the priority chips
// (Sprite / Glyph / Text reorder controls) and the prewarm action list with
// autocomplete from PlayerInputSystem::GetActions().
class InputPromptStyleInspector
{
public:
    static InputPromptStyleInspector* Get();
    void Draw(InputPromptStyle* style);

private:
    InputPromptStyleInspector() = default;
    static InputPromptStyleInspector* sInstance;

    char mNewPrewarmBuf[128] = "";
};

#endif // EDITOR
