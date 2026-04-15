#pragma once

#include "Nodes/Widgets/Canvas.h"
#include "Nodes/Widgets/Text.h"

enum class StatDisplayMode : uint8_t
{
    None,
    FrameText,
    CpuStatText,
    CpuStatBars,
    GpuStatText,
    GpuStatBars,
    AllStatText,
    Memory,
    Network,

    Count
};

class StatsOverlay : public Canvas
{
public:

    DECLARE_NODE(StatsOverlay, Canvas);

    StatsOverlay();
    virtual ~StatsOverlay();

    virtual void GatherProperties(std::vector<Property>& outProps) override;

    // Every live StatsOverlay registers itself here so the Renderer can tell
    // if a scene-placed one exists before showing its own fallback.
    static const std::vector<StatsOverlay*>& GetAllInstances();

    virtual void Tick(float deltaTime) override;
    virtual void EditorTick(float deltaTime) override;
    void TickCommon(float deltaTime);

    void SetDisplayMode(StatDisplayMode mode);
    StatDisplayMode GetDisplayMode() const;

    void SetStatText(uint32_t index, const char* key, float value, glm::vec4 color, float& y);

    float mTextSize = 14.0f;

    std::vector<Text*> mStatKeyTexts;
    std::vector<Text*> mStatValueTexts;
    StatDisplayMode mDisplayMode = StatDisplayMode::AllStatText;
    bool mTextChildrenInitialized = false;
};
