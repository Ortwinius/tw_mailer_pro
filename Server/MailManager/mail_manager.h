#ifndef MAIL_MANAGER_H
#define MAIL_MANAGER_H

#include <filesystem>
#include <semaphore.h>

namespace fs = std::filesystem;

class MailManager 
{
public:
    MailManager(const std::filesystem::path &mail_directory);

    void handle_list(int consfd, const std::string &authenticated_user, sem_t *sem);
    void handle_send(int consfd, const std::string &buffer, const std::string &authenticated_user, sem_t *sem);
    void handle_read(int consfd, const std::string &buffer, const std::string &authenticated_user, sem_t *sem);
    void handle_delete(int consfd, const std::string &buffer, const std::string &authenticated_user, sem_t *sem);
    
private:
    std::filesystem::path mail_directory;
};
#endif //MAIL_MANAGER_H