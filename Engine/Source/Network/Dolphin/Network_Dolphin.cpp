#if PLATFORM_DOLPHIN

#include "Network/Network.h"

#include "Log.h"

#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <ogcsys.h>
#include <gccore.h>
#include <network.h>

// Wii and GameCube build against two DIFFERENT libogc trees (Engine/Makefile_Wii
// uses devkitPPC/wii_rules -> libogc; Engine/Makefile_GCN uses
// devkitPro/libogc2/gamecube_rules -> libogc2), and their network.h files are not
// interchangeable:
//   - libogc's (Wii) network.h does NOT define the POLL* event flags; they only
//     exist in the separate <poll.h>, which the Wii tree ships.
//   - libogc2's (GameCube) network.h defines POLLOUT/POLLERR/etc. itself, and
//     that tree does not ship a <poll.h> at all - including it is a hard
//     compile failure on GCN.
#if PLATFORM_WII
#include <poll.h>
#endif

static uint32_t sLocalIp = 0;
static uint32_t sGateway = 0;
static uint32_t sSubnetMask = 0;

static bool sActive = false;

void NET_Initialize()
{
    struct in_addr localIp, netMask, gateway;

    // Different signature per libogc tree (see the network.h comment above):
    // libogc's (Wii) if_configex takes a trailing max_retries; libogc2's
    // (GameCube) does not.
#if PLATFORM_WII
    int32_t result = if_configex(&localIp, &netMask, &gateway, true, 1);
#else
    int32_t result = if_configex(&localIp, &netMask, &gateway, true);
#endif

    if (result >= 0)
    {
        sLocalIp = ntohl(localIp.s_addr);
        sSubnetMask = ntohl(netMask.s_addr);
        sGateway = ntohl(gateway.s_addr);

        sActive = true;

        // Log the interface config. Without it, "connect failed" is
        // indistinguishable between a wrong address, a subnet the target isn't
        // on, and a missing default gateway -- and a missing gateway fails
        // instantly rather than timing out, so it looks nothing like a network
        // problem from the caller's side.
        char ipString[32] = {};
        char maskString[32] = {};
        char gatewayString[32] = {};
        NET_IpUint32ToString(sLocalIp, ipString);
        NET_IpUint32ToString(sSubnetMask, maskString);
        NET_IpUint32ToString(sGateway, gatewayString);

        LogDebug("Network up: ip=%s mask=%s gateway=%s", ipString, maskString, gatewayString);

        if (sGateway == 0)
        {
            LogWarning("No default gateway. Only hosts on this subnet are reachable; "
                       "anything off-subnet will fail to connect immediately.");
        }
    }
    else
    {
        LogError("Failed to initialize Network");
        sActive = false;
    }
}

void NET_Shutdown()
{

}

void NET_Update()
{

}

bool NET_IsActive()
{
    return sActive;
}

// Two libogc quirks, both of which turn into silent failures much further
// downstream:
//
//  - IOS wants IPPROTO_IP (0) as the protocol for BOTH socket types. libogc's
//    own TCP sample creates its stream socket with socket(AF_INET, SOCK_STREAM,
//    IPPROTO_IP); passing the BSD-conventional IPPROTO_TCP gets rejected.
//  - net_socket reports failure as a negated errno, not as -1. The engine's
//    callers test `== NET_INVALID_SOCKET`, so an error code like -22 sails
//    through as a "valid" handle and every later call on it fails for no
//    visible reason. Normalise to NET_INVALID_SOCKET here and say what broke.
static SocketHandle CreateDolphinSocket(u32 type, const char* what)
{
    const s32 sock = net_socket(AF_INET, type, IPPROTO_IP);

    if (sock < 0)
    {
        LogError("net_socket(%s) failed: rc=%d (errno %d)", what, (int)sock, (int)(-sock));
        // -1, matching NET_INVALID_SOCKET (NetworkConstants.h) and what every
        // caller in this file already tests SocketHandle against.
        return SocketHandle(-1);
    }

    return SocketHandle(sock);
}

SocketHandle NET_SocketCreate()
{
    return CreateDolphinSocket(SOCK_DGRAM, "udp");
}

SocketHandle NET_SocketCreateStream()
{
    return CreateDolphinSocket(SOCK_STREAM, "tcp");
}

// Wii sockets live in IOS, and IOS connects are asynchronous: net_connect
// hands back -EINPROGRESS immediately and completion is observed separately
// with net_poll. libogc's net_* report errors as a negated errno in the return
// value rather than through errno itself, so the obvious `rc == 0` test reads
// a perfectly healthy in-progress connect as a hard failure -- which is why
// every TCP connect on this platform used to fail a few milliseconds after it
// started, looking nothing like the timeout a real unreachable host produces.
#define NET_DOLPHIN_CONNECT_TIMEOUT_MS 15000

// libogc's headers (lwip/arch.h) list EINPROGRESS=115, EALREADY=114 - lwIP's
// own internal numbering. That is NOT what the compiled net_connect actually
// returns: confirmed on-device, an in-progress connect comes back as -119,
// which is newlib <errno.h>'s EINPROGRESS. Whatever translation happens inside
// libogc's net.c, callers observe newlib numbers, so use <errno.h> directly
// rather than the lwIP header's constants.
static bool IsConnectInProgress(int32_t rc)
{
    const int32_t err = (rc < 0) ? -rc : 0;

    return err == EINPROGRESS ||
           err == EALREADY    ||
           err == EAGAIN      ||
           err == EISCONN;
}

bool NET_SocketConnectAsync(SocketHandle socketHandle, uint32_t ipAddr, uint16_t port)
{
    if (socketHandle < 0) return false;

    // Not every IOS revision honours ioctl(FIONBIO) on its sockets, so keep the
    // result: if this failed, the connect below may still be running blocking
    // and the return code needs reading in that light.
    int32_t nonBlocking = 1;
    const int32_t ioctlRc = net_ioctl(socketHandle, FIONBIO, &nonBlocking);

    struct sockaddr_in addr = {};
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(ipAddr);
    addr.sin_port = htons(port);

    const int32_t rc = net_connect(socketHandle, (struct sockaddr*)&addr, sizeof(addr));

    if (rc == 0 || IsConnectInProgress(rc))
    {
        return true;
    }

    // libogc reports failures as a negated errno in the return value. Print it
    // raw: "connect failed" on its own says nothing about whether IOS refused
    // the socket, the address, or the route.
    LogDebug("net_connect failed: rc=%d (errno %d), FIONBIO rc=%d",
        (int)rc, (int)((rc < 0) ? -rc : 0), (int)ioctlRc);

    return false;
}

#if PLATFORM_WII

int32_t NET_SocketConnectPoll(SocketHandle socketHandle)
{
    if (socketHandle < 0) return -1;

    struct pollsd sd = {};
    sd.socket = socketHandle;
    sd.events = POLLOUT;

    const int32_t rc = net_poll(&sd, 1, 0);
    if (rc < 0)  return -1;
    if (rc == 0) return 0;

    if (sd.revents & (POLLERR | POLLHUP | POLLNVAL))
    {
        return -1;
    }

    if (sd.revents & POLLOUT)
    {
        // Writable can still mean "connect finished, and it failed".
        int32_t soErr = 0;
        socklen_t soErrLen = sizeof(soErr);
        if (net_getsockopt(socketHandle, SOL_SOCKET, SO_ERROR, &soErr, &soErrLen) == 0 &&
            soErr != 0)
        {
            return -1;
        }

        return 1;
    }

    return 0;
}

#else  // GameCube

// libogc2's GameCube network.h DECLARES net_poll, but the BBA driver
// (libbba.a) never implements it - linking anything that calls it fails with
// "undefined reference to net_poll". net_select is what the driver actually
// ships, so GameCube polls a writability check through that instead. Same
// completion semantics as the Wii POLLOUT + SO_ERROR path above.
int32_t NET_SocketConnectPoll(SocketHandle socketHandle)
{
    if (socketHandle < 0) return -1;

    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(socketHandle, &writeSet);

    struct timeval timeout = {};

    const int32_t rc = net_select(socketHandle + 1, nullptr, &writeSet, nullptr, &timeout);
    if (rc < 0)  return -1;
    if (rc == 0) return 0;

    if (!FD_ISSET(socketHandle, &writeSet))
    {
        return 0;
    }

    // Writable can still mean "connect finished, and it failed".
    int32_t soErr = 0;
    socklen_t soErrLen = sizeof(soErr);
    if (net_getsockopt(socketHandle, SOL_SOCKET, SO_ERROR, &soErr, &soErrLen) == 0 &&
        soErr != 0)
    {
        return -1;
    }

    return 1;
}

#endif  // PLATFORM_WII

bool NET_SocketConnect(SocketHandle socketHandle, uint32_t ipAddr, uint16_t port, int32_t timeoutMs)
{
    if (socketHandle < 0) return false;

    if (!NET_SocketConnectAsync(socketHandle, ipAddr, port))
    {
        return false;
    }

    // Blocking-with-timeout contract, matching the desktop platforms: spin the
    // async poll until it resolves, then hand the socket back in blocking mode
    // the way callers like HttpBackend_Dolphin expect it.
    const int32_t waitMs = (timeoutMs > 0) ? timeoutMs : NET_DOLPHIN_CONNECT_TIMEOUT_MS;
    int32_t elapsedMs = 0;
    int32_t poll = 0;

    while (elapsedMs < waitMs)
    {
        poll = NET_SocketConnectPoll(socketHandle);
        if (poll != 0)
        {
            break;
        }

        usleep(1000);
        elapsedMs += 1;
    }

    NET_SocketSetBlocking(socketHandle, true);
    return poll == 1;
}

bool NET_SocketWouldBlock(SocketHandle, int32_t opResult)
{
    // Negated errno in the return value, same as the connect path above.
    // Confirmed on-device to be newlib's numbering (see IsConnectInProgress).
    const int32_t err = (opResult < 0) ? -opResult : 0;
    return err == EAGAIN || err == EINTR;
}

int32_t NET_SocketSend(SocketHandle socketHandle, const char* buffer, uint32_t size)
{
    return net_send(socketHandle, buffer, size, 0);
}

uint32_t NET_ResolveHost(const char* hostname)
{
    if (hostname == nullptr || *hostname == '\0') return 0;

    // Literal-IP fast path: works on every PLATFORM_DOLPHIN target via
    // inet_aton. libogc's gethostbyname can also be flaky on dotted-quad
    // inputs depending on stack state, so we prefer this even on Wii.
    struct in_addr litAddr = {};
    if (inet_aton(hostname, &litAddr) != 0)
    {
        return ntohl(litAddr.s_addr);
    }

#if PLATFORM_WII
    // DNS lookup is only available on the Wii — GameCube's BBA driver
    // (-lbba) doesn't ship the higher-level resolver. GCN callers must
    // pass a literal IPv4 address.
    struct hostent* he = net_gethostbyname((char*)hostname);
    if (he == nullptr || he->h_addr_list == nullptr || he->h_addr_list[0] == nullptr)
    {
        return 0;
    }

    uint32_t ip = 0;
    memcpy(&ip, he->h_addr_list[0], sizeof(ip));
    return ntohl(ip);
#else
    // GameCube: no DNS. Caller must pre-resolve.
    return 0;
#endif
}

void NET_SocketBind(SocketHandle socketHandle, uint32_t ipAddr, uint16_t port)
{
    struct sockaddr_in bindAddr;
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(ipAddr);
    bindAddr.sin_port = htons(port);

    if (net_bind(socketHandle, (struct sockaddr *) &bindAddr, sizeof(bindAddr)) < 0 )
    {
        LogError("Failed to bind socket");
    }
}

int32_t NET_SocketRecv(SocketHandle socketHandle, char* buffer, uint32_t size)
{
    return net_recv(socketHandle, buffer, size, 0);
}

int32_t NET_SocketRecvFrom(SocketHandle socketHandle, char* buffer, uint32_t size, uint32_t& addr, uint16_t& port)
{
    struct sockaddr_in fromAddr;
    uint32_t fromAddrLen = (uint32_t) sizeof(fromAddr);
    int32_t numBytes = net_recvfrom(socketHandle, buffer, size, 0, (struct sockaddr*) &fromAddr, &fromAddrLen);
    addr = ntohl(fromAddr.sin_addr.s_addr);
    port = ntohs(fromAddr.sin_port);
    return numBytes;
}

int32_t NET_SocketSendTo(SocketHandle socketHandle, const char* buffer, uint32_t size, uint32_t addr, uint16_t port)
{
    // I can't send messages on Wii unless toAddr.sin_len = 8 is set and passed in as the socket addr length in net_sendto.
    // I don't know why but check out this devkitpro forum post: https://devkitpro.org/viewtopic.php?f=3&t=2177&p=5509&hilit=udp#p5509
    // sendto was returning -22 otherwise.
    struct sockaddr_in toAddr;
    toAddr.sin_family = AF_INET;
    toAddr.sin_addr.s_addr = htonl(addr);
    toAddr.sin_port = htons(port);
    toAddr.sin_len = 8;
    int32_t bytesSent = net_sendto(socketHandle, buffer, size, 0, (struct sockaddr*) &toAddr, toAddr.sin_len);
    
    //if (bytesSent < 0)
    //{
    //    LogError("Sock: %d, BytesSent = %d", socketHandle, bytesSent);
    //    LogWarning("LocalIP = %08x", sLocalIp);
    //}
    
    return bytesSent;
}

void NET_SocketClose(SocketHandle socketHandle)
{
    net_close(socketHandle);
}

void NET_SocketSetBlocking(SocketHandle socketHandle, bool blocking)
{
    int32_t flag = !blocking;
    net_ioctl(socketHandle, FIONBIO, &flag);
}

void NET_SocketSetBroadcast(SocketHandle socketHandle, bool broadcast)
{
    int broadcastEnable = (int) broadcast;
    int32_t result = net_setsockopt(socketHandle, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    if (result != 0)
    {
        LogError("Failed to set Broadcast flag on socket.");
    }
}

void NET_SocketGetIpAndPort(SocketHandle socketHandle, uint32_t& ipAddr, uint16_t& port)
{
    struct sockaddr_in localAddr = {};
    socklen_t len = sizeof(localAddr);
    net_getsockname(socketHandle, (struct sockaddr *) &localAddr, &len);
    ipAddr = ntohl(localAddr.sin_addr.s_addr);
    port = ntohs(localAddr.sin_port);
}

uint32_t NET_IpStringToUint32(const char* ipString)
{
    uint32_t retAddr = 0;
    struct in_addr addr = {};
    inet_aton(ipString, &addr);
    retAddr = ntohl(addr.s_addr);
    return retAddr;
}

void NET_IpUint32ToString(uint32_t ipUint32, char* outIpString)
{
    struct sockaddr_in sa = {};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(ipUint32);
    sa.sin_port = 0;

    char* staticString = inet_ntoa(sa.sin_addr);

    if (staticString != nullptr)
    {
        strncpy(outIpString, staticString, 16);
        outIpString[15] = 0;
    }
}

uint32_t NET_GetIpAddress()
{
    return sLocalIp;
}

uint32_t NET_GetSubnetMask()
{
    return sSubnetMask;
}

#endif
