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
    streamID |= SERVERSTREAMMASK;
    if (reliable)
        streamID |= RELIABLESTREAMMASK;
    else
        streamID &= ~RELIABLESTREAMMASK;

    auto stream = std::make_unique<Stream>(streamID, clients[client].IP, clients[client].Port, *this);
    streams.push_back(stream->streamID);
    return stream;
}

std::unique_ptr<Stream> Falcon::CreateStream(bool reliable) {
    uint32_t streamID = nextStreamID++;
    streamID &= ~SERVERSTREAMMASK;
    if (reliable)
        streamID |= RELIABLESTREAMMASK;
    else
        streamID &= ~RELIABLESTREAMMASK;

    auto stream = std::make_unique<Stream>(streamID, clientInfoFromServer.IP, clientInfoFromServer.Port, *this);
    streams.push_back(stream->streamID);
    return stream;
}

void Falcon::CloseStream(const Stream& stream) {
    // spdlog::debug("Closing Stream {}", stream.GetStreamID());
    streams.erase(std::remove_if(streams.begin(), streams.end(), [&](const auto& id) {
        return id == stream.streamID;
    }), streams.end());
}

int Falcon::SendTo(const std::string &to, uint16_t port, const std::span<const char> message)
{
    int sent = SendToInternal(to, port, message);
    if (sent > 0) {
        Msg msg;
        msg.IP = to;
        msg.Port = port;
        msg.data = std::vector<char>(message.begin(), message.end());

        if (MsgStandard msg_standard{}; DeserializeMessage(msg, MSG_STANDARD, msg_standard)) {
            // add to the front of the queue
            reliableMessagesSent[msg_standard.streamID].insert(reliableMessagesSent[msg_standard.streamID].begin(), msg_standard);
            if (reliableMessagesSent[msg_standard.streamID].size() > 64) {
                reliableMessagesSent[msg_standard.streamID].pop_back();
            }
        }
    }
    return sent;
}

int Falcon::ReceiveFrom(std::string& from, const std::span<char, 65535> message)
{
    return ReceiveFromInternal(from, message);
}

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
                    int sent = falcon->SendTo(c.IP, c.Port, Falcon::SerializeMessage(Ping{PING}));
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

void Falcon::OnStreamCreated(const std::function<void(uint32_t)> &handler) {
    onStreamCreatedHandlers.push_back(handler);
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
    auto stream = std::find_if(streams.begin(), streams.end(), [&](const auto& id) {
        return id == msg_standard.streamID;
    });

    if (stream == streams.end()) {
        std::cout << "Warning: Stream " << msg_standard.streamID << " does not exist, creating it on local!\n";
        std::string ip = clients[msg_standard.clientID].IP;
        int port = clients[msg_standard.clientID].Port;
        auto newStream = std::make_unique<Stream>(msg_standard.streamID, ip, port, *this);
        streams.push_back(newStream->streamID);

        for (const auto& handler: onStreamCreatedHandlers) {
            handler(newStream->streamID);
        }
    }
    Stream::OnDataReceived(msg_standard.data);

    if (Stream::IsReliable(msg_standard.streamID)) {
        reliableMessagesReceived[msg_standard.streamID].insert(reliableMessagesReceived[msg_standard.streamID].begin(), msg_standard);
        if (reliableMessagesReceived[msg_standard.streamID].size() > 64) {
            reliableMessagesReceived[msg_standard.streamID].pop_back();
        }

        uint64_t trace = GetTrace(msg_standard.streamID, msg_standard.messageID);

        // send ack
        const MsgAck msgAck = {MSG_ACK, msg_standard.clientID, msg_standard.streamID, msg_standard.messageID, trace};
        int sent = SendTo(clients[msg_standard.clientID].IP, clients[msg_standard.clientID].Port, SerializeMessage(msgAck));
        if (sent < 0) {
            std::cerr << "Failed to send ack\n";
        }
    }

}

void Falcon::handleAckMessage(const MsgAck &msg_ack) {
    std::cout << "Ack received from " << msg_ack.clientID << " on stream " << msg_ack.streamID << "\n";
    for (auto& m : reliableMessagesSent[msg_ack.streamID]) {
        int delta = msg_ack.messageID - m.messageID;

        if (msg_ack.trace & (RELIABLE_ACK_MASK >> delta)) {
            reliableMessagesSent[msg_ack.streamID].erase(std::remove_if(reliableMessagesSent[msg_ack.streamID].begin(), reliableMessagesSent[msg_ack.streamID].end(), [&](const auto& msg) {
                return msg.messageID == m.messageID;
            }), reliableMessagesSent[msg_ack.streamID].end());
        }
    }

    // resend lost packets
    for (const auto& m : reliableMessagesSent[msg_ack.streamID]) {
        int sent = SendTo(clients[msg_ack.clientID].IP, clients[msg_ack.clientID].Port, SerializeMessage(m));
        if (sent < 0) {
            std::cerr << "Failed to resend lost packet\n";
        }
    }
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

uint64_t Falcon::GetTrace(uint32_t streamID, uint8_t messageID) {
    uint64_t trace = 0;
    for (const MsgStandard& m : reliableMessagesReceived[streamID]) {
        int delta = messageID - m.messageID;
        if (delta >= 0 && delta < 64) {
            trace |= RELIABLE_ACK_MASK >> delta;
        }
    }
    return trace;
}