#pragma once

#include "Nodes/Widgets/Canvas.h"
#include "Nodes/Widgets/Text.h"
#include <deque>

class Poly;

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
    FrameGraph,

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

    // Frame-graph mode state.
    Poly* mGraphPoly = nullptr;          // transient child, rebuilt each load
    std::deque<float> mFrameTimeHistory; // ring of recent frame times (ms)
    uint32_t mFrameGraphSamples = 120;   // how many samples to keep / draw
    float mFrameGraphMaxMs = 33.33f;     // vertical scale; set to expected worst-case
};
