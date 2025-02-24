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
    auto stream = std::make_unique<Stream>(this, reliable, client);
    streams.push_back(std::move(stream));
    return stream;
}

std::unique_ptr<Stream> Falcon::CreateStream(bool reliable) {
    auto stream = std::make_unique<Stream>(reliable);
    streams.push_back(std::move(stream));
    return std::make_unique<Stream>(reliable);
}

void Falcon::CloseStream(const Stream& stream) {
    // spdlog::debug("Closing Stream {}", stream.GetStreamID());
    std::erase_if(streams, [&](const auto& s) {
        return s->GetStreamID() == stream.GetStreamID();
    });
}

int Falcon::SendTo(const std::string &to, uint16_t port, const std::span<const char> message)
{
    return SendToInternal(to, port, message);
}

int Falcon::ReceiveFrom(std::string& from, const std::span<char, 65535> message)
{
    return ReceiveFromInternal(from, message);
}

/*
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
*/

std::unique_ptr<Falcon> Falcon::Listen(const std::string &endpoint, const uint16_t port)
{
    auto falcon = ListenInternal("127.0.0.1", port);

    // server thread to handle messages
    falcon->m_thread = std::thread([&falcon]() {
        while (falcon->m_running) {
            std::string fullClientIP;
            std::vector<char> buffer(65535);

            int received = falcon->ReceiveFrom(fullClientIP, std::span<char, 65535>(buffer.data(), buffer.size()));

            if (received < 0) {
                // std::cerr << "Failed to receive message\n";
            }
            if (received > 0) {
                auto [IP, port] = falcon->portFromIp(fullClientIP);

                Msg msg;
                msg.IP = IP;
                msg.Port = port;
                msg.data = std::vector<char>(buffer.begin(), buffer.end());

                for (auto& [id,c]: falcon->clients) {
                    if (c.IP == IP && c.Port == port) {
                        c.lastPing = std::chrono::steady_clock::now();
                        c.pinged = false;
                        break;
                    }
                }

                falcon->handleMessage(msg);
            }

            for (auto& [id,c]: falcon->clients) {
                std::chrono::steady_clock::duration delta_time = std::chrono::steady_clock::now() - c.lastPing;

                // std::cout << "Client " << c.ID << " last pinged " << std::chrono::duration_cast<std::chrono::seconds>(delta_time).count() << " seconds ago\n";
                // std::cout << "Client " << c.ID << " pinged " << c.pinged << "\n";

                if (delta_time > std::chrono::seconds(1) && !c.pinged) {
                    c.pinged = true;
                    int sent = falcon->SendTo(c.IP, c.Port, falcon->SerializeMessage(Ping{PING}));
                    if (sent < 0) {
                        std::cerr << "Failed to ping client " << c.ID << "\n";
                    }
                    else {
                        std::cout << "Pinging client " << c.ID << "\n";
                    }
                }
                else if (delta_time > std::chrono::seconds(2) && c.pinged) {
                    falcon->clients.erase(id);
                    std::cerr << "Client " << c.ID << " disconnected\n";

                    for (const auto& handler: falcon->onClientDisconnectedHandlers) {
                        handler(c.ID);
                    }
                    break;
                }
            }
        }
    });

    return falcon;
}


void Falcon::OnClientConnected(const std::function<void(uint64_t)>& handler) {
    onClientConnectedHandlers.push_back(handler);
}

void Falcon::OnConnectionEvent(const std::function<void(bool, uint64_t)> &handler) {
    onConnectionEventHandlers.push_back(handler);
}

void Falcon::OnClientDisconnected(const std::function<void(uint64_t)>& handler) {
    onClientDisconnectedHandlers.push_back(handler);
}

void Falcon::OnDisconnect(const std::function<void()>& handler) {
    onDisconnectHandlers.push_back(handler);
}



void Falcon::handleMessage(const Msg &msg) {
    if (MsgConn msg_conn; DeserializeMessage(msg, MSG_CONN, msg_conn)) {
        handleConnectionMessage(msg_conn, msg.IP, msg.Port);
    }
    else if (MsgConnAck msg_conn_ack; DeserializeMessage(msg, MSG_CONN_ACK, msg_conn_ack)) {
        handleConnectionAckMessage(msg_conn_ack);
    }
    else if (MsgStandard msg_standard; DeserializeMessage(msg, MSG_STANDARD, msg_standard)) {
        handleStandardMessage(msg_standard);
    }
    else if (MsgAck msg_ack; DeserializeMessage(msg, MSG_ACK, msg_ack)) {
        handleAckMessage(msg_ack);
    }
    else if (Ping ping; DeserializeMessage(msg, PING, ping)) {
        handlePingMessage(ping);
    }
    else {
        std::cerr << "Error: Failed to deserialize message\n";

    }
}

void Falcon::handleConnectionMessage(const MsgConn &msg_conn, const std::string& msgIp, int msgPort) {
    // check if client exists
    for (auto& [id,c] : clients) {
        if (c.IP == msgIp && c.Port == msgPort) {
            std::cerr << "Client already exists\n";
            return;
        }
    }

    // add client to list
    uint64_t clientID = nextClientID++;

    clients[clientID] = {clientID, msgIp, msgPort,false, std::chrono::steady_clock::now()};

    // send clientID to client
    const MsgConnAck msgConnAck = {MSG_CONN_ACK, clientID};

    int sent = SendTo(msgIp, msgPort, SerializeMessage(msgConnAck));

    if (sent < 0) {
        std::cerr << "Failed to send connection ack to " << msgIp << ":" << msgPort << "\n";
    } else {
        std::cout << "Connection ack sent to " << msgIp << ":" << msgPort << "\n";
        for (const auto& handler: onClientConnectedHandlers) {
            handler(clientID);
        }
    }
}

void Falcon::handleConnectionAckMessage(const MsgConnAck &msg_conn_ack) {
    clientInfoFromServer.ID = msg_conn_ack.clientID;
    for (const auto& handler: onConnectionEventHandlers) {
        handler(true, msg_conn_ack.clientID);
    }
}

void Falcon::handleStandardMessage(const MsgStandard &msg_standard) {
    std::cout << "From " << msg_standard.clientID << " On Stream " << msg_standard.streamID << "\n";
    // get the stream
    auto stream = std::find_if(streams.begin(), streams.end(), [&](const auto& s) {
        return s->GetStreamID() == msg_standard.streamID;
    });

    if (stream != streams.end()) {

        (*stream)->OnDataReceived(SerializeMessage(msg_standard));
    }
    else {
        std::cerr << "Error: Stream " << msg_standard.streamID << " does not exist!\n";
    }
}

void Falcon::handleAckMessage(const MsgAck &msg_ack) {
    // TODO: handle ack
}

void Falcon::handlePingMessage(const Ping &ping) {
    std::cout << "Ping " << ping.pingID << "received\n";
    if (clientInfoFromServer.ID != 0) { // check if we are client
        std::cout << "Ponging\n";
        int sent = SendTo(clientInfoFromServer.IP, clientInfoFromServer.Port, SerializeMessage(Ping{PING, clientInfoFromServer.ID, ping.pingID, ping.time}));
        if (sent < 0) {
            std::cerr << "Failed to send pong\n";
        }
    }
}

