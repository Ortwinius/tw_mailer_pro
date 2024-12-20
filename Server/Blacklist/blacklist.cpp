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

Blacklist::Blacklist()
{
    std::ofstream blacklist_file(path, std::ios::app);
    if (!blacklist_file) {
        std::cout << "Error creating or accessing the blacklist file at " << path << std::endl;
    } else {
        std::cout << "Blacklist file ensured at " << path << std::endl;
    }
}

void Blacklist::add(const std::string &ip, sem_t *blacklist_sem)
{
    sem_wait(blacklist_sem);

    std::ofstream blacklist_file(path, std::ios::app); // Open file in append mode
    if (!blacklist_file)
    {
        std::cout << "Failed to open blacklist file for writing." << std::endl;
        sem_post(blacklist_sem);

        return;
    }

    time_t curr_time = std::time(nullptr);                 // Get current timestamp
    blacklist_file << ip << "," << curr_time << std::endl; // Write the IP and timestamp
    sem_post(blacklist_sem);
    std::cout << "IP added to blacklist: " << ip << " at " << curr_time << std::endl;
}

bool Blacklist::is_blacklisted(const std::string &ip, sem_t *blacklist_sem)
{
    // Lock the semaphore
    sem_wait(blacklist_sem);

    std::ifstream blacklist_file(path);
    if (!blacklist_file)
    {
        std::cout << "Failed to open blacklist file for reading." << std::endl;
        // Unlock semaphore before returning
        sem_post(blacklist_sem);
        return false;
    }

    std::string line;
    time_t curr_time = std::time(nullptr); // Get current timestamp
    bool blacklisted = false;

    while (std::getline(blacklist_file, line))
    {
        std::istringstream line_stream(line);
        std::string stored_ip;
        std::string timestamp_str;

        if (std::getline(line_stream, stored_ip, ',') && std::getline(line_stream, timestamp_str))
        {
            // Convert the timestamp to time_t
            time_t timestamp = std::stoll(timestamp_str);

            // If the IP matches and the timestamp is within 60 seconds
            if (stored_ip == ip && difftime(curr_time, timestamp) <= ServerConstants::BLACKLIST_TIMEOUT)
            {
                blacklisted = true;
                break;
            }
        }
    }

    // Unlock the semaphore before returning
    sem_post(blacklist_sem);

    return blacklisted;
}

void Blacklist::cleanUp(sem_t *blacklist_sem)
{
    sem_wait(blacklist_sem);

    std::ifstream blacklist_file(path);
    if (!blacklist_file)
    {
        std::cout << "Failed to open blacklist file for reading." << std::endl;
        sem_post(blacklist_sem);
        return;
    }

    std::stringstream valid_entries;
    std::string line;
    time_t curr_time = std::time(nullptr); // Get current timestamp

    // Read through the blacklist and filter out expired entries
    while (std::getline(blacklist_file, line))
    {
        std::istringstream line_stream(line);
        std::string stored_ip;
        std::string timestamp_str;

        if (std::getline(line_stream, stored_ip, ',') && std::getline(line_stream, timestamp_str))
        {
            time_t timestamp = std::stoll(timestamp_str);

            // Keep only those entries that are not older than BLACKLIST_TIMEOUT
            if (difftime(curr_time, timestamp) <= ServerConstants::BLACKLIST_TIMEOUT)
            {
                valid_entries << stored_ip << "," << timestamp_str << std::endl;
            }
        }
    }

    // Open file and only print valid entries in file
    std::ofstream blacklist_file_out(path, std::ios::trunc);
    if (!blacklist_file_out)
    {
        std::cout << "Failed to open blacklist file for cleaning." << std::endl;
        sem_post(blacklist_sem);
        return;
    }

    blacklist_file_out << valid_entries.str();

    sem_post(blacklist_sem);
    std::cout << "Blacklist cleaned up." << std::endl;
}
