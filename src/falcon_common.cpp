#include <thread>
#include <cstring>
#include "falcon.h"
#include <iostream>
#include <mutex>

#include "stream.h"
#include "spdlog/spdlog.h"

template<typename T>
bool Falcon::processMessage(const Msg &msg, uint8_t expectedType, T& out) {
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


std::unique_ptr<Stream> Falcon::CreateStream(uint64_t client, bool reliable) {
    uint32_t streamID = nextStreamID++;
    std::cout << "Creating Stream " << streamID << " for client " << client << "\n";

    auto stream = std::make_unique<Stream>(streamID, reliable);
    streams[streamID] = std::move(stream);
    return std::make_unique<Stream>(streamID, reliable);
}

std::unique_ptr<Stream> Falcon::CreateStream(bool reliable) {
    uint32_t streamID = nextStreamID++;
    std::cout << "Creating Stream " << streamID << " for client\n";

    auto stream = std::make_unique<Stream>(streamID, reliable);
    streams[streamID] = std::move(stream);
    return std::make_unique<Stream>(streamID, reliable);
}

void Falcon::CloseStream(const Stream& stream) {
    std::cout << "Closing Stream " << stream.GetStreamID() << "\n";
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

std::unique_ptr<Falcon> Falcon::Listen(const std::string &endpoint, uint16_t port)
{
    auto falcon = ListenInternal("127.0.0.1", port);

    std::thread([&falcon]() {
        while (true) {
            std::string clientIP;
            int clientPort;
            std::array<char, 65535> buffer;

            int received = falcon->ReceiveFrom(clientIP, std::span<char, 65535>(buffer.data(), sizeof(buffer)));

            if (received < 0) {
                std::cerr << "Failed to receive message\n";
                continue;
            }
            if (received > 0) {
                clientPort = falcon->portFromIp(clientIP);

                Msg msg;
                msg.IP = clientIP;
                msg.Port = clientPort;
                msg.data = std::vector<char>(buffer.begin(), buffer.end());

                std::lock_guard<std::mutex> lock(falcon->queueMutex);
                falcon->messageQueue.push(msg);
            }

        }
    }).detach();

    return falcon;
}


void Falcon::OnClientConnected(std::function<void(uint64_t)> handler) {
    std::thread([this, handler]() {
        std::cout << "Listening for connections on server" << "\n";
        MsgConn msgConn;
        while (true) {
            Msg msg;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (messageQueue.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                msg = messageQueue.front();

                // check if message is a connection message before popping
                if (!processMessage<MsgConn>(msg, MSG_CONN, msgConn)) {
                    continue;
                }
                messageQueue.pop();
            }

            std::cout << "Connection request received from " << msg.IP << ":" << msg.Port << "\n";
            // check if client exists
            bool clientExists = false;
            for (const auto& [id, ip] : clients) {
                if (ip == msg.IP) {
                    clientExists = true;
                    break;
                }
            }
            if (clientExists) {
                std::cout << "Client already exists, ignoring connection request\n";
                continue;
            }

            // add client to list
            uint64_t clientID = nextClientID++;
            clients[clientID] = msg.IP + ":" + std::to_string(msg.Port);
            lastPingsTime[clientID] = std::chrono::steady_clock::now();
            pingedClients[clientID] = false;

            // send clientID to client
            MsgConnAck msgConnAck = {MSG_CONN_ACK, clientID};

            std::vector<char> message(sizeof(msgConnAck));
            std::memcpy(message.data(), &msgConnAck, sizeof(msgConnAck));


            int sent = SendTo(msg.IP, msg.Port, message);

            if (sent < 0) {
                std::cerr << "Failed to send connection ack to " << msg.IP << ": " << msg.Port <<" \n Error: " << sent << "\n";
            } else {
                std::cout << "Connection ack sent to " << msg.IP << ":" << msg.Port << "\n";
            }

            // call handler
            handler(msgConnAck.clientID);
        }
    }).detach();
}

void Falcon::OnConnectionEvent(std::function<void(bool, uint64_t)> handler) {
    std::thread([this, handler]() {
        while (true) {
            MsgConnAck msgConnAck;
            Msg msg;
            {
                std::lock_guard<std::mutex> lock(queueMutex);

                if (messageQueue.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                msg = messageQueue.front();

                // check if message is a connection message before popping
                if (!processMessage<MsgConnAck>(msg, MSG_CONN_ACK, msgConnAck)) {
                    handler(false, 0);
                    break;
                }
                messageQueue.pop();
            }

            handler(true, msgConnAck.clientID);
            break;
        }

    }).detach();
}

void Falcon::OnClientDisconnected(std::function<void(uint64_t)> handler) {
    std::thread([this, handler]() {

        uint8_t pingID = 0;
        std::chrono::steady_clock::time_point time;
        while (true) {
            for (const auto& client: clients) {
                Ping receive_ping;
                Msg msg;
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    if (messageQueue.empty()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                    msg = messageQueue.front();

                    // check if message is a ping message before popping
                    if (!processMessage<Ping>(msg, PING, receive_ping)) {
                        continue;
                    }

                    messageQueue.pop();
                }

                pingedClients[receive_ping.clientID] = false;
                lastPingsTime[receive_ping.clientID] = std::chrono::steady_clock::now();
            }


            for (auto client_ping: lastPingsTime) {
                std::chrono::duration<double> delta_t = std::chrono::steady_clock::now() - client_ping.second;

                uint64_t clientID = client_ping.first;
                std::string clientIP = clients[clientID];



                if (delta_t.count() < 0) {
                    std::cerr << "Error: Negative time difference\n";
                    continue;
                }
                else if (delta_t.count() < 1 && !pingedClients[clientID]) { // less than 1 second
                    continue;
                }
                else if (delta_t.count() < 2) { // 1-2 seconds

                    if (pingedClients[clientID]) {
                        continue;
                    }

                    time = std::chrono::steady_clock::now();
                    Ping ping = {PING, pingID, clientID, time};
                    int port = portFromIp(clientIP);

                    int sent = SendTo(clientIP, port, std::span<const char>((char*)&ping, sizeof(ping)));

                    if (sent < 0) {
                        std::cerr << "Failed to send ping to " << clientIP << ":" << port << "\n";
                    }

                    pingedClients[clientID] = true;
                    std::cout << "Ping sent to " << clientIP << ":" << port << "\n";

                }
                else if (pingedClients[clientID] && delta_t.count() > 2) { // more than 2 seconds
                    clients.erase(clientID);
                    lastPingsTime.erase(clientID);
                    pingedClients.erase(clientID);
                    handler(clientID);
                }
            }

            pingID++;
        }
    }).detach();
}

void Falcon::OnDisconnect(std::function<void()> handler) {
    std::thread([this, handler]() {
        while (true) {
            Ping receive_ping;
            Msg msg;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (messageQueue.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                msg = messageQueue.front();

                // check if message is a ping message before popping
                if (!processMessage<Ping>(msg, PING, receive_ping)) {
                    continue;
                }
                messageQueue.pop();
            }


            int sent = SendTo(msg.IP, msg.Port, std::span<const char>((char*)&receive_ping, sizeof(receive_ping)));

            if (sent < 0) {
                std::cerr << "Failed to send ping ack to " << msg.IP << ":" << msg.Port << "\n";
            }

            std::cout << "Ping ack sent to " << msg.IP << ":" << msg.Port << "\n";
        }
    }).detach();
}

int Falcon::portFromIp(std::string &ip) {
    size_t colonPos = ip.find(':');
    if (colonPos != std::string::npos) {
        int port = std::stoi(ip.substr(colonPos + 1, ip.size()));
        ip = ip.substr(0,colonPos);
        return port;
    } else {
        std::cerr << "Error: Invalid client IP : " << ip << "\n";
        return -1;
    }
}
