#include "Nodes/Widgets/StatsOverlay.h"
#include "Nodes/Widgets/Poly.h"
#include "Assets/Font.h"
#include "AssetManager.h"
#include "Renderer.h"
#include "Profiler.h"
#include "Engine.h"
#include "Clock.h"
#include "NetworkManager.h"

#include "System/System.h"

#include <algorithm>

FORCE_LINK_DEF(StatsOverlay);
DEFINE_NODE(StatsOverlay, Canvas);

#define DEFAULT_STAT_COLOR glm::vec4(0.4f, 1.0f, 0.4f, 1.0f)

static const char* sStatDisplayModeStrings[] =
{
    "None",
    "Frame Text",
    "CPU Stat Text",
    "CPU Stat Bars",
    "GPU Stat Text",
    "GPU Stat Bars",
    "All Stat Text",
    "Memory",
    "Network",
    "Frame Graph",
};

static std::vector<StatsOverlay*>& GetStatsOverlayInstancesMutable()
{
    static std::vector<StatsOverlay*> sInstances;
    return sInstances;
}

const std::vector<StatsOverlay*>& StatsOverlay::GetAllInstances()
{
    return GetStatsOverlayInstancesMutable();
}

StatsOverlay::StatsOverlay()
{
    const float width = 200.0f;
    const float height = 240.0f;
    float x = -width;
    float y = 10.0f;

    SetAnchorMode(AnchorMode::TopRight);
    SetRect(x, y, width, height);

    GetStatsOverlayInstancesMutable().push_back(this);
}

StatsOverlay::~StatsOverlay()
{
    auto& v = GetStatsOverlayInstancesMutable();
    v.erase(std::remove(v.begin(), v.end(), this), v.end());
}

void StatsOverlay::Tick(float deltaTime)
{
    TickCommon(deltaTime);
    Canvas::Tick(deltaTime);
}

void StatsOverlay::EditorTick(float deltaTime)
{
    TickCommon(deltaTime);
    Canvas::EditorTick(deltaTime);
}

void StatsOverlay::TickCommon(float deltaTime)
{
    // (0) On the first tick after construction/load, nuke any serialized Key/
    // Value Text children left over in the scene. Text children are now marked
    // transient when created (see step 3), so fresh instances start clean —
    // but scenes saved before that fix can carry dozens of stale children
    // that pile up on top of new ones. Destroying them here is self-healing.
    if (!mTextChildrenInitialized)
    {
        std::vector<Node*> toDestroy;
        for (uint32_t i = 0; i < GetNumChildren(); ++i)
        {
            Node* child = GetChild(i);
            if (child == nullptr) continue;
            const std::string& name = child->GetName();
            if (name == "Key" || name == "Value" || name == "Graph")
                toDestroy.push_back(child);
        }
        for (Node* n : toDestroy)
        {
            n->Destroy();
        }
        mStatKeyTexts.clear();
        mStatValueTexts.clear();
        mGraphPoly = nullptr;
        mTextChildrenInitialized = true;
    }

    // (1) Determine the number of stats to display.
    uint32_t numStats = 0;

    switch (mDisplayMode)
    {
    case StatDisplayMode::FrameText:
        numStats = 1;
        break;
    case StatDisplayMode::CpuStatText:
    case StatDisplayMode::CpuStatBars:
        numStats = (uint32_t)GetProfiler()->GetCpuFrameStats().size();
        break;
    case StatDisplayMode::GpuStatText:
    case StatDisplayMode::GpuStatBars:
        numStats = (uint32_t)GetProfiler()->GetGpuStats().size();
        break;
    case StatDisplayMode::AllStatText:
        numStats = (uint32_t)GetProfiler()->GetCpuFrameStats().size();
        numStats += (uint32_t)GetProfiler()->GetGpuStats().size();
        break;
    case StatDisplayMode::Memory:
        numStats = (uint32_t)SYS_GetMemoryStats().size();
        break;
    case StatDisplayMode::Network:
        numStats = 2;
        break;
    case StatDisplayMode::FrameGraph:
        numStats = 0; // Graph takes over — no text rows.
        break;
    default:
        numStats = 0;
        break;
    }

    // (2) Hide excess stats
    OCT_ASSERT(mStatKeyTexts.size() == mStatValueTexts.size());
    for (uint32_t i = numStats; i < mStatKeyTexts.size(); ++i)
    {
        mStatKeyTexts[i]->SetVisible(false);
        mStatValueTexts[i]->SetVisible(false);
    }

    // Hide the frame-graph poly whenever we're not in FrameGraph mode.
    if (mGraphPoly != nullptr && mDisplayMode != StatDisplayMode::FrameGraph)
    {
        mGraphPoly->SetVisible(false);
    }

    // (3) Create widget(s) if needed.
    if (numStats > mStatKeyTexts.size())
    {
        Font* font = LoadAsset<Font>("F_RobotoMono16");
        uint32_t numToAlloc = (numStats - (uint32_t)mStatKeyTexts.size());

        for (uint32_t i = 0; i < numToAlloc; ++i)
        {
            Text* newKeyText = CreateChild<Text>("Key");
            newKeyText->SetColor(DEFAULT_STAT_COLOR);
            newKeyText->SetFont(font);
            newKeyText->SetTransient(true);

            Text* newValueText = CreateChild<Text>("Value");
            newValueText->SetColor(DEFAULT_STAT_COLOR);
            newValueText->SetFont(font);
            newValueText->SetTransient(true);

            mStatKeyTexts.push_back(newKeyText);
            mStatValueTexts.push_back(newValueText);
        }
    }

    float statY = 0.0f;

    if (mDisplayMode == StatDisplayMode::Memory)
    {
        std::vector<MemoryStat> stats = SYS_GetMemoryStats();

        for (uint32_t i = 0; i < stats.size(); ++i)
        {
            std::string statText = stats[i].mName;
            uint64_t statValue = 0;

            if (stats[i].mBytesAllocated == 0)
            {
                statText += ": (Free)";
                statValue = stats[i].mBytesFree;
            }
            else
            {
                statText += ": (Used)";
                statValue = stats[i].mBytesAllocated;
            }

            SetStatText(i, statText.c_str(), statValue / static_cast<float>(1024 * 1024), DEFAULT_STAT_COLOR, statY);
        }
    }
    else if (mDisplayMode == StatDisplayMode::Network)
    {
        NetworkManager* netMan = NetworkManager::Get();
        SetStatText(0, "Upload", netMan->GetUploadRate() / 1024, DEFAULT_STAT_COLOR, statY);
        SetStatText(1, "Download", netMan->GetDownloadRate() / 1024, DEFAULT_STAT_COLOR, statY);
    }
    else if (mDisplayMode == StatDisplayMode::FrameGraph)
    {
        // Lazily create the graph's line widget (transient — never serialized).
        if (mGraphPoly == nullptr)
        {
            mGraphPoly = CreateChild<Poly>("Graph");
            mGraphPoly->SetTransient(true);
            mGraphPoly->SetLineWidth(1.5f);
        }
        mGraphPoly->SetVisible(true);

        // Fill the entire StatsOverlay rect.
        const float graphW = GetWidth();
        const float graphH = GetHeight();
        mGraphPoly->SetRect(0.0f, 0.0f, graphW, graphH);

        // Record the most recent frame time (ms).
        const Clock* clock = GetAppClock();
        float frameMs = clock ? (clock->DeltaTime() * 1000.0f) : 0.0f;
        mFrameTimeHistory.push_back(frameMs);
        while (mFrameTimeHistory.size() > mFrameGraphSamples)
        {
            mFrameTimeHistory.pop_front();
        }

        // Rebuild the poly's line strip from history. X goes left-to-right,
        // Y is inverted (0 = top = largest frame time at maxMs).
        mGraphPoly->ClearVertices();
        const uint32_t n = (uint32_t)mFrameTimeHistory.size();
        if (n >= 2 && mFrameGraphMaxMs > 0.0f)
        {
            const float stepX = graphW / (float)(mFrameGraphSamples - 1);
            for (uint32_t i = 0; i < n; ++i)
            {
                float ms = mFrameTimeHistory[i];
                float normalized = ms / mFrameGraphMaxMs;
                if (normalized > 1.0f) normalized = 1.0f;

                float x = stepX * (float)i;
                float y = graphH - (normalized * graphH);

                // Colour ramps green -> yellow -> red as frame time climbs.
                glm::vec4 color;
                if (normalized < 0.5f)       color = glm::vec4(0.4f, 1.0f, 0.4f, 1.0f);
                else if (normalized < 0.75f) color = glm::vec4(1.0f, 1.0f, 0.3f, 1.0f);
                else                         color = glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);

                mGraphPoly->AddVertex(glm::vec2(x, y), color);
            }
            mGraphPoly->MarkVerticesDirty();
        }
    }
    else
    {
        const std::vector<CpuStat>& cpuStats = GetProfiler()->GetCpuFrameStats();
        const std::vector<GpuStat>& gpuStats = GetProfiler()->GetGpuStats();
        OCT_ASSERT(numStats <= (cpuStats.size() + gpuStats.size()));
        uint32_t uStat = 0;

        if (mDisplayMode == StatDisplayMode::CpuStatBars ||
            mDisplayMode == StatDisplayMode::CpuStatText ||
            mDisplayMode == StatDisplayMode::AllStatText)
        {
            // Cpu Stats first
            for (uint32_t i = 0; i < cpuStats.size(); ++i)
            {
                SetStatText(uStat, cpuStats[i].mName, cpuStats[i].mSmoothedTime, DEFAULT_STAT_COLOR, statY);
                ++uStat;
            }
        }

        if (mDisplayMode == StatDisplayMode::GpuStatBars ||
            mDisplayMode == StatDisplayMode::GpuStatText ||
            mDisplayMode == StatDisplayMode::AllStatText)
        {
            // Gpu Stats second
            for (uint32_t i = 0; i < gpuStats.size(); ++i)
            {
                SetStatText(uStat, gpuStats[i].mName, gpuStats[i].mSmoothedTime, glm::vec4(1.0f, 0.4f, 0.4f, 1.0f), statY);
                ++uStat;
            }
        }
    }
}

void StatsOverlay::GatherProperties(std::vector<Property>& outProps)
{
    Canvas::GatherProperties(outProps);

    SCOPED_CATEGORY("Stats Overlay");

    outProps.push_back(Property(DatumType::Byte, "Display Mode", this, &mDisplayMode, 1,
        nullptr, NULL_DATUM, int32_t(StatDisplayMode::Count), sStatDisplayModeStrings));
    outProps.push_back(Property(DatumType::Float, "Text Size", this, &mTextSize));
    outProps.push_back(Property(DatumType::Integer, "Graph Samples", this, &mFrameGraphSamples));
    outProps.push_back(Property(DatumType::Float, "Graph Max (ms)", this, &mFrameGraphMaxMs));
}

void StatsOverlay::SetDisplayMode(StatDisplayMode mode)
{
    mDisplayMode = mode;
}

StatDisplayMode StatsOverlay::GetDisplayMode() const
{
    return mDisplayMode;
}

void StatsOverlay::SetStatText(uint32_t index, const char* key, float value, glm::vec4 color, float& y)
{
    float keyX = 0.0f;
    float valueX = 150.0f;

    Text* keyText = mStatKeyTexts[index];
    Text* valueText = mStatValueTexts[index];
    keyText->SetVisible(true);
    valueText->SetVisible(true);
    keyText->SetColor(color);
    valueText->SetColor(color);

    keyText->SetText(key);
    char valueString[16];
    snprintf(valueString, 16, "%.2f", value);
    valueText->SetText(valueString);

    keyText->SetPosition(keyX, y);
    valueText->SetPosition(valueX, y);
    keyText->SetTextSize(mTextSize);
    valueText->SetTextSize(mTextSize);

    y += mTextSize;
}
