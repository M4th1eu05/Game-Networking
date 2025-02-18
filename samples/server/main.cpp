#include <iostream>

#include <falcon.h>

#include "spdlog/spdlog.h"

int main()
{
    spdlog::set_level(spdlog::level::debug);
    spdlog::debug("This is the server!");

    auto server = Falcon::Listen("127.0.0.1", 5555);

    server->OnClientConnected([&](uint64_t clientID) {
        std::cout << "Client " << clientID << " connected!\n";
        // auto stream = server.CreateStream(clientID, true);
        //
        // stream->OnDataReceived([](std::span<const char> data) {
        //     std::cout << "Received: " << std::string(data.begin(), data.end()) << "\n";
        // });
    });

    while (true) {}
}
