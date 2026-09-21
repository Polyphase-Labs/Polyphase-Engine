#pragma once

#if EDITOR

#include "../../PreferencesModule.h"
#include "Maths.h"

class ViewportModule : public PreferencesModule
{
public:
    DECLARE_PREFERENCES_MODULE(ViewportModule)

    ViewportModule();
    virtual ~ViewportModule();

    virtual const char* GetName() const override { return GetStaticName(); }
    virtual const char* GetParentPath() const override { return GetStaticParentPath(); }
    virtual void Render() override;
    virtual void LoadSettings(const rapidjson::Document& doc) override;
    virtual void SaveSettings(rapidjson::Document& doc) override;

    // Settings accessors
    glm::vec4 GetBackgroundColor() const { return mBackgroundColor; }
    bool GetShowGrid() const { return mShowGrid; }
    glm::vec4 GetGridColor() const { return mGridColor; }
    float GetGridSize() const { return mGridSize; }
    glm::vec4 GetSelectedColor() const { return mSelectedColor; }
    float GetSelectedCheckerSize() const { return mSelectedCheckerSize; }
    float GetMenuBarPadding() const { return mMenuBarPadding; }
    bool GetShowGizmosInPreview() const { return mShowGizmosInPreview; }
    void SetShowGizmosInPreview(bool show);
    // 0 = Auto (50% on Intel/AMD integrated GPUs driving a HiDPI window,
    // else 100%), 1 = 100%, 2 = 75%, 3 = 50%.
    int GetResolutionScaleMode() const { return mResolutionScaleMode; }
    // Pushes the mode's scale to the renderer. Auto depends on the window's
    // pixel size, so EditorState::Update calls this whenever that changes.
    void ApplyResolutionScale() const;

    // Transform snapping. Mode: 0 = Increment, 1 = Vertex, 2 = Edge, 3 = Face.
    bool GetSnapEnabled() const { return mSnapEnabled; }
    void SetSnapEnabled(bool enabled);
    int GetSnapMode() const { return mSnapMode; }
    void SetSnapMode(int mode);
    float GetSnapTranslate() const { return mSnapTranslate; }
    float GetSnapRotate() const { return mSnapRotate; }
    float GetSnapScale() const { return mSnapScale; }
    float GetSnapWidgetPixels() const { return mSnapWidgetPixels; }
    float GetSnapPixelThreshold() const { return mSnapPixelThreshold; }
    float GetPrecisionNormal() const { return mPrecisionNormal; }
    float GetPrecisionCtrl() const { return mPrecisionCtrl; }

    static ViewportModule* Get();
    static void HandleExternalGridToggle(bool enabled);

private:
    void ApplyBackgroundColorToRenderer() const;
    void ApplyGridVisibility();
    void ApplySelectedOverlay() const;

    int mResolutionScaleMode = 0;
    glm::vec4 mBackgroundColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
    bool mShowGrid = true;
    glm::vec4 mGridColor = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);
    float mGridSize = 1.0f;
    glm::vec4 mSelectedColor = glm::vec4(0.2f, 0.1f, 1.0f, 0.6f);
    float mSelectedCheckerSize = 8.0f;
    float mMenuBarPadding = 8.0f;
    bool mShowGizmosInPreview = false;

    bool mSnapEnabled = false;
    int mSnapMode = 0;
    float mSnapTranslate = 1.0f;
    float mSnapRotate = 15.0f;
    float mSnapScale = 0.1f;
    float mSnapWidgetPixels = 8.0f;
    float mSnapPixelThreshold = 16.0f;
    float mPrecisionNormal = 1.0f;
    float mPrecisionCtrl = 0.1f;

    static ViewportModule* sInstance;
    static bool sSyncingGridState;
};

#endif
