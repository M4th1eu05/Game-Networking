#include "Stream.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

Stream::Stream(uint32_t id, bool reliable) : streamID(id), reliable(reliable) {
    if (reliable) {
        std::thread(&Stream::ResendLostPackets, this).detach();
    }
}

Stream::~Stream() {}

void Stream::SendData(std::span<const char> data) {
    uint32_t packetID = nextPacketID++;

    std::vector<char> packet(sizeof(packetID) + data.size());

    // Copier packetID et données dans le buffer
    std::memcpy(packet.data(), &packetID, sizeof(packetID));
    std::memcpy(packet.data() + sizeof(packetID), data.data(), data.size());

    std::cout << "Stream " << streamID << " sending data: "
              << std::string(data.begin(), data.end()) << "\n";

    // Simuler une perte de paquet (10% de chance)
    if (reliable && (rand() % 10) == 0) {
        std::cout << "Packet lost, resending...\n";
        SendData(data); // Réessai immédiat pour un Stream fiable
    }
}

void Stream::OnDataReceived(std::function<void(std::span<const char>)> handler) {
    std::cout << "Stream " << streamID << " waiting for data...\n";
    // Cette fonction sera déclenchée par le serveur lorsqu'il reçoit des données
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