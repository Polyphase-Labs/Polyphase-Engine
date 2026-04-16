#pragma once

#if EDITOR

#include "../PreferencesModule.h"

class CuttingEdgeModule : public PreferencesModule
{
public:
    DECLARE_PREFERENCES_MODULE(CuttingEdgeModule)

    CuttingEdgeModule();
    virtual ~CuttingEdgeModule();

    virtual const char* GetName() const override { return GetStaticName(); }
    virtual const char* GetParentPath() const override { return GetStaticParentPath(); }
    virtual void Render() override;
    virtual void LoadSettings(const rapidjson::Document& doc) override;
    virtual void SaveSettings(rapidjson::Document& doc) override;

    bool GetCuttingEdgeEnabled() const { return mCuttingEdgeEnabled; }
    void SetCuttingEdgeEnabled(bool enabled);

    static CuttingEdgeModule* Get();

private:
    bool mCuttingEdgeEnabled = false;

    static CuttingEdgeModule* sInstance;
};

#endif
