#include "../Socket/Socket.h"
#include <iostream>

int main() {
    Socket server;
    if (!server.bind("0.0.0.0", 5555)) {
        std::cerr << "Failed to bind server socket.\n";
        return 1;
    }

    std::cout << "Server is running on port 5555...\n";

    while (true) {
        std::string senderAddress;
        int senderPort;
        std::string message = server.receiveFrom(senderAddress, senderPort); // passed by reference

        std::cout << "Received: " << message << " from " << senderAddress << ":" << senderPort << "\n";

        server.sendTo(senderAddress, senderPort, message);
    }

    return 0;
}