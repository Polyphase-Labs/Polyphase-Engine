#pragma once

#if EDITOR

#include "../PreferencesModule.h"

class AppearanceModule : public PreferencesModule
{
public:
    DECLARE_PREFERENCES_MODULE(AppearanceModule)

    AppearanceModule();
    virtual ~AppearanceModule();

    virtual const char* GetName() const override { return GetStaticName(); }
    virtual const char* GetParentPath() const override { return GetStaticParentPath(); }
    virtual void Render() override;
    virtual void LoadSettings(const rapidjson::Document& doc) override;
    virtual void SaveSettings(rapidjson::Document& doc) override;

    // Reads the saved text scale straight from disk. The editor fonts are
    // loaded before PreferencesManager exists, so EditorImguiInit uses this.
    static float LoadSavedTextScale();

private:
    float mTabRoundingLeft = 0.0f;
    float mTabRoundingRight = 0.0f;
    float mTextScale = 1.0f;

    // Slider edit buffers. Nothing is applied until Apply / Reset is pressed.
    float mPendingTextScale = 1.0f;
    float mPendingInterfaceScale = 1.0f;
    float mSeenInterfaceScale = 0.0f;

    void ApplyTabRounding();
    void ApplyTextScale(float scale);
};

#endif
