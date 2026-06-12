#pragma once

#include "Asset.h"
#include "Assets/InputPromptMap.h"  // for InputPromptKind

#include <array>
#include <string>
#include <vector>

class POLYPHASE_API InputPromptStyle : public Asset
{
public:

    DECLARE_ASSET(InputPromptStyle, Asset);

    static constexpr int32_t kPriorityCount = 3;

    InputPromptStyle();
    virtual ~InputPromptStyle();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;

    float     GetIconSize() const     { return mIconSize; }
    float     GetSpacing()  const     { return mSpacing; }
    glm::vec4 GetTint()     const     { return mTint; }
    InputPromptKind GetPriorityAt(int32_t i) const;
    const std::array<InputPromptKind, kPriorityCount>& GetPriority() const { return mPriority; }
    void      SetPriorityAt(int32_t i, InputPromptKind kind);

    const std::vector<std::string>& GetPrewarmActions() const { return mPrewarmActions; }
    std::vector<std::string>&       GetPrewarmActions()       { return mPrewarmActions; }

    float     mIconSize = 24.0f;
    float     mSpacing  = 2.0f;
    glm::vec4 mTint     = glm::vec4(1.0f);

    std::array<InputPromptKind, kPriorityCount> mPriority = {
        InputPromptKind::Sprite,
        InputPromptKind::Glyph,
        InputPromptKind::Text,
    };

    std::vector<std::string> mPrewarmActions;

private:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);
};
