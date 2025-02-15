#pragma once

#include <vector>
#include <span>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <chrono>

class Stream {
public:
    Stream(uint32_t id, bool reliable);
    ~Stream();

    void SendData(std::span<const char> data);
    void OnDataReceived(std::function<void(std::span<const char>)> handler);
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
    uint32_t nextPacketID = 1;
    std::unordered_map<uint32_t, Packet> pendingPackets; // Paquets en attente d’ACK
    void ResendLostPackets();
};
