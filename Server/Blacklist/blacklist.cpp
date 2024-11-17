#include "blacklist.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <ctime>
#include "../../utils/constants.h"

Blacklist::Blacklist() {
    // Create or open shared memory
    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        throw std::runtime_error("Failed to create shared memory: " + std::to_string(errno));
    }

    // Resize the shared memory
    if (ftruncate(shm_fd, shm_size) == -1) {
        throw std::runtime_error("Failed to resize shared memory: " + std::to_string(errno));
    }

    // Map shared memory to address space
    shm_ptr = mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        throw std::runtime_error("Failed to map shared memory: " + std::to_string(errno));
    }

    // Initialize semaphore
    shm_sem = sem_open(sem_name, O_CREAT, 0666, 1);
    if (shm_sem == SEM_FAILED) {
        throw std::runtime_error("Failed to create semaphore: " + std::to_string(errno));
    }

    // Initialize the unordered_map (hashmap) in shared memory
    blacklist = new (shm_ptr) std::unordered_map<std::string, time_t>();
}

Blacklist::~Blacklist() {
    // Clean up shared memory and semaphore
    munmap(shm_ptr, shm_size);
    close(shm_fd);
    shm_unlink(shm_name);
    sem_close(shm_sem);
    sem_unlink(sem_name);
}

void Blacklist::add(const std::string& ip) {
    sem_wait(shm_sem); // Lock
    time_t currTime = std::time(nullptr); // Get current timestamp
    (*blacklist)[ip] = currTime;
    sem_post(shm_sem); // Unlock
    std::cout << "IP added to blacklist: " << ip << " at " << currTime << std::endl;
}

bool Blacklist::is_blacklisted(const std::string& ip) {
    sem_wait(shm_sem);

    // Check if the IP is in the blacklist
    auto it = blacklist->find(ip);
    if (it != blacklist->end()) {
        
        time_t current_time = std::time(nullptr);  // Get current time
        time_t timestamp = it->second;  // Get timestamp from the blacklist entry

        // Check if the difference between current time and the timestamp is within ServerConstants::BLACKLIST_TIMEOUT
        if (current_time - timestamp <= ServerConstants::BLACKLIST_TIMEOUT) {
            sem_post(shm_sem); // Unlock shared memory
            return true;  // IP is blacklisted and not expired
        }
    }

    sem_post(shm_sem); 
    return false;  // Either IP is not in blacklist or expired
}

void Blacklist::clean() {
    sem_wait(shm_sem); 

    time_t now = std::time(nullptr); // Get current time
    auto it = blacklist->begin();

    while (it != blacklist->end()) {
        
        time_t time_diff = now - it->second;

        if (time_diff > ServerConstants::BLACKLIST_TIMEOUT) {
            // IP is expired, remove it
            std::cout << "Removing IP " << it->first << " due to expiration (last added " 
                      << time_diff << " seconds ago)" << std::endl;
            it = blacklist->erase(it);  // Erase and move to the next element
        } else {
            ++it;  // Move to the next IP
        }
    }

    sem_post(shm_sem);
}