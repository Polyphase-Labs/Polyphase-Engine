#pragma once

#if EDITOR

#include "../../../PreferencesModule.h"
#include "Maths.h"

// Preferences entry under Appearance > Viewport > Tilemap Grid.
// Owns the colors used by TilePaintManager::DrawGridOverlay so users can
// customize the cell grid appearance without recompiling.
class TilemapGridModule : public PreferencesModule
{
public:
    DECLARE_PREFERENCES_MODULE(TilemapGridModule)

    TilemapGridModule();
    virtual ~TilemapGridModule();

    virtual const char* GetName() const override { return GetStaticName(); }
    virtual const char* GetParentPath() const override { return GetStaticParentPath(); }
    virtual void Render() override;
    virtual void LoadSettings(const rapidjson::Document& doc) override;
    virtual void SaveSettings(rapidjson::Document& doc) override;

    // Settings accessors used by TilePaintManager.
    glm::vec4 GetMinorColor() const { return mMinorColor; }
    glm::vec4 GetAxisColor() const { return mAxisColor; }
    bool GetHighlightAxes() const { return mHighlightAxes; }

    static TilemapGridModule* Get();

private:
    // Defaults match the original hardcoded values in TilePaintManager so
    // existing users don't see a sudden visual change after the upgrade.
    glm::vec4 mMinorColor    = glm::vec4(1.0f, 1.0f, 1.0f, 0.35f);
    glm::vec4 mAxisColor     = glm::vec4(1.0f, 1.0f, 0.4f, 0.85f);
    bool      mHighlightAxes = true;

    static TilemapGridModule* sInstance;
};

#endif
