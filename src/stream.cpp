#include "stream.h"
#include <iostream>
#include <utility>


Stream::Stream(uint32_t ID, std::string ip, int port, Falcon &falcon)
    : streamID(ID), targetIP(std::move(ip)), targetPort(port), falcon(falcon)
{
}

Stream::~Stream() = default;

void Stream::SendData(std::span<const char> data)
{
    int sent = falcon.SendTo(targetIP, targetPort, data);

    if (sent < 0) {
        std::cerr << "Failed to send data to " << targetIP << ":" << targetPort << "\n";
    } else {
        std::cout << "Data sent to " << targetIP << ":" << targetPort << "\n";
    }
}

void Stream::OnDataReceived(std::span<const char> data)
{
    std::cout << "Received data :\n" << std::string(data.begin(), data.end()) << "\n";
}
