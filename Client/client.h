#ifndef CLIENT_H
#define CLIENT_H

#include <string>

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

    // Private methods for internal functionality
    void init_socket();
    void connect_to_server();
    void handle_user_input();
    void send_command(const std::string &command);
    void merge_multiline_command(const std::string &initial_command);
    void handle_response();
    void handle_send();
    void handle_list();
    void handle_read();
    void handle_delete();
    void handle_quit();
};

#endif // CLIENT_H
