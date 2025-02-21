#include <iostream>

#include <falcon.h>

#include "spdlog/spdlog.h"

int main() {
    spdlog::set_level(spdlog::level::debug);
    spdlog::debug("This is the client!");

    std::unique_ptr<Falcon> client = std::make_unique<Falcon>();
    client->ConnectTo("127.0.0.1", 5555);

    client->OnConnectionEvent([&](bool success, uint64_t clientID) {
        if (success) {
            std::cout << "Connected to server with ID " << clientID << "\n";
        } else {
            std::cerr << "Connection failed!\n";
        }
    });

    client->OnDisconnect([&]() {
        std::cerr << "Disconnected from server!\n";
    });

    while (true) {}

}
