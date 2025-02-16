#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include <memory>
#include <fmt/core.h>
#include "falcon.h"
#include <thread>
#include <iostream>


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
    int error = inet_pton(AF_INET, ip.c_str(), &result);
    if (error == 1) {
        result.sa_family = AF_INET;
#ifndef __linux__
        result.sa_len = sizeof(sockaddr_in);
#endif
        reinterpret_cast<sockaddr_in*>(&result)->sin_port = htons(port);
        return result;
    }

    memset(&result, 0, sizeof(result));
    error = inet_pton(AF_INET6, ip.c_str(), &result);
    if (error == 1) {
        result.sa_family = AF_INET6;
#ifndef __linux__
        result.sa_len = sizeof(sockaddr_in6);
#endif
        reinterpret_cast<sockaddr_in6*>(&result)->sin6_port = htons(port);
        return result;
    }
    memset(&result, 0, sizeof(result));
    return result;
}

Falcon::Falcon() {

}

Falcon::~Falcon() {
    if(m_socket > 0)
    {
        close(m_socket);
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
        close(falcon->m_socket);
        return nullptr;
    }

    return falcon;
}

void Falcon::ConnectTo(const std::string& ip, uint16_t port) {
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    std::string initMessage = "CONNECT";
    sendto(socketFd, initMessage.c_str(), initMessage.size(), 0,
           (struct sockaddr*)&serverAddr, sizeof(serverAddr));
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

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(m_socket, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 1; // 1 second
    timeout.tv_usec = 0;

    int select_result = select(m_socket + 1, &read_fds, nullptr, nullptr, &timeout);
    if (select_result > 0 && FD_ISSET(m_socket, &read_fds)) {
        const int read_bytes = recvfrom(m_socket,
            message.data(),
            message.size_bytes(),
            0,
            reinterpret_cast<sockaddr*>(&peer_addr),
            &peer_addr_len);

        from = IpToString(reinterpret_cast<const sockaddr*>(&peer_addr));
        return read_bytes;
    } else if (select_result == 0) {
        // Timeout occurred
        return -1; // or any other value to indicate timeout
    } else {
        // Error occurred
        return -2;
    }
}

void Falcon::OnClientConnected(std::function<void(uint64_t)> handler) {
    std::thread([this, handler]() {
        while (true) {
            char buffer[1024];
            sockaddr_in clientAddr{};
            socklen_t clientAddrLen = sizeof(clientAddr);

            int received = recvfrom(socketFd, buffer, sizeof(buffer) - 1, 0,
                                    (struct sockaddr*)&clientAddr, &clientAddrLen);
            if (received > 0) {
                buffer[received] = '\0';

                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);

                if (clients.find(nextClientID) == clients.end()) {
                    uint64_t clientID = nextClientID++;
                    clients[clientID] = clientIP;
                    handler(clientID);

                    // Send the clientID as a confirmation
                    int sent = sendto(socketFd, reinterpret_cast<const char*>(&clientID), sizeof(clientID), 0,
                                      (struct sockaddr*)&clientAddr, clientAddrLen);
                    if (sent < 0) {
                        std::cerr << "Failed to send confirmation to client.\n";
                    }
                }
            }
        }
    }).detach();
}

void Falcon::OnConnectionEvent(std::function<void(bool, uint64_t)> handler) {
    std::thread([this, handler]() {
        uint64_t clientID;
        sockaddr_in serverAddr{};
        socklen_t serverAddrLen = sizeof(serverAddr);

        int received = recvfrom(socketFd, reinterpret_cast<char*>(&clientID), sizeof(clientID), 0,
                                (struct sockaddr*)&serverAddr, &serverAddrLen);
        if (received == sizeof(clientID)) {
            handler(true, clientID);
        } else {
            handler(false, 0);
        }
    }).detach();
}

void Falcon::OnClientDisconnected(std::function<void(uint64_t)> handler) { // TODO: Replace with custom message type
    std::thread([this, handler]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            for (auto it = clients.begin(); it != clients.end();) {
                sockaddr_in clientAddr{};
                socklen_t clientAddrLen = sizeof(clientAddr);

                std::string pingMessage = "PING";
                int sent = sendto(socketFd, pingMessage.c_str(), pingMessage.size(), 0,
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
