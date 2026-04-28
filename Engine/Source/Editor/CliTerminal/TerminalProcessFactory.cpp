#if EDITOR

#include "ITerminalProcess.h"

#if PLATFORM_WINDOWS
#include "TerminalProcess_Windows.h"
#include "TerminalProcess_WindowsConPTY.h"
#elif PLATFORM_LINUX
#include "TerminalProcess_Posix.h"
#include "TerminalProcess_LinuxPty.h"
#endif

ITerminalProcess* CreateTerminalProcess()
{
#if PLATFORM_WINDOWS
    return new TerminalProcess_Windows();
#elif PLATFORM_LINUX
    return new TerminalProcess_Posix();
#else
    return nullptr;
#endif
}

ITerminalProcess* CreateTerminalProcessConPTY()
{
    // Returns the TTY-capable backend for the current platform:
    //   - Windows: ConPTY-based (CreatePseudoConsole)
    //   - Linux:   forkpty-based
    // Returns nullptr if no TTY backend is available so the caller can fall
    // back to the pipe-based backend via CreateTerminalProcess().
#if PLATFORM_WINDOWS
    if (!TerminalProcess_WindowsConPTY::IsConPtyAvailable())
    {
        return nullptr;
    }
    return new TerminalProcess_WindowsConPTY();
#elif PLATFORM_LINUX
    return new TerminalProcess_LinuxPty();
#else
    return nullptr;
#endif
}

#endif
