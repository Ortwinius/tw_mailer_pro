#ifndef SERVER_H
#define SERVER_H

#include <filesystem>
#include <string>
#include <semaphore.h>
#include "../utils/constants.h"
#include "mail_manager/mail_manager.h"

namespace fs = std::filesystem;

class Server 
{
public:
    Server(int port, const std::filesystem::path& mail_directory);
    ~Server();
    void run();

private:
    int attempted_logins_cnt;
    int port;
    int socket_fd;
    MailManager mail_manager;
    std::filesystem::path mail_directory;

    void init_socket();
    void listen_for_connections();
    void handle_communication(int consfd, sem_t *sem);
    void resizeBuffer(char *&buffer, ssize_t& currentSize, ssize_t newCapacity);
    bool checkContentLengthHeader(std::string &contentLengthHeader, int &contentLength);
    void handle_login(int consfd, const std::string &buffer, std::string &username, bool &loggedIn);
    void handle_list(int consfd, const std::string &authenticatedUser, sem_t *sem);
    void handle_send(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem);
    void handle_read(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem);
    void handle_delete(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem);
    const bool is_valid_username(const std::string &name);
    const void send_error(const int consfd, const std::string errorMessage);
};

#endif