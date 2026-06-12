#if EDITOR

#include "InputPromptStyleInspector.h"
#include "Assets/InputPromptStyle.h"
#include "Assets/InputPromptMap.h"
#include "Input/PlayerInputSystem.h"
#include "Input/InputPromptResolver.h"
#include "Asset.h"

#include "imgui.h"

#include <string>

InputPromptStyleInspector* InputPromptStyleInspector::sInstance = nullptr;

InputPromptStyleInspector* InputPromptStyleInspector::Get()
{
    if (!sInstance)
        sInstance = new InputPromptStyleInspector();
    return sInstance;
}

static const char* KindName(InputPromptKind k)
{
    switch (k)
    {
    case InputPromptKind::Sprite: return "Sprite";
    case InputPromptKind::Glyph:  return "Glyph";
    case InputPromptKind::Text:   return "Text";
    default:                      return "?";
    }
}

void InputPromptStyleInspector::Draw(InputPromptStyle* style)
{
    if (!style) return;

    ImGui::Separator();
    ImGui::TextUnformatted("Resolution Priority");
    ImGui::TextDisabled("Sprite > Glyph > Text by default. Click chips to swap with neighbour.");

    const int N = InputPromptStyle::kPriorityCount;
    for (int i = 0; i < N; ++i)
    {
        ImGui::PushID(i);
        InputPromptKind k = style->GetPriorityAt(i);
        char label[32];
        snprintf(label, sizeof(label), "%d. %s", i + 1, KindName(k));
        if (ImGui::Button(label, ImVec2(110.0f, 0.0f)))
        {
            // Cycle this chip's kind through the 3 options.
            int next = ((int)k + 1) % (int)InputPromptKind::Count;
            // Swap with whichever chip currently holds `next` so no kind appears
            // twice — keeps the priority list a permutation.
            for (int j = 0; j < N; ++j)
            {
                if (j != i && style->GetPriorityAt(j) == (InputPromptKind)next)
                {
                    style->SetPriorityAt(j, k);
                    break;
                }
            }
            style->SetPriorityAt(i, (InputPromptKind)next);
            style->SetDirtyFlag();
            if (auto* r = InputPromptResolver::Get()) r->Invalidate();
        }
        if (i + 1 < N) ImGui::SameLine();
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Prewarm Actions");
    ImGui::TextDisabled("Category/Name pairs resolved once on asset load.");

    auto& actions = style->GetPrewarmActions();
    for (int i = 0; i < (int)actions.size(); ++i)
    {
        ImGui::PushID(i);
        char buf[160];
        strncpy(buf, actions[i].c_str(), sizeof(buf));
        buf[sizeof(buf) - 1] = 0;
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::InputText("##actName", buf, sizeof(buf)))
        {
            actions[i] = buf;
            style->SetDirtyFlag();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X"))
        {
            actions.erase(actions.begin() + i);
            style->SetDirtyFlag();
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    // Add row with combobox of known actions.
    if (PlayerInputSystem* pis = PlayerInputSystem::Get())
    {
        const auto& known = pis->GetActions();
        std::string current = mNewPrewarmBuf;
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("##pickAction", current.empty() ? "<pick action>" : current.c_str()))
        {
            for (const auto& a : known)
            {
                std::string disp = a.category + "/" + a.name;
                if (ImGui::Selectable(disp.c_str()))
                {
                    strncpy(mNewPrewarmBuf, disp.c_str(), sizeof(mNewPrewarmBuf));
                    mNewPrewarmBuf[sizeof(mNewPrewarmBuf) - 1] = 0;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Add"))
        {
            if (mNewPrewarmBuf[0] != 0)
            {
                actions.push_back(mNewPrewarmBuf);
                style->SetDirtyFlag();
                mNewPrewarmBuf[0] = 0;
            }
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Live Preview");
    ImGui::TextDisabled("Tint and icon size apply to in-game prompts. ");
    // Render three label-only previews — one per device type. Sprite/glyph
    // previews require a Quad/Text rendered into ImGui texture space; that
    // hookup belongs in the runtime widget path. Showing the resolved label
    // is enough to verify style/priority changes affect the right path.
    if (PlayerInputSystem* pis = PlayerInputSystem::Get())
    {
        InputDeviceDescriptor devs[3];
        devs[0].kind = InputDeviceKind::Keyboard;
        devs[1].kind = InputDeviceKind::Gamepad; devs[1].gamepadType = GamepadType::Standard;
        devs[2].kind = InputDeviceKind::Gamepad; devs[2].gamepadType = GamepadType::DualSense;
        const char* labels[3] = { "Keyboard", "Xbox", "DualSense" };
        for (int i = 0; i < 3; ++i)
        {
            ImGui::Text("%s:", labels[i]);
            ImGui::SameLine();
            std::string previewLabel = "—";
            if (auto* r = InputPromptResolver::Get())
            {
                if (auto* p = r->Resolve(nullptr, style, "Game", "Interact", &devs[i]))
                    if (!p->label.empty()) previewLabel = p->label;
            }
            ImGui::Text("%s", previewLabel.c_str());
        }
    }
}

#endif // EDITOR
