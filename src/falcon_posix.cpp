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

std::unique_ptr<Falcon> Falcon::ListenInternal(const std::string& endpoint, uint16_t port)
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

    int sent = sendto(m_socket, buffer.data(), buffer.size(), 0,
                      (struct sockaddr*)&serverAddr, sizeof(serverAddr));

    if (sent < 0) {
        std::cout << "Failed to send connection request to " << serverIp << ":" << port << std::endl;
    }
    else {
        // std::cout << "Connection request sent to " << serverIp << ":" << port << std::endl;
    }

    std::thread([this, serverIp]() {
        while (true) {
            std::string serverIP = serverIp;
            int serverPort;
            std::vector<char> buffer(65535);

            int received = ReceiveFrom(serverIP, std::span<char, 65535>(buffer.data(), buffer.size()));

            if (received < 0) {
                std::cerr << "Failed to receive message\n";
                continue;
            }
            if (received > 0) {
                serverPort = portFromIp(serverIP);

                Msg msg;
                msg.IP = serverIP;
                msg.Port = serverPort;
                msg.data = std::vector<char>(buffer.begin(), buffer.end());

                std::lock_guard<std::mutex> lock(queueMutex);
                messageQueue.push(msg);
            }
        }
    }).detach();
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
        // std::cerr << "Failed to receive data. Error: " << WSAGetLastError() << std::endl;
    }

    return read_bytes;
}
