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
private:
    uint32_t streamID;
    bool reliable;
};

