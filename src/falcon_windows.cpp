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

#include <iostream>
#include <thread>

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

std::unique_ptr<Falcon> Falcon::Listen(const std::string& endpoint, uint16_t port) {
    sockaddr local_endpoint = StringToIp(endpoint, port);

    auto falcon = std::make_unique<Falcon>();
    falcon->m_socket = socket(local_endpoint.sa_family, SOCK_DGRAM, IPPROTO_UDP);
    if (falcon->m_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        return nullptr;
    }

    if (int error = bind(falcon->m_socket, &local_endpoint, sizeof(local_endpoint)); error != 0) {
        std::cerr << "Socket bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(falcon->m_socket);
        return nullptr;
    }

    std::cout << "Server is listening on " << endpoint << ":" << port << std::endl;
    return falcon;
}

void Falcon::ConnectTo(const std::string& serverIp, uint16_t port)
{
    // Create the socket
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket < 0) {
        throw std::runtime_error("Socket creation failed");
    }

    // Configure the server address
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr);

    // Prepare the connection message
    MsgConn connInfo{MSG_CONN};

    std::vector<char> buffer(sizeof(connInfo));
    std::memcpy(buffer.data(), &connInfo, sizeof(connInfo));

    int sent = SendTo(serverIp, port, buffer);

    if (sent < 0) {
        throw std::runtime_error("Failed to send connection request");
    }
    else {
        std::cout << "Connection request sent to " << serverIp << ":" << port << std::endl;
    }
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
    sockaddr_storage peer_addr{};
    socklen_t peer_addr_len = sizeof( sockaddr_storage);
    const int read_bytes = recvfrom(m_socket,
        message.data(),
        message.size_bytes(),
        0,
        reinterpret_cast<sockaddr*>(&peer_addr),
        &peer_addr_len);

    from = IpToString(reinterpret_cast<const sockaddr*>(&peer_addr));

    if (read_bytes < 0) {
        std::cerr << "Failed to receive data. Error: " << WSAGetLastError() << std::endl;
    }

    return read_bytes;
}

void Falcon::OnClientDisconnected(std::function<void(uint64_t)> handler) { // TODO: Replace with custom message type
    std::thread([this, handler]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            for (auto it = clients.begin(); it != clients.end();) {
                sockaddr_in clientAddr{};
                socklen_t clientAddrLen = sizeof(clientAddr);

                std::string pingMessage = "PING";
                int sent = sendto(m_socket, pingMessage.c_str(), pingMessage.size(), 0,
                                  (struct sockaddr*)&clientAddr, clientAddrLen);

                char buffer[1024];
                std::string from;
                int received = ReceiveFrom(from, std::span<char, 65535>(buffer, sizeof(buffer) - 1));

                if (received < 0) {
                    uint64_t clientID = it->first;
                    it = clients.erase(it);
                    handler(clientID);
                } else {
                    ++it;
                }
            }
        }
    }).detach();
}

void Falcon::OnDisconnect(std::function<void()> handler) {
    std::thread([this, handler]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        sockaddr_in serverAddr{};
        socklen_t serverAddrLen = sizeof(serverAddr);

        char buffer[1024];
        std::string from;
        int received = ReceiveFrom(from, std::span<char, 65535>(buffer, sizeof(buffer) - 1));

        if (received < 0) {
            handler();
        }
    }).detach();
}
