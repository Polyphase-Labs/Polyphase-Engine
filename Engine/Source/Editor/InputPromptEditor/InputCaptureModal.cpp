#if EDITOR

#include "InputCaptureModal.h"
#include "Input/InputPath.h"
#include "Input/InputMap.h"
#include "Input/Input.h"
#include "Engine.h"

#include "imgui.h"

#include <cmath>

static constexpr float kCaptureTimeout = 5.0f;
static constexpr float kCaptureAxisThreshold = 0.6f;

void InputCaptureModal::Start(Callback onCaptured)
{
    mCapturing = true;
    mJustStarted = true;
    mTimer = kCaptureTimeout;
    mOnCaptured = std::move(onCaptured);

    if (PlayerInputSystem* pis = PlayerInputSystem::Get())
        pis->SetActionEvaluationEnabled(false);
}

bool InputCaptureModal::Draw()
{
    if (!mCapturing)
        return false;

    if (mJustStarted)
    {
        ImGui::OpenPopup("Input Capture##InputPromptMap");
        mJustStarted = false;
    }

    bool finished = false;
    Result result;

    if (ImGui::BeginPopupModal("Input Capture##InputPromptMap", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        mTimer -= ImGui::GetIO().DeltaTime;

        // Timeout — cancel.
        if (mTimer <= 0.0f)
        {
            ImGui::CloseCurrentPopup();
            mCapturing = false;
            if (PlayerInputSystem* pis = PlayerInputSystem::Get())
                pis->SetActionEvaluationEnabled(true);
            ImGui::EndPopup();
            return false;
        }

        ImGui::TextUnformatted("Press any key, mouse button, or gamepad input...");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.2f, 1.0f));
        ImGui::Text("Listening %.0fs (Esc cancels)", mTimer);
        ImGui::PopStyleColor();

        // ---------- Poll ----------
        // Keyboard
        for (int32_t k = 0; k < INPUT_MAX_KEYS && !finished; ++k)
        {
            if (k == POLYPHASE_KEY_ESCAPE)
                continue;
            if (INP_IsKeyJustDown(k))
            {
                result.binding = MakeBinding(InputSourceType::Keyboard, k);
                finished = true;
            }
        }
        // Mouse
        for (int32_t mb = 0; mb < MOUSE_BUTTON_COUNT && !finished; ++mb)
        {
            if (INP_IsMouseButtonJustDown(mb))
            {
                result.binding = MakeBinding(InputSourceType::MouseButton, mb);
                finished = true;
            }
        }
        // Gamepad
        if (!finished)
        {
            const InputState& input = GetEngineState()->mInput;
            for (int32_t gp = 0; gp < INPUT_MAX_GAMEPADS && !finished; ++gp)
            {
                if (!input.mGamepads[gp].mConnected)
                    continue;

                const GamepadState& cur = input.mGamepads[gp];
                const GamepadState& prev = input.mPrevGamepads[gp];

                for (int32_t b = 0; b < GAMEPAD_BUTTON_COUNT && !finished; ++b)
                {
                    if (cur.mButtons[b] && !prev.mButtons[b])
                    {
                        result.binding = MakeBinding(InputSourceType::GamepadButton, b,
                                                     AxisDirection::Positive, gp);
                        result.gamepadDetected = true;
                        result.gamepadIndex = gp;
                        result.gamepadType = cur.mType;
                        finished = true;
                    }
                }
                for (int32_t a = 0; a < GAMEPAD_AXIS_COUNT && !finished; ++a)
                {
                    float cv = cur.mAxes[a];
                    float pv = prev.mAxes[a];
                    if (std::abs(cv) >= kCaptureAxisThreshold &&
                        std::abs(pv) < kCaptureAxisThreshold)
                    {
                        AxisDirection dir = cv > 0.0f ? AxisDirection::Positive
                                                      : AxisDirection::Negative;
                        result.binding = MakeBinding(InputSourceType::GamepadAxis, a, dir, gp);
                        result.gamepadDetected = true;
                        result.gamepadIndex = gp;
                        result.gamepadType = cur.mType;
                        finished = true;
                    }
                }
            }
        }

        // Escape cancels
        if (INP_IsKeyJustDown(POLYPHASE_KEY_ESCAPE))
        {
            ImGui::CloseCurrentPopup();
            mCapturing = false;
            if (PlayerInputSystem* pis = PlayerInputSystem::Get())
                pis->SetActionEvaluationEnabled(true);
            ImGui::EndPopup();
            return false;
        }

        if (finished)
        {
            result.path = MakeInputPath(result.binding);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (finished)
    {
        mCapturing = false;
        if (PlayerInputSystem* pis = PlayerInputSystem::Get())
            pis->SetActionEvaluationEnabled(true);
        if (mOnCaptured)
            mOnCaptured(result);
        return false;
    }

    return mCapturing;
}

#endif // EDITOR
