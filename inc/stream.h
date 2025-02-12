#pragma once
#include <span>
#include <cstdint>

class Stream {
public:
    Stream(uint32_t id, bool reliable);
    ~Stream();

    void SendData(std::span<const char> data);
    void OnDataReceived(std::span<const char> data);

private:
    uint32_t streamID;
    bool reliable;
};

