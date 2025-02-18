#include "stream.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

#include "falcon.h"

Stream::Stream(uint32_t streamID, bool reliable, Falcon& socketRef, const std::string& remoteIP, uint16_t remotePort)
    : streamID(streamID), reliable(reliable), socket(socketRef), remoteIP(remoteIP), remotePort(remotePort) {
    if (reliable) {
        std::thread(&Stream::ResendLostPackets, this).detach();
    }
}

Stream::~Stream() {}

void Stream::SendData(std::span<const char> data) {
    uint32_t packetID = nextPacketID++;

    std::vector<char> packet(sizeof(packetID) + data.size());

    // Copy packetID and data into the buffer
    std::memcpy(packet.data(), &packetID, sizeof(packetID));
    std::memcpy(packet.data() + sizeof(packetID), data.data(), data.size());

    std::cout << "Stream " << streamID << " sending data: "
              << std::string(data.begin(), data.end()) << "\n";

    // Use Falcon's SendTo method instead of Winsock's sendto
    int sent = socket.SendTo(remoteIP, remotePort, packet);
    if (sent < 0) {
        std::cerr << "Failed to send packet." << std::endl;
    }
}

void Stream::OnDataReceived(std::span<const char> data) {
    std::cout << "Stream" << streamID << " received data \n";
}

void Stream::Acknowledge(uint32_t packetID) {
    if (reliable) {
        pendingPackets.erase(packetID);
        std::cout << "Packet " << packetID << " acknowledged!\n";
    }
}

void Stream::ResendLostPackets() {
    while (reliable) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto now = std::chrono::steady_clock::now();
        for (auto& [packetID, packet] : pendingPackets) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - packet.lastSentTime).count() > 1000) {
                std::cout << "Resending lost packet " << packetID << "\n";
                packet.lastSentTime = now;
            }
        }
    }
}