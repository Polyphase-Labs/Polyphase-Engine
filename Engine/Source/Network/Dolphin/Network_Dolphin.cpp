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

static uint32_t sLocalIp = 0;
static uint32_t sGateway = 0;
static uint32_t sSubnetMask = 0;

static bool sActive = false;

void NET_Initialize()
{
    struct in_addr localIp, netMask, gateway;
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

SocketHandle NET_SocketCreate()
{
    return net_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
}

SocketHandle NET_SocketCreateStream()
{
    return net_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

bool NET_SocketConnect(SocketHandle socketHandle, uint32_t ipAddr, uint16_t port, int32_t /*timeoutMs*/)
{
    if (socketHandle < 0) return false;

    // libogc's net_connect blocks. Without a portable non-blocking-connect
    // path on devkitPPC the simplest correct behaviour is to honour the kernel
    // default and ignore the caller-provided timeout.
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(ipAddr);
    addr.sin_port = htons(port);

    int32_t rc = net_connect(socketHandle, (struct sockaddr*)&addr, sizeof(addr));
    return rc == 0;
}

// libogc's net_connect always blocks and devkitPPC has no portable
// non-blocking connect for it. The sanctioned fallback (documented in
// Network.h) is to run the blocking connect inside Async and have Poll hand
// back the stored result - a per-frame pump still works, it just eats one
// connect stall. Slots are freed by NET_SocketClose.
#define NET_DOLPHIN_MAX_PENDING_CONNECTS 8

struct DolphinConnectResult
{
    SocketHandle mSocket = -1;
    int32_t      mResult = 0;
};

static DolphinConnectResult sConnectResults[NET_DOLPHIN_MAX_PENDING_CONNECTS];

static DolphinConnectResult* FindDolphinConnectSlot(SocketHandle socketHandle, bool allocate)
{
    for (int32_t i = 0; i < NET_DOLPHIN_MAX_PENDING_CONNECTS; ++i)
    {
        if (sConnectResults[i].mSocket == socketHandle)
        {
            return &sConnectResults[i];
        }
    }

    if (!allocate)
    {
        return nullptr;
    }

    for (int32_t i = 0; i < NET_DOLPHIN_MAX_PENDING_CONNECTS; ++i)
    {
        if (sConnectResults[i].mSocket < 0)
        {
            sConnectResults[i].mSocket = socketHandle;
            sConnectResults[i].mResult = 0;
            return &sConnectResults[i];
        }
    }

    return nullptr;
}

bool NET_SocketConnectAsync(SocketHandle socketHandle, uint32_t ipAddr, uint16_t port)
{
    if (socketHandle < 0) return false;

    DolphinConnectResult* slot = FindDolphinConnectSlot(socketHandle, true);
    if (slot == nullptr)
    {
        LogError("Too many pending stream connects");
        return false;
    }

    const bool connected = NET_SocketConnect(socketHandle, ipAddr, port, 0);
    slot->mResult = connected ? 1 : -1;

    if (connected)
    {
        // Leave the socket non-blocking, matching the other platforms.
        NET_SocketSetBlocking(socketHandle, false);
    }

    return connected;
}

int32_t NET_SocketConnectPoll(SocketHandle socketHandle)
{
    DolphinConnectResult* slot = FindDolphinConnectSlot(socketHandle, false);
    return slot != nullptr ? slot->mResult : -1;
}

bool NET_SocketWouldBlock(SocketHandle, int32_t opResult)
{
    // libogc's net_* return the negated errno rather than setting errno.
    const int32_t err = (opResult < 0) ? -opResult : 0;
    return err == EAGAIN || err == EWOULDBLOCK || err == EINTR;
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
    DolphinConnectResult* slot = FindDolphinConnectSlot(socketHandle, false);
    if (slot != nullptr)
    {
        slot->mSocket = -1;
        slot->mResult = 0;
    }

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
