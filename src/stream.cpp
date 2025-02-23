#include "stream.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

Stream::Stream(bool reliable)
{
    streamID = ++nextStreamID;
    streamID |= SERVERSTREAMMASK; // set the most significant bit to 1 to indicate that this is a server stream
    if (reliable)
        streamID |= RELIABLESTREAMMASK; // set the second most significant bit to 1 to indicate that this is a reliable stream
    else
        streamID &= ~RELIABLESTREAMMASK; // set the second most significant bit to 0 to indicate that this is an unreliable stream
}

Stream::~Stream() {}

void Stream::SendData(std::span<const char> data) {

}

void Stream::OnDataReceived(std::span<const char> data) {
    std::cout << "Stream " << streamID << " waiting for data...\n";
    // Cette fonction sera déclenchée par le serveur lorsqu'il reçoit des données
}

/*
void Stream::Acknowledge(uint32_t packetID) {
    if (IsReliable()) {
        pendingPackets.erase(packetID);
        std::cout << "Packet " << packetID << " acknowledged!\n";
    }
}

void Stream::ResendLostPackets() {
    while (IsReliable()) {
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
*/