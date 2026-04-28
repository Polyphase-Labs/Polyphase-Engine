#pragma once

#include "Nodes/Widgets/Canvas.h"

class Text;
class ProgressBar;
class Quad;

class DebugResourcesWidget : public Canvas
{
public:

    DECLARE_NODE(DebugResourcesWidget, Canvas);

    virtual void Create() override;
    virtual void Tick(float deltaTime) override;
    virtual void EditorTick(float deltaTime) override;
    virtual void PreRender() override;

    void SetShowMultipleRAM(bool show);
    void SetShowFPS(bool show);
    void SetShowVRAM(bool show);

private:

    struct ResourceRow
    {
        Text* mLabel = nullptr;
        ProgressBar* mBar = nullptr;
        Text* mValue = nullptr;
    };

    void RebuildLayout();
    void UpdateRow(ResourceRow& row, float value, float maxValue, const char* suffix);
    void UpdateMetrics(float deltaTime);

    ResourceRow mRAMRow;
    ResourceRow mVRAMRow;
    ResourceRow mRAM1Row;
    ResourceRow mRAM2Row;
    ResourceRow mFPSRow;

    bool mShowMultipleRAM = true;
    bool mShowFPS = true;
    bool mShowVRAM = true;
    bool mLayoutDirty = true;
    float mUpdateTimer = 0.0f;
    float mFpsAccum = 0.0f;
    uint32_t mFpsSamples = 0;
    static constexpr float sUpdateInterval = 0.5f;

    float mRowHeight = 22.0f;
    float mLabelWidth = 50.0f;
    float mValueWidth = 80.0f;
    float mPadding = 4.0f;

    Quad* mBackground = nullptr;
};
