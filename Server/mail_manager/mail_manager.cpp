#include "mail_manager.h"
#include <semaphore.h>
#include <filesystem>

MailManager::MailManager(const std::filesystem::path &mail_directory):mail_directory(mail_directory) {
}

void MailManager::handle_list(int consfd, const std::string &authenticatedUser, sem_t *sem) {
    // TODO
}

void MailManager::handle_send(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem) {
    // TODO
}

void MailManager::handle_read(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem) {
    // TODO
}

void MailManager::handle_delete(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem) {
    // TODO
}