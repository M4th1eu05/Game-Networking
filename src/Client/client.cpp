#include "../Socket/Socket.h"
#include <iostream>

int main() {
    Socket client;

    std::string serverAddress = "127.0.0.1";
    unsigned short serverPort = 5555;

    while (true) {
        std::cout << "Enter message: ";
        std::string message;
        std::getline(std::cin, message);

        client.sendTo(serverAddress, serverPort, message);

        std::string senderAddress;
        int senderPort;
        std::string response = client.receiveFrom(senderAddress, senderPort);

        std::cout << "Server replied: " << response << "\n";
    }

    return 0;
}
