#include <thread>
#include <cstring>
#include "falcon.h"
#include <iostream>
#include "stream.h"


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

std::unique_ptr<Falcon> Falcon::Listen(const uint16_t port)
{
    return Listen("127.0.0.1", port);
}

void Falcon::OnClientConnected(std::function<void(uint64_t)> handler) {
    std::thread([this, handler]() {
        std::cout << "Listening for connections on server" << "\n";
        while (true) {
            std::string clientIP;
            int port;
            MsgConn msgConn;
            std::array<char, 65535> buffer;

            int received = ReceiveFrom(clientIP, std::span<char, 65535>(buffer.data(), sizeof(buffer)));

            // extract port from clientIP
            size_t colonPos = clientIP.find(':');
            if (colonPos != std::string::npos) {
                port = std::stoi(clientIP.substr(colonPos + 1));
                clientIP = clientIP.substr(0, colonPos);
                std::cout << "Received connection request from " << clientIP << ":" << port << "\n";
            } else {
                std::cerr << "Error: Invalid client IP : " << clientIP << "\n";
                continue;
            }


            // turn buffer into msgConn
            try {
                std::memcpy(&msgConn, buffer.data(), sizeof(msgConn));
                if (msgConn.messageType != MSG_CONN) {
                    std::cerr << "Error: Received message is not a connection message\n";
                    continue;
                }
            } catch (std::exception& e) {
                std::cerr << "Error: " << e.what() << "\n";
            }


            if (received == sizeof(msgConn)) {

                // check if client exists
                bool clientExists = false;
                for (const auto& [id, ip] : clients) {
                    if (ip == clientIP) {
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
                clients[clientID] = clientIP;

                // send clientID to client
                MsgConnAck msgConnAck = {MSG_CONN_ACK, clientID};

                std::vector<char> buffer(sizeof(msgConnAck));
                std::memcpy(buffer.data(), &msgConnAck, sizeof(msgConnAck));


                int sent = SendTo(clientIP, port, buffer);

                if (sent < 0) {
                    std::cerr << "Failed to send connection ack to " << clientIP << ": " << port <<" \n Error: " << sent << "\n";
                } else {
                    std::cout << "Connection ack sent to " << clientIP << ":" << port << "\n";
                }

                // call handler
                handler(msgConnAck.clientID);
            }
        }
    }).detach();
}

void Falcon::OnConnectionEvent(std::function<void(bool, uint64_t)> handler) {
    std::thread([this, handler]() {
        while (true) {
            std::string serverIP;
            std::array<char, 65535> buffer{};

            int received = ReceiveFrom(serverIP, std::span<char, 65535>(buffer.data(), sizeof(buffer)));

            // turn buffer into msgConnAck
            MsgConnAck msgConnAck;
            try {
                std::memcpy(&msgConnAck, buffer.data(), sizeof(msgConnAck));
                if (msgConnAck.messageType != MSG_CONN_ACK) {
                    std::cerr << "Error: Received message is not a connection message\n";
                    continue;
                }
            } catch (std::exception& e) {
                std::cerr << "Error: " << e.what() << "\n";
            }

            // check if message is a connection message
            if (received == sizeof(msgConnAck)) {
                handler(true, msgConnAck.clientID);
            } else {
                handler(false, 0);
            }
        }
    }).detach();
}