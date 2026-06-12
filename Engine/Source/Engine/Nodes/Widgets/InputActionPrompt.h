#pragma once

#include "Nodes/Widgets/Widget.h"
#include "AssetRef.h"

class Quad;
class Text;

// Widget that displays the currently bound input for an action (e.g. "Game.Interact")
// rendered as a sprite, glyph, or text fallback per the assigned InputPromptStyle's
// priority chain.
class POLYPHASE_API InputActionPrompt : public Widget
{
public:

    DECLARE_NODE(InputActionPrompt, Widget);

    InputActionPrompt();
    virtual ~InputActionPrompt();

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual void PreRender() override;

    void SetActionCategory(const std::string& cat);
    void SetActionName(const std::string& name);
    const std::string& GetActionCategory() const { return mActionCategory; }
    const std::string& GetActionName()     const { return mActionName; }

    void SetPromptMap(class InputPromptMap* map);
    void SetPromptStyle(class InputPromptStyle* style);
    class InputPromptMap*   GetPromptMap();
    class InputPromptStyle* GetPromptStyle();

    // Last-resolved label — useful for Lua/UI to mirror the displayed prompt
    // (e.g. an accessibility readout or a "rebind" tooltip).
    const std::string& GetResolvedLabel() const { return mLastLabel; }

    Quad* GetQuadChild() { return mQuadChild; }
    Text* GetTextChild() { return mTextChild; }

protected:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    std::string mActionCategory = "Game";
    std::string mActionName     = "Interact";

    AssetRef    mPromptMap;     // InputPromptMap
    AssetRef    mPromptStyle;   // InputPromptStyle

    bool        mAutoSize = true;

    Quad* mQuadChild = nullptr;
    Text* mTextChild = nullptr;

    std::string mLastLabel;
};
