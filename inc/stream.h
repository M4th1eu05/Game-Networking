#pragma once

#include <vector>
#include <span>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <chrono>

#include "falcon.h"

class Stream {
public:
    Stream(bool reliable);
    Stream(bool reliable, uint64_t clientID);
    ~Stream();

    void SendData(std::span<const char> data);
    void OnDataReceived(std::span<const char> data);
    void Acknowledge(uint32_t packetID);

    uint32_t GetStreamID() const { return streamID; }
    bool IsReliable() const {
        // check if bit at position 30 is set
        return streamID & (1 << 30);
    }

    bool IsServerStream() const {
        // check if bit at position 31 is set
        return streamID & (1 << 31);
    }

private:
    uint32_t streamID; // Identifiant unique du Stream

    static uint32_t nextStreamID; // ID unique des Streams

    std::unique_ptr<Falcon> falcon; // Référence au socket Falcon


    //uint32_t nextPacketID = 1;
    //std::unordered_map<uint32_t, Packet> pendingPackets; // Paquets en attente d’ACK
    //void ResendLostPackets();
};
