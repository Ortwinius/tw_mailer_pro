#ifndef LDAP_MODULE_H
#define LDAP_MODULE_H

#include <ldap.h>
#include <lber.h>
#include <string>
#include <stdexcept>
#include <iostream>

class LDAP_Module {
public:
    LDAP_Module(const std::string& ldap_url);
    ~LDAP_Module();

    bool authenticate(const std::string& username, const std::string& password);

private:
    std::string ldap_url;
    LDAP* ldap_obj;
    
    void init_ldap();
    void cleanup();
};

#endif // LDAP_MODULE_H