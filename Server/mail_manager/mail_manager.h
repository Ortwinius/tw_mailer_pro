#include <filesystem>
#include <semaphore.h>

class MailManager 
{
public:
    MailManager(const std::filesystem::path &mail_directory);

    void handle_list(int consfd, const std::string &authenticatedUser, sem_t *sem);
    void handle_send(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem);
    void handle_read(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem);
    void handle_delete(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem);
private:
    std::filesystem::path mail_directory;
};
