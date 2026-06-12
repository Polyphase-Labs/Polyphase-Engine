#include "Nodes/Widgets/InputActionPrompt.h"
#include "Nodes/Widgets/Quad.h"
#include "Nodes/Widgets/Text.h"

#include "Assets/InputPromptMap.h"
#include "Assets/InputPromptStyle.h"
#include "Assets/Texture.h"
#include "Assets/Font.h"

#include "Input/InputPromptResolver.h"
#include "Property.h"

FORCE_LINK_DEF(InputActionPrompt);
DEFINE_NODE(InputActionPrompt, Widget);

InputActionPrompt::InputActionPrompt()
{
    SetName("InputActionPrompt");
}

InputActionPrompt::~InputActionPrompt()
{
}

void InputActionPrompt::Create()
{
    Super::Create();

    mQuadChild = CreateChild<Quad>("Quad");
    mTextChild = CreateChild<Text>("Text");

    mQuadChild->SetAnchorMode(AnchorMode::FullStretch);
    mTextChild->SetAnchorMode(AnchorMode::FullStretch);
    mQuadChild->SetRatios(0.0f, 0.0f, 1.0f, 1.0f);
    mTextChild->SetRatios(0.0f, 0.0f, 1.0f, 1.0f);
    mQuadChild->SetTransient(true);
    mTextChild->SetTransient(true);
    mQuadChild->SetVisible(false);
    mTextChild->SetVisible(false);

#if EDITOR
    mQuadChild->mHiddenInTree = true;
    mTextChild->mHiddenInTree = true;
#endif

    SetDimensions(24.0f, 24.0f);
}

void InputActionPrompt::Destroy()
{
    Super::Destroy();
}

void InputActionPrompt::GatherProperties(std::vector<Property>& outProps)
{
    Super::GatherProperties(outProps);

    SCOPED_CATEGORY("Input Action Prompt");
    outProps.push_back(Property(DatumType::String, "Action Category", this, &mActionCategory, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::String, "Action Name",     this, &mActionName,     1, HandlePropChange));
    outProps.push_back(Property(DatumType::Asset,  "Prompt Map",      this, &mPromptMap,      1, HandlePropChange,
                                int32_t(InputPromptMap::GetStaticType())));
    outProps.push_back(Property(DatumType::Asset,  "Prompt Style",    this, &mPromptStyle,    1, HandlePropChange,
                                int32_t(InputPromptStyle::GetStaticType())));
    outProps.push_back(Property(DatumType::Bool,   "Auto Size",       this, &mAutoSize,       1, HandlePropChange));
}

void InputActionPrompt::PreRender()
{
    Super::PreRender();

    InputPromptResolver* resolver = InputPromptResolver::Get();
    InputPromptMap* map = mPromptMap.Get<InputPromptMap>();
    InputPromptStyle* style = mPromptStyle.Get<InputPromptStyle>();

    if (!resolver)
    {
        mQuadChild->SetVisible(false);
        mTextChild->SetVisible(false);
        return;
    }

    const ResolvedPrompt* resolved = resolver->Resolve(map, style, mActionCategory, mActionName);
    if (!resolved)
    {
        mQuadChild->SetVisible(false);
        mTextChild->SetVisible(false);
        return;
    }

    mLastLabel = resolved->label;

    const float iconSize = style ? style->GetIconSize() : 24.0f;
    const glm::vec4 tint = style ? style->GetTint() : glm::vec4(1.0f);

    switch (resolved->kind)
    {
    case InputPromptKind::Sprite:
        mQuadChild->SetTexture(resolved->sprite);
        mQuadChild->SetColor(tint);
        mQuadChild->SetVisible(true);
        mTextChild->SetVisible(false);
        if (mAutoSize)
            SetDimensions(iconSize, iconSize);
        break;

    case InputPromptKind::Glyph:
    {
        // Render glyphs by routing the codepoint through Text's new
        // single-codepoint path (Text::SetGlyphCodepoint), bypassing its
        // ASCII-only filter. Reusing Text means we inherit its proven font-
        // atlas binding instead of routing through Quad with a transient
        // Texture from Font::GetTexture().
        Font* font = resolved->font;
        bool glyphFound = false;
        if (font)
        {
            for (const Character& c : font->GetCharacters())
            {
                if ((uint32_t)c.mCodePoint == resolved->codepoint)
                {
                    glyphFound = true;
                    break;
                }
            }
        }

        if (font && glyphFound)
        {
            mTextChild->SetFont(font);
            mTextChild->SetText("");                       // clear string mode
            mTextChild->SetGlyphCodepoint(resolved->codepoint);
            mTextChild->SetTextSize(iconSize);
            mTextChild->SetColor(tint);
            mTextChild->SetHorizontalJustification(Justification::Center);
            mTextChild->SetVerticalJustification(Justification::Center);
            mTextChild->SetVisible(true);
            mQuadChild->SetVisible(false);
            if (mAutoSize)
                SetDimensions(iconSize, iconSize);
        }
        else
        {
            // No font, or codepoint absent from the font's character table —
            // show the text label so artists see a useful fallback instead
            // of a blank slot.
            mTextChild->SetGlyphCodepoint(0);              // back to string mode
            mTextChild->SetText(resolved->label);
            mTextChild->SetTextSize(iconSize);
            mTextChild->SetColor(tint);
            mTextChild->SetHorizontalJustification(Justification::Center);
            mTextChild->SetVerticalJustification(Justification::Center);
            mTextChild->SetVisible(true);
            mQuadChild->SetVisible(false);
            if (mAutoSize)
            {
                float w = iconSize * 0.6f * float(resolved->label.size() + 1);
                SetDimensions(w, iconSize);
            }
        }
        break;
    }

    case InputPromptKind::Text:
    default:
        mTextChild->SetText(resolved->label);
        mTextChild->SetTextSize(iconSize);
        mTextChild->SetColor(tint);
        mTextChild->SetHorizontalJustification(Justification::Center);
        mTextChild->SetVerticalJustification(Justification::Center);
        mTextChild->SetVisible(true);
        mQuadChild->SetVisible(false);
        if (mAutoSize)
        {
            // Text fallback size = icon height, width scales with label length —
            // a rough heuristic; the parent layout can override via mAutoSize=false.
            float w = iconSize * 0.6f * float(resolved->label.size() + 1);
            SetDimensions(w, iconSize);
        }
        break;
    }
}

void InputActionPrompt::SetActionCategory(const std::string& cat)
{
    mActionCategory = cat;
    MarkDirty();
}

void InputActionPrompt::SetActionName(const std::string& name)
{
    mActionName = name;
    MarkDirty();
}

void InputActionPrompt::SetPromptMap(InputPromptMap* map)
{
    mPromptMap = map;
    MarkDirty();
}

void InputActionPrompt::SetPromptStyle(InputPromptStyle* style)
{
    mPromptStyle = style;
    MarkDirty();
}

InputPromptMap* InputActionPrompt::GetPromptMap()
{
    return mPromptMap.Get<InputPromptMap>();
}

InputPromptStyle* InputActionPrompt::GetPromptStyle()
{
    return mPromptStyle.Get<InputPromptStyle>();
}

bool InputActionPrompt::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    InputActionPrompt* w = static_cast<InputActionPrompt*>(prop->mOwner);
    w->MarkDirty();
    return false;
}
