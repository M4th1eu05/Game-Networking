#pragma once

#include <vector>
#include <span>
#include <cstdint>
#include <functional>

class Stream {
public:
    Stream(uint32_t id, bool reliable);
    ~Stream();

    void SendData(std::span<const char> data);
    void OnDataReceived(std::function<void(std::span<const char>)> handler);

    uint32_t GetStreamID() const { return streamID; }
    bool IsReliable() const { return reliable; }

private:
    uint32_t streamID; // Identifiant unique du Stream
    bool reliable;      // Indique si le Stream doit assurer la fiabilité
};
