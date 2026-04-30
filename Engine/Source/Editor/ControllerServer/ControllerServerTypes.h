#pragma once

#if EDITOR

#include <chrono>
#include <functional>
#include <future>
#include <string>

struct ControllerCommand
{
    std::function<std::string()> mFunction;
    std::promise<std::string> mPromise;
};

// Bounded wait for a queued command. Returns the result, or a JSON error body
// if the wait expires. Used by every route handler so a wedged or no-longer-
// ticking main thread never strands a Crow worker forever (see ControllerServer
// shutdown notes).
inline std::string WaitForCommand(std::future<std::string>& f, int timeoutMs = 2000)
{
    if (f.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready)
        return f.get();
    return R"({"error":"timeout","detail":"command did not complete in time"})";
}

#endif
