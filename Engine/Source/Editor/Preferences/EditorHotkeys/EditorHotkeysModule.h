#pragma once

#if EDITOR

#include "../PreferencesModule.h"

class EditorHotkeysModule : public PreferencesModule
{
public:
    DECLARE_PREFERENCES_MODULE(EditorHotkeysModule)

    EditorHotkeysModule();
    virtual ~EditorHotkeysModule();

    virtual const char* GetName() const override       { return GetStaticName(); }
    virtual const char* GetParentPath() const override { return GetStaticParentPath(); }
    virtual void Render() override;
    virtual void LoadSettings(const rapidjson::Document& doc) override;
    virtual void SaveSettings(rapidjson::Document& doc) override;
};

#endif
