#pragma once

#if EDITOR

#include "../PreferencesModule.h"

#include <string>

struct TerminalLaunchConfig;

/**
 * @brief Preferences submodule for the CLI Terminal panel.
 *
 * Lives under External/CLI in the preferences tree. Stores executable
 * configuration, working directory mode, environment variables, and
 * panel behavior toggles. Persisted as JSON.
 */
class CliModule : public PreferencesModule
{
public:
    DECLARE_PREFERENCES_MODULE(CliModule)

    CliModule();
    virtual ~CliModule();

    virtual const char* GetName() const override { return GetStaticName(); }
    virtual const char* GetParentPath() const override { return GetStaticParentPath(); }
    virtual void Render() override;
    virtual void LoadSettings(const rapidjson::Document& doc) override;
    virtual void SaveSettings(rapidjson::Document& doc) override;

    /** Resolve current settings into a launch config the session can hand to ITerminalProcess. */
    TerminalLaunchConfig BuildLaunchConfig() const;

    // Public-by-default fields, accessed by TerminalSession/TerminalPanel.
    bool        mEnabled = true;
    int         mShellPresetIdx = 0;          // ShellPreset enum value
    std::string mCustomExe;
    std::string mCustomArgs;                  // space-separated, parsed via SplitCommandLine
    int         mWorkingDirMode = 0;          // 0=Project, 1=Engine, 2=Custom
    std::string mCustomWorkingDir;
    std::string mEnvVars;                     // multi-line KEY=VALUE
    bool        mLaunchOnPanelOpen = false;
    bool        mReuseSession = true;
    bool        mCloseProcessOnPanelClose = true;
    bool        mUseConPty = true;            // Windows: use ConPTY for interactive shells
    int         mGracefulShutdownMs = 2000;
    std::string mLastCommand;                 // optional persistence

private:
    bool DrawPathInput(const char* label, std::string& path);
};

#endif
