#ifndef SERVER_H
#define SERVER_H

#include <filesystem>
#include <string>

class Server 
{
public:
    Server(int port, const std::filesystem::path& mail_directory);
    ~Server();
    void run();

private:
    int port;
    std::filesystem::path mail_directory;
    int socket_fd;

    void init_socket();
    void listen_for_connections();
    void handle_communication(int consfd);
    void handle_list(int consfd, const std::string &buffer);
    void handle_send(int consfd, const std::string &buffer);
    void handle_read(int consfd, const std::string &buffer);
    void handle_delete(int consfd, const std::string &buffer);
    const bool is_valid_username(const std::string &name);
};

#endif