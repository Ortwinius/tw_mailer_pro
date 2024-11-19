#ifndef CONSTANTS_H
#define CONSTANTS_H

namespace GenericConstants
{
    constexpr ssize_t STD_BUFFER_SIZE = 64;
}
namespace ServerConstants
{
    constexpr int MAX_PENDING_CONNECTIONS = 6;
    constexpr int MAX_LOGIN_ATTEMPTS = 3;

    // LDAP
    constexpr const char *HOST_URL = "ldap://ldap.technikum-wien.at:389";
    constexpr int DESIRED_LDAP_VERSION = 3; // from ldap.h LDAP_VERSION_3 enum

    // Blacklist
    constexpr int BLACKLIST_TIMEOUT = 60; // in sec
    
    // Responses
    constexpr const char* RESPONSE_OK = "OK\n";
    constexpr const char* RESPONSE_ERR = "ERR\n";
    constexpr const char* RESPONSE_UNAUTHORIZED = "Unauthorized\n";    
}

#endif