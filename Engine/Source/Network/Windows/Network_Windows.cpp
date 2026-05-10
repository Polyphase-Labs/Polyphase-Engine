#if PLATFORM_WINDOWS

// Include these guys before <Windows.h> or else you get redefinition errors.
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>

#include "Network/Network.h"
#include "Assertion.h"

#include "Log.h"

static bool sActive = false;

void NET_Initialize()
{
    WSADATA wsadata;

    int32_t error = WSAStartup(0x0202, &wsadata);

    //Did something happen?
    if (error)
    {
        LogError("Error initializing winsock library.");
        OCT_ASSERT(0);
        sActive = false;
        return;
    }

    //Did we get the right Winsock version?
    if (wsadata.wVersion != 0x0202)
    {
        WSACleanup(); //Clean up Winsock
        LogError("Could not init correct winsock version.");
        OCT_ASSERT(0);
        sActive = false;
        return;
    }

    sActive = true;
}

void NET_Shutdown()
{
    WSACleanup();
}

void NET_Update()
{

}

bool NET_IsActive()
{
    return true;
}

SocketHandle NET_SocketCreate()
{
    return socket(AF_INET, SOCK_DGRAM, 0);
}

SocketHandle NET_SocketCreateStream()
{
    return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

bool NET_SocketConnect(SocketHandle socketHandle, uint32_t ipAddr, uint16_t port, int32_t timeoutMs)
{
    if (socketHandle == INVALID_SOCKET) return false;

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(ipAddr);
    addr.sin_port = htons(port);

    // Switch to non-blocking, issue connect, then select() with timeout, then
    // restore blocking. This avoids both the indefinite-block and the
    // platform-specific SO_RCVTIMEO/SO_SNDTIMEO nuances.
    unsigned long nb = 1;
    ioctlsocket(socketHandle, FIONBIO, &nb);

    int rc = connect(socketHandle, (const struct sockaddr*)&addr, sizeof(addr));
    bool connected = false;
    if (rc == 0)
    {
        connected = true;
    }
    else
    {
        const int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS)
        {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(socketHandle, &wfds);
            timeval tv;
            tv.tv_sec  = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;
            int sel = select(0, nullptr, &wfds, nullptr, &tv);
            if (sel > 0 && FD_ISSET(socketHandle, &wfds))
            {
                int soErr = 0;
                int soErrLen = sizeof(soErr);
                getsockopt(socketHandle, SOL_SOCKET, SO_ERROR, (char*)&soErr, &soErrLen);
                connected = (soErr == 0);
            }
        }
    }

    nb = 0;
    ioctlsocket(socketHandle, FIONBIO, &nb);
    return connected;
}

int32_t NET_SocketSend(SocketHandle socketHandle, const char* buffer, uint32_t size)
{
    return send(socketHandle, buffer, (int)size, 0);
}

uint32_t NET_ResolveHost(const char* hostname)
{
    if (hostname == nullptr || *hostname == '\0') return 0;

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &res) != 0 || res == nullptr)
    {
        return 0;
    }

    uint32_t ip = 0;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next)
    {
        if (p->ai_family == AF_INET && p->ai_addr != nullptr)
        {
            const struct sockaddr_in* sa = (const struct sockaddr_in*)p->ai_addr;
            ip = ntohl(sa->sin_addr.s_addr);
            break;
        }
    }
    freeaddrinfo(res);
    return ip;
}

void NET_SocketBind(SocketHandle socketHandle, uint32_t ipAddr, uint16_t port)
{
    struct sockaddr_in bindAddr;
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(ipAddr);
    bindAddr.sin_port = htons(port);

    if (bind(socketHandle, (const struct sockaddr *) &bindAddr, sizeof(bindAddr)) < 0 )
    {
        LogError("Failed to bind socket");
    }
}

int32_t NET_SocketRecv(SocketHandle socketHandle, char* buffer, uint32_t size)
{
    return recv(socketHandle, buffer, size, 0);
}

int32_t NET_SocketRecvFrom(SocketHandle socketHandle, char* buffer, uint32_t size, uint32_t& addr, uint16_t& port)
{
    struct sockaddr_in fromAddr;
    int32_t fromAddrLen = (int32_t) sizeof(fromAddr);
    int32_t numBytes = recvfrom(socketHandle, buffer, int32_t(size), 0, (struct sockaddr*) &fromAddr, &fromAddrLen);
    addr = ntohl(fromAddr.sin_addr.s_addr);
    port = ntohs(fromAddr.sin_port);
    return numBytes;
}

int32_t NET_SocketSendTo(SocketHandle socketHandle, const char* buffer, uint32_t size, uint32_t addr, uint16_t port)
{
    struct sockaddr_in toAddr;
    toAddr.sin_family = AF_INET;
    toAddr.sin_addr.s_addr = htonl(addr);
    toAddr.sin_port = htons(port);
    int32_t bytesSent = sendto(socketHandle, buffer, size, 0, (const struct sockaddr*) &toAddr, (uint32_t) sizeof(toAddr));
    return bytesSent;
}

void NET_SocketClose(SocketHandle socketHandle)
{
    closesocket(socketHandle);
}

void NET_SocketSetBlocking(SocketHandle socketHandle, bool blocking)
{
    unsigned long flag = !blocking;
    ioctlsocket(socketHandle, FIONBIO, &flag);
}

void NET_SocketSetBroadcast(SocketHandle socketHandle, bool broadcast)
{
    char broadcastEnable = (char) broadcast;
    int32_t result = setsockopt(socketHandle, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    if (result != 0)
    {
        LogError("Failed to set Broadcast flag on socket.");
    }
}

void NET_SocketGetIpAndPort(SocketHandle socketHandle, uint32_t& ipAddr, uint16_t& port)
{
    struct sockaddr_in localAddr = {};
    int32_t len = sizeof(localAddr);
    getsockname(socketHandle, (struct sockaddr *) &localAddr, &len);
    ipAddr = ntohl(localAddr.sin_addr.s_addr);
    port = ntohs(localAddr.sin_port);
}

uint32_t NET_IpStringToUint32(const char* ipString)
{
    uint32_t retAddr = 0;
    struct in_addr addr = {};
    inet_pton(AF_INET, ipString, &addr);
    retAddr = ntohl(addr.s_addr);
    return retAddr;
}

void NET_IpUint32ToString(uint32_t ipUint32, char* outIpString)
{
    struct sockaddr_in sa = {};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(ipUint32);
    sa.sin_port = 0;

    inet_ntop(AF_INET, &sa.sin_addr, outIpString, INET_ADDRSTRLEN);
}

uint32_t NET_GetIpAddress()
{
    uint32_t retIp = 0;
    
    PIP_ADAPTER_INFO adapterInfo = nullptr;
    PIP_ADAPTER_INFO adapter = nullptr;
    unsigned long adapterInfoLen = 0;

    adapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
    adapterInfoLen = sizeof(IP_ADAPTER_INFO);

    if (GetAdaptersInfo(adapterInfo, &adapterInfoLen) == ERROR_BUFFER_OVERFLOW)\
    {
        free(adapterInfo);
        adapterInfo = (IP_ADAPTER_INFO*)malloc(adapterInfoLen);
    }

    if (GetAdaptersInfo(adapterInfo, &adapterInfoLen) == NO_ERROR)
    {
        adapter = adapterInfo;

        while (adapter != nullptr)
        {
            IP_ADDR_STRING* addrString = &adapter->IpAddressList;

            while (addrString != nullptr)
            {
                uint32_t ipAddr = NET_IpStringToUint32(addrString->IpAddress.String);

                if ((ipAddr & 0x7f000000) != 0x7f000000)
                {
                    retIp = ipAddr;
                    break;
                }

                addrString = addrString->Next;
            }

            adapter = adapter->Next;
        }
    }
    else
    {
        LogError("Failed to get ip address.");
        OCT_ASSERT(0);
    }

    free(adapterInfo);
    adapterInfo = nullptr;

    return retIp;
}

uint32_t NET_GetSubnetMask()
{
    uint32_t retMask = 0;

    PIP_ADAPTER_INFO adapterInfo = nullptr;
    PIP_ADAPTER_INFO adapter = nullptr;
    unsigned long adapterInfoLen = 0;

    adapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
    adapterInfoLen = sizeof(IP_ADAPTER_INFO);

    if (GetAdaptersInfo(adapterInfo, &adapterInfoLen) == ERROR_BUFFER_OVERFLOW)\
    {
        free(adapterInfo);
        adapterInfo = (IP_ADAPTER_INFO*)malloc(adapterInfoLen);
    }

    if (GetAdaptersInfo(adapterInfo, &adapterInfoLen) == NO_ERROR)
    {
        adapter = adapterInfo;

        while (adapter != nullptr)
        {
            IP_ADDR_STRING* addrString = &adapter->IpAddressList;

            while (addrString != nullptr)
            {
                uint32_t ipAddr = NET_IpStringToUint32(addrString->IpAddress.String);
                uint32_t mask = NET_IpStringToUint32(addrString->IpMask.String);
                if ((ipAddr & 0x7f000000) != 0x7f000000)
                {
                    retMask = mask;
                    break;
                }

                addrString = addrString->Next;
            }

            adapter = adapter->Next;
        }
    }
    else
    {
        LogError("Failed to get ip address.");
        OCT_ASSERT(0);
    }

    free(adapterInfo);
    adapterInfo = nullptr;

    return retMask;
}

#endif
