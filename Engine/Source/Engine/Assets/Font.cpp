#include "Assets/Font.h"
#include "Assets/Texture.h"
#include "Renderer.h"

#include "Nodes/Widgets/Text.h"
#include "Nodes/3D/TextMesh3d.h"

#include "AssetManager.h"

#if EDITOR
#include <stb_truetype.h>
#include <stb_image_write.h>
#include <IrrXML/irrXML.h>
#include "EditorState.h"
using namespace irr;
using namespace io;
#endif

#define USE_TTF_IMPORT 1

FORCE_LINK_DEF(Font);
DEFINE_ASSET(Font);

extern const char* gFilterEnumStrings[];
extern const char* gWrapEnumStrings[];

bool Font::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);

    OCT_ASSERT(prop != nullptr);
    bool success = false;

#if EDITOR
    Font* font = static_cast<Font*>(prop->mOwner);

    if (prop->mName == "Size")
    {
        int32_t newSize = *((int32_t*)newValue);
        if (font->mSize != newSize)
        {
            font->mSize = newSize;
        }

        success = true;
    }
    else if (prop->mName == "Rebuild Font")
    {
        font->RebuildFont();

        success = true;
    }
#endif

    HandleAssetPropChange(datum, index, newValue);

    return success;
}

Font::Font()
{
    mType = Font::GetStaticType();
}

Font::~Font()
{

}

void Font::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    mSize = stream.ReadInt32();
    mWidth = stream.ReadInt32();
    mHeight = stream.ReadInt32();
    mLineSpacing = stream.ReadFloat();
    mBold = stream.ReadBool();
    mItalic = stream.ReadBool();

    if (mVersion >= ASSET_VERSION_FONT_TTF_FLAG)
    {
        mTtf = stream.ReadBool();
    }

    mFilterType = (FilterType)stream.ReadUint8();
    mWrapMode = (WrapMode)stream.ReadUint8();
    mMipmapped = stream.ReadBool();

    int32_t numChars = stream.ReadInt32();
    mCharacters.resize(numChars);
    for (int32_t i = 0; i < numChars; ++i)
    {
        Character& charData = mCharacters[i];
        charData.mCodePoint = stream.ReadInt32();
        charData.mX = stream.ReadFloat();
        charData.mY = stream.ReadFloat();
        charData.mWidth = stream.ReadFloat();
        charData.mHeight = stream.ReadFloat();
        charData.mOriginX = stream.ReadFloat();
        charData.mOriginY = stream.ReadFloat();
        charData.mAdvance = stream.ReadFloat();
    }

    if (mTtf)
    {
        bool validTexture = stream.ReadBool();

        if (validTexture)
        {
            Texture* texture = NewTransientAsset<Texture>();
            texture->LoadStream(stream, platform);
            texture->Create();
            texture->SetName("FontTexture");
            texture->SetForceHighQuality(true);

            mTexture = texture;
        }
    }
    else
    {
        stream.ReadAsset(mTexture);
    }

#if EDITOR
    uint32_t ttfDataSize = stream.ReadUint32();
    if (ttfDataSize > 0)
    {
        // Read the TTF payload out of `stream` INTO `mTtfData`. The original
        // code did `mTtfData.ReadBytes(stream.GetData() + stream.GetPos(), ...)`
        // which read FROM mTtfData (uninitialized memory) and overwrote the
        // stream's buffer — leaving mTtfData garbage. Latent for the lifetime
        // of a single editor session because Font::Import populates mTtfData
        // directly via ReadFile (bypassing this path), but any "Rebuild Font"
        // press after a project reload tries to bake from garbage and crashes
        // in stbtt_InitFont's TTF header parse.
        mTtfData.Resize(ttfDataSize);
        stream.ReadBytes((uint8_t*)mTtfData.GetData(), ttfDataSize);
    }
#endif
}

void Font::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteInt32(mSize);
    stream.WriteInt32(mWidth);
    stream.WriteInt32(mHeight);
    stream.WriteFloat(mLineSpacing);
    stream.WriteBool(mBold);
    stream.WriteBool(mItalic);
    stream.WriteBool(mTtf);

    stream.WriteUint8((uint8_t)mFilterType);
    stream.WriteUint8((uint8_t)mWrapMode);
    stream.WriteBool(mMipmapped);

    stream.WriteInt32((int32_t)mCharacters.size());
    for (uint32_t i = 0; i < mCharacters.size(); ++i)
    {
        const Character& charData = mCharacters[i];
        stream.WriteInt32(charData.mCodePoint);
        stream.WriteFloat(charData.mX);
        stream.WriteFloat(charData.mY);
        stream.WriteFloat(charData.mWidth);
        stream.WriteFloat(charData.mHeight);
        stream.WriteFloat(charData.mOriginX);
        stream.WriteFloat(charData.mOriginY);
        stream.WriteFloat(charData.mAdvance);
    }

    if (mTtf)
    {
        Texture* texture = mTexture.Get<Texture>();
        bool validTexture = (texture != nullptr);
        stream.WriteBool(validTexture);

        if (validTexture)
        {
            texture->SaveStream(stream, platform);
        }
    }
    else
    {
        stream.WriteAsset(mTexture);
    }

#if EDITOR
    if (platform == Platform::Count)
    {
        // Save TTF data so we can rebuild the font bitmap with different settings.
        uint32_t ttfDataSize = mTtfData.GetSize();
        stream.WriteUint32(ttfDataSize);

        if (ttfDataSize > 0)
        {
            stream.WriteBytes((uint8_t*)mTtfData.GetData(), ttfDataSize);
        }
    }
#endif
}

void Font::Create()
{
    Asset::Create();
}

void Font::Destroy()
{
    Asset::Destroy();
}

bool Font::Import(const std::string& path, ImportOptions* options)
{
    bool success = Asset::Import(path, options);
    if (!success)
    {
        return false;
    }

#if EDITOR

    bool ttf = path.size() > 4 && !(path.substr(path.size() - 4) == ".xml");

    if (ttf)
    {
        // Reset the ttf data.
        mTtf = true;
        mTtfData.Reset();
        mTtfData.ReadFile(path.c_str(), false);
        RebuildFont();
    }
    else
    {
        mTtf = false;
        ImportXml(path, options);
    }

#endif // EDITOR

    return true;
}

void Font::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);

    outProps.push_back(Property(DatumType::Asset, "Texture", this, &mTexture, 1, HandleAssetPropChange, int32_t(Texture::GetStaticType())));

    outProps.push_back(Property(DatumType::Integer, "Size", this, &mSize, 1, Font::HandlePropChange));
    outProps.push_back(Property(DatumType::Float, "Line Spacing", this, &mLineSpacing, 1, HandleAssetPropChange));

    outProps.push_back(Property(DatumType::Bool, "Mipmapped", this, &mMipmapped, 1, HandleAssetPropChange));
    outProps.push_back(Property(DatumType::Integer, "Filter Type", this, &mFilterType, 1, Font::HandlePropChange, NULL_DATUM, int32_t(FilterType::Count), gFilterEnumStrings));
    outProps.push_back(Property(DatumType::Integer, "Wrap Mode", this, &mWrapMode, 1, Font::HandlePropChange, NULL_DATUM, int32_t(WrapMode::Count), gWrapEnumStrings));

#if EDITOR
    static bool sFakeRebuild = false;
    outProps.push_back(Property(DatumType::Bool, "Rebuild Font", this, &sFakeRebuild, 1, Font::HandlePropChange));
#endif
}

glm::vec4 Font::GetTypeColor()
{
    return glm::vec4(0.65f, 1.0f, 0.45f, 1.0f);
}

const char* Font::GetTypeName()
{
    return "Font";
}

const char* Font::GetTypeImportExt()
{
    return ".ttf";
}

int32_t Font::GetSize() const
{
    return mSize;
}

int32_t Font::GetWidth() const
{
    return mWidth;
}

int32_t Font::GetHeight() const
{
    return mHeight;
}

float Font::GetLineSpacing() const
{
    return mLineSpacing;
}

const std::vector<Character>& Font::GetCharacters() const
{
    return mCharacters;
}

Texture* Font::GetTexture() const
{
    return mTexture.Get<Texture>();
}

bool Font::IsBold() const
{
    return mBold;
}

bool Font::IsItalic() const
{
    return mItalic;
}

void Font::SetTexture(Texture* texture)
{
    mTexture = texture;
}

void Font::RebuildFont()
{
#if EDITOR
    if (mTtfData.GetSize() > 0)
    {
        int bakeResult = -1;
        int32_t texWidth = 128;
        int32_t texHeight = 128;

        // ASCII visible range + Private Use Area first block. PUA is where
        // gamepad/keyboard prompt fonts (Kenney, Xelu, etc.) ship their
        // glyphs — without baking it, InputActionPrompt's Glyph kind can
        // never find a codepoint in mCharacters even when the .ttf has it.
        constexpr int32_t kAsciiFirst = 32;
        constexpr int32_t kAsciiCount = 95;       // 32..126 inclusive
        constexpr int32_t kPuaFirst   = 0xE000;
        constexpr int32_t kPuaCount   = 256;      // U+E000..U+E0FF
        stbtt_packedchar asciiChars[kAsciiCount] = {};
        stbtt_packedchar puaChars[kPuaCount] = {};
        float fontSize = (float)mSize;

        // Clamp to something reasonable
        fontSize = glm::clamp(fontSize, 4.0f, 96.0f);

        // Compute the em scale factor that we need to use to produce proper sized bitmap.
        float emFontSize = fontSize;

        {
            stbtt_fontinfo fontInfo;
            stbtt_InitFont(&fontInfo, (uint8_t*)mTtfData.GetData(), 0);

            float normalScale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);
            float emScale = stbtt_ScaleForMappingEmToPixels(&fontInfo, fontSize);

            emFontSize = (emScale / normalScale) * fontSize;
        }

        while (bakeResult < 0)
        {
            uint8_t* tempBitmap = new uint8_t[texWidth * texHeight];
            memset(tempBitmap, 0, texWidth * texHeight);

            // Pack API supports multiple ranges in one bake; we use it to fit
            // ASCII + PUA-first-block into the same atlas. Returns 1 on
            // success, 0 if the bitmap ran out of room.
            stbtt_pack_range ranges[2] = {};
            ranges[0].font_size = emFontSize;
            ranges[0].first_unicode_codepoint_in_range = kAsciiFirst;
            ranges[0].num_chars = kAsciiCount;
            ranges[0].chardata_for_range = asciiChars;

            ranges[1].font_size = emFontSize;
            ranges[1].first_unicode_codepoint_in_range = kPuaFirst;
            ranges[1].num_chars = kPuaCount;
            ranges[1].chardata_for_range = puaChars;

            stbtt_pack_context spc;
            int packBeginOk = stbtt_PackBegin(&spc, tempBitmap, texWidth, texHeight, 0, 1, nullptr);
            int packOk = 0;
            if (packBeginOk)
            {
                packOk = stbtt_PackFontRanges(&spc, (const uint8_t*)mTtfData.GetData(), 0, ranges, 2);
                stbtt_PackEnd(&spc);
            }
            bakeResult = packOk ? 1 : -1;

            if (bakeResult >= 0)
            {
                // Debug: Write out .png
                //stbi_write_png("C:/Scrap/font_test.png", texWidth, texHeight, 1, tempBitmap, texWidth);

                // 1. Gather font metadata
                mSize = int32_t(fontSize + 0.5f);
                mWidth = texWidth;
                mHeight = texHeight;
                mBold = false;
                mItalic = false;

                // 2. Create transient Texture asset for the font.
                Texture* texture = NewTransientAsset<Texture>();
                // Texture stores pixel data in RGBA8 always in Editor.
                uint8_t* dataRgba = new uint8_t[texWidth * texHeight * 4];

                uint32_t numPixels = texWidth * texHeight;
                for (uint32_t i = 0; i < numPixels; ++i)
                {
                    dataRgba[i * 4 + 0] = 255;
                    dataRgba[i * 4 + 1] = 255;
                    dataRgba[i * 4 + 2] = 255;
                    dataRgba[i * 4 + 3] = tempBitmap[i];
                }

                texture->Init(texWidth, texHeight, dataRgba);
                texture->SetFilterType(mFilterType);
                texture->SetWrapMode(mWrapMode);
                texture->SetMipmapped(mMipmapped);
                texture->SetFormat(PixelFormat::LA4);
                texture->Create();
                texture->SetName("FontTexture");
                mTexture = texture;

                delete[] dataRgba;

                // 3. Gather character data — ASCII first (always), then any
                //    PUA codepoints the font actually has glyphs for.
                mCharacters.clear();

                // ASCII visible range — same set the engine has always shipped.
                for (int32_t i = 0; i < kAsciiCount; ++i)
                {
                    const stbtt_packedchar& pc = asciiChars[i];
                    Character newChar;
                    newChar.mCodePoint = kAsciiFirst + i;
                    newChar.mX = (float)pc.x0;
                    newChar.mY = (float)pc.y0;
                    newChar.mWidth = (float)(pc.x1 - pc.x0);
                    newChar.mHeight = (float)(pc.y1 - pc.y0);
                    newChar.mOriginX = -pc.xoff;
                    newChar.mOriginY = -pc.yoff;
                    newChar.mAdvance = pc.xadvance;
                    mCharacters.push_back(newChar);
                }

                // PUA first block — only push codepoints that actually have
                // glyph data. PackFontRanges leaves the packedchar zeroed for
                // missing codepoints; a width-zero + xadvance-zero entry means
                // "no glyph for this codepoint" and would otherwise pollute
                // mCharacters with thousands of empty hits.
                for (int32_t i = 0; i < kPuaCount; ++i)
                {
                    const stbtt_packedchar& pc = puaChars[i];
                    const int width  = (int)pc.x1 - (int)pc.x0;
                    const int height = (int)pc.y1 - (int)pc.y0;
                    if (width <= 0 && height <= 0 && pc.xadvance == 0.0f)
                        continue;
                    Character newChar;
                    newChar.mCodePoint = kPuaFirst + i;
                    newChar.mX = (float)pc.x0;
                    newChar.mY = (float)pc.y0;
                    newChar.mWidth = (float)width;
                    newChar.mHeight = (float)height;
                    newChar.mOriginX = -pc.xoff;
                    newChar.mOriginY = -pc.yoff;
                    newChar.mAdvance = pc.xadvance;
                    mCharacters.push_back(newChar);
                }
            }
            else
            {
                if (texHeight < texWidth)
                    texHeight = texHeight * 2;
                else
                    texWidth = texWidth * 2;
            }

            delete[] tempBitmap;
            tempBitmap = nullptr;

            // Dirty all text widgets 
            for (uint32_t i = 0; i < GetEditorState()->mEditScenes.size(); ++i)
            {
                if (GetEditorState()->GetEditScene(i)->mRootNode != nullptr)
                {
                    Node* rootNode = GetEditorState()->GetEditScene(i)->mRootNode.Get();

                    auto dirtyText = [&](Node* node) -> bool
                    {
                        Text* text = node->As<Text>();
                        if (text)
                        {
                            text->MarkDirty();
                            text->MarkVerticesDirty();
                        }

                        TextMesh3D* text3d = node->As<TextMesh3D>();
                        if (text3d)
                        {
                            text3d->MarkVerticesDirty();
                        }

                        return true;
                    };

                    if (rootNode != nullptr)
                    {
                        rootNode->Traverse(dirtyText);
                    }
                }
            }
        }
    }
#endif
}

void Font::ImportXml(const std::string& path, ImportOptions* options)
{
#if EDITOR
    mTtf = false;

    // This xml reading code is taken from the sample in irrXML.h
    IrrXMLReader* xml = createIrrXMLReader(path.c_str());

    while (xml && xml->read())
    {
        switch (xml->getNodeType())
        {
        case EXN_TEXT:
            // in this xml file, the only text which occurs is the messageText
            //messageText = xml->getNodeData();
            break;
        case EXN_ELEMENT:
        {
            if (!strcmp("font", xml->getNodeName()))
            {
                std::string size = xml->getAttributeValue("size");
                std::string width = xml->getAttributeValue("width");
                std::string height = xml->getAttributeValue("height");
                std::string bold = xml->getAttributeValue("bold");
                std::string italic = xml->getAttributeValue("italic");

                try
                {
                    mSize = std::stoi(size);
                    mWidth = std::stoi(width);
                    mHeight = std::stoi(height);
                    mBold = (bold == "true");
                    mItalic = (italic == "true");
                }
                catch (...)
                {

                }

            }
            else if (!strcmp("character", xml->getNodeName()))
            {
                std::string codePoint = xml->getAttributeValue("text");
                std::string x = xml->getAttributeValue("x");
                std::string y = xml->getAttributeValue("y");
                std::string width = xml->getAttributeValue("width");
                std::string height = xml->getAttributeValue("height");
                std::string originX = xml->getAttributeValue("origin-x");
                std::string originY = xml->getAttributeValue("origin-y");
                std::string advance = xml->getAttributeValue("advance");

                if (codePoint == "&quot")
                    codePoint = "\"";
                else if (codePoint == "&amp")
                    codePoint = "&";
                else if (codePoint == "&lt")
                    codePoint = "<";
                else if (codePoint == "&gt")
                    codePoint = ">";

                if (codePoint.length() >= 1)
                {
                    try
                    {
                        Character newChar;
                        newChar.mCodePoint = codePoint[0];
                        newChar.mX = std::stof(x);
                        newChar.mY = std::stof(y);
                        newChar.mWidth = std::stof(width);
                        newChar.mHeight = std::stof(height);
                        newChar.mOriginX = std::stof(originX);
                        newChar.mOriginY = std::stof(originY);
                        newChar.mAdvance = std::stof(advance);

                        mCharacters.push_back(newChar);
                    }
                    catch (...)
                    {

                    }
                }
            }
        }
        break;
        }
    }

    delete xml;
    xml = nullptr;

#endif
}
