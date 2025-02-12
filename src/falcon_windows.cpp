#ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
#elif _WIN32_WINNT < 0x0600
    #undef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <fmt/core.h>

#pragma comment(lib, "Ws2_32.lib")

#include "falcon.h"

struct WinSockInitializer
{
    WinSockInitializer()
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            printf("WSAStartup failed with error: %d\n", WSAGetLastError());
        }
    }

    ~WinSockInitializer()
    {
        WSACleanup();
    }
};

std::string IpToString(const sockaddr* sa)
{
    switch(sa->sa_family)
    {
        case AF_INET: {
            char ip[INET_ADDRSTRLEN + 6];
            const char* ret = inet_ntop(AF_INET,
                &reinterpret_cast<const sockaddr_in*>(sa)->sin_addr,
                ip,
                INET_ADDRSTRLEN);
            return fmt::format("{}:{}", ret, ntohs(reinterpret_cast<const sockaddr_in*>(sa)->sin_port));
        }
        case AF_INET6: {
            char ip[INET6_ADDRSTRLEN + 8];
            const char* ret = inet_ntop(AF_INET6,
                &reinterpret_cast<const sockaddr_in6*>(sa)->sin6_addr,
                ip+ 1,
                INET6_ADDRSTRLEN);
            return fmt::format("[{}]:{}", ret, ntohs(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_port));
        }
    }

    return "";
}

sockaddr StringToIp(const std::string& ip, uint16_t port)
{
    sockaddr result {};
    int error = inet_pton(AF_INET, ip.c_str(), &reinterpret_cast<sockaddr_in*>(&result)->sin_addr);
    if (error == 1) {
        result.sa_family = AF_INET;
        reinterpret_cast<sockaddr_in*>(&result)->sin_port = htons(port);
        return result;
    }

    memset(&result, 0, sizeof(result));
    error = inet_pton(AF_INET6, ip.c_str(), &reinterpret_cast<sockaddr_in6*>(&result)->sin6_addr);
    if (error == 1) {
        result.sa_family = AF_INET6;
        reinterpret_cast<sockaddr_in6*>(&result)->sin6_port = htons(port);
        return result;
    }
    memset(&result, 0, sizeof(result));
    return result;
}

Falcon::Falcon()
{
    static WinSockInitializer winsockInitializer{};
}

Falcon::~Falcon() {
    if(m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
    }
}

std::unique_ptr<Falcon> Falcon::Listen(const std::string& endpoint, uint16_t port)
{
    sockaddr local_endpoint = StringToIp(endpoint, port);
    auto falcon = std::make_unique<Falcon>();
    falcon->m_socket = socket(local_endpoint.sa_family,
        SOCK_DGRAM,
        IPPROTO_UDP);
    if (int error = bind(falcon->m_socket, &local_endpoint, sizeof(local_endpoint)); error != 0)
    {
        closesocket(falcon->m_socket);
        return nullptr;
    }

    return falcon;
}

std::unique_ptr<Falcon> Falcon::Connect(const std::string& serverIp, uint16_t port)
{
    sockaddr local_endpoint = StringToIp(serverIp, port);
    auto falcon = std::make_unique<Falcon>();
    falcon->m_socket = socket(local_endpoint.sa_family,
        SOCK_DGRAM,
        IPPROTO_UDP);
    if (int error = bind(falcon->m_socket, &local_endpoint, sizeof(local_endpoint)); error != 0)
    {
        closesocket(falcon->m_socket);
        return nullptr;
    }

    return falcon;
}

int Falcon::SendToInternal(const std::string &to, uint16_t port, std::span<const char> message)
{
    const sockaddr destination = StringToIp(to, port);
    int error = sendto(m_socket,
        message.data(),
        message.size(),
        0,
        &destination,
        sizeof(destination));
    return error;
}

int Falcon::ReceiveFromInternal(std::string &from, std::span<char, 65535> message)
{
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len = sizeof(struct sockaddr_storage);
    const int read_bytes = recvfrom(m_socket,
        message.data(),
        message.size_bytes(),
        0,
        reinterpret_cast<sockaddr*>(&peer_addr),
        &peer_addr_len);

    from = IpToString(reinterpret_cast<const sockaddr*>(&peer_addr));

    return read_bytes;
}