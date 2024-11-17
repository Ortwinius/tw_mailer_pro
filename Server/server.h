#ifndef SERVER_H
#define SERVER_H

#include <filesystem>
#include <string>
#include <semaphore.h>
#include "../utils/constants.h"
#include "MailManager/mail_manager.h"
#include "Blacklist/blacklist.h"

namespace fs = std::filesystem;

class Server 
{
public:
    Server(int port, const std::filesystem::path& mail_directory);
    ~Server();
    void run();

private:
    int attempted_logins_cnt;
    bool loggedIn=false;
    std::string authenticatedUser;
    int port;
    int socket_fd;
    MailManager mail_manager;
    Blacklist blacklist;

    void init_socket();
    void listen_for_connections();
    void handle_communication(int consfd, sem_t *sem, std::string client_ip);
    void resizeBuffer(char *&buffer, ssize_t& currentSize, ssize_t newCapacity);
    bool checkContentLengthHeader(std::string &contentLengthHeader, int &contentLength);
    void handle_login(int consfd, const std::string &buffer, std::string &authenticated_user, bool &logged_in, std::string client_ip);
    const bool is_valid_username(const std::string &name);
    const void send_error(const int consfd, const std::string errorMessage);
};

#endif