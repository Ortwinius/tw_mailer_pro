#include <iostream>
#include "server.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) 
{
    if (argc != 3) 
    {
        std::cerr << "Usage: ./twmailer-server <port> <mail-spool-directoryname>\n";
        return EXIT_FAILURE;
    }

    int port = std::stoi(argv[1]);
    fs::path mailDirectory(argv[2]);

    try 
    {
        Server server(port, mailDirectory);
        server.run();
    } 
    catch (const std::exception &e) 
    {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
