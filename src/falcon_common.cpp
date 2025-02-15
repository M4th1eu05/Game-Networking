#include <thread>

#include "falcon.h"
#include <iostream>
#include "stream.h"

std::unique_ptr<Stream> Falcon::CreateStream(uint64_t client, bool reliable) {
    uint32_t streamID = nextStreamID++;
    std::cout << "Creating Stream " << streamID << " for client " << client << "\n";

    auto stream = std::make_unique<Stream>(streamID, reliable);
    streams[streamID] = std::move(stream);
    return std::make_unique<Stream>(streamID, reliable);
}

std::unique_ptr<Stream> Falcon::CreateStream(bool reliable) {
    uint32_t streamID = nextStreamID++;
    std::cout << "Creating Stream " << streamID << " for client\n";

    auto stream = std::make_unique<Stream>(streamID, reliable);
    streams[streamID] = std::move(stream);
    return std::make_unique<Stream>(streamID, reliable);
}

void Falcon::CloseStream(const Stream& stream) {
    std::cout << "Closing Stream " << stream.GetStreamID() << "\n";
    streams.erase(stream.GetStreamID());
}

int Falcon::SendTo(const std::string &to, uint16_t port, const std::span<const char> message)
{
    return SendToInternal(to, port, message);
}

int Falcon::ReceiveFrom(std::string& from, const std::span<char, 65535> message)
{
    return ReceiveFromInternal(from, message);
}

void Falcon::OnClientConnected(std::function<void(uint64_t)> handler) {
    std::thread t([this, handler] {
        while (true) {
            std::string from_ip;
            from_ip.resize(255);
            std::array<char, 65535> buffer;
            int read_bytes = ReceiveFrom(from_ip, buffer);
            if (read_bytes > 0) {
                handler(nextClientID);
                clients[nextClientID] = from_ip;
                nextClientID++;
            }
        }
    });
}

void Falcon::OnConnectionEvent(std::function<void(bool, uint64_t)> handler) {
     std::thread t([this, handler] {
        while (true) {

        }
    });
}

/*
void Stream::SendData(uint32_t streamID, std::span<const char> data) {
    if (streams.find(streamID) != streams.end()) {
        streams[streamID]->SendData(data);
    } else {
        std::cerr << "Error: Stream " << streamID << " does not exist!\n";
    }
}

void Stream::OnDataReceived(uint32_t streamID, std::function<void(std::span<const char>)> handler) {
    if (streams.find(streamID) != streams.end()) {
        streams[streamID]->OnDataReceived(handler);
    } else {
        std::cerr << "Error: Stream " << streamID << " does not exist!\n";
    }
}
*/