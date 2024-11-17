#include <string>
#include <map>
#include <semaphore.h>
#include <unordered_map>
class Blacklist
{
private:
    const char* shm_name = "/blacklist_shm";
    const char* sem_name = "/blacklist_sem";
    size_t shm_size = 8192; // Shared memory size

    int shm_fd;
    void* shm_ptr;
    sem_t* shm_sem;
    std::unordered_map<std::string, time_t>* blacklist;
    const int BLACKLIST_DURATION = 60;

public:
    Blacklist();
    ~Blacklist();

    void add(const std::string& ip);
    bool is_blacklisted(const std::string &ip);
    time_t getTimestamp(const std::string& ip);
    void clean();
};
