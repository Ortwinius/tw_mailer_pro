#ifndef SERVER_H
#define SERVER_H

#include <filesystem>
#include <string>
#include <semaphore.h>
#include "MailManager/mail_manager.h"
#include "Blacklist/blacklist.h"
#include "LdapModule/ldap_module.h"

namespace fs = std::filesystem;

class Server 
{
public:
    Server(int port, const std::filesystem::path& mail_directory);
    ~Server();
    void run();


private:
    int port;
    int socket_fd;
    int attempted_logins_cnt;
    bool loggedIn=false;
    std::string authenticatedUser;

    MailManager mail_manager;
    Blacklist blacklist;

    sem_t mail_sem;           // Semaphore for mail management
    sem_t blacklist_sem;      // Semaphore for blacklist management

    void init_socket();
    void listen_for_connections();
    void handle_communication(int consfd, std::string client_addr_ip);
    void handle_login(int consfd, const std::string &buffer, std::string &authenticated_user, bool &logged_in, std::string client_ip);
};

#endif