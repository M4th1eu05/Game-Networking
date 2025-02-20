#include <thread>
#include <cstring>
#include "falcon.h"
#include <iostream>
#include <mutex>
#include <chrono>

std::pair<std::string, int> Falcon::portFromIp(const std::string& ip) {
    int colonPos = ip.rfind(':');
    if (colonPos != std::string::npos && ip.find(']') == std::string::npos) {
        int port = std::stoi(ip.substr(colonPos + 1, ip.size()));
        std::string ipAddress = ip.substr(0, colonPos);
        return {ipAddress, port};
    } else if (ip.find('[') != std::string::npos && ip.find(']') != std::string::npos) {
        int portPos = ip.rfind(':');
        int bracketPos = ip.rfind(']');
        if (portPos != std::string::npos && portPos > bracketPos) {
            int port = std::stoi(ip.substr(portPos + 1, ip.size()));
            std::string ipAddress = ip.substr(1, bracketPos - 1);
            return {ipAddress, port};
        }
    }
    std::cerr << "Error: Invalid client IP : " << ip << "\n";
    return {"", -1};
}


std::unique_ptr<Stream> Falcon::CreateStream(uint64_t client, bool reliable) {
    uint32_t streamID = nextStreamID++;
    // spdlog::debug("Creating Stream {} for client {}", streamID, client);

    auto stream = std::make_unique<Stream>(streamID, reliable);
    streams[streamID] = std::move(stream);
    return std::make_unique<Stream>(streamID, reliable);
}

std::unique_ptr<Stream> Falcon::CreateStream(bool reliable) {
    uint32_t streamID = nextStreamID++;

    // spdlog::debug("Creating Stream {}", streamID);

    auto stream = std::make_unique<Stream>(streamID, reliable);
    streams[streamID] = std::move(stream);
    return std::make_unique<Stream>(streamID, reliable);
}

void Falcon::CloseStream(const Stream& stream) {
    // spdlog::debug("Closing Stream {}", stream.GetStreamID());
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


void Falcon::SendData(uint32_t streamID, std::span<const char> data) {
    if (streams.find(streamID) != streams.end()) {
        streams[streamID]->SendData(data);
    } else {
        std::cerr << "Error: Stream " << streamID << " does not exist!\n";
    }
}

void Falcon::OnDataReceived(uint32_t streamID, std::function<void(std::span<const char>)> handler) {
    if (streams.find(streamID) != streams.end()) {
        streams[streamID]->OnDataReceived(handler);
    } else {
        std::cerr << "Error: Stream " << streamID << " does not exist!\n";
    }
}

std::unique_ptr<Falcon> Falcon::Listen(const std::string &endpoint, const uint16_t port)
{
    auto falcon = ListenInternal("127.0.0.1", port);

    // server thread to handle messages
    std::thread([&falcon]() {
        while (true) {
            std::string fullClientIP;
            std::vector<char> buffer(65535);

            int received = falcon->ReceiveFrom(fullClientIP, std::span<char, 65535>(buffer.data(), buffer.size()));

            if (received < 0) {
                // std::cerr << "Failed to receive message\n";
                for (auto [id,c]: falcon->clients) {
                    if (c.lastPing + std::chrono::seconds(5) < std::chrono::steady_clock::now()) {
                        falcon->clients.erase(id);
                        std::cout << "Client " << c.ID << " disconnected\n";
                        // TODO: call handler disconnect
                    }
                }
                continue;
            }
            if (received > 0) {
                auto [IP, port] = falcon->portFromIp(fullClientIP);

                Msg msg;
                msg.IP = IP;
                msg.Port = port;
                msg.data = std::vector<char>(buffer.begin(), buffer.end());

                for (auto [id,c]: falcon->clients) {
                    if (c.IP == IP && c.Port == port) {
                        c.lastPing = std::chrono::steady_clock::now();
                        break;
                    }
                }

                falcon->handleMessage(msg);
            }
        }
    }).detach();

    return falcon;
}


void Falcon::OnClientConnected(const std::function<void(uint64_t)>& handler) {
    // std::thread([this, handler]() {
    //     // spdlog::debug("Listening for connections on server");
    //     MsgConn msgConn;
    //     while (true) {
    //         Msg msg;
    //         {
    //             std::lock_guard<std::mutex> lock(queueMutex);
    //             if (messageQueue.empty()) {
    //                 std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //                 continue;
    //             }
    //             msg = messageQueue.front();
    //
    //             // check if message is a connection message before popping
    //             if (!deserializeMessage<MsgConn>(msg, MSG_CONN, msgConn)) {
    //                 continue;
    //             }
    //             messageQueue.pop();
    //         }
    //
    //         std::cout << "Client connected from " << msg.IP << ":" << msg.Port << std::endl;
    //         // check if client exists
    //         bool clientExists = false;
    //         std::lock_guard<std::mutex> lock_clients(clientsMutex);
    //         for (const auto& [id, ip] : clients) {
    //             if (ip == msg.IP) {
    //                 clientExists = true;
    //                 break;
    //             }
    //         }
    //         if (clientExists) {
    //             // spdlog::debug("Client already exists, ignoring connection request\n");
    //             continue;
    //         }
    //
    //         // add client to list
    //         uint64_t clientID = nextClientID++;
    //         clients[clientID] = msg.IP + ":" + std::to_string(msg.Port);
    //         std::lock_guard<std::mutex> lock_ping(lastPingsTimeMutex);
    //         lastPingsTime[clientID] = std::chrono::steady_clock::now();
    //         std::lock_guard<std::mutex> lock_pinged(pingedClientsMutex);
    //         pingedClients[clientID] = false;
    //
    //         // send clientID to client
    //         MsgConnAck msgConnAck = {MSG_CONN_ACK, clientID};
    //
    //         std::vector<char> message(sizeof(msgConnAck));
    //         std::memcpy(message.data(), &msgConnAck, sizeof(msgConnAck));
    //
    //
    //         int sent = SendTo(msg.IP, msg.Port, message);
    //
    //         if (sent < 0) {
    //             std::cerr << "Failed to send connection ack to " << msg.IP << ":" << msg.Port << std::endl;
    //         } else {
    //             // spdlog::debug("Connection ack sent to {}:{}\n", msg.IP, msg.Port);
    //         }
    //
    //         // call handler
    //         handler(msgConnAck.clientID);
    //     }
    // }).detach();
}

void Falcon::OnConnectionEvent(std::function<void(bool, uint64_t)> handler) {
    // std::thread([this, handler]() {
    //     while (true) {
    //         MsgConnAck msgConnAck;
    //         Msg msg;
    //         {
    //             std::lock_guard<std::mutex> lock(queueMutex);
    //
    //             if (messageQueue.empty()) {
    //                 std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //                 continue;
    //             }
    //             msg = messageQueue.front();
    //
    //             // check if message is a connection message before popping
    //             if (!deserializeMessage<MsgConnAck>(msg, MSG_CONN_ACK, msgConnAck)) {
    //                 handler(false, 0);
    //                 break;
    //             }
    //             messageQueue.pop();
    //         }
    //
    //         handler(true, msgConnAck.clientID);
    //         break;
    //     }
    //
    // }).detach();
}

void Falcon::OnClientDisconnected(std::function<void(uint64_t)> handler) {
    // std::thread([this, handler]() {
    //
    //     uint8_t pingID = 0;
    //     std::chrono::steady_clock::time_point time;
    //     while (true) {
    //         std::lock_guard<std::mutex> lock_clients(clientsMutex);
    //         for (const auto& client: clients) {
    //             Ping receive_ping;
    //             Msg msg;
    //             {
    //                 std::lock_guard<std::mutex> lock(queueMutex);
    //                 if (messageQueue.empty()) {
    //                     std::this_thread::sleep_for(std::chrono::milliseconds(1));
    //                     continue;
    //                 }
    //                 msg = messageQueue.front();
    //
    //                 // check if message is a ping message before popping
    //                 if (!deserializeMessage<Ping>(msg, PING, receive_ping)) {
    //                     continue;
    //                 }
    //
    //                 messageQueue.pop();
    //             }
    //
    //             std::lock_guard<std::mutex> lock_pinged(pingedClientsMutex);
    //             pingedClients[receive_ping.clientID] = false;
    //             std::lock_guard<std::mutex> lock_ping(lastPingsTimeMutex);
    //             lastPingsTime[receive_ping.clientID] = std::chrono::steady_clock::now();
    //         }
    //
    //
    //         std::lock_guard<std::mutex> lock_pinged(lastPingsTimeMutex);
    //         for (auto client_ping: lastPingsTime) {
    //             std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - client_ping.second;
    //
    //             uint64_t clientID = client_ping.first;
    //             std::string clientIP = clients[clientID];
    //
    //
    //
    //             if (delta_t.count() < 0) {
    //                 std::cerr << "Error: Negative time difference\n";
    //                 continue;
    //             }
    //             else if (delta_t.count() < 1 && !pingedClients[clientID]) { // less than 1 second
    //                 continue;
    //             }
    //             else if (delta_t.count() < 2) { // 1-2 seconds
    //
    //                 if (pingedClients[clientID]) {
    //                     continue;
    //                 }
    //
    //                 time = std::chrono::steady_clock::now();
    //                 Ping ping = {PING, pingID, clientID, time};
    //                 int port = portFromIp(clientIP);
    //
    //                 int sent = SendTo(clientIP, port, std::span<const char>((char*)&ping, sizeof(ping)));
    //
    //                 if (sent < 0) {
    //                     // spdlog::error("Failed to send ping to {}:{}\n", clientIP, port);
    //                 }
    //
    //                 pingedClients[clientID] = true;
    //                 // spdlog::debug("Ping sent to {}:{}\n", clientIP, port);
    //
    //             }
    //             else if (pingedClients[clientID] && delta_t.count() > 2) { // more than 2 seconds
    //                 clients.erase(clientID);
    //                 lastPingsTime.erase(clientID);
    //                 pingedClients.erase(clientID);
    //                 handler(clientID);
    //             }
    //         }
    //
    //         pingID++;
    //     }
    // }).detach();
}

void Falcon::OnDisconnect(std::function<void()> handler) {
    // std::thread([this, handler]() {
    //     std::chrono::steady_clock::time_point time = std::chrono::steady_clock::now();
    //
    //     while (true) {
    //         std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - time;
    //         if (delta_t.count() > 5) {
    //             break;
    //         }
    //
    //         Ping receive_ping;
    //         Msg msg;
    //         {
    //             std::lock_guard<std::mutex> lock(queueMutex);
    //             if (messageQueue.empty()) {
    //                 std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //                 continue;
    //             }
    //             msg = messageQueue.front();
    //
    //             // check if message is a ping message before popping
    //             if (!deserializeMessage<Ping>(msg, PING, receive_ping)) {
    //                 continue;
    //             }
    //             messageQueue.pop();
    //         }
    //
    //
    //         int sent = SendTo(msg.IP, msg.Port, std::span<const char>((char*)&receive_ping, sizeof(receive_ping)));
    //
    //         if (sent < 0) {
    //             // spdlog::error("Failed to send ping ack to {}:{}\n", msg.IP, msg.Port);
    //         }
    //
    //         time = std::chrono::steady_clock::now();
    //
    //         // spdlog::debug("Ping ack sent to {}:{}\n", msg.IP, msg.Port);
    //     }
    //     handler();
    // }).detach();
}



void Falcon::handleMessage(const Msg &msg) {
    // try to get message type
    uint8_t messageType;
    if (msg.data.size() >= sizeof(uint8_t)) {
        std::memcpy(&messageType, msg.data.data(), sizeof(uint8_t));
    } else {
        std::cerr << "Error: Message too short\n";
        return;
    }
    switch (messageType) {
        case MSG_CONN: {
            MsgConn msgConn{};
            if (deserializeMessage<MsgConn>(msg, MSG_CONN, msgConn)) {
                handleConnectionMessage(msgConn, msg.IP, msg.Port);
            }
            break;
        }
        case MSG_CONN_ACK: {
            MsgConnAck msgConnAck{};
            if (deserializeMessage<MsgConnAck>(msg, MSG_CONN_ACK, msgConnAck)) {
                handleConnectionAckMessage(msgConnAck);
            }
            break;
        }
        case MSG_STANDARD: {
            MsgStandard msgStandard{};
            if (deserializeMessage<MsgStandard>(msg, MSG_STANDARD, msgStandard)) {
                handleStandardMessage(msgStandard);
            }
            break;
        }
        case MSG_ACK: {
            MsgAck msgAck{};
            if (deserializeMessage<MsgAck>(msg, MSG_ACK, msgAck)) {
                handleAckMessage(msgAck);
            }
            break;
        }
        case PING: {
            Ping ping{};
            if (deserializeMessage<Ping>(msg, PING, ping)) {
                handlePingMessage(ping);
            }
            break;
        }
        default:
            std::cerr << "Error: Unknown message type " << messageType << "\n";
    }
}

void Falcon::handleConnectionMessage(const MsgConn &msg_conn, const std::string& msgIp, int msgPort) {
    // check if client exists
    std::string fullIp = msgIp + ":" + std::to_string(msgPort);
    for (auto [id,c] : clients) {
        if (c.IP == msgIp) {
            // spdlog::debug("Client already exists, ignoring connection request\n");
            return;
        }
    }

    // add client to list
    uint64_t clientID = nextClientID++;
    clients[clientID] = {clientID, msgIp, msgPort, std::chrono::steady_clock::now()};
    // pingedClients[clientID] = false;

    // send clientID to client
    const MsgConnAck msgConnAck = {MSG_CONN_ACK, clientID};

    int sent = SendTo(msgIp, msgPort, serializeMessage(msgConnAck));

    if (sent < 0) {
        std::cerr << "Failed to send connection ack to " << msgIp << ":" << msgPort << "\n";
    } else {
        // TODO: call handler
    }
}

void Falcon::handleConnectionAckMessage(const MsgConnAck &msg_conn_ack) {
    m_client = Client{msg_conn_ack.clientID, "", 0, std::chrono::steady_clock::now()};
    // TODO: call handler
}

void Falcon::handleStandardMessage(const MsgStandard &msg_standard) {
}

void Falcon::handleAckMessage(const MsgAck &msg_ack) {
}

void Falcon::handlePingMessage(const Ping &ping) {
}

