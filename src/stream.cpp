#include "Stream.h"
#include <iostream>
#include <cstring>

Stream::Stream(uint32_t id, bool reliable) : streamID(id), reliable(reliable) {}

Stream::~Stream() {}

void Stream::SendData(std::span<const char> data) {
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
