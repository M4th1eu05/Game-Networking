#pragma once

#include <functional>
#include <memory>
#include <string>
#include <span>
#include <unordered_map>
#include "stream.h"

#ifdef WIN32
    using SocketType = unsigned int;
#else
    using SocketType = int;
#endif

struct ConnectionInfo {
    uint64_t uuid;
    uint32_t messageType; // 1 = CONNECT, 2 = DISCONNECT
    char message[256];
};

class Stream;

class Falcon {
public:
    Falcon();
    ~Falcon();
    Falcon(const Falcon&) = default;
    Falcon& operator=(const Falcon&) = default;
    Falcon(Falcon&&) = default;
    Falcon& operator=(Falcon&&) = default;

    [[nodiscard]] static std::unique_ptr<Falcon> Listen(const std::string& endpoint, uint16_t port);
    [[nodiscard]] static std::unique_ptr<Falcon> Listen(uint16_t port);
    void ConnectTo(const std::string& serverIp, uint16_t port);

    int SendTo(const std::string& to, uint16_t port, std::span<const char> message);
    int ReceiveFrom(std::string& from, std::span<char, 65535> message);

    void OnClientConnected(std::function<void(uint64_t)> handler);
    void OnConnectionEvent(std::function<void(bool, uint64_t)> handler);
    void OnClientDisconnected(std::function<void(uint64_t)> handler);
    void OnDisconnect(std::function<void()> handler);

    // Gestion des Streams
    [[nodiscard]] std::unique_ptr<Stream> CreateStream(uint64_t client, bool reliable);
    [[nodiscard]] std::unique_ptr<Stream> CreateStream(bool reliable);
    void CloseStream(const Stream& stream);
    void SendData(uint32_t streamID, std::span<const char> data);
    void OnDataReceived(uint32_t streamID, std::function<void(std::span<const char>)> handler);


private:
    int SendToInternal(const std::string& to, uint16_t port, std::span<const char> message);
    int ReceiveFromInternal(std::string& from, std::span<char, 65535> message);

    int socketFd; // Identifiant du socket
    uint64_t nextClientID = 1; // ID unique attribué aux clients
    std::unordered_map<uint64_t, std::string> clients; // Liste des clients connectés
    uint32_t nextStreamID = 1; // ID unique des Streams
    std::unordered_map<uint32_t, std::unique_ptr<Stream>> streams; // Liste des Stream

    SocketType m_socket;
};
