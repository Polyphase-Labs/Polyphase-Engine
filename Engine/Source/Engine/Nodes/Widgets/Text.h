#pragma once

#include "Nodes/Widgets/Widget.h"
#include "Vertex.h"

#include "AssetRef.h"

#include "Graphics/Graphics.h"

class Font;

enum class Justification : uint8_t
{
    Left,
    Center,
    Right,
    Top = 0,
    Bottom = 2,

    Count
};

class POLYPHASE_API Text : public Widget
{
public:

    DECLARE_NODE(Text, Widget);

    friend class Button;

    Text();
    virtual ~Text();

    virtual void Create() override;
    virtual void Destroy() override;

    TextResource* GetResource();

    virtual void GatherProperties(std::vector<Property>& outProps) override;
    void GatherTextProperties(std::vector<Property>& outProps);

    virtual DrawData GetDrawData() override;

    virtual void PreRender() override;

    virtual void Render() override;

    virtual void SetColor(glm::vec4 color) override;

    virtual void MarkDirty();

    void SetFont(Font* font);
    Font* GetFont();

    void SetOutlineColor(glm::vec4 color);
    glm::vec4 GetOutlineColor() const;

    void SetTextSize(float size);
    float GetTextSize() const;
    float GetScaledTextSize() const;

    float GetOutlineSize() const;
    float GetSoftness() const;
    float GetCutoff() const;

    void SetHorizontalJustification(Justification just);
    Justification GetHorizontalJustification() const;
    void SetVerticalJustification(Justification just);
    Justification GetVerticalJustification() const;

    bool IsWordWrapEnabled() const;
    void EnableWordWrap(bool wrap);

    void SetText(const std::string& text);
    void SetText(const char* text);
    const std::string& GetText() const;

    // When non-zero, UpdateVertexData renders this single Unicode codepoint
    // instead of walking mText character-by-character. Bypasses the ASCII
    // filter that would otherwise drop PUA-range glyphs (e.g. Kenney input
    // prompt fonts at U+E000+). Reuses the Font asset's existing atlas
    // binding so no GFX path changes are needed.
    void SetGlyphCodepoint(uint32_t codepoint);
    uint32_t GetGlyphCodepoint() const { return mGlyphCodepoint; }

    VertexUI* GetVertices();
    uint32_t GetNumCharactersAllocated() const;
    uint32_t GetNumVisibleCharacters() const;

    void MarkVerticesDirty();

    float GetTextWidth();
    float GetTextHeight();
    glm::vec2 GetScaledMinExtent();
    glm::vec2 GetScaledMaxExtent();

    virtual bool ContainsPoint(int32_t x, int32_t y) override;

    glm::vec2 GetJustifiedOffset();
    static float GetJustificationRatio(Justification just);

protected:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    void UpdateVertexData();
    void UploadVertexData();
    void JustifyLine(VertexUI* vertices, Justification just, int32_t& lineVertStart, int32_t numVerts);

    FontRef mFont;
    std::string mText;
    float mCutoff;
    float mOutlineSize;
    float mTextSize;
    float mSoftness;
    glm::vec4 mOutlineColor;
    VertexUI* mVertices;
    bool mWordWrap = false;

    glm::vec2 mMinExtent = {};
    glm::vec2 mMaxExtent = {};
    int32_t mVisibleCharacters; // ( \n excluded )
    uint32_t mNumCharactersAllocated;
    Justification mHoriJust = Justification::Left;
    Justification mVertJust = Justification::Top;
    bool mUploadVertices[MAX_FRAMES] = {};
    bool mReconstructVertices = false;

    // Non-zero -> single-codepoint glyph mode (see SetGlyphCodepoint).
    uint32_t mGlyphCodepoint = 0;

    // Graphics Resource
    TextResource mResource;
};