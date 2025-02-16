#include <string>
#include <array>
#include <span>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "falcon.h"

TEST_CASE( "Can Listen", "[falcon]" ) {
    auto receiver = Falcon::Listen("127.0.0.1", 5555);
    REQUIRE(receiver != nullptr);
}

TEST_CASE("Client can connect to server", "[Socket]") {
    Falcon server;
    server.Listen( 5555);

    Falcon client;
    REQUIRE_NOTHROW(client.ConnectTo("127.0.0.1", 5555));

    bool connectionSuccess = false;
    uint64_t clientID = 0;

    client.OnConnectionEvent([&](bool success, uint64_t id) {
        connectionSuccess = success;
        clientID = id;
    });

    // Attendre que l'événement se déclenche
    std::this_thread::sleep_for(std::chrono::seconds(1));

    REQUIRE(connectionSuccess == true);
    REQUIRE(clientID > 0);
}