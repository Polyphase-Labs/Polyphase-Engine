#pragma once

#if EDITOR

class ControllerServer;

// app is a crow::SimpleApp* — crow types are hidden from headers to avoid WinSock conflicts.
void RegisterRoutes(void* app, ControllerServer* server);

// Used at the top of every route handler so requests arriving while the editor
// is tearing down get a 503 instead of parking a Crow worker on a future that
// will never be ticked. Keeps the diff in ControllerServerRoutes.cpp readable.
#define RETURN_IF_SHUTTING_DOWN(serverPtr)                                      \
    do {                                                                        \
        if ((serverPtr) && (serverPtr)->IsShuttingDown())                       \
            return crow::response(503, "application/json",                      \
                                  R"({"error":"shutting_down"})");              \
    } while (0)

#endif
