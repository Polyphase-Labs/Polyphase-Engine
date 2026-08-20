#pragma once

#include "Network/NetworkTypes.h"

void NET_Initialize();
void NET_Shutdown();
void NET_Update();

bool NET_IsActive();

SocketHandle NET_SocketCreate();
void NET_SocketBind(SocketHandle socketHandle, uint32_t ipAddr, uint16_t port);
int32_t NET_SocketRecv(SocketHandle socketHandle, char* buffer, uint32_t size);
int32_t NET_SocketRecvFrom(SocketHandle socketHandle, char* buffer, uint32_t size, uint32_t& addr, uint16_t& port);
int32_t NET_SocketSendTo(SocketHandle socketHandle, const char* buffer, uint32_t size, uint32_t addr, uint16_t port);
void NET_SocketClose(SocketHandle socketHandle);
void NET_SocketSetBlocking(SocketHandle socketHandle, bool blocking);
void NET_SocketSetBroadcast(SocketHandle socketHandle, bool broadcast);
void NET_SocketGetIpAndPort(SocketHandle socketHandle, uint32_t& ipAddr, uint16_t& port);

// ---------------------------------------------------------------------------
// TCP / stream-oriented primitives. Used by HTTP/HTTPS, websockets, and any
// future protocol that needs reliable streaming. UDP-only platforms can leave
// these as no-op stubs that return failure.
// ---------------------------------------------------------------------------
SocketHandle NET_SocketCreateStream();
bool         NET_SocketConnect(SocketHandle socketHandle, uint32_t ipAddr, uint16_t port, int32_t timeoutMs);
int32_t      NET_SocketSend(SocketHandle socketHandle, const char* buffer, uint32_t size);

// Begin a non-blocking connect on a stream socket. Returns false on immediate
// failure. Poll with NET_SocketConnectPoll each frame. The socket is left
// non-blocking afterwards, which is what a per-frame pump (WebSocket) wants -
// NET_SocketConnect above is the blocking-with-timeout variant and restores
// blocking mode, so worker-thread callers should keep using that one.
bool         NET_SocketConnectAsync(SocketHandle socketHandle, uint32_t ipAddr, uint16_t port);

// -1 = failed, 0 = still connecting, 1 = connected.
int32_t      NET_SocketConnectPoll(SocketHandle socketHandle);

// Classify a negative NET_SocketSend / NET_SocketRecv result. A non-blocking
// stream socket returns a negative count both when it is merely out of data
// and when the connection is dead, and there is no portable way to tell those
// apart from the count alone. Pass the value the call returned - libogc
// reports the error in the return value while the BSD platforms use errno /
// WSAGetLastError. True means "still healthy, just nothing to do".
bool         NET_SocketWouldBlock(SocketHandle socketHandle, int32_t opResult);

// Synchronous DNS resolve. Returns 0 on failure. Hostname may be a literal IP.
// NOTE: this blocks. Callers that pump per frame should either connect at a
// moment where a stall is acceptable (menu / level load) or pass a numeric IP,
// which bypasses the resolver on every platform.
uint32_t     NET_ResolveHost(const char* hostname);

uint32_t NET_IpStringToUint32(const char* ipString);
void NET_IpUint32ToString(uint32_t ipUint32, char* outIpString);

uint32_t NET_GetIpAddress();
uint32_t NET_GetSubnetMask();

// TODO: Update LAN servers on NET_Update()
//void NET_GetLanServers();
