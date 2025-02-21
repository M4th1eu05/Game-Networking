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
    });

    server->OnClientDisconnected([&](uint64_t clientID) {
        std::cout << "Client " << clientID << " disconnected!\n";
    });

    while (true) {}
}
