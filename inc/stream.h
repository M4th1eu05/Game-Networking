#pragma once

#include <vector>
#include <span>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <chrono>

class Falcon;

class Stream {
public:
    Stream(uint32_t streamID, bool reliable, Falcon& socketRef, const std::string& remoteIP, uint16_t remotePort);
    ~Stream();

    void SendData(std::span<const char> data);
    void OnDataReceived(std::span<const char> data);
    void Acknowledge(uint32_t packetID);

    uint32_t GetStreamID() const { return streamID; }
    bool IsReliable() const { return reliable; }

private:
    struct Packet {
        uint32_t packetID;
        std::vector<char> data;
        std::chrono::steady_clock::time_point lastSentTime;
    };

    uint32_t streamID; // Identifiant unique du Stream
    bool reliable;      // Indique si le Stream doit assurer la fiabilité
    std::string remoteIP;
    uint16_t remotePort;

    uint32_t nextPacketID = 1;
    std::unordered_map<uint32_t, Packet> pendingPackets; // Paquets en attente d’ACK

    Falcon& socket; // Reference to the Falcon socket

    void ResendLostPackets();
};
