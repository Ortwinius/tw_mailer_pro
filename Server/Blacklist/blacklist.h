#ifndef BLACKLIST_H
#define BLACKLIST_H

#include <string>
#include <semaphore.h>

class Blacklist {
public:
    // Constructor and Destructor
    Blacklist();
    ~Blacklist();

    // Check if an IP is blacklisted
    bool is_blacklisted(const std::string& ip, sem_t* blacklist_sem);

    // Add an IP to the blacklist with the current timestamp
    void add(const std::string& ip, sem_t* blacklist_sem);

    // Clean up expired blacklist entries
    void cleanUp(sem_t* blacklist_sem);

private:
    const std::string path="Server/Blacklist/blacklist.txt";
    // Private helper functions or member variables can be added here if needed.
};

#endif // BLACKLIST_H
