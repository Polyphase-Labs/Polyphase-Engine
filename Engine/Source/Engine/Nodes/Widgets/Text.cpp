#include "Nodes/Widgets/Text.h"
#include "Renderer.h"
#include "Assets/Font.h"
#include "AssetManager.h"
#include "Vertex.h"
#include "Maths.h"
#include "Utilities.h"

#include "Graphics/Graphics.h"

#include "Input/InputPromptResolver.h"

#include <cstdlib>   // strtoul (for [u:HEX] tag parsing)

FORCE_LINK_DEF(Text);
DEFINE_NODE(Text, Widget);

// Decode one UTF-8 codepoint starting at `s[i]`. Returns the codepoint and
// advances `i` past the consumed bytes. Falls back to single-byte read for
// malformed input so the loop always makes progress.
static uint32_t DecodeUtf8(const char* s, size_t len, size_t& i)
{
    if (i >= len) return 0;
    uint8_t c = (uint8_t)s[i];

    // 0xxxxxxx — ASCII
    if (c < 0x80) { i++; return c; }

    // 110xxxxx 10xxxxxx — 2 bytes
    if ((c & 0xE0) == 0xC0 && i + 1 < len)
    {
        uint32_t cp = ((c & 0x1Fu) << 6) | ((uint8_t)s[i + 1] & 0x3Fu);
        i += 2;
        return cp;
    }

    // 1110xxxx 10xxxxxx 10xxxxxx — 3 bytes (covers PUA: U+E000..U+F8FF)
    if ((c & 0xF0) == 0xE0 && i + 2 < len)
    {
        uint32_t cp = ((c & 0x0Fu) << 12) |
                      (((uint8_t)s[i + 1] & 0x3Fu) << 6) |
                       ((uint8_t)s[i + 2] & 0x3Fu);
        i += 3;
        return cp;
    }

    // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx — 4 bytes
    if ((c & 0xF8) == 0xF0 && i + 3 < len)
    {
        uint32_t cp = ((c & 0x07u) << 18) |
                      (((uint8_t)s[i + 1] & 0x3Fu) << 12) |
                      (((uint8_t)s[i + 2] & 0x3Fu) << 6) |
                       ((uint8_t)s[i + 3] & 0x3Fu);
        i += 4;
        return cp;
    }

    // Invalid — consume one byte and return the raw value so we make forward
    // progress; the codepoint lookup will simply miss and the loop will skip.
    i++;
    return c;
}

// Linear search for a Character matching `codepoint`. Font tables are small
// (~95 ASCII + a sparse PUA range), so this is cheap; no need for a hash map.
static const Character* FindCharacter(const std::vector<Character>& chars, uint32_t codepoint)
{
    for (const Character& c : chars)
    {
        if ((uint32_t)c.mCodePoint == codepoint) return &c;
    }
    return nullptr;
}

// Encode a Unicode codepoint as a UTF-8 byte sequence and append it to `out`.
// Used by [u:HEX] tag substitution; the new UTF-8 decoder in UpdateVertexData
// then walks the bytes and looks the codepoint up in the font.
static void AppendUtf8(uint32_t cp, std::string& out)
{
    if (cp < 0x80)
    {
        out.push_back((char)cp);
    }
    else if (cp < 0x800)
    {
        out.push_back((char)(0xC0u | (cp >> 6)));
        out.push_back((char)(0x80u | (cp & 0x3Fu)));
    }
    else if (cp < 0x10000)
    {
        out.push_back((char)(0xE0u |  (cp >> 12)));
        out.push_back((char)(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back((char)(0x80u |  (cp        & 0x3Fu)));
    }
    else
    {
        out.push_back((char)(0xF0u |  (cp >> 18)));
        out.push_back((char)(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back((char)(0x80u | ((cp >> 6)  & 0x3Fu)));
        out.push_back((char)(0x80u |  (cp         & 0x3Fu)));
    }
}

// Walk `in` looking for inline tags, substituting them in `out`. Returns true
// if anything was substituted. The walker is a single linear pass — it locates
// the next '[' once per tag, parses the tag prefix, and skips on to the
// matching ']'. Two tag kinds are supported today:
//
//   [action:Category.Name] -> resolves the current binding label
//                              (e.g. "F", "Gamepad.A") via InputPromptResolver
//   [u:HEX]                -> raw Unicode codepoint as UTF-8
//                              (accepts "E012", "U+E012", "0xE012")
//
// Anything else is left verbatim, including unclosed tags.
static bool SubstituteTextTags(const std::string& in, std::string& out)
{
    out.clear();
    out.reserve(in.size());
    size_t i = 0;
    bool changed = false;

    while (i < in.size())
    {
        size_t open = in.find('[', i);
        if (open == std::string::npos)
        {
            out.append(in, i, in.size() - i);
            break;
        }
        out.append(in, i, open - i);

        size_t close = in.find(']', open + 1);
        if (close == std::string::npos)
        {
            // Malformed — leave the rest of the string verbatim.
            out.append(in, open, in.size() - open);
            break;
        }

        // Tag content lives between '[' and ']'. The colon separates the tag
        // kind from the payload; missing colon => not a tag, render literal.
        std::string body = in.substr(open + 1, close - open - 1);
        size_t colon = body.find(':');
        if (colon == std::string::npos)
        {
            out.append(in, open, close - open + 1);
            i = close + 1;
            continue;
        }
        std::string kind = body.substr(0, colon);
        std::string spec = body.substr(colon + 1);

        if (kind == "action")
        {
            size_t dot = spec.find('.');
            std::string category = (dot == std::string::npos) ? std::string("Game") : spec.substr(0, dot);
            std::string name     = (dot == std::string::npos) ? spec : spec.substr(dot + 1);

            std::string label = name;
            if (InputPromptResolver* r = InputPromptResolver::Get())
            {
                if (const ResolvedPrompt* res = r->Resolve(nullptr, nullptr, category, name))
                {
                    if (!res->label.empty()) label = res->label;
                }
            }
            out.append(label);
            changed = true;
        }
        else if (kind == "u")
        {
            // Strip optional "U+" or "0x" prefix; rest is hex codepoint.
            const char* digits = spec.c_str();
            if (spec.size() >= 2 && (spec[0] == 'U' || spec[0] == 'u') && spec[1] == '+') digits += 2;
            else if (spec.size() >= 2 && spec[0] == '0' && (spec[1] == 'x' || spec[1] == 'X')) digits += 2;

            char* endp = nullptr;
            unsigned long cp = strtoul(digits, &endp, 16);
            if (endp != digits && cp > 0 && cp <= 0x10FFFFu)
            {
                AppendUtf8((uint32_t)cp, out);
                changed = true;
            }
            else
            {
                // Couldn't parse — leave the original tag visible so the
                // typo is obvious instead of silently swallowed.
                out.append(in, open, close - open + 1);
            }
        }
        else
        {
            // Unknown tag — pass through.
            out.append(in, open, close - open + 1);
        }

        i = close + 1;
    }
    return changed;
}

static const char* sHoriJustStrings[] =
{
    "Left",
    "Center",
    "Right"
};
static_assert(int32_t(Justification::Count) == 3, "Need to update string conversion table");

static const char* sVertJustStrings[] =
{
    "Top",
    "Center",
    "Bottom"
};
static_assert(int32_t(Justification::Count) == 3, "Need to update string conversion table");

static uint32_t HexCharToInt(char c)
{
    uint32_t ret = 255;

    if (c >= '0' && c <= '9')
        ret = c - '0';
    else if (c >= 'A' && c <= 'F')
        ret = (c - 'A') + 10;
    else if (c >= 'a' && c <= 'f')
        ret = (c - 'a') + 10;

    return ret;
}

static uint32_t HexCharToColorComp(char c)
{
    uint32_t value = HexCharToInt(c);
    float alpha = value / 15.0f;
    float fValue = glm::mix(0.0f, 255.0f, alpha);
    uint32_t ret = glm::clamp<uint32_t>(uint32_t(fValue + 0.5f), 0, 255);
    return ret;
}

bool Text::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);

    OCT_ASSERT(prop != nullptr);
    Text* text = static_cast<Text*>(prop->mOwner);
    bool success = false;

    text->MarkDirty();
    text->MarkVerticesDirty();

    return success;
}

float Text::GetJustificationRatio(Justification just)
{
    float ret = 0.0f;

    switch (just)
    {
    case Justification::Left:
        ret = 0.0f;
        break;
    case Justification::Right:
        ret = 1.0f;
        break;
    case Justification::Center:
        ret = 0.5f;
        break;

    default:
        break;
    }

    return ret;
}

Text::Text() :
    mFont(nullptr),
    mText("Text"),
    mCutoff(0.55f),
    mOutlineSize(0.0f),
    mTextSize(16.0f),
    mSoftness(0.125f),
    mOutlineColor(0.0f, 0.0f, 0.0, 1.0f),
    mVertices(nullptr),
    mVisibleCharacters(0),
    mNumCharactersAllocated(0)
{
    SetName("Text");
}

Text::~Text()
{

}

void Text::Create()
{
    Widget::Create();

    mFont = LoadAsset<Font>("F_Roboto32");
    MarkVerticesDirty();
    GFX_CreateTextResource(this);
}

void Text::Destroy()
{
    if (mVertices != nullptr)
    {
        delete[] mVertices;
        mVertices = nullptr;
    }

    GFX_DestroyTextResource(this);

    Widget::Destroy();
}

TextResource* Text::GetResource()
{
    return &mResource;
}

void Text::GatherProperties(std::vector<Property>& outProps)
{
    Widget::GatherProperties(outProps);

    SCOPED_CATEGORY("Text");
    GatherTextProperties(outProps);
}

void Text::GatherTextProperties(std::vector<Property>& outProps)
{
    outProps.push_back(Property(DatumType::Asset, "Font", this, &mFont, 1, Text::HandlePropChange, int32_t(Font::GetStaticType())));
    outProps.push_back(Property(DatumType::String, "Text", this, &mText, 1, Text::HandlePropChange));
    outProps.push_back(Property(DatumType::Float, "Text Size", this, &mTextSize, 1, Text::HandlePropChange));
    outProps.push_back(Property(DatumType::Bool, "Word Wrap", this, &mWordWrap, 1, Text::HandlePropChange));
    outProps.push_back(Property(DatumType::Byte, "Hori Justification", this, &mHoriJust, 1, Text::HandlePropChange, NULL_DATUM, int32_t(Justification::Count), sHoriJustStrings));
    outProps.push_back(Property(DatumType::Byte, "Vert Justification", this, &mVertJust, 1, Text::HandlePropChange, NULL_DATUM, int32_t(Justification::Count), sVertJustStrings));
}

DrawData Text::GetDrawData()
{
    DrawData data = {};

    data.mNode = this;

    return data;
}

void Text::PreRender()
{
    Super::PreRender();

    UpdateVertexData();
    UploadVertexData();
}

void Text::SetFont(class Font* font)
{
    if (mFont != font)
    {
        mFont = font;
        MarkVerticesDirty();
        MarkDirty();
    }
}

Font* Text::GetFont()
{
    return mFont.Get<Font>();
}

void Text::SetColor(glm::vec4 color)
{
    Widget::SetColor(color);

    // No longer need to mark vertices dirty, as text color is applied as a uniform.
    // The text vertex data still includes vertex colors, but that is so inline coloring
    // can be used like `FC8`.
}

void Text::MarkDirty()
{
    Widget::MarkDirty();

    if (mWordWrap)
    {
        MarkVerticesDirty();
    }
}

void Text::SetOutlineColor(glm::vec4 color)
{
    if (mColor != color)
    {
        mOutlineColor = color;
        MarkDirty();
    }
}

glm::vec4 Text::GetOutlineColor() const
{
    return mOutlineColor;
}

void Text::SetTextSize(float size)
{
    if (mTextSize != size)
    {
        mTextSize = size;
        MarkDirty();
    }
}

float Text::GetTextSize() const
{
    return mTextSize;
}

float Text::GetScaledTextSize() const
{
    return mTextSize * glm::min(mAbsoluteScale.x, mAbsoluteScale.y);
}

float Text::GetOutlineSize() const
{
    return mOutlineSize;
}

float Text::GetSoftness() const
{
    return mSoftness;
}

float Text::GetCutoff() const
{
    return mCutoff;
}

void Text::SetHorizontalJustification(Justification just)
{
    if (mHoriJust != just)
    {
        mHoriJust = just;
        MarkVerticesDirty();
    }
}

Justification Text::GetHorizontalJustification() const
{
    return mHoriJust;
}

void Text::SetVerticalJustification(Justification just)
{
    if (mVertJust != just)
    {
        mVertJust = just;
        MarkVerticesDirty();
    }
}

Justification Text::GetVerticalJustification() const
{
    return mVertJust;
}

bool Text::IsWordWrapEnabled() const
{
    return mWordWrap;
}

void Text::EnableWordWrap(bool wrap)
{
    if (mWordWrap != wrap)
    {
        mWordWrap = wrap;
        MarkVerticesDirty();
    }
}

void Text::SetText(const std::string& text)
{
    SetText(text.c_str());
}

void Text::SetText(const char* text)
{
    if (mText != text)
    {
        mText = text;
        MarkVerticesDirty();
        MarkDirty();
    }
}

void Text::SetGlyphCodepoint(uint32_t codepoint)
{
    if (mGlyphCodepoint != codepoint)
    {
        mGlyphCodepoint = codepoint;
        MarkVerticesDirty();
        MarkDirty();
    }
}

const std::string& Text::GetText() const
{
    return mText;
}

VertexUI* Text::GetVertices()
{
    return mVertices;
}

uint32_t Text::GetNumCharactersAllocated() const
{
    return mNumCharactersAllocated;
}

uint32_t Text::GetNumVisibleCharacters() const
{
    return mVisibleCharacters;
}

void Text::MarkVerticesDirty()
{
    mReconstructVertices = true;
    for (uint32_t i = 0; i < MAX_FRAMES; ++i)
    {
        mUploadVertices[i] = true;
    }
}

float Text::GetTextWidth()
{
    UpdateVertexData();
    glm::vec2 scaledMax = GetScaledMaxExtent();
    glm::vec2 scaledMin = GetScaledMinExtent();
    return (scaledMax.x - scaledMin.x);
}

float Text::GetTextHeight()
{
    UpdateVertexData();
    glm::vec2 scaledMax = GetScaledMaxExtent();
    glm::vec2 scaledMin = GetScaledMinExtent();
    return (scaledMax.y - scaledMin.y);
}

glm::vec2 Text::GetScaledMinExtent()
{
    UpdateVertexData();

    // TODO: Need to account for GetJustifiedOffset().

    Font* font = mFont.Get<Font>();
    float scale = font ? (mTextSize / font->GetSize()) : 1.0f;
    return mMinExtent * scale;
}

glm::vec2 Text::GetScaledMaxExtent()
{
    UpdateVertexData();

    // TODO: Need to account for GetJustifiedOffset().

    Font* font = mFont.Get<Font>();
    float scale = font ? (mTextSize / font->GetSize()) : 1.0f;
    return mMaxExtent * scale;
}

bool Text::ContainsPoint(int32_t x, int32_t y)
{
    // This code isn't tested? 
    // But I think it's right.
    glm::vec2 minExt = GetScaledMinExtent();
    glm::vec2 maxExt = GetScaledMaxExtent();

    Rect textRect;
    textRect.mX = minExt.x;
    textRect.mY = minExt.y;
    textRect.mWidth = (maxExt.x - minExt.x);
    textRect.mHeight = (maxExt.y - minExt.y);

    textRect.Clamp(mScissorRect);
    return textRect.ContainsPoint((float)x, (float)y);
}

void Text::UpdateVertexData()
{
    if (!mReconstructVertices ||
        mFont == nullptr)
        return;

    // Single-codepoint glyph mode (see SetGlyphCodepoint). Used by
    // InputActionPrompt to render PUA-range glyphs from gamepad-prompt fonts
    // that the normal ASCII-only string path filters out.
    if (mGlyphCodepoint != 0)
    {
        Font* font = mFont.Get<Font>();
        OCT_ASSERT(font != nullptr);
        const std::vector<Character>& fontChars = font->GetCharacters();
        const Character* glyph = nullptr;
        for (const Character& c : fontChars)
        {
            if ((uint32_t)c.mCodePoint == mGlyphCodepoint) { glyph = &c; break; }
        }

        if (glyph == nullptr)
        {
            // Codepoint missing from font — render nothing rather than a
            // missing-glyph block. Inspector validation surfaces this case
            // before runtime so artists notice early.
            mVisibleCharacters = 0;
            mMinExtent = mMaxExtent = glm::vec2(0.0f);
            mReconstructVertices = false;
            return;
        }

        // Make sure the vertex buffer has at least one char of capacity.
        if (mNumCharactersAllocated < 1)
        {
            if (mVertices != nullptr) { delete[] mVertices; mVertices = nullptr; }
            const uint32_t allocGranularity = 32;
            mVertices = new VertexUI[allocGranularity * 6];
            mNumCharactersAllocated = allocGranularity;
        }

        const int32_t fontWidth = font->GetWidth();
        const int32_t fontHeight = font->GetHeight();
        const int32_t fontSize = font->GetSize();
        const uint32_t color32 = 0xffffffff;

        const float cursorX = 0.0f;
        const float cursorY = (float)fontSize;

        VertexUI* v = mVertices;
        v[0].mPosition = glm::vec2(cursorX - glyph->mOriginX,
                                   cursorY - glyph->mOriginY);
        v[0].mTexcoord = glm::vec2(glyph->mX, glyph->mY);

        v[1].mPosition = glm::vec2(cursorX - glyph->mOriginX,
                                   cursorY - glyph->mOriginY + glyph->mHeight);
        v[1].mTexcoord = glm::vec2(glyph->mX, glyph->mY + glyph->mHeight);

        v[2].mPosition = glm::vec2(cursorX - glyph->mOriginX + glyph->mWidth,
                                   cursorY - glyph->mOriginY);
        v[2].mTexcoord = glm::vec2(glyph->mX + glyph->mWidth, glyph->mY);

        v[3] = v[2];
        v[4] = v[1];

        v[5].mPosition = glm::vec2(cursorX - glyph->mOriginX + glyph->mWidth,
                                   cursorY - glyph->mOriginY + glyph->mHeight);
        v[5].mTexcoord = glm::vec2(glyph->mX + glyph->mWidth,
                                   glyph->mY + glyph->mHeight);

        for (int32_t i = 0; i < 6; ++i)
        {
            v[i].mColor = color32;
            v[i].mTexcoord /= glm::vec2(fontWidth, fontHeight);
        }

        mVisibleCharacters = 1;
        mMinExtent = v[0].mPosition;
        mMaxExtent = v[5].mPosition;

        // Justification offset (matches the string-mode tail block at the end
        // of UpdateVertexData).
        if (mVertJust != Justification::Top)
        {
            float vertJust = GetJustificationRatio(mVertJust);
            float deltaY = -(mMaxExtent.y - mMinExtent.y + mMinExtent.y * 2) * vertJust;
            for (int32_t i = 0; i < 6; ++i) v[i].mPosition.y += deltaY;
            mMinExtent.y += deltaY;
            mMaxExtent.y += deltaY;
        }
        if (mHoriJust != Justification::Left)
        {
            float horiJust = GetJustificationRatio(mHoriJust);
            float deltaX = -(mMaxExtent.x - mMinExtent.x) * horiJust;
            for (int32_t i = 0; i < 6; ++i) v[i].mPosition.x += deltaX;
            mMinExtent.x += deltaX;
            mMaxExtent.x += deltaX;
        }

        mReconstructVertices = false;
        return;
    }

    // Inline tag substitution. The author writes tags like "[action:Game.Interact]"
    // or "[u:E012]" in mText; we walk the working copy after substitution. mText
    // itself is never mutated — that would clobber what the editor/scripts read
    // back. When no tag is present, `work` aliases mText with zero overhead
    // beyond a single pointer copy. The cheap `find('[')` short-circuit keeps
    // the no-tag path one memcmp per call.
    //
    // We re-run the walker on its own output (bounded loop) so the common
    // chain `[action:X] -> resolver label "[u:E012]" -> UTF-8 bytes` collapses
    // in a single UpdateVertexData pass. The bound stops cyclic substitutions
    // dead — in practice nothing exceeds 2 passes.
    std::string substituted;
    bool hasTag = mText.find('[') != std::string::npos &&
                  SubstituteTextTags(mText, substituted);
    if (hasTag)
    {
        for (int pass = 0; pass < 3; ++pass)
        {
            if (substituted.find('[') == std::string::npos) break;
            std::string next;
            if (!SubstituteTextTags(substituted, next)) break;
            substituted.swap(next);
        }
    }
    const std::string& work = hasTag ? substituted : mText;

    // Check if we need to reallocate a bigger buffer.
    if (work.size() > mNumCharactersAllocated)
    {
        if (mVertices != nullptr)
        {
            delete [] mVertices;
            mVertices = nullptr;
        }

        const uint32_t allocGranularity = 32;
        uint32_t numCharsToAllocate = allocGranularity * ((uint32_t(work.size()) + allocGranularity - 1) / allocGranularity);

        mVertices = new VertexUI[numCharsToAllocate * 6];
        mNumCharactersAllocated = numCharsToAllocate;
    }

    Font* font = mFont.Get<Font>();
    OCT_ASSERT(font != nullptr);
    int32_t fontSize = font->GetSize();
    int32_t fontWidth = font->GetWidth();
    int32_t fontHeight = font->GetHeight();
    float lineSpacing = font->GetLineSpacing();
    const std::vector<Character>& fontChars = font->GetCharacters();

    mVisibleCharacters = 0;
    mMinExtent = glm::vec2(0.0f, 0.0f);
    mMaxExtent = glm::vec2(0.0f, 0.0f);

    if (work.size() == 0)
    {
        mReconstructVertices = false;
        return;
    }

    // Run through each of the characters and construct vertices for it.
    // Not using an index buffer currently, so each character is 6 vertices.
    // Topology is triangles.
    int32_t lineVertStart = 0;
    int32_t wordVertStart = 0;

    uint32_t color32 = 0xffffffff;

    const char* characters = work.c_str();
    float cursorX = 0.0f;
    float cursorY = 0.0f + (font->GetSize() + lineSpacing);

    float textScale = GetScaledTextSize() / fontSize;

    size_t i = 0;
    while (i < work.size())
    {
        // UTF-8 decode for arbitrary codepoints. ASCII bytes (< 0x80) decode
        // to themselves as a single byte, so plain-ASCII text takes the
        // identical path as before. Multi-byte sequences for codepoints like
        // PUA glyphs (U+E000+) decode to one codepoint per iteration.
        uint32_t codepoint = DecodeUtf8(characters, work.size(), i);

        if (codepoint == '\n')
        {
            cursorY += (fontSize + lineSpacing);
            cursorX = 0.0f;

            JustifyLine(mVertices, mHoriJust, lineVertStart, mVisibleCharacters * TEXT_VERTS_PER_CHAR);
            wordVertStart = lineVertStart;
            continue;
        }

        // Inline color escape `RGB` (ASCII-only — every byte fits in one
        // codepoint, so we can peek raw bytes ahead of `i`).
        if (codepoint == '`')
        {
            if (i < work.size() && work[i] == '`')
            {
                // `` is an escape for a literal backtick — skip the second
                // ` and fall through to render one ` glyph.
                i++;
            }
            else if (i + 3 < work.size() && work[i + 3] == '`')
            {
                uint32_t R = HexCharToColorComp(work[i + 0]);
                uint32_t G = HexCharToColorComp(work[i + 1]);
                uint32_t B = HexCharToColorComp(work[i + 2]);
                uint32_t A = 255;

                color32 = R | (G << 8) | (B << 16) | (A << 24);

                i += 4;  // consume RGB + closing `
                continue;
            }
            // else: bad format — fall through and render the literal `.
        }

        // Look up the codepoint in the font. If the font doesn't ship a glyph
        // for this codepoint, skip — same effect as the old ASCII filter for
        // unsupported chars, but now extended to any codepoint the font has.
        const Character* fontCharPtr = FindCharacter(fontChars, codepoint);
        if (fontCharPtr == nullptr)
        {
            continue;
        }
        const Character& fontChar = *fontCharPtr;
        VertexUI* vertices = mVertices + (mVisibleCharacters * TEXT_VERTS_PER_CHAR);

        //   0---2  3
        //   |  / / |
        //   | / /  |
        //   1  4---5
        vertices[0].mPosition.x = cursorX - fontChar.mOriginX;
        vertices[0].mPosition.y = cursorY - fontChar.mOriginY;
        vertices[0].mTexcoord.x = fontChar.mX;
        vertices[0].mTexcoord.y = fontChar.mY;

        vertices[1].mPosition.x = cursorX - fontChar.mOriginX;
        vertices[1].mPosition.y = cursorY - fontChar.mOriginY + fontChar.mHeight;
        vertices[1].mTexcoord.x = fontChar.mX;
        vertices[1].mTexcoord.y = fontChar.mY + fontChar.mHeight;

        vertices[2].mPosition.x = cursorX - fontChar.mOriginX + fontChar.mWidth;
        vertices[2].mPosition.y = cursorY - fontChar.mOriginY;
        vertices[2].mTexcoord.x = fontChar.mX + fontChar.mWidth;
        vertices[2].mTexcoord.y = fontChar.mY;

        vertices[3] = vertices[2]; // duplicated
        vertices[4] = vertices[1]; // duplicated

        vertices[5].mPosition.x = cursorX - fontChar.mOriginX + fontChar.mWidth;
        vertices[5].mPosition.y = cursorY - fontChar.mOriginY + fontChar.mHeight;
        vertices[5].mTexcoord.x = fontChar.mX + fontChar.mWidth;
        vertices[5].mTexcoord.y = fontChar.mY + fontChar.mHeight;

        for (int32_t i = 0; i < 6; ++i)
        {
            // Fill out uniform data first.
            // TODO: Maybe we should just remove color from vertex data.
            vertices[i].mColor = color32;

            // Transform texcoords into 0-1 UV space
            vertices[i].mTexcoord /= glm::vec2(fontWidth, fontHeight);
        }

        mVisibleCharacters++;
        cursorX += fontChar.mAdvance;

        // Check for wordwrap
        if (codepoint == ' ')
        {
            wordVertStart = (mVisibleCharacters) * TEXT_VERTS_PER_CHAR;
        }
        else if (mWordWrap &&
            wordVertStart != lineVertStart &&
            (cursorX - fontChar.mOriginX + fontChar.mWidth) * textScale > mRect.mWidth)
        {
            JustifyLine(mVertices, mHoriJust, lineVertStart, wordVertStart);

            float deltaX = -mVertices[wordVertStart].mPosition.x;
            float deltaY = (float)fontSize;

            for (int32_t i = wordVertStart; i < mVisibleCharacters * TEXT_VERTS_PER_CHAR; ++i)
            {
                mVertices[i].mPosition.x += deltaX;
                mVertices[i].mPosition.y += deltaY;
            }

            cursorX += deltaX;
            cursorY += deltaY;
        }
    }

    JustifyLine(mVertices, mHoriJust, lineVertStart, mVisibleCharacters * TEXT_VERTS_PER_CHAR);

    // Update extents
    mMinExtent = glm::vec2(FLT_MAX, FLT_MAX);
    mMaxExtent = glm::vec2(-FLT_MAX, -FLT_MAX);
    for (int32_t i = 0; i < mVisibleCharacters; ++i)
    {
        mMinExtent = glm::min(mMinExtent, mVertices[i * TEXT_VERTS_PER_CHAR + 0].mPosition);
        mMaxExtent = glm::max(mMaxExtent, mVertices[i * TEXT_VERTS_PER_CHAR + 5].mPosition);
    }

    // Vertical Justification
    if (mVertJust != Justification::Top)
    {
        float topGap = mMinExtent.y;
        float vertJust = GetJustificationRatio(mVertJust);
        float deltaY = -(mMaxExtent.y - mMinExtent.y + topGap * 2) * vertJust;

        const int32_t numVerts = mVisibleCharacters * TEXT_VERTS_PER_CHAR;
        for (int32_t i = 0; i < numVerts; ++i)
        {
            mVertices[i].mPosition.y += deltaY;
        }

        mMinExtent.y += deltaY;
        mMaxExtent.y += deltaY;
    }

    mReconstructVertices = false;
}

void Text::UploadVertexData()
{
    uint32_t frameIndex = Renderer::Get()->GetFrameIndex();
    bool secondaryScreen = (Renderer::Get()->GetScreenIndex() != 0);

    if (mUploadVertices[frameIndex] || secondaryScreen)
    {
        GFX_UpdateTextResourceVertexData(this);

        if (!secondaryScreen)
        {
            mUploadVertices[frameIndex] = false;
        }
    }
}

void Text::JustifyLine(VertexUI* vertices, Justification just, int32_t& lineVertStart, int32_t numVerts)
{
    if (just != Justification::Left &&
        lineVertStart < numVerts)
    {
        float horiJust = GetJustificationRatio(just);
        float deltaX = -(vertices[numVerts - 1].mPosition.x - vertices[lineVertStart].mPosition.x);
        deltaX *= horiJust;

        for (int32_t i = lineVertStart; i < numVerts; ++i)
        {
            vertices[i].mPosition.x += deltaX;
        }
    }

    lineVertStart = numVerts;
}

void Text::Render()
{
    Widget::Render();
    GFX_DrawText(this);
}

glm::vec2 Text::GetJustifiedOffset()
{
    glm::vec2 offset = glm::vec2(
        mRect.mWidth * GetJustificationRatio(mHoriJust),
        mRect.mHeight * GetJustificationRatio(mVertJust));

    return offset;
}
