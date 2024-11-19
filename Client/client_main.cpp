#include "client.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: ./twmailer-client <ip> <port>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string ip = argv[1];
    int port = std::stoi(argv[2]);

    try {
        Client client_instance(ip, port);
        client_instance.start();
    } catch (const std::exception &e) {
        std::cout << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
