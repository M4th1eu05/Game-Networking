#include "Socket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <iostream>


Socket::Socket() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == INVALID_SOCKET) {
        throw std::runtime_error("Socket creation failed");
    }
}

Socket::~Socket() {
    closesocket(sockfd);
    WSACleanup();
}

bool Socket::bind(const std::string& ip, int port) {
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    if (::bind(sockfd, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        return false;
    }
    return true;
}

bool Socket::sendTo(const std::string& ip, int port, const std::string& message) {
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    if (sendto(sockfd, message.c_str(), message.size(), 0, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        return false;
    }
    return true;
}

std::string Socket::receiveFrom(std::string& senderIp, int& senderPort) {
    char buffer[1024];
    sockaddr_in addr;
    int addrLen = sizeof(addr);
    int bytesReceived = recvfrom(sockfd, buffer, 1024, 0, (sockaddr*)&addr, &addrLen);
    if (bytesReceived == SOCKET_ERROR) {
        return "";
    }
    senderIp = inet_ntoa(addr.sin_addr);
    senderPort = ntohs(addr.sin_port);
    return std::string(buffer, bytesReceived);
}

void Socket::close() {
    closesocket(sockfd);
}
#endif