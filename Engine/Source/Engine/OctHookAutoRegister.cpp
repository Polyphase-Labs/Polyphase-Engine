// W1: Auto-registration of game Oct* hooks for static-lib builds.
//
// The engine routes its hook calls through the OctGameHooks function-pointer
// struct (see Engine.h). For DLL builds the consuming exe registers its own
// hooks at startup. For static-lib builds the existing extern model needs to
// keep working with zero source change in game projects — so this TU takes
// the address of the six Oct* externs at static-init time and forwards them
// into RegisterOctHooks().
//
// The whole file is excluded when building the engine DLL
// (POLYPHASE_DLL_BUILD=1, set only in Engine.vcxproj's Shared configs). In
// the DLL there are no exe-side Oct* externs to take the address of.

#include "Engine.h"
#include "Utilities.h"

// W1: Force-link symbol — referenced from Engine.cpp's ForceLinkage() so this
// translation unit is pulled into the final executable. Without this, the MSVC
// linker would garbage-collect the TU (its only other symbols are namespace-
// scoped static initializers with no external references), the static init
// below would never run, and the engine's GetOctHooks() would return an empty
// struct — leaving Standalone/Main.cpp's OctPreInitialize unreachable and
// `engineState->mStandalone` stuck at false.
FORCE_LINK_DEF(OctHookAutoRegister);

#if !POLYPHASE_DLL_BUILD

namespace
{
    struct OctHooksAutoRegister
    {
        OctHooksAutoRegister()
        {
            OctGameHooks h;
            h.preInitialize  = &OctPreInitialize;
            h.postInitialize = &OctPostInitialize;
            h.preUpdate      = &OctPreUpdate;
            h.postUpdate     = &OctPostUpdate;
            h.preShutdown    = &OctPreShutdown;
            h.postShutdown   = &OctPostShutdown;
            RegisterOctHooks(h);
        }
    } sOctHooksAutoRegister;
}

#endif  // !POLYPHASE_DLL_BUILD
