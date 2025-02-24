#pragma once

#include <vector>
#include <span>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <chrono>

#include "falcon.h"

constexpr uint32_t RELIABLESTREAMMASK = 1<<30;
constexpr uint32_t SERVERSTREAMMASK = 1<<31;

class Stream {
public:
    Stream(Falcon& falcon, bool reliable); // Client API
    Stream(Falcon& falcon, bool reliable, uint64_t clientID); // Server API
    Stream(Falcon& falcon, uint32_t StreamID); // Client API
    Stream(Falcon& falcon, uint32_t StreamID, uint64_t clientID); // Server API
    ~Stream();

    void SendData(std::span<const char> data);
    void OnDataReceived(std::span<const char> data);
    void Acknowledge(uint32_t packetID);

    uint32_t GetStreamID() const { return streamID; }
    static bool IsReliable(uint32_t ID) {
        // check if bit at position 30 is set
        return ID & RELIABLESTREAMMASK;
    }

    static bool IsServerStream(uint32_t ID) {
        // check if bit at position 31 is set
        return ID & SERVERSTREAMMASK;
    }

private:
    uint32_t streamID;
    static uint32_t nextStreamID;

    Falcon& falcon;

    uint64_t clientID = 0;

    //uint32_t nextPacketID = 1;
    //std::unordered_map<uint32_t, Packet> pendingPackets; // Paquets en attente d’ACK
    //void ResendLostPackets();
};
