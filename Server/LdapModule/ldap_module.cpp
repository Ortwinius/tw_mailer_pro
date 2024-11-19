#include "ldap_module.h"
#include "../../utils/constants.h"

LDAP_Module::LDAP_Module(const std::string& ldap_url)
    : ldap_url(ldap_url)
    , ldap_obj(nullptr)
{
    init_ldap();
}

LDAP_Module::~LDAP_Module()
{
    cleanup();
}

// Initialize LDAP object
void LDAP_Module::init_ldap() {
    int result = ldap_initialize(&ldap_obj, ldap_url.c_str());
    if (result != LDAP_SUCCESS) {
        std::cout << "LDAP initialization failed: " << ldap_err2string(result) << std::endl;
        throw std::runtime_error("Error initializing LDAP");
    }

    result = ldap_set_option(ldap_obj, LDAP_OPT_PROTOCOL_VERSION, &ServerConstants::DESIRED_LDAP_VERSION);
    if (result != LDAP_SUCCESS) {
        std::cout << "Failed to set LDAP version: " << ldap_err2string(result) << std::endl;
        cleanup();
        throw std::runtime_error("Error setting LDAP version");
    }

    std::cout << "LDAP initialized and set to version 3\n";
}

bool LDAP_Module::authenticate(const std::string &username, const std::string &password) {

    std::string dn = "uid=" + username + ",ou=People,dc=technikum-wien,dc=at";
    // cast password to LDAP-compatible BER-type
    BerValue cred;
    cred.bv_val = const_cast<char*>(password.c_str()); //const_cast to remove (const) bc ber-type expects non-const
    cred.bv_len = password.length();

    // Perform SASL bind using SIMPLE mechanism
    int result = ldap_sasl_bind_s(
        ldap_obj,       // LDAP session handle
        dn.c_str(),     // Distinguished Name (DN)
        nullptr,        // Mechanism ("SIMPLE" uses nullptr)
        &cred,          // Pointer to credentials (BerValue)
        nullptr,        // Server controls (nullptr for none)
        nullptr,        // client_addr controls (nullptr for none)
        nullptr         // Pointer for result server credentials (not needed here)
    );

    if (result == LDAP_SUCCESS) {
        std::cout << "LDAP SASL bind successful for user: " << username << std::endl;
        return true;
    } else {
        std::cout << "LDAP SASL bind failed: " << ldap_err2string(result) << std::endl;
        return false;
    }
}

void LDAP_Module::cleanup() {
    if (ldap_obj) {
        ldap_unbind_ext_s(ldap_obj, nullptr, nullptr);
        ldap_obj = nullptr;
    }
}
