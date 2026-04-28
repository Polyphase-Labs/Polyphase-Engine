#pragma once

#if EDITOR

#include <string>
#include <vector>
#include <cstdint>

struct SshHostEntry
{
    std::string mHost;
    std::string mHostName;
    std::string mUser;
};

class GitRemoteEditDialog
{
public:
    enum class Mode
    {
        Add,
        Edit
    };

    void Open();
    void SetMode(Mode mode);
    void SetMode(Mode mode, const std::string& name, const std::string& url);
    void Draw();

private:
    void ParseSshConfig();
    void RebuildUrlFromSshHost();

    bool mIsOpen = false;
    bool mJustOpened = false;

    Mode mMode = Mode::Add;

    char mRemoteName[128] = {0};
    char mRemoteUrl[512] = {0};

    // SSH host selection
    std::vector<SshHostEntry> mSshHosts;
    bool mSshConfigParsed = false;
    int32_t mSelectedSshHost = -1; // -1 = custom / HTTPS
    char mSshRepoPath[256] = {0}; // e.g. "polyphase-engine/tilemaptest00101.git"

    std::string mOriginalName;
};

GitRemoteEditDialog* GetGitRemoteEditDialog();

#endif
