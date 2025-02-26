#pragma once

#include <vector>
#include <span>
#include <cstdint>
#include <functional>

#include "falcon.h"

class Falcon;
struct Client;

class Stream {
public:
    Stream(uint32_t ID, std::string ip, int port, Falcon& falcon);
    // Stream(Falcon& falcon, bool reliable); // Client API
    // Stream(Falcon& falcon, bool reliable, uint64_t clientID); // Server API
    // Stream(Falcon& falcon, uint32_t StreamID); // Client API
    // Stream(Falcon& falcon, uint32_t StreamID, uint64_t clientID); // Server API
    ~Stream();

    void SendData(std::span<const char> data);
    static void OnDataReceived(std::span<const char> data); // Called when data is received by the Falcon object, Really want to rename this HandleDataReceived but the tech plan says otherwise

    // uint32_t GetStreamID() const { return streamID; }
    static bool IsReliable(uint32_t ID) {
        // check if bit at position 30 is set
        return ID & RELIABLESTREAMMASK;
    }
    //
    static bool IsServerStream(uint32_t ID) {
        // check if bit at position 31 is set
        return ID & SERVERSTREAMMASK;
    }
    uint32_t streamID;

private:
    const std::string targetIP;
    const int targetPort;

    Falcon& falcon;

    std::vector<std::function<void(std::span<const char>)>> onDataReceivedHandlers;

    //uint32_t nextPacketID = 1;
    //std::unordered_map<uint32_t, Packet> pendingPackets; // Paquets en attente d’ACK
    //void ResendLostPackets();
};
