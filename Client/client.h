#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <unordered_map>
#include "commandBuilder/commandBuilder.h"
#include "../utils/helpers.h"

class Client
{
public:
    // Constructor to initialize IP and port
    Client(const std::string& ip, int port);
    
    // Destructor
    ~Client();

    // Public method to start the client
    void start();

private:
    std::string ip_address;
    int port;
    int socket_fd;
    // user credentials
    std::string username;
    bool logged_in;
    // flags
    bool clientsideValidation;

    // Private methods for internal functionality
    void init_socket();
    void connect_to_server();
    void handle_user_input();
    void send_command(const std::string &command);
    void handle_login();
    void handle_response();
    void handle_send();
    void handle_list();
    void handle_read();
    void handle_delete();
    void handle_quit();
    bool validate_action();
};

#endif // CLIENT_H
