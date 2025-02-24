#include <string>
#include <array>
#include <span>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "falcon.h"
#include "spdlog/spdlog.h"

TEST_CASE("Can Listen", "[falcon]") {
    auto receiver = Falcon::Listen("127.0.0.1", 5555);
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
    std::this_thread::sleep_for(std::chrono::seconds(2));

    REQUIRE(clientDisconnected == true);
}

//
// TEST_CASE("Stream sends and receives data", "[Stream]") {
//     Stream stream(1, false); // Stream non fiable
//
//     std::string message = "Hello, world!";
//     std::span<const char> data(message.data(), message.size());
//
//     bool received = false;
//     stream.OnDataReceived([&](std::span<const char> receivedData) {
//         REQUIRE(std::string(receivedData.begin(), receivedData.end()) == message);
//         received = true;
//     });
//
//     stream.SendData(data);
//
//     REQUIRE(received == true);
// }
//
// TEST_CASE("Reliable Stream retransmits lost packets", "[Stream]") {
//     Stream stream(2, true);
//
//     std::string message = "Important data!";
//     std::span<const char> data(message.data(), message.size());
//
//     stream.SendData(data);
//
//     // Simule la perte de paquets
//     stream.Acknowledge(1); // On ne confirme que le premier envoi
//
//     // Attendre pour voir si le paquet est renvoyé
//     std::this_thread::sleep_for(std::chrono::seconds(2));
//
//     REQUIRE(true); // Si on arrive ici, c'est que le test est passé
// }