#include "stream.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

Stream::Stream(Falcon& falcon, bool reliable) : falcon(falcon) {
    streamID = ++nextStreamID;
    if (reliable)
        streamID |= RELIABLESTREAMMASK;
    else
        streamID &= ~RELIABLESTREAMMASK;
}

Stream::Stream(Falcon& falcon, bool reliable, uint64_t clientID) : falcon(falcon), clientID(clientID) {
    streamID = ++nextStreamID;
    if (reliable)
        streamID |= RELIABLESTREAMMASK;
    else
        streamID &= ~RELIABLESTREAMMASK;
}

Stream::Stream(Falcon &falcon, uint32_t StreamID) : falcon(falcon) {
    streamID = StreamID;
}

Stream::Stream(Falcon &falcon, uint32_t StreamID, uint64_t clientID) : falcon(falcon), clientID(clientID){
    streamID = StreamID;
}


Stream::~Stream() {}

void Stream::SendData(std::span<const char> data) {
    Client target;
    if (IsServerStream(streamID)) {
        target = falcon.GetClient(clientID);
    }
    else {
        target = falcon.GetClientInfoFromServer();
    }

    falcon.SendTo(target.IP, target.Port, data);
}

void Stream::OnDataReceived(std::span<const char> data) {
    std::cout << "Received data\n";
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