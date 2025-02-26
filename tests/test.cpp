#include <string>
#include <span>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "falcon.h"
#include "spdlog/spdlog.h"

TEST_CASE("Can Listen", "[falcon]") {
    auto receiver = Falcon::Listen("127.0.0.1", 5555);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    REQUIRE(receiver != nullptr);
}

TEST_CASE("Client can connect to server", "[falcon]") {
    spdlog::set_level(spdlog::level::debug);

    const std::unique_ptr<Falcon> server = Falcon::Listen("127.0.0.1",5555);
    const auto client = std::make_unique<Falcon>();

    bool connectionSuccess = false;
    uint64_t clientID = 0;


    server->OnClientConnected([&](uint64_t id) {
        spdlog::debug("Client connected with ID {}", id);
    });

    client->OnConnectionEvent([&](bool success, uint64_t id) {
        spdlog::debug("Connection event called on client! Success: {}, ID: {}", success, id);
        connectionSuccess = success;
        clientID = id;
    });

    REQUIRE_NOTHROW(client->ConnectTo("127.0.0.1", 5555));

    // Wait for the event to trigger
    std::this_thread::sleep_for(std::chrono::seconds(1));

    spdlog::debug("Connection success: {}, Client ID: {}", connectionSuccess, clientID);

    REQUIRE(connectionSuccess == true);
    REQUIRE(clientID > 0);

}

TEST_CASE("Does timeout work", "[falcon]") {
    spdlog::set_level(spdlog::level::debug);

    const std::unique_ptr<Falcon> server = Falcon::Listen("127.0.0.1", 5555);
    auto client = std::make_unique<Falcon>();

    bool connectionSuccess = false;
    uint64_t clientID = 0;
    bool clientDisconnected = false;

    server->OnClientConnected([&](uint64_t id) {
        spdlog::debug("Client connected with ID {}", id);
    });

    server->OnClientDisconnected([&](uint64_t id) {
        spdlog::debug("Client disconnected with ID {}", id);
        clientDisconnected = true;
    });

    client->OnConnectionEvent([&](bool success, uint64_t id) {
        spdlog::debug("Connection event called on client! Success: {}, ID: {}", success, id);
        connectionSuccess = success;
        clientID = id;
    });

    REQUIRE_NOTHROW(client->ConnectTo("127.0.0.1", 5555));

    // Wait for the connection event to trigger
    std::this_thread::sleep_for(std::chrono::seconds(1));

    spdlog::debug("Connection success: {}, Client ID: {}", connectionSuccess, clientID);

    REQUIRE(connectionSuccess == true);
    REQUIRE(clientID > 0);

    // Destroy the client
    client.reset();

    // Wait for the disconnection event to trigger
    std::this_thread::sleep_for(std::chrono::seconds(3));

    REQUIRE(clientDisconnected == true);
}


TEST_CASE("Stream sends and receives data", "[Stream]") {
    spdlog::set_level(spdlog::level::debug);

    const std::unique_ptr<Falcon> server = Falcon::Listen("127.0.0.1",5555);
    const auto client = std::make_unique<Falcon>();

    uint64_t clientID = 0;


    server->OnClientConnected([&](uint64_t id) {
        spdlog::debug("Client connected with ID {}", id);
    });

    client->OnConnectionEvent([&](bool success, uint64_t id) {
        spdlog::debug("Connection event called on client! Success: {}, ID: {}", success, id);
        clientID = id;
    });

    REQUIRE_NOTHROW(client->ConnectTo("127.0.0.1", 5555));

    // Wait for the event to trigger
    std::this_thread::sleep_for(std::chrono::seconds(1));

    bool hasReceivedData = false;

    auto clientStream = client->CreateStream(false);

    server->OnStreamCreated([&hasReceivedData](const uint32_t streamID) {
        spdlog::debug("Stream created with ID {}", streamID);
        hasReceivedData = true;
    });

    clientStream->SendData(Falcon::SerializeMessage(MsgStandard{MSG_STANDARD, 0, 0,0, "Hello, World!"}));

    std::this_thread::sleep_for(std::chrono::seconds(1));

    client->CloseStream(*clientStream);

    REQUIRE(hasReceivedData == true);
}