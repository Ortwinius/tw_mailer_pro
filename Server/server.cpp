#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <stdexcept>
#include <cstring>
#include <fstream>
#include <sstream>
#include "server.h"

namespace fs = std::filesystem;

Server::Server(int port, const fs::path &mailDirectory) 
: port(port)
, mail_directory(mailDirectory)
{
    init_socket();
}

Server::~Server()
{
    close(socket_fd);
}

void Server::run()
{
    listen_for_connections();
}

void Server::init_socket()
{
    // AF_INET (IPv4), SOCK_STREAM (typically TCP), 0 (default protocol)
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        throw std::runtime_error("Error initializing the server socket: " + std::to_string(errno));
    }

    std::cout << "Socket was created with id " << socket_fd << "\n";

    // set up server address struct to bind socket to a specific address
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY; // Accept connections from any interface
    serveraddr.sin_port = htons(port);

    // set socket options (SO_REUSEADDRE - reusing local address and same port even if in TIME_WAIT)
    int enable = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1)
    {
        throw std::runtime_error("Unable to set socket options due to: " + std::to_string(errno));
    }

    // binds the socket with a specific address
    if (bind(socket_fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
    {
        throw std::runtime_error("Binding failed with error: " + std::to_string(errno));
    }

    // socket starts listening for connection requests
    if (listen(socket_fd, 6) == -1)
    {
        throw std::runtime_error("Unable to establish connection: " + std::to_string(errno));
    }
}

void Server::listen_for_connections()
{
    while (true)
    {
        struct sockaddr client;
        socklen_t addrlen = sizeof(client);
        int peersoc = accept(socket_fd, &client, &addrlen);
        if (peersoc == -1)
        {
            std::cerr << "Unable to accept client connection.\n";
            continue; // Continue accepting other connections
        }

        std::cout << "Accepted connection with file descriptor: " << peersoc << "\n";
        handle_communication(peersoc);
    }
}

void Server::handle_communication(int consfd)
{
    const int bufferSize = 1024;
    char buffer[bufferSize];

    while (true)
    {
        ssize_t size = recv(consfd, buffer, bufferSize - 1, 0);
        if (size <= 0)
        {
            std::cerr << "Receive error or connection closed.\n";
            break;
        }

        buffer[size] = '\0'; // Null-terminate the received message at currentSize (counted by recv)

        std::cout << "Received: " << buffer << "\n";

        // QUIT to close conn
        if (strncmp(buffer, "QUIT", 4) == 0)
        {
            std::cout << "Closing connection with client" << std::endl;
            close(consfd);
            break;
        }
        if (strncmp(buffer, "SEND", 4) == 0)
        {
            std::cout << "Processing SEND command" << std::endl;
            handle_send(consfd, buffer);
        }
        else if (strncmp(buffer, "LIST", 4) == 0)
        {
            std::cout << "Processing LIST command" << std::endl;
            handle_list(consfd, buffer);
        }
        else if (strncmp(buffer, "READ", 4) == 0)
        {
            std::cout << "Processing READ command" << std::endl;
            handle_read(consfd, buffer);
        }
        else if (strncmp(buffer, "DEL", 3) == 0)
        {
            std::cout << "Processing DEL command" << std::endl;
            handle_delete(consfd, buffer);
        }
        else
        {
            std::cout << "Unknown command received" << std::endl;
            send(consfd, "ERR\n", 4, 0); // Respond with an error message
        }
    }
    close(consfd); // Ensure the peer socket is closed
}

void Server::handle_list(int consfd, const std::string &buffer)
{
    std::istringstream iss(buffer.substr(5)); // Extract the part after "LIST "
    std::string username;
    std::getline(iss, username); // Read the username

    if (!username.empty())
    {
        fs::path userInbox = mail_directory / username; // Path to the user's inbox
        if (!fs::exists(userInbox) || !fs::is_directory(userInbox))
        {
            std::cout << "No messages or user unknown" << std::endl;
            send(consfd, "0\n", 2, 0);
            return;
        }

        int messageCount = 0;
        std::ostringstream response;

        // Iterate over the directory and gather subjects in one pass
        int counter = 0;
        for (const auto &entry : fs::directory_iterator(userInbox))
        {
            counter++;
            if (fs::is_regular_file(entry.path()))
            {
                std::ifstream messageFile(entry.path());
                std::string line;
                std::string subject;

                std::getline(messageFile, line);
                std::getline(messageFile, line); // skip sender and receiver

                if (std::getline(messageFile, subject))
                {
                    messageCount++;
                    response << "[" << counter << "] " << subject << "\n"; // Add subject to the response
                }
            }
        }

        // Construct the final response with the count first
        std::ostringstream finalResponse;
        finalResponse << messageCount << "\n"; // Add the message count first
        finalResponse << response.str();       // Append all subjects

        // Send the complete response
        send(consfd, finalResponse.str().c_str(), finalResponse.str().size(), 0);
    }
    else
    {
        send(consfd, "ERR\n", 4, 0); // Send error if username is not provided
    }
}

void Server::handle_send(int consfd, const std::string &buffer)
{
    std::string command;
    std::string sender;
    std::string receiver;
    std::string subject;
    std::string message;

    std::istringstream iss(buffer);
    std::getline(iss, command, '\n'); // skip command

    if (!std::getline(iss, sender) || sender.empty() || (!is_valid_username(sender)))
    {
        send(consfd, "ERR\n", 4, 0);
        return;
    }

    if (!std::getline(iss, receiver) || receiver.empty() || (!is_valid_username(receiver)))
    {
        send(consfd, "ERR\n", 4, 0);
        return;
    }

    if (!std::getline(iss, subject) || subject.empty())
    {
        send(consfd, "ERR\n", 4, 0);
        return;
    }

    std::string line;
    while (std::getline(iss, line))
    {
        if (line == ".")
            break;
        else
            message += line + "\n";
    }

    fs::path receiverPath = mail_directory / receiver / (std::to_string(std::time(nullptr)) + "_" + sender + ".txt");
    fs::create_directories(mail_directory / receiver);

    std::ofstream messageFile(receiverPath);
    if (messageFile.is_open())
    {
        // Write the structured message
        messageFile << sender << "\n";
        messageFile << receiver << "\n";
        messageFile << subject << "\n";
        messageFile << message;

        std::cout << "Saved Mail " << subject << " in inbox of " << receiver << std::endl;
        send(consfd, "OK\n", 3, 0);
    }
    else
    {
        send(consfd, "ERR\n", 4, 0);
    }
}

void Server::handle_read(int consfd, const std::string &buffer)
{
    std::string command;
    std::string username;
    std::string messageNrString;

    std::istringstream iss(buffer);
    std::getline(iss, command, '\n'); // skip command

    if (!std::getline(iss, username) || username.empty())
    {
        send(consfd, "ERR\n", 4, 0);
        return;
    }

    if (!std::getline(iss, messageNrString) || messageNrString.empty())
    {
        send(consfd, "ERR\n", 4, 0);
        return;
    }

    int messageNr=-1;
    try
    {
        messageNr = std::stoi(messageNrString);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        send(consfd, "ERR\n", 4, 0);
        return;
    }

    fs::path userInbox = mail_directory / username; // Path to the user's inbox
    if (!fs::exists(userInbox) || !fs::is_directory(userInbox))
    {
        std::cout << "No messages or user unknown" << std::endl;
        send(consfd, "ERR\n", 4, 0);
        return;
    }

    int counter=0;
    for (const auto &entry : fs::directory_iterator(userInbox))
    {
        counter++;
        if (counter==messageNr)
        {
            std::ifstream messageFile(entry.path());
            if (!messageFile.is_open())
            {
                send(consfd, "ERR\n", 4, 0); // Unable to open the message file
                return;
            }

            std::ostringstream messageContent;
            std::string line;

            while(std::getline(messageFile, line))
            {
                messageContent<<line<<"\n";
            }

            std::string response = "OK\n" + messageContent.str() + "\n"; // Add an additional newline at the end
            send(consfd, response.c_str(), response.size(), 0);
            return;
        }
    }

    // invalid message number
    send(consfd, "ERR\n", 4, 0);
}

void Server::handle_delete(int consfd, const std::string &buffer)
{
    std::string command;
    std::string username;
    std::string messageNrString;

    std::istringstream iss(buffer);
    std::getline(iss, command, '\n'); // Skip the command

    if (!std::getline(iss, username) || username.empty())
    {
        send(consfd, "ERR\n", 4, 0);
        return;
    }

    if (!std::getline(iss, messageNrString) || messageNrString.empty())
    {
        send(consfd, "ERR\n", 4, 0);
        return;
    }

    int messageNr = -1;
    try
    {
        messageNr = std::stoi(messageNrString);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        send(consfd, "ERR\n", 4, 0);
        return;
    }

    fs::path userInbox = mail_directory / username; // Path to the user's inbox
    if (!fs::exists(userInbox) || !fs::is_directory(userInbox))
    {
        std::cout << "No messages or user unknown" << std::endl;
        send(consfd, "ERR\n", 4, 0);
        return;
    }

    int counter = 0;
    bool messageFound = false;
    for (const auto &entry : fs::directory_iterator(userInbox))
    {
        counter++;
        if (counter == messageNr)
        {
            // Attempt to delete the message file
            if (fs::remove(entry.path()))
            {
                messageFound = true; // Mark as found if deletion was successful
                break;
            }
            else
            {
                send(consfd, "ERR\n", 4, 0); // Failed to delete the file
                return;
            }
        }
    }

    if (messageFound)
    {
        send(consfd, "OK\n", 3, 0); // Message deleted successfully
    }
    else
    {
        send(consfd, "ERR\n", 4, 0); // Message number does not exist
    }
}

const bool Server::is_valid_username(const std::string &name) 
{
    // Prüfen, ob die Länge des Strings größer als 8 ist
    if (name.length() > 8) {
        return false;
    }

    // Jeden Charakter im String checken ob 0-9 oder a-z
    for (const char &c : name) 
    {
        if (!(isdigit(c) || (islower(c)))) 
        {
            return false;
        }
    }

    return true;
}
