
#include "Nodes/Widgets/DebugResourcesWidget.h"
#include "Nodes/Widgets/Text.h"
#include "Nodes/Widgets/ProgressBar.h"
#include "Nodes/Widgets/Quad.h"

#include "Engine.h"
#include "System/System.h"

#include <cstdio>

FORCE_LINK_DEF(DebugResourcesWidget);
DEFINE_NODE(DebugResourcesWidget, Canvas);

void DebugResourcesWidget::Create()
{
    Super::Create();
    SetName("DebugResourcesWidget");

    mBackground = CreateChild<Quad>("Background");
    mBackground->SetTransient(true);
    mBackground->SetAnchorMode(AnchorMode::FullStretch);
    mBackground->SetMargins(0.0f, 0.0f, 0.0f, 0.0f);
    mBackground->SetColor(glm::vec4(0.05f, 0.05f, 0.05f, 0.85f));

#if EDITOR
    mBackground->mHiddenInTree = true;
#endif

    // Create all rows
    auto createRow = [this](ResourceRow& row, const char* label)
    {
        row.mLabel = CreateChild<Text>(label);
        row.mLabel->SetTransient(true);
        row.mLabel->SetAnchorMode(AnchorMode::TopLeft);
        row.mLabel->SetTextSize(14.0f);
        row.mLabel->SetColor(glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
        row.mLabel->SetVerticalJustification(Justification::Center);

        row.mBar = CreateChild<ProgressBar>(label);
        row.mBar->SetTransient(true);
        row.mBar->SetAnchorMode(AnchorMode::TopLeft);
        row.mBar->SetMinValue(0.0f);
        row.mBar->SetMaxValue(100.0f);
        row.mBar->SetShowPercentage(false);
        row.mBar->SetBackgroundColor(glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
        row.mBar->SetFillColor(glm::vec4(0.2f, 0.6f, 0.9f, 1.0f));

        row.mValue = CreateChild<Text>(label);
        row.mValue->SetTransient(true);
        row.mValue->SetAnchorMode(AnchorMode::TopLeft);
        row.mValue->SetTextSize(14.0f);
        row.mValue->SetColor(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        row.mValue->SetVerticalJustification(Justification::Center);
        row.mValue->SetHorizontalJustification(Justification::Right);

#if EDITOR
        row.mLabel->mHiddenInTree = true;
        row.mBar->mHiddenInTree = true;
        row.mValue->mHiddenInTree = true;
#endif
    };

    createRow(mRAMRow, "RAM");
    createRow(mVRAMRow, "VRAM");
    createRow(mRAM1Row, "RAM1");
    createRow(mRAM2Row, "RAM2");
    createRow(mFPSRow, "FPS");

    mRAMRow.mLabel->SetText("RAM");
    mVRAMRow.mLabel->SetText("VRAM");
    mRAM1Row.mLabel->SetText("RAM1");
    mRAM2Row.mLabel->SetText("RAM2");
    mFPSRow.mLabel->SetText("FPS");

    // FPS bar has a different color
    mFPSRow.mBar->SetFillColor(glm::vec4(0.9f, 0.5f, 0.2f, 1.0f));

    SetDimensions(300.0f, 140.0f);
    mLayoutDirty = true;
}

void DebugResourcesWidget::RebuildLayout()
{
    float y = mPadding;
    float totalWidth = GetWidth();
    float barWidth = totalWidth - mLabelWidth - mValueWidth - mPadding * 4.0f;

    auto layoutRow = [&](ResourceRow& row, bool visible)
    {
        row.mLabel->SetVisible(visible);
        row.mBar->SetVisible(visible);
        row.mValue->SetVisible(visible);

        if (visible)
        {
            row.mLabel->SetPosition(mPadding, y);
            row.mLabel->SetDimensions(mLabelWidth, mRowHeight);

            row.mBar->SetPosition(mPadding + mLabelWidth + mPadding, y + 2.0f);
            row.mBar->SetDimensions(barWidth, mRowHeight - 4.0f);

            row.mValue->SetPosition(totalWidth - mValueWidth - mPadding, y);
            row.mValue->SetDimensions(mValueWidth, mRowHeight);

            y += mRowHeight + mPadding;
        }
    };

    layoutRow(mRAMRow, true);
    layoutRow(mVRAMRow, mShowVRAM);
    layoutRow(mRAM1Row, mShowMultipleRAM);
    layoutRow(mRAM2Row, mShowMultipleRAM);
    layoutRow(mFPSRow, mShowFPS);

    SetHeight(y);
    mLayoutDirty = false;
}

void DebugResourcesWidget::UpdateRow(ResourceRow& row, float value, float maxValue, const char* suffix)
{
    if (!row.mLabel->IsVisible())
        return;

    row.mBar->SetMaxValue(maxValue);
    row.mBar->SetValue(value);

    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f %s", value, suffix);
    row.mValue->SetText(buf);
}

void DebugResourcesWidget::Tick(float deltaTime)
{
    if (mLayoutDirty)
        RebuildLayout();

    UpdateMetrics(deltaTime);
    Canvas::Tick(deltaTime);
}

void DebugResourcesWidget::EditorTick(float deltaTime)
{
    if (mLayoutDirty)
        RebuildLayout();

    UpdateMetrics(deltaTime);
    Canvas::EditorTick(deltaTime);
}

void DebugResourcesWidget::PreRender()
{
    // Runtime safety net: some packaged targets can present/render while
    // game delta time is effectively zero, so force a refresh from real delta.
    if (GetLastTickedFrame() != GetEngineState()->mFrameNumber)
    {
        UpdateMetrics(GetEngineState()->mRealDeltaTime);
    }

    Canvas::PreRender();
}

void DebugResourcesWidget::UpdateMetrics(float deltaTime)
{
    float effectiveDelta = deltaTime;
    if (effectiveDelta <= 0.0f)
    {
        effectiveDelta = GetEngineState()->mRealDeltaTime;
    }

    // Accumulate FPS samples every frame
    if (effectiveDelta > 0.0f)
    {
        mFpsAccum += 1.0f / effectiveDelta;
        mFpsSamples++;
    }

    mUpdateTimer += effectiveDelta;
    if (mUpdateTimer >= sUpdateInterval)
    {
        mUpdateTimer -= sUpdateInterval;

        float ram = SYS_GetRAMUsage();
        float vram = SYS_GetVRAMUsage();
        float ram1 = SYS_GetRAM1Usage();
        float ram2 = SYS_GetRAM2Usage();
        float fps = (mFpsSamples > 0) ? (mFpsAccum / mFpsSamples) : 0.0f;
        mFpsAccum = 0.0f;
        mFpsSamples = 0;

        float ramMax = SYS_GetTotalRAM();
        float vramMax = SYS_GetTotalVRAM();
        float ram1Max = SYS_GetTotalRAM1();
        float ram2Max = SYS_GetTotalRAM2();

        // Fallback if total is unknown or zero
        if (ramMax <= 0.0f) ramMax = 512.0f;
        if (vramMax <= 0.0f) vramMax = 256.0f;
        if (ram1Max <= 0.0f) ram1Max = 256.0f;
        if (ram2Max <= 0.0f) ram2Max = 128.0f;

        UpdateRow(mRAMRow, ram, ramMax, "MB");
        UpdateRow(mVRAMRow, vram, vramMax, "MB");
        UpdateRow(mRAM1Row, ram1, ram1Max, "MB");
        UpdateRow(mRAM2Row, ram2, ram2Max, "MB");
        UpdateRow(mFPSRow, fps, 60.0f, "FPS");
    }
}

void DebugResourcesWidget::SetShowMultipleRAM(bool show)
{
    if (mShowMultipleRAM != show)
    {
        mShowMultipleRAM = show;
        mLayoutDirty = true;
    }
}

void DebugResourcesWidget::SetShowFPS(bool show)
{
    if (mShowFPS != show)
    {
        mShowFPS = show;
        mLayoutDirty = true;
    }
}

void DebugResourcesWidget::SetShowVRAM(bool show)
{
    if (mShowVRAM != show)
    {
        mShowVRAM = show;
        mLayoutDirty = true;
    }
}
