#ifndef SOCKET_H
#define SOCKET_H

#include <string>

class Socket {
public:
    Socket();
    ~Socket();

    bool create();
    bool bind(const std::string& ip, int port);
    bool sendTo(const std::string& ip, int port, const std::string& message);
    std::string receiveFrom(std::string& senderIp, int& senderPort);
    void close();

private:
    int sockfd;
};

#endif // SOCKET_H