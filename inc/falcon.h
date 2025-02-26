#pragma once

#include <functional>
#include <memory>
#include <string>
#include <span>
#include <unordered_map>
#include <chrono>
#include <vector>
#include <cstring>
#include <atomic>
#include <thread>
#include <cstdint>

#ifdef WIN32
    using SocketType = unsigned int;
#else
    using SocketType = int;
#endif

static constexpr uint32_t RELIABLESTREAMMASK = 1<<30;
static constexpr uint32_t SERVERSTREAMMASK = 1<<31;
static constexpr uint64_t RELIABLE_ACK_MASK = uint64_t(1)<<63;

#include "stream.h"

enum MsgType: uint8_t {
    MSG_CONN,
    MSG_CONN_ACK,
    MSG_STANDARD,
    MSG_ACK,
    PING
};

struct Msg {
    std::string IP;
    int Port;
    std::vector<char> data;
};

struct MsgConn {
    uint8_t messageType;
};

struct MsgConnAck {
    uint8_t messageType;
    uint64_t clientID;
};

struct MsgStandard {
    uint8_t messageType;
    uint64_t clientID;
    uint32_t streamID;
    uint8_t messageID;
    std::array<char, 1024> data;
};

struct MsgAck {
    uint8_t messageType;
    uint64_t clientID;
    uint32_t streamID;
    uint8_t messageID;
    uint64_t trace;
};

struct Ping {
    uint8_t messageType;
    uint64_t clientID;
    uint8_t pingID;
    std::chrono::steady_clock::time_point time;
};

struct Client {
    uint64_t ID;
    std::string IP;
    int Port;
    bool pinged;
    std::chrono::time_point<std::chrono::steady_clock> lastPing;
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
    void ConnectTo(const std::string& serverIp, uint16_t port);

    int SendTo(const std::string& to, uint16_t port, std::span<const char> message);
    int ReceiveFrom(std::string& from, std::span<char, 65535> message);

    void OnClientConnected(const std::function<void(uint64_t)>& handler);
    void OnConnectionEvent(const std::function<void(bool, uint64_t)> &handler);
    void OnClientDisconnected(const std::function<void(uint64_t)>& handler);
    void OnDisconnect(const std::function<void()>& handler);
    void OnStreamCreated(const std::function<void(uint32_t)> &handler);

    // Gestion des Streams
    [[nodiscard]] std::unique_ptr<Stream> CreateStream(uint64_t client, bool reliable); // Server API
    [[nodiscard]] std::unique_ptr<Stream> CreateStream(bool reliable); // Client API
    void CloseStream(const Stream& stream);


    Client GetClient(const uint64_t id) {
        return clients[id];
    }

    Client GetClientInfoFromServer() {
        return clientInfoFromServer;
    }

    template<typename T>
    static bool DeserializeMessage(const Msg &msg, uint8_t expectedType, T& out) {
        if (msg.data.size() >= sizeof(T)) {
            T message;
            std::memcpy(&message, msg.data.data(), sizeof(T));
            if (message.messageType == expectedType) {
                out = message;
                return true;
            }
        }
        return false;
    }

    template<typename T>
    [[nodiscard]] static std::vector<char> SerializeMessage(const T &message) {
        std::vector<char> buffer(sizeof(T));
        std::memcpy(buffer.data(), &message, sizeof(T));
        return buffer;
    }


private:

    uint64_t nextClientID = 1; // ID unique attribué aux clients
    uint32_t nextStreamID = 1; // ID unique attribué aux Stream
    std::vector<uint32_t> streams; // Liste des Stream
    std::unordered_map<uint32_t, std::vector<MsgStandard>> reliableMessagesReceived;
    std::unordered_map<uint32_t, std::vector<MsgStandard>> reliableMessagesSent;

    SocketType m_socket;

    std::thread m_thread;
    std::atomic<bool> m_running = true;

    std::vector<std::function<void(uint64_t)>> onClientConnectedHandlers;
    std::vector<std::function<void(bool, uint64_t)>> onConnectionEventHandlers;
    std::vector<std::function<void(uint64_t)>> onClientDisconnectedHandlers;
    std::vector<std::function<void()>> onDisconnectHandlers;
    std::vector<std::function<void(uint32_t)>> onStreamCreatedHandlers;

    std::unordered_map<uint64_t,Client> clients; // server reference to clients
    Client clientInfoFromServer; // store client info from server



    int SendToInternal(const std::string& to, uint16_t port, std::span<const char> message);
    int ReceiveFromInternal(std::string& from, std::span<char, 65535> message);
    [[nodiscard]] static std::unique_ptr<Falcon> ListenInternal(const std::string& endpoint, uint16_t port);

    std::pair<std::string, int> portFromIp(const std::string &ip);


    void handleConnectionMessage(const MsgConn &msg_conn, const std::string& msgIp, int msgPort);

    void handleConnectionAckMessage(const MsgConnAck& msg_conn_ack);

    void handleStandardMessage(const MsgStandard& msg_standard);

    void handleAckMessage(const MsgAck & msg_ack);

    void handlePingMessage(const Ping & ping);

    void handleMessage(const Msg& msg);

    uint64_t GetTrace(uint32_t streamID, uint8_t messageID);

};
