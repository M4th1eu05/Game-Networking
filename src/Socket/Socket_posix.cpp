#include "Socket.h"

#ifdef __unix__
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <iostream>

Socket::Socket() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Socket creation failed");
    }
}

Socket::~Socket() {
    close(sockfd);
}

bool Socket::create() {
    return true; // Socket is created in the constructor for POSIX
}

bool Socket::bind(const std::string& ip, int port) {
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    if (::bind(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        return false;
    }
    return true;
}

bool Socket::sendTo(const std::string& ip, int port, const std::string& message) {
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    if (sendto(sockfd, message.c_str(), message.size(), 0, (sockaddr*)&addr, sizeof(addr)) < 0) {
        return false;
    }
    return true;
}

std::string Socket::receiveFrom(std::string& senderIp, int& senderPort) {
    char buffer[1024];
    sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    int bytesReceived = recvfrom(sockfd, buffer, 1024, 0, (sockaddr*)&addr, &addrLen);
    if (bytesReceived < 0) {
        return "";
    }
    senderIp = inet_ntoa(addr.sin_addr);
    senderPort = ntohs(addr.sin_port);
    return std::string(buffer, bytesReceived);
}

void Socket::close() {
    close(sockfd);
}
#endif