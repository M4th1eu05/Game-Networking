#include "stream.h"

Stream::Stream(uint32_t id, bool reliable)
    : streamID(id), reliable(reliable)
{
}

Stream::~Stream()
{
}

void Stream::SendData(std::span<const char> data)
{
    std::cout << "Stream " << streamID << " sending data: "
                << std::string(data.begin(), data.end()) << "\n";

    // Simuler une perte de paquet (10% de chance)
    if (reliable && (rand() % 10) == 0) {
        std::cout << "Packet lost, resending...\n";
        SendData(data); // Réessai immédiat pour un Stream fiable
    }
}

void Stream::OnDataReceived(std::span<const char> data)
{
   std::cout << "Stream " << streamID << " waiting for data...\n";
}